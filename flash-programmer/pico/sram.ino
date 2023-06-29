#define SRAM_ADDRBITS 13

inline void sramSelect() {
  digitalWriteFast(PIN_ROMCE, HIGH);
  digitalWriteFast(PIN_ROMWE, HIGH);
  digitalWriteFast(PIN_OE, HIGH);
  digitalWriteFast(PIN_RAMCS1, LOW);
  digitalWriteFast(PIN_RAMCS2, HIGH);
}

inline void sramDeselect() {
  digitalWriteFast(PIN_RAMCS1, HIGH);
  digitalWriteFast(PIN_RAMCS2, LOW);
}

uint8_t sramReadByte(uint32_t addr) {
  // braindead 1 cycle read (address controlled)
  setAddress(addr);
  // tAA | Address access time | 55ns
  NOP;
  NOP;
  NOP;
  NOP;
  NOP;
  NOP;
  NOP;
  NOP;
  return readByte();
}

void sramWriteByte(uint32_t addr, uint8_t byte) {
  databusReadMode();

  // OE should be high the whole time
  // writeByte(addr, byte);
  setAddress(addr);

  digitalWriteFast(PIN_RAMWE, LOW);

  // tWHZ | Write to output in High-Z | 20ns
  NOP;
  NOP;
  NOP;

  databusWriteMode();
  mcpD.setPort(byte, A);

  digitalWriteFast(PIN_RAMWE, HIGH);

  databusReadMode();

#if false

  // CE controlled writing for compatibility with FRAM
  // digitalWriteFast(PIN_RAMCS1, LOW);

  // tWP | Write pulse width | min 60 ns
  // NOP;
  // NOP;
  // NOP;
  // NOP;
  // NOP;
  // NOP;
  // NOP;
  // NOP;
  // NOP;
  // NOP;
  delayMicroseconds(1);

  digitalWriteFast(PIN_RAMWE, HIGH);

  // CE controlled writing for compatibility with FRAM
  // digitalWriteFast(PIN_RAMCS1, HIGH);

  // Write cycle time | min 100 ns
  delayMicroseconds(1);
  // NOP;
  // NOP;
  // NOP;
  // NOP;
  // NOP;
  // NOP;
  // NOP;
#endif
}

void sramInspect(uint32_t starting = 0, uint32_t upto = (1 << SRAM_ADDRBITS)) {
  sramSelect();
  databusReadMode();

  // continuous read mode
  digitalWriteFast(PIN_OE, LOW);

  for (uint32_t addr = starting; addr < upto; addr++) {
    if (addr % 0x10 == 0) {
      echo_all("\r", 1);
      len = sprintf(S, "%06xh\t\t", addr);
      echo_all(S, len);
    }
    len = sprintf(S, "%02x\t", sramReadByte(addr));
    echo_all(S, len);
  }

  // continuous read mode
  digitalWriteFast(PIN_OE, HIGH);

  sramDeselect();
  databusWriteMode();
  echo_all("\r\n\r\n", 4);
}

void sramDump(uint32_t starting = 0, uint32_t upto = (1 << SRAM_ADDRBITS)) {
  digitalWriteFast(LED_BUILTIN, HIGH);
  sramSelect();
  databusReadMode();

  // continuous read mode
  digitalWriteFast(PIN_OE, LOW);


  for (uint32_t addr = starting; addr < upto; addr++) {
    uint8_t byte = sramReadByte(addr);
    usb_web.write(&byte, 1);
  }

  // continuous read mode
  digitalWriteFast(PIN_OE, HIGH);

  sramDeselect();
  databusWriteMode();
  // echo_all("\r\n\r\n", 4);
  digitalWriteFast(LED_BUILTIN, LOW);
}

void sramErase() {
  digitalWriteFast(LED_BUILTIN, HIGH);
  const uint32_t upperAddress = 1 << SRAM_ADDRBITS;
  echo_all("Erasing SRAM");

  stopwatch = millis();

  sramSelect();
  databusWriteMode();
  digitalWriteFast(PIN_OE, HIGH);
  digitalWriteFast(PIN_RAMWE, HIGH);

  for (uint32_t addr = 0; addr < upperAddress; addr++) {
    if (addr % 0x100 == 0) {
      echo_all(".");
    }
    sramWriteByte(addr, 0xff);
  }

  len = sprintf(S, "\r\nErased %d bytes of SRAM in %0.2f sec\r\n", upperAddress, (millis() - stopwatch) / 1000.0);
  echo_all(S, len);

  sramDeselect();
  digitalWriteFast(LED_BUILTIN, LOW);
}

// Returns whether programming should continue
bool sramWriteBuffer(uint8_t *buf, size_t bufLen, uint32_t &addr) {
  const size_t SRAM_BYTES = 1 << SRAM_ADDRBITS;
  for (int bufPtr = 0; bufPtr < bufLen && addr < SRAM_BYTES; bufPtr++, addr++) {
    sramWriteByte(addr, buf[bufPtr]);
  }

  if (addr >= SRAM_BYTES) {
    sramDeselect();
    digitalWriteFast(LED_BUILTIN, LOW);

    len = sprintf(S, "Finished! Wrote %d bytes in %f sec\r\n", addr, (millis() - stopwatch) / 1000.0);
    echo_all(S, len);

    return false;
  }
  return true;
}
