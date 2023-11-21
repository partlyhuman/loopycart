#include <SPI.h>
#include <MCP23S17.h>
#include <Adafruit_TinyUSB.h>
#include <Adafruit_NeoPixel.h>
#include <LittleFS.h>
#include "generated.h"

#define OPT_MULTIBYTE 1
#define BOARD_REVISION 8
#define SPI_SPEED 10000000
// TODO something bad is going on that flashing dies midway if this is not defined
#define DEBUG_LED

// Delays one clock cycle or 7ns | 133MhZ = 0.000000007518797sec = 7.518797ns
#define NOP __asm__("nop\n\t")
#define HALT while (true)

#define ADDRBITS 22
#define CMD_RESET 0xff
#define PIN_INSERTED 14

Adafruit_USBD_WebUSB usb_web;
WEBUSB_URL_DEF(landingPage, 1, "f.loopy.land");

// ~RESET is pulled high when pico is powered up, >=99 means dummy reset pin
#define MCP_NO_RESET_PIN 100
// SPI addresses for MCP23017 are: 0 1 0 0 A2 A1 A0
// Note we almost certainly don't need differing addresses any more since REV6 gave each a unique CE, but no harm keeping it
MCP23S17 mcpData = MCP23S17(5, MCP_NO_RESET_PIN, 0b0100000);   //Data IO, D0-D15, Address 0x0
MCP23S17 mcpAddr0 = MCP23S17(6, MCP_NO_RESET_PIN, 0b0100001);  //Address IO, A0-A15, Address 0x1
MCP23S17 mcpAddr1 = MCP23S17(7, MCP_NO_RESET_PIN, 0b0100010);  //Address and control IO, A16-A21, OE, RAMCE, RAMWE, ROMCE, ROMWE, RESET, Address 0x2

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
  // cache address and only set bits that changed
  static uint32_t lastAddr = 0;
  uint32_t diff = addr ^ lastAddr;

  // A0-A7
  if ((diff & 0x000000ff) != 0) {
    mcpAddr0.setPort(addr & 0xff, A);
  }
  // A8-A15
  if ((diff & 0x0000ff00) != 0) {
    mcpAddr0.setPort((addr >> 8) & 0xff, B);
  }
  // A16-A21
  if ((diff & 0x00ff0000) != 0) {
    mcpAddr1.setPort((addr >> 16) & 0xff, A);
  }

  lastAddr = addr;
}

// Control pins migrated to io expanders so build a bitmask with these. They're all active low so & these:
const uint8_t IDLE  = 0xff;
const uint8_t OE    = ~(1 << 0);
const uint8_t RAMCE = ~(1 << 1);
const uint8_t RAMWE = ~(1 << 2);
const uint8_t ROMCE = ~(1 << 3);
const uint8_t ROMWE = ~(1 << 4);
const uint8_t RESET = ~(1 << 5);

// Any bits in bitmask that are 0 will go low (active), bits are 0=OE 1=RAMCE 2=RAMWE 3=ROMCE 4=ROMWE 5=RESET
inline void setControl(uint8_t bitmask = IDLE) {
  static uint8_t lastControl = 0;
  if (bitmask != lastControl) {
    mcpAddr1.setPort(bitmask, B);
  }
  lastControl = bitmask;
}

inline uint16_t readWord() {
  return (mcpData.getPort(B) << 8) | mcpData.getPort(A);
}

inline uint8_t readByte() {
  return mcpData.getPort(A);
}

inline void writeWord(uint32_t addr, uint16_t word) {
  setAddress(addr);
  // In testing, caching last write is slower
  mcpData.setPort(word & 0xff, (word >> 8) & 0xff);
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

void echo_ok() {
  echo_all("!OK\r\n", 4);
}

void line_state_callback(bool connected) {
  if (connected) {
    // Ensure that a full 64 bytes are sent, chunk length used by client
    const char *connectStr = "!FW ";
    usb_web.print(connectStr);
    usb_web.print(GIT_COMMIT);
    usb_web.print("\r\n");
    for (int i = 2 + strlen(connectStr) + strlen(GIT_COMMIT); i <= 64; i++) {
      usb_web.write(' ');
    }
    usb_web.flush();

    // Dump any errors that occurred before USB connect
    if (len > 0) {
      echo_all("!ERR boot log errors found:\r\n");
      echo_all(S, len);
    }
  }
}