void loop() {
  static uint8_t buf[64];
  static bool isProgrammingFlash = false;
  static bool isProgrammingSram = false;
  static uint32_t addr = 0;
  static uint32_t expectedWords;
  static uint16_t idle = 0;

  if (!usb_web.available()) {
    if (++idle == 5000) {
      ledColor(0x303000); // yellow
      echo_all("idle\r\n");
    }
    delay(1);
    return;
  }

  idle = 0;

  size_t bufLen = usb_web.read(buf, 64);

  if (bufLen == 0) {
    return;
  }

  if (isProgrammingFlash) {
    isProgrammingFlash = flashWriteBuffer(buf, bufLen, addr, expectedWords);
    if (!isProgrammingFlash) ledColor(0);
  } else if (isProgrammingSram) {
    isProgrammingSram = sramWriteBuffer(buf, bufLen, addr);
    if (!isProgrammingSram) ledColor(0);
  }

  else if (buf[0] == 'E') {
    // ERASE COMMAND
    ledColor(BLUE);
    if (buf[1] == '\r') {
      flashClearLocks();
      flashErase();
      flashCommand(0, CMD_RESET);
      echo_all("!OK\r\n");
    } else if (buf[1] == '0' && buf[2] == '\r') {
      flashClearLocks();
      flashEraseBank(0);
      flashCommand(0, CMD_RESET);
      echo_all("!OK\r\n");
    } else if (buf[1] == '1' && buf[2] == '\r') {
      flashClearLocks();
      flashEraseBank(1);
      flashCommand(0, CMD_RESET);
      echo_all("!OK\r\n");
    } else if (buf[1] == 's' && buf[2] == '\r') {
      sramErase();
      echo_all("!OK\r\n");
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
    } else if (buf[1] == 's' && buf[2] == '\r') {
      // program SRAM always uses full 8kb
      // TODO no it doesn't
      echo_all("Programming SRAM\r");
      isProgrammingSram = true;
      addr = 0;
      stopwatch = millis();
      databusWriteMode();
      sramSelect();
      ledColor(BLUE);
    }
  }

  else if (buf[0] == 'S') {
    // SAVE FILE MANIPULATION
    ledColor(BLUE);

    // ID the header
    uint32_t cartId = flashCartHeaderId();

    // we always use <crc32>.sav which is 8 1 3 = 12 chars plus string terminator = 13 bytes
    const char SAVE_FILENAME_LEN = 13;
    char filename[SAVE_FILENAME_LEN];
    sprintf(filename, "%08x.sav", cartId);

    if (buf[1] == 'r' && buf[2] == '\r') {
      // backup SRAM to file
      len = sprintf(S, "Backing up SRAM contents to %s\r\n", filename);
      echo_all(S, len);
      sramSaveFile(filename);
      echo_all("Done!\r\n");
    } else if (buf[1] == 'w' && buf[2] == '\r') {
      // restore SRAM from file
      len = sprintf(S, "Restoring SRAM contents from %s\r\n", filename);
      echo_all(S, len);
      bool success = sramLoadFile(filename);
      echo_all(success ? "Success!\r\n" : "Failure!\r\n");
    }
    ledColor(0);
  }

  else {
    // Received unknown command OR it's possible we returned to idle state before the sender was done sending?
    echo_all("Unrecognized command\r\n");
    ledColor(0xce4e04); // orange
  }
}

void setup() {
  int problems = 0;
  led.begin();

  // USB setup
  TinyUSB_Device_Init(0);
  usb_web.setLandingPage(&landingPage);
  usb_web.setLineStateCallback(line_state_callback);
  usb_web.setStringDescriptor("Loopycart");
  usb_web.begin();

  // TODO remove this if we're purely going through webUSB, which seems to be the case now
  // TODO don't forget to update echo_all
  // Serial.begin(115200);

  // TODO use the LittleFS constructor to set the block size, currently 16kb, ideally 8kb
  // Filesystem. Auto formats. Make sure to reserve size in Tools > Flash Size
  if (!LittleFS.begin()) {
    len = sprintf(S, "%s littleFS fail", S);
  }

  // Setup IO expanders
  SPI.begin();
  if (!mcpAddr0.Init()) {
    len = sprintf(S, "%s mcpAddr0 fail", S);
    problems++;
    ledColor(0x800000);
    delay(2000);
    ledColor(0);
    delay(500);
  }
  if (!mcpAddr1.Init()) {
    len = sprintf(S, "%s mcpAddr1 fail", S);
    problems++;
    ledColor(0x804000);
    delay(2000);
    ledColor(0);
    delay(500);
  }
  if (!mcpData.Init()) {
    len = sprintf(S, "%s mcpData fail", S);
    problems++;
    ledColor(0x800040);
    delay(2000);
    ledColor(0);
    delay(500);
  }

  // Rated for 10MHZ
  mcpAddr0.setSPIClockSpeed(SPI_SPEED);
  mcpAddr1.setSPIClockSpeed(SPI_SPEED);
  mcpData.setSPIClockSpeed(SPI_SPEED);
  ioWriteMode(&mcpAddr0);
  ioWriteMode(&mcpAddr1);
  ioWriteMode(&mcpData);
  Serial.println("IO expanders initialized");

  // Externally pulled up, active/inserted low
  pinMode(PIN_INSERTED, INPUT);

  sramDeselect();
  busIdle();

  while (!TinyUSBDevice.mounted()) {
    delay(1);
  }

  ledColor(0xffffff);
  delay(100);
  ledColor(0);
}
