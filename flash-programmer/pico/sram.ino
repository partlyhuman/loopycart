#define SRAM_ADDRBITS 13
const uint32_t SRAM_SIZE = 1 << SRAM_ADDRBITS;

inline void sramSelect() {
  digitalWriteFast(PIN_ROMCE, HIGH);
  digitalWriteFast(PIN_ROMWE, HIGH);
  digitalWriteFast(PIN_OE, HIGH);
  digitalWriteFast(PIN_RAMWE, HIGH);
  digitalWriteFast(PIN_RAMCS1, LOW);
  digitalWriteFast(PIN_RAMCS2, HIGH);
}

inline void sramDeselect() {
  digitalWriteFast(PIN_ROMCE, LOW);
  digitalWriteFast(PIN_ROMWE, HIGH);
  digitalWriteFast(PIN_OE, HIGH);
  digitalWriteFast(PIN_RAMWE, HIGH);
  digitalWriteFast(PIN_RAMCS1, HIGH);
  digitalWriteFast(PIN_RAMCS2, HIGH);
}

uint8_t sramReadByte(uint32_t addr) {
  // braindead 1 cycle read (address controlled)
  databusReadMode();
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
  // Write Cycle 1, Note 4, p6 - During this period, I/O pins are in the output state, and input signals must not be applied.
  databusReadMode();

  setAddress(addr);
  NOP;
  NOP;
  NOP;

  digitalWriteFast(PIN_RAMWE, LOW);
  NOP;
  NOP;
  NOP;

  databusWriteMode();
  mcpD.setPort(byte, A);

  digitalWriteFast(PIN_RAMWE, HIGH);
}

void sramSaveFile(const char* filename) {
  File file = LittleFS.open(filename, "w");

  digitalWriteFast(LED_BUILTIN, HIGH);
  sramSelect();
  databusReadMode();

  // Address controlled Read Cycle 1, p5, no control necessary just set an address and read
  digitalWriteFast(PIN_OE, LOW);

  for (uint32_t addr = 0; addr < SRAM_SIZE; addr++) {
    uint8_t byte = sramReadByte(addr);
    file.write(&byte, 1);
  }

  digitalWriteFast(PIN_OE, HIGH);

  databusWriteMode();
  sramDeselect();
  digitalWriteFast(LED_BUILTIN, LOW);

  file.flush();
  file.close();
}

bool sramLoadFile(const char* filename) {
  if (!LittleFS.exists(filename)) {
    len = sprintf(S, "file %s doesn't exist\r\n", filename);
    echo_all(S, len);
    return false;
  }
  File file = LittleFS.open(filename, "r");
  if (file.size() != SRAM_SIZE) {
    len = sprintf(S, "file %s is wrong size, expected %d, actual %d\r\n", filename, SRAM_SIZE, file.size());
    echo_all(S, len);
    // TODO delete?
    return false;
  }

  digitalWriteFast(LED_BUILTIN, HIGH);
  sramSelect();
  databusWriteMode();
  digitalWriteFast(PIN_OE, HIGH);
  digitalWriteFast(PIN_RAMWE, HIGH);

  // read 1kb at a time?
  const size_t CHUNK_SIZE = 1 << 10;
  uint8_t buf[CHUNK_SIZE];
  for (uint32_t addr = 0; addr < SRAM_SIZE;) {
    // file.read() will do this for you if not enough available i guess
    // size_t bufSize = MIN(CHUNK_SIZE, SRAM_SIZE - addr);
    // if (bufSize <= 0) break;
    size_t bufSize = file.read(buf, CHUNK_SIZE);
    for (size_t bufPtr = 0; bufPtr < bufSize; bufPtr++, addr++) {
      sramWriteByte(addr, buf[bufPtr]);
    }
  }

  sramDeselect();
  digitalWriteFast(LED_BUILTIN, LOW);

  file.close();
  return true;
}


void sramInspect(uint32_t starting = 0, uint32_t upto = SRAM_SIZE) {
  sramSelect();
  databusReadMode();

  // Address controlled Read Cycle 1, p5, no control necessary just set an address and read
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

  digitalWriteFast(PIN_OE, HIGH);

  sramDeselect();
  databusWriteMode();
  echo_all("\r\n\r\n", 4);
}

void sramDump(uint32_t starting = 0, uint32_t upto = SRAM_SIZE) {
  digitalWriteFast(LED_BUILTIN, HIGH);
  sramSelect();
  databusReadMode();

  // Address controlled Read Cycle 1, p5, no control necessary just set an address and read
  digitalWriteFast(PIN_OE, LOW);

  for (uint32_t addr = starting; addr < upto; addr++) {
    uint8_t byte = sramReadByte(addr);
    usb_web.write(&byte, 1);
  }

  digitalWriteFast(PIN_OE, HIGH);

  sramDeselect();
  databusWriteMode();
  // echo_all("\r\n\r\n", 4);
  digitalWriteFast(LED_BUILTIN, LOW);
}

void sramErase() {
  digitalWriteFast(LED_BUILTIN, HIGH);
  echo_all("Erasing SRAM");

  stopwatch = millis();

  sramSelect();
  databusWriteMode();
  digitalWriteFast(PIN_OE, HIGH);
  digitalWriteFast(PIN_RAMWE, HIGH);

  for (uint32_t addr = 0; addr < SRAM_SIZE; addr++) {
    if (addr % 0x100 == 0) {
      echo_all(".");
    }
    sramWriteByte(addr, 0);
  }

  len = sprintf(S, "\r\nErased %d bytes of SRAM in %0.2f sec!\r\n", SRAM_SIZE, (millis() - stopwatch) / 1000.0);
  echo_all(S, len);

  sramDeselect();
  digitalWriteFast(LED_BUILTIN, LOW);
}

// Returns whether programming should continue
bool sramWriteBuffer(uint8_t* buf, size_t bufLen, uint32_t& addr) {
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
