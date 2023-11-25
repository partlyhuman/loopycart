#define ABORT_AFTER_MS 1000

void loop_programming() {
  static uint8_t buf[64];
  static bool isProgrammingFlash = false;
  static bool isProgrammingSram = false;
  static uint32_t addr = 0;
  static uint32_t expectedWords;
  static uint32_t idleSince = 0;


  if (!usb_web.available()) {
    // DEBUGGING idle detect shows the pico is still looping if other things have halted due to error
    if (idleSince == 0) {
      idleSince = millis();
    } else if (millis() - idleSince > ABORT_AFTER_MS) {
      if (isProgrammingFlash || isProgrammingSram) {
        ledColor(0x303000);  // yellow
        isProgrammingFlash = false;
        isProgrammingSram = false;
        echo_all("!ERROR Aborted programming after no data\r\n");
      }
    }
    return;
  }
  idleSince = 0;

  size_t bufLen = usb_web.read(buf, 64);

  if (bufLen == 0) {
    return;
  }

  if (isProgrammingFlash) {
    isProgrammingFlash = flashWriteBuffer(buf, bufLen, addr, expectedWords);
    if (!isProgrammingFlash) ledColor(0);
  } else if (isProgrammingSram) {
    isProgrammingSram = sramWriteBuffer(buf, bufLen, addr, expectedWords * 2);
    if (!isProgrammingSram) ledColor(0);
  }

  else if (buf[0] == 'E') {
    // ERASE COMMAND
    ledColor(BLUE);
    if (buf[1] == '\r') {
      flashClearLocks();
      flashErase();
      flashCommand(0, CMD_RESET);
      echo_ok();
    } else if (buf[1] == '0' && buf[2] == '\r') {
      flashClearLocks();
      flashEraseBank(0);
      flashCommand(0, CMD_RESET);
      echo_ok();
    } else if (buf[1] == '1' && buf[2] == '\r') {
      flashClearLocks();
      flashEraseBank(1);
      flashCommand(0, CMD_RESET);
      echo_ok();
    } else if (buf[1] == 's' && buf[2] == '\r') {
      sramErase();
      echo_ok();
    }
    ledColor(0);
  }

  else if (buf[0] == 'I' && buf[1] == '\r') {
    // INSPECT COMMAND
    // flashId();
    echo_all("\r\n---CART-ID---\r\n");
    len = sprintf(S, "Cart header %s\r\n", flashCartHeaderCheck() ? "OK" : "NOT OK");
    echo_all(S, len);
    len = sprintf(S, "Cart ID %08x\r\n", flashCartHeaderId());
    echo_all(S, len);
    echo_all("\r\n---FLASH-lower---\r\n");
    flashInspect(0x000000, 0x000100);
    echo_all("\r\n------\r\n");
    flashInspect(0x100000, 0x100100);
    echo_all("\r\n---FLASH-upper---\r\n");
    flashInspect(0x200000, 0x200100);
    echo_all("\r\n------\r\n");
    flashInspect(0x300000, 0x300100);
    echo_all("\r\n---SRAM---\r\n");
    sramInspect(0, 0x100);
    echo_all("\r\n---FILESYSTEM---\r\n");
    Dir dir = LittleFS.openDir("/");
    while (dir.next()) {
      echo_all(dir.fileName().c_str());
      echo_all("\r\n");
    }

    // Some info in the info64 struct comes out as zeroes
    // FSInfo64 info;
    // LittleFS.info64(info);
    FSInfo info;
    LittleFS.info(info);
    len = sprintf(S, "%d/%d bytes used, %0.0f%% full\r\ntotal bytes=%d, block size=%d, pageSize=%d\r\n", info.usedBytes, info.totalBytes, 100.0 * (info.usedBytes + 0.0) / info.totalBytes, info.blockSize, info.pageSize);
    echo_all(S, len);

    busIdle();
  }

  else if (buf[0] == 'D') {
    // DUMP COMMAND
    ledColor(BLUE);
    // followed by expected # of bytes
    if (sscanf((char *)buf, "D%d\r", &expectedWords) && expectedWords > 0 && expectedWords <= 1 << ADDRBITS) {
      flashDump(0, expectedWords);
    } else if (buf[1] == 's' && buf[2] == '\r') {
      sramDump();
    }
    ledColor(0);
  }

  else if (buf[0] == 'P') {
    // PROGRAM COMMAND
    if (sscanf((char *)buf, "P%d\r", &expectedWords)) {
      // program flash followed by expected # of bytes
      len = sprintf(S, "Programming %d bytes to flash\r", expectedWords * 2);
      echo_all(S, len);
      isProgrammingFlash = true;
      addr = 0;
      stopwatch = millis();
      busIdle();
      ledColor(BLUE);
    } else if (sscanf((char *)buf, "Ps%d\r", &expectedWords)) {
      // program SRAM followed by expected # of bytes
      len = sprintf(S, "Programming %d bytes to SRAM\r", expectedWords * 2);
      echo_all(S, len);
      isProgrammingSram = true;
      addr = 0;
      stopwatch = millis();
      databusWriteMode();
      sramSelect();
      ledColor(BLUE);
    }
  }

  else if (buf[0] == 'S') {
    // SRAM BACKUP & RESTORE FROM FILESYSTEM
    ledColor(BLUE);

    // ID the header
    uint32_t cartId = flashCartHeaderId();

    // we always use <crc32>.sav which is 8 1 3 = 12 chars plus string terminator = 13 bytes
    const size_t SAVE_FILENAME_LEN = 13;
    char filename[SAVE_FILENAME_LEN];
    sprintf(filename, "%08x.sav", cartId);

    if (buf[1] == 'r' && buf[2] == '\r') {
      // backup SRAM to file
      uint32_t sramSize = min(flashCartHeaderSramSize(), SRAM_SIZE);
      len = sprintf(S, "Backing up first %dkb of SRAM to %s\r\n", sramSize / 1024, filename);
      echo_all(S, len);

      sramSaveFile(filename, sramSize);
      echo_ok();
    } else if (buf[1] == 'w' && buf[2] == '\r') {
      // restore SRAM from file
      len = sprintf(S, "Restoring SRAM contents from %s\r\n", filename);
      echo_all(S, len);
      sramLoadFile(filename) || sramErase();
      echo_ok();
    } else if (buf[1] == 'f' && buf[2] == '\r') {
      // Clear filesystem!
      echo_all("Clearing all save states stored on Floopy Drive!!\r\n");
      LittleFS.format();
      echo_ok();
    }

    ledColor(0);
  }

  else {
    // Received unknown command OR it's possible we returned to idle state before the sender was done sending?
    echo_all("Unrecognized command\r\n");
    ledColor(0xce4e04);  // orange
  }
}

void setup_programming() {
  // Serial allows us to more easily reset/reprogram the Pico
  // In production, consider reworking echo_all and removing this?
  Serial.begin(115200);

  // USB setup
  TinyUSB_Device_Init(0);
  usb_web.setLandingPage(&landingPage);
  usb_web.setLineStateCallback(line_state_callback);
  usb_web.setStringDescriptor("Floopy Drive");
  usb_web.begin();

  // TODO use the LittleFS constructor to set the block size, currently 16kb, ideally 8kb
  // Filesystem. Auto formats. Make sure to reserve size in Tools > Flash Size
  if (!LittleFS.begin()) {
    len = sprintf(S, "%s littleFS fail", S);
  }

  // Setup IO expanders
  SPI.begin();
  // Rated for 10MHZ
  mcpAddr0.setSPIClockSpeed(SPI_SPEED);
  mcpAddr1.setSPIClockSpeed(SPI_SPEED);
  mcpData.setSPIClockSpeed(SPI_SPEED);
  if (!mcpAddr0.Init()) {
    len = sprintf(S, "%s mcpAddr0 fail", S);
    ledColor(0x800000);
    delay(2000);
    ledColor(0);
    delay(500);
  }
  if (!mcpAddr1.Init()) {
    len = sprintf(S, "%s mcpAddr1 fail", S);
    ledColor(0x804000);
    delay(2000);
    ledColor(0);
    delay(500);
  }
  if (!mcpData.Init()) {
    len = sprintf(S, "%s mcpData fail", S);
    ledColor(0x800040);
    delay(2000);
    ledColor(0);
    delay(500);
  }

  ioWriteMode(&mcpData);
  ioWriteMode(&mcpAddr0);

  // CAREFUL what happens when some of these active-low control lines are set momentarily
  busIdle();
  ioWriteMode(&mcpAddr1);
  busIdle();

  // Backup SRAM anytime plugging in, woohoo
  uint32_t cartId = flashCartHeaderId();
  if (cartId != 0xffffffff) {
    ledColor(BLUE);
    const size_t SAVE_FILENAME_LEN = 13;
    char filename[SAVE_FILENAME_LEN];
    sprintf(filename, "%08x.sav", cartId);
    uint32_t sramSize = min(flashCartHeaderSramSize(), SRAM_SIZE);
    sramSaveFile(filename, sramSize);
    ledColor(0);
  }

  // Let either claim the USB
  while (!TinyUSBDevice.mounted() && !Serial) {
    delay(1);
  }

  ledColor(0xffffff);
  delay(100);
  ledColor(0);
}
