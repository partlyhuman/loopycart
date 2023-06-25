#include "Adafruit_TinyUSB.h"
#include "Wire.h"
#include "SPI.h"
#include "MCP23S17.h"

#define PROTOCOL_VERSION 1

// Delays one clock cycle or 7ns | 133MhZ = 0.000000007518797sec = 7.518797ns
#define NOP __asm__("nop\n\t")
#define HALT while (true)
#define W(pin, val) digitalWriteFast(pin, val)

#define PIN_RAMCS1 12
#define PIN_RAMCS2 13
#define PIN_ROMCE 11
#define PIN_OE 10
#define PIN_ROMWE 14
#define PIN_RAMWE 15

// Alias as BE0 is used by Sharp to mean CE
#define PIN_ROMBE0 PIN_ROMCE


// Using A0 to address bytes (e.g. on SRAM)
// ROM addresses will always have A0=0, A1 is the first address pin as ROM uses 2-byte words
// 16 bits of address are on IO expander mcpA
// Remainder of address pins on pico GPIO
#define PIN_A0 0
#define PIN_A17 1
#define PIN_A18 2
#define PIN_A19 4
#define PIN_A20 5
#define PIN_A21 6
#define ADDRBITS 22

#define CMD_RESET 0xff

// status register
static uint16_t SRD;
#define SR(n) bitRead(SRD, n)

Adafruit_USBD_WebUSB usb_web;
#define WEBUSB_HTTP 0
#define WEBUSB_HTTPS 1
WEBUSB_URL_DEF(landingPage, WEBUSB_HTTPS, "loopycart.surge.sh");

// SPI is pico's default SPI0: TX=19, SCK=18, CS=17, RX=16
#define SPI_CS_PIN 17  //SPI_CS
// ~RESET is pulled high when pico is powered up, >=99 means dummy reset pin
#define MCP_NO_RESET_PIN 100
// SPI addresses for MCP23017 are: 0 1 0 0 A2 A1 A0
MCP23S17 mcpA = MCP23S17(SPI_CS_PIN, MCP_NO_RESET_PIN, 0b0100001);  //Address IO, Address 0x1
MCP23S17 mcpD = MCP23S17(SPI_CS_PIN, MCP_NO_RESET_PIN, 0b0100000);  //Data IO,    Address 0x0

// sprintf buffer
char S[128];
// timing operations
unsigned long stopwatch;

void flashLed(int n, int d = 100) {
  for (int i = 0; i < n; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(d / 2);
    digitalWrite(LED_BUILTIN, LOW);
    delay(d / 2);
  }
}

inline void ioReadMode(MCP23S17 *mcp) {
  mcp->setPortMode(0, A);
  mcp->setPortMode(0, B);
}

inline void databusReadMode() {
  ioReadMode(&mcpD);
}

inline void ioWriteMode(MCP23S17 *mcp) {
  mcp->setPortMode(0b11111111, A);
  mcp->setPortMode(0b11111111, B);
}

inline void databusWriteMode() {
  ioWriteMode(&mcpD);
}

// TODO REMOVE -----------
inline void flashReadMode() {
  ioWriteMode(&mcpA);
  ioReadMode(&mcpD);
  setWeOeBe(HIGH, HIGH, HIGH);
  delayMicroseconds(1);
}

inline void flashWriteMode() {
  ioWriteMode(&mcpA);
  ioWriteMode(&mcpD);
  setWeOeBe(HIGH, HIGH, HIGH);
  delayMicroseconds(1);
}

inline void flashIdleMode() {
  setWeOeBe(HIGH, HIGH, HIGH);
  delayMicroseconds(1);
}
// TODO END REMOVE -----------

// NOW we expect you to send BYTE addresses, if you have a word address it must be <<1
inline void setAddress(uint32_t addr) {
  // for (int i = 0, mask = 1; i < ADDRBITS; i++, mask <<= 1) {
  //   digitalWrite(PIN_A0 + i, (mask & addr) > 0);
  // }

  // A1-A16 on mcpA
  mcpA.setPort((addr >> 1) & 0xff, B);
  mcpA.setPort((addr >> 9) & 0xff, A);

  // remaining
  digitalWriteFast(PIN_A0, addr & 0b1);
  digitalWriteFast(PIN_A17, (addr >> 17) & 0b1);
  digitalWriteFast(PIN_A18, (addr >> 18) & 0b1);
  digitalWriteFast(PIN_A19, (addr >> 19) & 0b1);
  digitalWriteFast(PIN_A20, (addr >> 20) & 0b1);
  digitalWriteFast(PIN_A21, (addr >> 21) & 0b1);

  // TODO: Could use pico port with A17-A21
  // sio_hw->gpio_clr = 0b11111111111111111111 << PIN_A0;
  // sio_hw->gpio_set = addr << PIN_A0;
}

inline uint16_t readAddress() {
  return digitalReadFast(PIN_A0)
         | (mcpA.getPort(B) << 1)
         | (mcpA.getPort(A) << 9)
         | (digitalReadFast(PIN_A17) << 17)
         | (digitalReadFast(PIN_A18) << 18)
         | (digitalReadFast(PIN_A19) << 19)
         | (digitalReadFast(PIN_A20) << 20)
         | (digitalReadFast(PIN_A21) << 21);
}

inline uint16_t readData() {
  return (mcpD.getPort(B) << 8) | mcpD.getPort(A);
}

inline void writeWord(uint32_t addr, uint16_t word) {
  // Don't forget to switch data bus mode
  setAddress(addr);
  //delayMicroseconds()
  mcpD.setPort(word & 0xff, A);
  mcpD.setPort(word >> 8, B);
}

uint16_t readWord(uint32_t addr, bool swapend = false) {
  busIdle();

  setAddress(addr);

  busRead();

  // tAVQV | Address to output delay | MAX 100ns
  NOP;
  NOP;
  NOP;
  NOP;
  NOP;
  NOP;
  NOP;
  NOP;
  NOP;
  NOP;
  NOP;
  NOP;
  NOP;
  NOP;
  NOP;

  uint16_t data = readData();
  busIdle();

  // tAVAV | Read Cycle Time | MIN 120ns
  NOP;
  NOP;
  NOP;

  return data;
}

// TODO replace with bus* -----------
inline void setWeOeBe(int we, int oe, int be) {
  digitalWriteFast(PIN_ROMWE, we);
  digitalWriteFast(PIN_OE, oe);
  digitalWriteFast(PIN_ROMCE, be);
}
// -------------------------

inline void busRead() {
  W(PIN_ROMBE0, LOW);
  W(PIN_OE, LOW);
  W(PIN_ROMWE, HIGH);
  databusReadMode();
}

inline void busWrite() {
  W(PIN_ROMBE0, LOW);
  W(PIN_OE, HIGH);
  W(PIN_ROMWE, LOW);
  databusWriteMode();
}

inline void busIdle() {
  W(PIN_ROMBE0, HIGH);
  W(PIN_OE, HIGH);
  W(PIN_ROMWE, HIGH);
  databusWriteMode();
}

inline void flashCommand(uint16_t data) {
  busIdle();

  // tEHEL | BE# Pulse Width High | Min 25ns
  // This will easily be accomplished during the following writeWord.
  NOP;
  NOP;
  NOP;
  NOP;

  mcpD.setPort(data & 0xff, A);
  mcpD.setPort(data >> 8, B);

  busWrite();

  // tELEH | BE# Pulse Width | Min 70ns
  NOP;
  NOP;
  NOP;
  NOP;
  NOP;
  NOP;
  NOP;
  NOP;
  NOP;
  NOP;

  busIdle();
}


// TODO make a version that skips setting address
inline void flashCommand(uint32_t addr, uint16_t data) {
  busIdle();

  // tEHEL | BE# Pulse Width High | Min 25ns
  // This will easily be accomplished during the following writeWord.
  NOP;
  NOP;
  NOP;
  NOP;

  // Don't forget to switch data bus mode
  writeWord(addr, data);

  busWrite();

  // tELEH | BE# Pulse Width | Min 70ns
  NOP;
  NOP;
  NOP;
  NOP;
  NOP;
  NOP;
  NOP;
  NOP;
  NOP;
  NOP;

  busIdle();
}

// function to echo to both Serial and WebUSB
void echo_all(const char *buf, uint32_t count = 0) {
  if (count == 0) {
    count = strlen(buf);
  }
  if (usb_web.connected()) {
    usb_web.write((uint8_t *)buf, count);
    usb_web.flush();
  }

  if (Serial) {
    Serial.write(buf);
    // for (uint32_t i = 0; i < count; i++) {
    //   Serial.write(buf[i]);
    //   if (buf[i] == '\r') Serial.write('\n');
    // }
    Serial.flush();
  }
}

void flashEraseBank(int bank) {
  int32_t bankAddress = bank ? (1 << 21) : 0;
  int len = sprintf(S, "Erase bank address %06xh\r", bankAddress);
  echo_all(S, len);

  delayMicroseconds(100);
  flashCommand(bankAddress, 0x70);
  do {
    delayMicroseconds(100);
    flashReadStatus();
  } while (SR(7) == 0);
  delayMicroseconds(100);

  delayMicroseconds(100);
  flashCommand(bankAddress, 0x30);
  delayMicroseconds(100);
  flashCommand(bankAddress, 0xd0);

  const int TIMEOUT_MS = 30000;
  const int CHECK_INTERVAL_MS = 1000;
  for (int time = 0; time < TIMEOUT_MS; time += CHECK_INTERVAL_MS) {
    flashReadStatus();
    if (SR(7)) {
      echo_all("DONE!\r");
      break;
    } else {
      echo_all(".");
      delay(CHECK_INTERVAL_MS);
    }
  }

  if (SR(4) == 1 && SR(5) == 1) {
    echo_all("Invalid Bank Erase command sequence\r");
  } else if (SR(5) == 1) {
    echo_all("Bank Erase error\r");
  } else {
    echo_all("Bank Erase successful!\r");
  }

  flashCommand(0, CMD_RESET);
  // Bank erase typ 17.6s
  // delay(20000);
}

void flashErase() {
  stopwatch = millis();
  digitalWrite(LED_BUILTIN, HIGH);

  echo_all("Erasing bank 0...\r");
  flashEraseBank(0);

  echo_all("Erasing bank 1...\r");
  flashEraseBank(1);

  flashCommand(0, CMD_RESET);

  digitalWrite(LED_BUILTIN, LOW);
  int len = sprintf(S, "Erased in %f sec\r", (millis() - stopwatch) / 1000.0);
  echo_all(S, len);
}


// This should display the manufacturer and device code: B0 D0
void flashId() {
  busIdle();
  delayMicroseconds(100);

  flashCommand(0, 0x90);
  delayMicroseconds(100);

  busRead();
  delayMicroseconds(100);

  uint16_t manufacturer = readWord(0);

  busIdle();
  delayMicroseconds(100);

  delay(100);
  uint16_t device = readWord(2);

  databusWriteMode();
  flashCommand(0, CMD_RESET);
  delay(100);

  int len = sprintf(S, "Manufacturer=%x Device=%x\r", manufacturer, device);
  echo_all(S, len);
}

void flashReadStatus() {
  busRead();
  NOP;
  NOP;
  NOP;
  NOP;
  NOP;
  NOP;
  SRD = readData();
  busIdle();
}

void flashInspect(uint32_t starting = 0, uint32_t upto = 1 << ADDRBITS) {
  // flashWriteMode();
  flashCommand(0, 0xff);
  delayMicroseconds(1);
  // flashReadMode();
  busRead();
  delayMicroseconds(1);

  int len;
  for (uint32_t addr = starting; addr < upto; addr += 2) {
    if (addr % 0x10 == 0) {
      echo_all("\r", 1);
      len = sprintf(S, "%06xh\t\t", addr);
      echo_all(S, len);
    }
    len = sprintf(S, "%04x\t", readWord(addr));
    echo_all(S, len);
  }

  echo_all("\r\n\r\n", 4);
  busIdle();
}

void flashDump(uint32_t starting = 0, uint32_t upto = 1 << ADDRBITS) {
  flashWriteMode();
  flashCommand(0, CMD_RESET);
  delayMicroseconds(1);
  flashReadMode();
  delayMicroseconds(1);

  digitalWrite(LED_BUILTIN, HIGH);
  for (uint32_t addr = starting; addr < upto; addr += 2) {
    uint16_t word = readWord(addr);
    usb_web.write((uint8_t *)&word, 2);
  }
  digitalWrite(LED_BUILTIN, LOW);

  flashIdleMode();
}

inline void flashProgram(uint32_t addr, uint16_t word) {
  flashCommand(addr, 0x40);
  flashCommand(addr, word);
  // TODO check status?
}

bool isProgramming = false;
uint32_t addr = 0;
uint32_t expectedWords;

void loop() {
  static uint8_t buf[64];
  uint32_t bufLen;
  int len;
  digitalWrite(LED_BUILTIN, LOW);

  if (!usb_web.available()) {
    delay(1);
    return;
  }

  bufLen = usb_web.read(buf, 64);

  if (bufLen == 0) {
    return;
  }

  // MULTIBYTE WRITE
  if (isProgramming) {

    if ((bufLen % 2) == 1) {
      len = sprintf(S, "WARNING: odd number of bytes %d\r", bufLen);
      echo_all(S, len);
    } else {
      // len = sprintf(S, "Programming block of %d bytes\r", bufLen);
      // echo_all(S, len);
      // echo_all(".", 1);
    }

    // echo progress every 16kb
    if ((addr & 8191) == 0) {
      len = sprintf(S, "%06xh ", addr);
      echo_all(S, len);
    }

    // All of these are in BYTES
    // addr - overall address
    // buf - multi-byte buffer sent over by USB
    // bufLen - length of it
    // bufPtr - step through buffer using this

    // Multi-word write can write up to 32 bytes / 16 words
    for (int bufPtr = 0; bufPtr < bufLen;) {
      int bytesToWrite = MIN(bufLen - bufPtr, 32);
      int wordsToWrite = bytesToWrite / 2;

      SRD = 0;
      do {
        flashCommand(addr, 0xe8);
        // delayMicroseconds(100);
        flashReadStatus();

        if (SR(7)) {
          break;
        } else {
          delayMicroseconds(1);
        }

        if (SR(1) == SR(4) == 1) {
          echo_all("Block lock error\r");
          isProgramming = false;
          return;
        }
        if (SR(3) == SR(4) == 1) {
          echo_all("Undervoltage error\r");
          isProgramming = false;
          return;
        }
        if (SR(4) == 1 || SR(5) == 1) {
          echo_all("Unable to multibyte write\r");
          isProgramming = false;
          return;
        }
      } while (!SR(7));

      // XSR.7 == 1 now, ready for write
      // A word/byte count (N)-1 is written with write address.
      flashCommand(addr, wordsToWrite - 1);

      // On the next write, device start address is written with buffer data.
      // Subsequent writes provide additional device address and data, depending on the count.
      // All subsequent address must lie within the start address plus the count.
      for (int j = 0; j < wordsToWrite; j++, addr += 2, bufPtr += 2) {
        uint16_t word = buf[bufPtr] << 8 | buf[bufPtr + 1];
        flashCommand(addr, word);
        // writeWord(addr, word);
      }

      // After the final buffer data is written, write confirm (DOH) must be written.
      // This initiates WSM to begin copying the buffer data to the Flash Array.
      flashCommand(0, 0xd0);
    }


    // for (int i = 0; i < bufLen; i += 2, addr += 2) {
    //   // echo progress every 16kb
    //   if ((addr & 8191) == 0) {
    //     len = sprintf(S, "%06xh\r", addr);
    //     echo_all(S, len);
    //   }
    //   uint16_t word = buf[i] << 8 | buf[i + 1];
    //   // Skip padding assuming you have erased first
    //   if (word == 0xffff) continue;
    //   flashProgram(addr, word);
    // }

    if (addr >= 2 * expectedWords) {
      do {
        delayMicroseconds(100);
        flashReadStatus();
      } while (!SR(7));

      flashCommand(0, CMD_RESET);

      isProgramming = false;
      len = sprintf(S, "Finished! Wrote %d bytes in %f sec\r", addr * 2, (millis() - stopwatch) / 1000.0);
      echo_all(S, len);
      flashIdleMode();

      W(LED_BUILTIN, LOW);
    }
  }  // isProgramming

  else if (buf[0] == 'E' && buf[1] == '\r') {
    // ERASE COMMAND
    flashErase();
  }

  else if (buf[0] == 'I' && buf[1] == '\r') {
    // INSPECT COMMAND
    flashId();
    flashInspect(0x000000, 0x000400);
    flashInspect(0x100000, 0x100400);
    flashInspect(0x200000, 0x200400);
    flashInspect(0x300000, 0x300400);
  }

  else if (buf[0] == 'D') {
    // DUMP COMMAND
    // followed by expected # of bytes
    if (sscanf((char *)buf, "D%d\r", &expectedWords) && expectedWords > 0 && expectedWords <= 1 << ADDRBITS) {
      flashDump(0, expectedWords);
    }
    // flashDump();
  }

  else if (buf[0] == 'P') {
    // PROGRAM COMMAND
    // followed by expected # of bytes
    if (sscanf((char *)buf, "P%d\r", &expectedWords)) {
      len = sprintf(S, "Programming %d bytes\r", expectedWords * 2);
      echo_all(S, len);

      isProgramming = true;
      addr = 0;
      stopwatch = millis();
      flashIdleMode();

      // Clear any status registers?
      databusWriteMode();
      flashCommand(0, 0x50);
      delayMicroseconds(100);

      digitalWrite(LED_BUILTIN, HIGH);
    }
  }
}

void line_state_callback(bool connected) {
  if (connected) {
    usb_web.print(PROTOCOL_VERSION, DEC);
    usb_web.print("\r\n");
    usb_web.write('\0');
    usb_web.print("CONNECTED\r");
    usb_web.flush();
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // USB setup
  TinyUSB_Device_Init(0);
  usb_web.setLandingPage(&landingPage);
  usb_web.setLineStateCallback(line_state_callback);
  usb_web.setStringDescriptor("Loopycart");
  usb_web.begin();

  Serial.begin(115200);

  // Setup IO expanders
  SPI.begin();
  if (!mcpA.Init()) HALT;
  if (!mcpD.Init()) HALT;
  Serial.println("IO expanders initialized");
  // Rated for 10MHZ
  mcpA.setSPIClockSpeed(10000000);
  mcpD.setSPIClockSpeed(10000000);
  ioWriteMode(&mcpD);
  ioWriteMode(&mcpA);

  // Disable SRAM - CS2 pulled low, redundant but set CS1 high
  pinMode(PIN_RAMCS2, OUTPUT);
  pinMode(PIN_RAMCS1, OUTPUT);
  pinMode(PIN_RAMWE, OUTPUT);
  digitalWrite(PIN_RAMCS2, LOW);
  digitalWrite(PIN_RAMCS1, HIGH);
  digitalWrite(PIN_RAMWE, HIGH);

  // Setup ROM pins
  pinMode(PIN_ROMCE, OUTPUT);
  pinMode(PIN_ROMWE, OUTPUT);
  pinMode(PIN_OE, OUTPUT);

  // Setup overflow address pins
  pinMode(PIN_A0, OUTPUT);
  pinMode(PIN_A17, OUTPUT);
  pinMode(PIN_A18, OUTPUT);
  pinMode(PIN_A19, OUTPUT);
  pinMode(PIN_A20, OUTPUT);
  pinMode(PIN_A21, OUTPUT);

  flashIdleMode();

  while (!TinyUSBDevice.mounted()) {
    delay(1);
  }
}
