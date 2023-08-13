#define SRAM_ADDRBITS 15
const uint32_t SRAM_SIZE = 1 << SRAM_ADDRBITS; // minus one

inline void sramSelect() {
  setControl(RAMCE);
}

inline void sramDeselect() {
  setControl(ROMCE);
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

  setControl(RAMCE & RAMWE);
  NOP;
  NOP;
  NOP;

  databusWriteMode();
  mcpData.setPort(byte, A);

  setControl(RAMCE);
}

void sramSaveFile(const char* filename) {
  File file = LittleFS.open(filename, "w");

  ledColor(BLUE);
  sramSelect();
  databusReadMode();

  // Address controlled Read Cycle 1, p5, no control necessary just set an address and read
  setControl(RAMCE & OE);

  for (uint32_t addr = 0; addr < SRAM_SIZE; addr++) {
    uint8_t byte = sramReadByte(addr);
    file.write(&byte, 1);
  }

  databusWriteMode();
  sramDeselect();
  ledColor(0);

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

  ledColor(BLUE);
  sramSelect();
  databusWriteMode();

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
  ledColor(0);

  file.close();
  return true;
}


void sramInspect(uint32_t starting = 0, uint32_t upto = SRAM_SIZE) {
  sramSelect();
  databusReadMode();

  // Address controlled Read Cycle 1, p5, no control necessary just set an address and read
  setControl(RAMCE & OE);

  for (uint32_t addr = starting; addr < upto; addr++) {
    if (addr % 0x10 == 0) {
      echo_all("\r", 1);
      len = sprintf(S, "%06xh\t\t", addr);
      echo_all(S, len);
    }
    len = sprintf(S, "%02x\t", sramReadByte(addr));
    echo_all(S, len);
  }

  sramDeselect();
  databusWriteMode();
  echo_all("\r\n\r\n", 4);
}

void sramDump(uint32_t starting = 0, uint32_t upto = SRAM_SIZE) {
  ledColor(BLUE);
  sramSelect();
  databusReadMode();

  // Address controlled Read Cycle 1, p5, no control necessary just set an address and read
  setControl(RAMCE & OE);

  for (uint32_t addr = starting; addr < upto; addr++) {
    uint8_t byte = sramReadByte(addr);
    usb_web.write(&byte, 1);
  }

  sramDeselect();
  databusWriteMode();
  // echo_all("\r\n\r\n", 4);
  ledColor(0);
}

void sramErase() {
  ledColor(BLUE);
  echo_all("Erasing SRAM");

  stopwatch = millis();

  sramSelect();
  databusWriteMode();

  for (uint32_t addr = 0; addr < SRAM_SIZE; addr++) {
    if (addr % 0x100 == 0) {
      echo_all(".");
    }
    sramWriteByte(addr, 0);
  }

  len = sprintf(S, "\r\nErased %d bytes of SRAM in %0.2f sec!\r\n", SRAM_SIZE, (millis() - stopwatch) / 1000.0);
  echo_all(S, len);

  sramDeselect();
  ledColor(0);
}

// Returns whether programming should continue
bool sramWriteBuffer(uint8_t* buf, size_t bufLen, uint32_t& addr) {
  const size_t SRAM_BYTES = 1 << SRAM_ADDRBITS;
  for (int bufPtr = 0; bufPtr < bufLen && addr < SRAM_BYTES; bufPtr++, addr++) {
    sramWriteByte(addr, buf[bufPtr]);
  }

  if (addr >= SRAM_BYTES) {
    sramDeselect();
    ledColor(0);

    len = sprintf(S, "Finished! Wrote %d bytes in %f sec\r\n", addr, (millis() - stopwatch) / 1000.0);
    echo_all(S, len);

    return false;
  }
  return true;
}
