#include <SPI.h>
#include <MCP23S17.h>
#include <Adafruit_TinyUSB.h>
#include <Adafruit_NeoPixel.h>
#include <LittleFS.h>

#define OPT_MULTIBYTE 1
#define PROTOCOL_VERSION 2
#define SPI_SPEED 10000000
#define DEBUG_LED

// Delays one clock cycle or 7ns | 133MhZ = 0.000000007518797sec = 7.518797ns
#define NOP __asm__("nop\n\t")
#define HALT while (true)

#define ADDRBITS 22
#define CMD_RESET 0xff
#define PIN_STATUS 6

Adafruit_USBD_WebUSB usb_web;
#define WEBUSB_HTTP 0
#define WEBUSB_HTTPS 1
WEBUSB_URL_DEF(landingPage, WEBUSB_HTTPS, "f.loopy.land");

#define SPI_CS_PIN 5
// ~RESET is pulled high when pico is powered up, >=99 means dummy reset pin
#define MCP_NO_RESET_PIN 100
// SPI addresses for MCP23017 are: 0 1 0 0 A2 A1 A0
MCP23S17 mcpData =  MCP23S17(SPI_CS_PIN, MCP_NO_RESET_PIN, 0b0100000);  //Data IO, D0-D15, Address 0x0
MCP23S17 mcpAddr0 = MCP23S17(SPI_CS_PIN, MCP_NO_RESET_PIN, 0b0100001);  //Address IO, A0-A15, Address 0x1
MCP23S17 mcpAddr1 = MCP23S17(SPI_CS_PIN, MCP_NO_RESET_PIN, 0b0100010);  //Address and control IO, A16-A21, OE, RAMCE, RAMWE, ROMCE, ROMWE, RESET, Address 0x2

// sprintf buffer
char S[128];
int len;
// timing operations
unsigned long stopwatch;

// status register
uint16_t SRD;
#define SR(n) bitRead(SRD, n)

#define BLUE 0x000040
Adafruit_NeoPixel led(1, 16, NEO_RGB);

inline void ledColor(uint32_t c) {
  led.setPixelColor(0, c);
  led.show();
}

void flashLed(int n, int d = 100) {
  for (int i = 0; i < n; i++) {
    led.setPixelColor(0, 0xff0000);
    led.show();
    delay(d / 2);
    ledColor(0);
    led.show();
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
  ioReadMode(&mcpData);
  databusMode = READ;
}

inline void ioWriteMode(MCP23S17 *mcp) {
  mcp->setPortMode(0b11111111, A);
  mcp->setPortMode(0b11111111, B);
}

inline void databusWriteMode() {
  if (databusMode == WRITE) return;
  ioWriteMode(&mcpData);
  databusMode = WRITE;
}

// NOTE address is in bytes, though we write words
inline void setAddress(uint32_t addr) {
  // A0-A15
  mcpAddr0.setPort(addr & 0xff, (addr >> 8) & 0xff);
  // mcpAddr0.setPort(addr & 0xff, A);
  // mcpAddr0.setPort((addr >> 8) & 0xff, B);
  
  // A16-A21
  mcpAddr1.setPort((addr >> 16) & 0xff, A);
}

// Control pins migrated to io expanders so build a bitmask with these. They're all active low so & these:
#define IDLE  0b11111111
#define OE    0b11111110
#define RAMCE 0b11111101
#define RAMWE 0b11111011
#define ROMCE 0b11110111
#define ROMWE 0b11101111
// Alias as BE0 is used in Sharp datasheet but we call it CE
#define ROMBE0 ROMCE

// Any bits in bitmask that are 0 will go low (active), bits are 0=OE 1=RAMCE 2=RAMWE 3=ROMCE 4=ROMWE
inline void setControl(uint8_t bitmask = IDLE) {
  mcpAddr1.setPort(0xff & bitmask, B);
}

inline uint16_t readWord() {
  return (mcpData.getPort(B) << 8) | mcpData.getPort(A);
}

inline uint8_t readByte() {
  return mcpData.getPort(A);
}

inline void writeWord(uint32_t addr, uint16_t word) {
  setAddress(addr);
  mcpData.setPort(word & 0xff, (word >> 8) & 0xff);
  // mcpData.setPort(word & 0xff, A);
  // mcpData.setPort((word >> 8) & 0xff, B);
}

inline void writeByte(uint32_t addr, uint8_t byte) {
  setAddress(addr);
  mcpData.setPort(byte, A);
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
  // if (Serial) {
  //   Serial.write(buf);
  //   // for (uint32_t i = 0; i < count; i++) {
  //   //   Serial.write(buf[i]);
  //   //   if (buf[i] == '\r') Serial.write('\n');
  //   // }
  //   Serial.flush();
  // }
}

void line_state_callback(bool connected) {
  if (connected) {
    usb_web.print(PROTOCOL_VERSION, DEC);
    usb_web.print("\r\n");
    usb_web.write('\0');
    usb_web.print("CONNECTED\r");
    if (len > 0) {
      usb_web.write((uint8_t *)S, len);
    }
    usb_web.flush();
  }
}