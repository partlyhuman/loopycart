#include <SPI.h>
#include <MCP23S17.h>
#include <Adafruit_TinyUSB.h>
#include <LittleFS.h>

#define OPT_MULTIBYTE 1
#define PROTOCOL_VERSION 2

// Delays one clock cycle or 7ns | 133MhZ = 0.000000007518797sec = 7.518797ns
#define NOP __asm__("nop\n\t")
#define HALT while (true)

#define PIN_RAMCS1 12
#define PIN_RAMCS2 13
#define PIN_RAMWE 15

#define PIN_ROMCE 11
#define PIN_ROMWE 14
#define PIN_OE 10

// Alias as BE0 is used in Sharp datasheet but we call it CE
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

Adafruit_USBD_WebUSB usb_web;
#define WEBUSB_HTTP 0
#define WEBUSB_HTTPS 1
WEBUSB_URL_DEF(landingPage, WEBUSB_HTTPS, "f.loopy.land");

// SPI is pico's default SPI0: TX=19, SCK=18, CS=17, RX=16
#define SPI_CS_PIN 17
// ~RESET is pulled high when pico is powered up, >=99 means dummy reset pin
#define MCP_NO_RESET_PIN 100
// SPI addresses for MCP23017 are: 0 1 0 0 A2 A1 A0
MCP23S17 mcpA = MCP23S17(SPI_CS_PIN, MCP_NO_RESET_PIN, 0b0100001);  //Address IO, Address 0x1
MCP23S17 mcpD = MCP23S17(SPI_CS_PIN, MCP_NO_RESET_PIN, 0b0100000);  //Data IO,    Address 0x0

// sprintf buffer
char S[128];
int len;
// timing operations
unsigned long stopwatch;

// status register
uint16_t SRD;
#define SR(n) bitRead(SRD, n)

void flashLed(int n, int d = 100) {
  for (int i = 0; i < n; i++) {
    digitalWriteFast(LED_BUILTIN, HIGH);
    delay(d / 2);
    digitalWriteFast(LED_BUILTIN, LOW);
    delay(d / 2);
  }
}

// Changing the port mode is superslow so let's cache it
enum DataBusMode { READ,
                   WRITE };
DataBusMode databusMode = WRITE;

inline void ioReadMode(MCP23S17 *mcp) {
  mcp->setPortMode(0, A);
  mcp->setPortMode(0, B);
}

inline void databusReadMode() {
  if (databusMode == READ) return;
  ioReadMode(&mcpD);
  databusMode = READ;
}

inline void ioWriteMode(MCP23S17 *mcp) {
  mcp->setPortMode(0b11111111, A);
  mcp->setPortMode(0b11111111, B);
}

inline void databusWriteMode() {
  if (databusMode == WRITE) return;
  ioWriteMode(&mcpD);
  databusMode = WRITE;
}

// NOTE address is in bytes, though we write words, A0 will always be 0 for ROM but not always for SRAM
inline void setAddress(uint32_t addr) {
  // for (int i = 0, mask = 1; i < ADDRBITS; i++, mask <<= 1) {
  //   digitalWriteFast(PIN_A0 + i, (mask & addr) > 0);
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
}

inline uint16_t readWord() {
  return (mcpD.getPort(B) << 8) | mcpD.getPort(A);
}

inline uint8_t readByte() {
  return mcpD.getPort(A);
}

inline void writeWord(uint32_t addr, uint16_t word) {
  setAddress(addr);
  mcpD.setPort(word & 0xff, A);
  mcpD.setPort(word >> 8, B);
}

inline void writeByte(uint32_t addr, uint8_t byte) {
  setAddress(addr);
  mcpD.setPort(byte, A);
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

void line_state_callback(bool connected) {
  if (connected) {
    usb_web.print(PROTOCOL_VERSION, DEC);
    usb_web.print("\r\n");
    usb_web.write('\0');
    usb_web.print("CONNECTED\r");
    usb_web.flush();
  }
}