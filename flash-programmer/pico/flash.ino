// Make sure the BE1 don't keep getting twiddled back and forth,
// use 0 as a command address BUT KEEP BE1 SET
inline uint32_t zeroWithBank(uint32_t addr) {
  return addr & (1 << ADDRBITS - 1);
}

// Bus states - Sharp flash uses 3 wire control and these are predefined states

inline void busRead() {
  setControl(ROMCE & OE);
  databusReadMode();
}

inline void busWrite() {
  setControl(ROMCE & ROMWE);
  databusWriteMode();
}

inline void busIdle() {
  setControl(IDLE);
  databusWriteMode();
}

// The two basic building blocks: read a word, and send a command / write a word

uint16_t flashReadWord(uint32_t addr) {
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

  uint16_t data = readWord();
  busIdle();

  // tAVAV | Read Cycle Time | MIN 120ns
  NOP;
  NOP;
  NOP;

  return data;
}

void flashCommand(uint32_t addr, uint16_t data) {
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

// TODO involve STS pin now that we have access to it
// returns TRUE if OK
bool flashStatusCheck(uint32_t addr) {
  bool ok = true;
  flashCommand(zeroWithBank(addr), 0x70);
  flashReadStatus();
  if (!SR(7)) {
    echo_all("STATUS busy");
    ok = false;
  }
  if (SR(5) && SR(4)) {
    echo_all("STATUS improper command");
    ok = false;
  }
  if (SR(3)) {
    echo_all("STATUS undervoltage");
    ok = false;
  }
  if (SR(1)) {
    echo_all("STATUS locked");
    ok = false;
  }
  if (SR(2)) {
    echo_all("STATUS write suspended");
    ok = false;
  }
  if (SR(6)) {
    echo_all("STATUS erase suspended");
    ok = false;
  }
  return ok;
}

// Major functions

void flashEraseBank(int bank) {
  int32_t bankAddress = bank ? (1 << 21) : 0;
  len = sprintf(S, "Erase bank address %06xh\r", bankAddress);
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

  if (flashStatusCheck(bankAddress)) {
    echo_all("Bank erase successful!\r");
  }

  // if (SR(4) == 1 && SR(5) == 1) {
  //   echo_all("Invalid Bank Erase command sequence\r");
  // } else if (SR(5) == 1) {
  //   echo_all("Bank Erase error\r");
  // } else {
  //   echo_all("Bank Erase successful!\r");
  // }

  // clear status register
  flashCommand(0, 0x50);
  delayMicroseconds(100);
  // Read mode
  flashCommand(0, 0xff);
  delayMicroseconds(100);
}

void flashErase() {
  stopwatch = millis();

  echo_all("Erasing bank 0...\r");
  flashEraseBank(0);

  echo_all("!P.5\r");
  echo_all("Erasing bank 1...\r");
  flashEraseBank(1);

  len = sprintf(S, "Erased in %f sec\r", (millis() - stopwatch) / 1000.0);
  echo_all(S, len);
}

void flashClearLocks() {
  flashCommand(0, 0x60);
  flashCommand(0, 0xd0);

  do {
    flashReadStatus();
    delayMicroseconds(10);
  } while (!SR(7));

  if (flashStatusCheck(0)) {
    echo_all("Lock bits cleared successfully\r");
  }
}

// This should display the manufacturer and device code of the FLASH CHIP: B0 D0
void flashChipId() {
  busIdle();
  delayMicroseconds(100);

  flashCommand(0, 0x90);
  delayMicroseconds(100);

  busRead();
  delayMicroseconds(100);

  uint16_t manufacturer = flashReadWord(0);

  busIdle();
  delayMicroseconds(100);

  delay(100);
  uint16_t device = flashReadWord(2);

  databusWriteMode();
  flashCommand(0, CMD_RESET);
  delay(100);

  len = sprintf(S, "Manufacturer=%x Device=%x\r", manufacturer, device);
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
  SRD = readByte();
  busIdle();
}

// Does the cart start with 0E00 0080 like a loopy cart?
bool flashCartHeaderCheck() {
  databusReadMode();
  return (flashReadWord(0x0) == 0x0e00 && flashReadWord(0x2) == 0x0080);
}

// What's the internal CRC32 in the cart header? Use this as a cart ID.
uint32_t flashCartHeaderId() {
  databusReadMode();
  return (flashReadWord(0x8) << 16) | flashReadWord(0xA);
}

uint32_t flashCartHeaderSramSize() {
  databusReadMode();
  uint32_t sramStart = (flashReadWord(0x10) << 16 | flashReadWord(0x12));
  uint32_t sramEnd = (flashReadWord(0x14) << 16 | flashReadWord(0x16));
  return sramEnd - sramStart + 1;
}

void flashInspect(uint32_t starting = 0, uint32_t upto = (1 << ADDRBITS)) {
  flashCommand(zeroWithBank(starting), 0xff);
  delayMicroseconds(1);
  busRead();
  delayMicroseconds(1);

  for (uint32_t addr = starting; addr < upto; addr += 2) {
    if (addr % 0x10 == 0) {
      echo_all("\r", 1);
      len = sprintf(S, "%06xh\t\t", addr);
      echo_all(S, len);
    }
    len = sprintf(S, "%04x\t", flashReadWord(addr));
    echo_all(S, len);
  }

  echo_all("\r\n\r\n", 4);
  busIdle();
}

void flashDump(uint32_t starting = 0, uint32_t upto = (1 << ADDRBITS)) {
  flashCommand(zeroWithBank(starting), CMD_RESET);
  delayMicroseconds(100);
  busRead();

  for (uint32_t addr = starting; addr < upto; addr += 2) {
    uint16_t word = flashReadWord(addr);
    usb_web.write(word >> 8);
    usb_web.write(word & 0xff);
  }
  usb_web.flush();
  busIdle();
}

// Returns whether programming should continue
bool flashWriteBuffer(uint8_t *buf, size_t bufLen, uint32_t &addr, uint32_t expectedWords) {
#ifdef DEBUG_LED
  ledColor(0x400040);
#endif
  static int retries = 0;

  if ((bufLen % 2) == 1) {
    len = sprintf(S, "WARNING: odd number of bytes %d\r", bufLen);
    echo_all(S, len);
  }

  // echo progress at fixed intervals
  if ((addr & 0x1fff) == 0) {
    len = sprintf(S, "%06xh ", addr);
    if (retries > 0) {
      len = sprintf(S, "%S%d retries ", retries);
      retries = 0;
    }
    echo_all(S, len);
  }

#if OPT_MULTIBYTE
  // All of these are in BYTES
  // addr - overall address
  // buf - multi-byte buffer sent over by USB
  // bufLen - length of it
  // bufPtr - step through buffer using this

  // Multi-word write can write up to 32 bytes / 16 words
  const uint32_t bankBoundary = 1 << (ADDRBITS - 1);
  const size_t MAX_MULTIBYTE_WRITE = 32;
  bool atBoundary = false;
  uint32_t currentBank = zeroWithBank(addr);

  // Do however many multibyte writes necessary to empty the buffer
  for (int bufPtr = 0; bufPtr < bufLen;) {

    // Skip blocks of 0xff *between* multibyte writes only, assuming flash has been erased
    if (bufPtr + 1 < bufLen && buf[bufPtr] == buf[bufPtr + 1] == 0xff) {
      bufPtr += 2;
      addr += 2;
      continue;
    }

    int bytesToWrite = MIN(bufLen - bufPtr, MAX_MULTIBYTE_WRITE);
    // Don't allow multibyte writes to cross banks
    if (addr < bankBoundary && addr + bytesToWrite >= bankBoundary) {
      atBoundary = true;
      bytesToWrite = MIN(bankBoundary - addr, MAX_MULTIBYTE_WRITE);
      len = sprintf(S, "\r\nFinal write into bank 0 writing %d bytes from %x to %x\r\n", bytesToWrite, addr, addr + bytesToWrite);
      echo_all(S, len);
    }
    int wordsToWrite = bytesToWrite / 2;

    // Check status until we're ready to write more
    do {
      flashCommand(addr, 0xe8);
      flashReadStatus();

      if (SR(7)) {
        // READY
        break;
      } else {
        if (++retries > 1000) {
          len = sprintf(S, "\r\nTIMEOUT @ %06x\r\n", addr);
          echo_all(S, len);
          ledColor(0xFF0000);
          HALT;
        }
        // delayMicroseconds(10);
        continue;
      }


      // for bigger errors, have a bigger delay before retry
      if (SR(1) == SR(4) == 1) {
        len = sprintf(S, "Block lock error @ %06x\r\n", addr);
        echo_all(S, len);
      }
      if (SR(3) == SR(4) == 1) {
        len = sprintf(S, "Undervoltage error @ %06x\r\n", addr);
        echo_all(S, len);
      }
      if (SR(4) == 1 || SR(5) == 1) {
        len = sprintf(S, "Unable to multibyte write @ %06x\r\n", addr);
        echo_all(S, len);
      }
      delay(10);
    } while (!SR(7));

#ifdef DEBUG_LED
    ledColor(0x006000);
#endif

    // XSR.7 == 1 now, ready for write
    // A word/byte count (N)-1 is written with write address.
    flashCommand(addr, wordsToWrite - 1);

    // On the next write, device start address is written with buffer data.
    // Subsequent writes provide additional device address and data, depending on the count.
    // All subsequent address must lie within the start address plus the count.
    for (int j = 0; j < wordsToWrite; j++, addr += 2, bufPtr += 2) {
      uint16_t word = buf[bufPtr] << 8 | buf[bufPtr + 1];
      flashCommand(addr, word);
    }

    // After the final buffer data is written, write confirm (DOH) must be written.
    // This initiates WSM to begin copying the buffer data to the Flash Array.
    // flashCommand(0, 0xd0);
    // Use the bank we started with not the bank we ended with
    flashCommand(currentBank, 0xd0);

    if (atBoundary) {
      echo_all("\r\nFinishing first block\r\n");
      // while (!flashStatusCheck(currentBank)) {
      //   delay(100);
      // }
      // clear status register
      flashCommand(0, 0x50);
      delay(100);
      echo_all("\r\nStarting new block\r\n");
      flashCommand(bankBoundary, 0xff);
      delay(100);
      flashCommand(bankBoundary, 0x50);
      delay(100);
    }
  }

  if (addr >= 2 * expectedWords) {
    do {
      delayMicroseconds(100);
      flashReadStatus();
    } while (!SR(7));

    flashCommand(0, CMD_RESET);

    len = sprintf(S, "\r\nWrote %d bytes in %f sec using multibyte programming\r", addr * 2, (millis() - stopwatch) / 1000.0);
    echo_all(S, len);
    
    busIdle();
    return false;
  }

#ifdef DEBUG_LED
  ledColor(0x101010);
#endif

  return true;
#else
  // Single-byte programming
  for (int bufPtr = 0; bufPtr < bufLen; bufPtr += 2, addr += 2) {
    uint16_t word = buf[bufPtr] << 8 | buf[bufPtr + 1];
    // Skip padding assuming you have erased first
    if (word == 0xffff) continue;

    flashCommand(addr, 0x40);
    flashCommand(addr, word);
  }

  if (addr >= expectedWords * 2) {
    flashCommand(0, CMD_RESET);

    len = sprintf(S, "\r\nWrote %d bytes in %f sec using single-byte programming\r", addr * 2, (millis() - stopwatch) / 1000.0);
    echo_all(S, len);
    busIdle();

    ledColor(0);
    return false;
  }

  return true;
#endif
}