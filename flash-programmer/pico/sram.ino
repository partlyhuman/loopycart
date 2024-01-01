#define SRAM_ADDRBITS 17  // R7 graduated to 1MBit/128KB SRAM
const uint32_t SRAM_SIZE = 1 << SRAM_ADDRBITS;

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

void sramSaveFile(const char* filename, uint32_t saveSize = SRAM_SIZE) {
  File file = LittleFS.open(filename, "w");

  sramSelect();
  databusReadMode();

  // Address controlled Read Cycle 1, p5, no control necessary just set an address and read
  setControl(RAMCE & OE);

  for (uint32_t addr = 0; addr < saveSize; addr++) {
    uint8_t byte = sramReadByte(addr);
    file.write(&byte, 1);
  }

  databusWriteMode();
  sramDeselect();

  file.flush();
  file.close();
}

bool sramLoadFile(const char* filename) {
  if (!LittleFS.exists(filename)) {
    sprintf(S, "file %s doesn't exist\r\n", filename);
    echo_all();
    return false;
  }
  File file = LittleFS.open(filename, "r");
  auto fileSize = file.size();
  auto expectedFileSize = flashCartHeaderSramSize();
  if (fileSize > SRAM_SIZE) {
    sprintf(S, "Backed up file is bigger than SRAM size, aborting");
    LittleFS.remove(filename);
    return false;
  }
  if (fileSize != expectedFileSize) {
    sprintf(S, "Backup size %s is wrong size, expected %d, actual %d, continuing anyway...\r\n", filename, expectedFileSize, fileSize);
    echo_all();
  }

  sramSelect();
  databusWriteMode();

  // read 1kb at a time?
  const size_t CHUNK_SIZE = 1 << 10;
  uint8_t buf[CHUNK_SIZE];
  for (uint32_t addr = 0; addr < fileSize;) {
    // file.read() will do this for you if not enough available i guess
    // size_t bufSize = MIN(CHUNK_SIZE, SRAM_SIZE - addr);
    size_t bufSize = file.read(buf, CHUNK_SIZE);
    if (bufSize <= 0) break;
    for (size_t bufPtr = 0; bufPtr < bufSize; bufPtr++, addr++) {
      sramWriteByte(addr, buf[bufPtr]);
    }
  }

  sramDeselect();

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
      sprintf(S, "\r%06xh\t\t", addr);
      echo_all();
    }
    sprintf(S, "%02x\t", sramReadByte(addr));
    echo_all();
  }

  sramDeselect();
  databusWriteMode();
  // echo_all("\r\n\r\n", 4);
}

void sramDump(uint32_t starting = 0, uint32_t upto = SRAM_SIZE) {
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
}

// TODO make this take an UPTO since SRAM is bigger than needed for all games now
bool sramErase() {
  echo_all("Erasing SRAM");

  stopwatch = millis();

  sramSelect();
  databusWriteMode();

  for (uint32_t addr = 0; addr < SRAM_SIZE; addr++) {
    if (addr % 0x100 == 0) {
      echo_all(".");
    }
    sramWriteByte(addr, 0xff);
  }

  sprintf(S, "\r\nErased %d bytes of SRAM in %0.2f sec!\r\n", SRAM_SIZE, (millis() - stopwatch) / 1000.0);
  echo_all();

  sramDeselect();
  return true;
}

// Returns whether programming should continue
bool sramWriteBuffer(uint8_t* buf, size_t bufLen, uint32_t& addr, uint32_t expectedBytes) {
  for (int bufPtr = 0; bufPtr < bufLen && addr < expectedBytes; bufPtr++, addr++) {
    sramWriteByte(addr, buf[bufPtr]);
    // echo progress at fixed intervals
    if ((addr & 0x1fff) == 0) {
      sprintf(S, "%06xh ", addr);
      echo_all();
    }
  }


  if (addr >= expectedBytes) {
    sramDeselect();

    sprintf(S, "\r\nWrote %d bytes in %0.2f sec\r\n", addr, (millis() - stopwatch) / 1000.0);
    echo_all();
    echo_ok();

    return false;
  }
  return true;
}
