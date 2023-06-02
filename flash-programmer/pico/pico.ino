#include "Adafruit_TinyUSB.h"
#include "Wire.h"
#include "TCA9539.h"

#define NOP __asm__("nop\n\t")

// can eliminate because of CS2 pulldown? test let floating, test high
#define PIN_RAMCS1 26
#define PIN_CE 29
#define PIN_OE 28
#define PIN_WE 27
#define PIN_A0 4
#define ADDRBITS 20

Adafruit_USBD_WebUSB usb_web;

#define WEBUSB_HTTP 0
#define WEBUSB_HTTPS 1
WEBUSB_URL_DEF(landingPage, WEBUSB_HTTPS, "loopycart.surge.sh");

// Reset pin / Int pin / 12C serial (see TCA9539 datasheet)
TCA9539 databus(2, 0, 0x74, &Wire);

// sprintf buffer
char S[128];

void flashLed(int n, int d = 100) {
  for (int i = 0; i < n; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(d/2);
    digitalWrite(LED_BUILTIN, LOW);
    delay(d/2);
  }
}

inline void databusReadMode() {
  for (int i = 0; i < 16; i++) {
    databus.TCA9539_set_dir(i, TCA9539_PIN_DIR_INPUT);
  }
}

inline void databusWriteMode() {
  for (int i = 0; i < 16; i++) {
    databus.TCA9539_set_dir(i, TCA9539_PIN_DIR_OUTPUT);
  }
}

inline void flashReadMode() {
  databusReadMode();
  digitalWrite(PIN_CE, LOW);
  digitalWrite(PIN_WE, HIGH);
  digitalWrite(PIN_OE, LOW);
  delayMicroseconds(1);
}

inline void flashWriteMode() {
  databusWriteMode();
  digitalWrite(PIN_CE, LOW);
  digitalWrite(PIN_WE, LOW);
  digitalWrite(PIN_OE, HIGH);
  delayMicroseconds(1);
}

inline void flashIdleMode() {
  digitalWrite(PIN_CE, HIGH);
  delayMicroseconds(1);
}

inline void setAddress(uint32_t addr) {
  // for (int i = 0, mask = 1; i < ADDRBITS; i++, mask <<= 1) {
  //   digitalWrite(PIN_A0 + i, (mask & addr) > 0);
  // }

  // Fast experiment
  sio_hw->gpio_clr = 0b11111111111111111111 << PIN_A0;
  sio_hw->gpio_set = addr << PIN_A0;
}

inline void writeWord(uint32_t addr, uint16_t word) {
  // Don't forget to switch data bus mode
  setAddress(addr);
  //delayMicroseconds()
  databus.TCA9539_set_word(word);
}

inline uint16_t readWord(uint32_t addr, bool swapend = false) {
  setAddress(addr);
  //delayMicroseconds()
  if (swapend) {
    return databus.TCA9539_read_word_bigend();
  }
  return databus.TCA9539_read_word();
}

inline void flashCommand(uint32_t addr, uint16_t data) {
  // To write a command to the device, system must drive WE# and CE# to Vil,
  // and OE# to Vih. In a command cycle, all address are latched at the later falling edge of CE# and WE#, and all data are latched at the earlier rising edge of CE# and WE#.

  // PREREQUISITE: OE should be high
  // digitalWriteFast(PIN_OE, HIGH);

  // Don't forget to switch data bus mode
  writeWord(addr, data);

  digitalWriteFast(PIN_WE, LOW);
  digitalWriteFast(PIN_CE, LOW);

  // TCWC >70ns pulse is incredibly short, 1 microsecond is too much
  NOP;
  NOP;

  digitalWriteFast(PIN_WE, HIGH);
  digitalWriteFast(PIN_CE, HIGH);
}

// function to echo to both Serial and WebUSB
void echo_all(const char* buf, uint32_t count) {
  if (usb_web.connected()) {
    usb_web.write((uint8_t*)buf, count);
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

void flashErase() {
  // The Chip Erase operation is used erase all the data within the memory array. All memory cells containing a "0" will be returned to the erased state of "1".
  // This operation requires 6 write cycles to initiate the action. The first two cycles are "unlock" cycles, the third is a configuration cycle, the fourth and fifth are also "unlock" cycles, and the sixth cycle initiates the chip erase operation.
  digitalWrite(LED_BUILTIN, HIGH);
  int len = sprintf(S, "ERASE START\r");
  echo_all(S, len);

  databusWriteMode();
  digitalWrite(PIN_OE, HIGH);

  delayMicroseconds(100);
  flashCommand(0x555, 0xaa);
  delayMicroseconds(100);
  flashCommand(0x2aa, 0x55);
  delayMicroseconds(100);
  flashCommand(0x555, 0x80);
  delayMicroseconds(100);
  flashCommand(0x555, 0xaa);
  delayMicroseconds(100);
  flashCommand(0x2aa, 0x55);
  delayMicroseconds(100);
  flashCommand(0x555, 0x10);
  delayMicroseconds(100);

  databusReadMode();
  digitalWrite(PIN_OE, HIGH);
  delay(100);
  const int TIMEOUT = 35;
  for (int i = 0; i < TIMEOUT; i++) {
    echo_all(".", 1);
    digitalWrite(PIN_OE, LOW);
    delayMicroseconds(1);
    if (databus.TCA9539_read_pin_val(2) && databus.TCA9539_read_pin_val(7)) {
      echo_all("\rOK!\r", 5);
      break;
    } else if (databus.TCA9539_read_pin_val(5)) {
      echo_all("\rTIMEOUT!\r", 10);
      break;
    }
    digitalWrite(PIN_OE, HIGH);
    delay(1000);
  }

  echo_all("\r", 2);
  len = sprintf(S, "ERASE COMPLETE (or timed out)\r");
  echo_all(S, len);
  digitalWrite(LED_BUILTIN, LOW);

  databusReadMode();
  flashIdleMode();
}

void flashEraseSector(uint8_t sector) {
  flashWriteMode();
  databusWriteMode();
  digitalWrite(PIN_OE, HIGH);
  flashCommand(0x555, 0xaa);
  flashCommand(0x2aa, 0x55);
  flashCommand(0x555, 0x80);
  flashCommand(0x555, 0xaa);
  flashCommand(0x2aa, 0x55);
  flashCommand(sector, 0x30);
  delay(100);
  flashIdleMode();
}

void flashInspect(uint32_t starting = 0, uint32_t upto = 1 << ADDRBITS) {
  flashReadMode();
  delayMicroseconds(1);

  int len;
  for (uint32_t addr = starting; addr < upto; addr++) {
    if (addr % 0x10 == 0) {
      echo_all("\r", 1);
      len = sprintf(S, "%06xh\t\t", addr * 2);
      echo_all(S, len);
    }
    len = sprintf(S, "%04x\t", readWord(addr, true));
    echo_all(S, len);
  }

  echo_all("\r\n\r\n", 4);
  flashIdleMode();
}

void flashDump(uint32_t starting = 0, uint32_t upto = 1 << ADDRBITS) {
  flashReadMode();
  delayMicroseconds(1);

  digitalWrite(LED_BUILTIN, HIGH);
  for (uint32_t addr = starting; addr < upto; addr++) {
    uint16_t word = readWord(addr);
    usb_web.write((uint8_t*)&word, 2);
  }
  digitalWrite(LED_BUILTIN, LOW);

  flashIdleMode();
}

void flashProgram(uint32_t addr, uint16_t word) {
  flashCommand(0x555, 0xaa);
  flashCommand(0x2aa, 0x55);
  flashCommand(0x555, 0xa0);
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
    delay(1000);
    return;
  }

  bufLen = usb_web.read(buf, 64);

  if (bufLen == 0) {
    return;
  }

  if (isProgramming) {
    // len = sprintf(S, "Programming block of %d bytes\r", bufLen);
    // echo_all(S, len);

    if ((bufLen % 2) == 1) {
      len = sprintf(S, "WARNING: odd number of bytes %d\r", bufLen);
      echo_all(S, len);
    }

    digitalWrite(LED_BUILTIN, HIGH);
    digitalWrite(PIN_OE, HIGH);
    for (int i = 0; i < bufLen; i += 2, addr++) {
      // echo progress every 1kb, echo status before early exit
      if (addr % 512 == 0) {
        len = sprintf(S, "%d\r", addr * 2);
        echo_all(S, len);
      }

      uint16_t word = buf[i + 1] << 8 | buf[i];

      // Skip padding assuming you have erased first
      if (word == 0xffff) continue;

      flashProgram(addr, word);

      // maybe do the first byte twice? this is dumb. is it still necessary? regress
      if (addr == 0) {
        delayMicroseconds(10);
        flashProgram(addr, word);
      }

    }
    digitalWrite(LED_BUILTIN, LOW);

    if (addr >= expectedWords) {
      isProgramming = false;
      len = sprintf(S, "Finished! Wrote %d bytes total\r", addr * 2);
      echo_all(S, len);
      flashIdleMode();
    }
  }

  else if (buf[0] == 'E' && buf[1] == '\r') {
    // ERASE COMMAND
    flashErase();
    // flashEraseSector(0);
  }

  else if (buf[0] == 'I' && buf[1] == '\r') {
    // INSPECT COMMAND
    flashInspect(0, 0x400);
  }

  else if (buf[0] == 'D') {
    // DUMP COMMAND
    // followed by expected # of bytes
    if (sscanf((char*)buf, "D%d\r", &expectedWords) && expectedWords > 0 && expectedWords <= 1 << ADDRBITS) {
      flashDump(0, expectedWords);
    }
    // flashDump();
  }

  else if (buf[0] == 'P') {
    // PROGRAM COMMAND
    // followed by expected # of bytes
    if (sscanf((char*)buf, "P%d\r", &expectedWords)) {
      len = sprintf(S, "Programming %d bytes\r", expectedWords * 2);
      echo_all(S, len);

      isProgramming = true;
      addr = 0;
      flashWriteMode();
    }
  }
}

void line_state_callback(bool connected) {
  if (connected) {
    usb_web.println("CONNECTED");
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

  // Setup IO expander on data bus
  // Board v2 uses alternate GPIO 0/1 for I2C port 0
  if (!Wire.setSDA(0)) Serial.println("FAIL setting i2c SDA");
  if (!Wire.setSCL(1)) Serial.println("FAIL setting i2c SCL");
  Wire.setClock(400000);
  databus.TCA9539_init();

  Serial.println("IO expander set up");

  // Disable SRAM - CS2 pulled low, redundant but set CS1 high
  pinMode(PIN_RAMCS1, OUTPUT);
  digitalWrite(PIN_RAMCS1, HIGH);

  // Setup ROM pins
  pinMode(PIN_CE, OUTPUT);
  pinMode(PIN_WE, OUTPUT);
  pinMode(PIN_OE, OUTPUT);

  // Setup address bus
  for (int i = 0; i < ADDRBITS; i++) {
    pinMode(PIN_A0 + i, OUTPUT);
  }

  flashIdleMode();

  while (!TinyUSBDevice.mounted()) {
    delay(1);
  }
}
