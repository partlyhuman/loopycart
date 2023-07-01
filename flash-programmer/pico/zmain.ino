void loop() {
  static uint8_t buf[64];
  static bool isProgrammingFlash = false;
  static bool isProgrammingSram = false;
  static uint32_t addr = 0;
  static uint32_t expectedWords;

  if (!usb_web.available()) {
    delay(1);
    return;
  }

  size_t bufLen = usb_web.read(buf, 64);

  if (bufLen == 0) {
    return;
  }

  if (isProgrammingFlash) {
    isProgrammingFlash = flashWriteBuffer(buf, bufLen, addr, expectedWords);
  } else if (isProgrammingSram) {
    isProgrammingSram = sramWriteBuffer(buf, bufLen, addr);
  }

  else if (buf[0] == 'E') {
    // ERASE COMMAND
    if (buf[1] == '\r') {
      flashClearLocks();
      flashErase();
      flashCommand(0, CMD_RESET);
    } else if (buf[1] == '0' && buf[2] == '\r') {
      flashClearLocks();
      flashEraseBank(0);
      flashCommand(0, CMD_RESET);
    } else if (buf[1] == '1' && buf[2] == '\r') {
      flashClearLocks();
      flashEraseBank(1);
      flashCommand(0, CMD_RESET);
    } else if (buf[1] == 's' && buf[2] == '\r') {
      sramErase();
    }
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

    FSInfo64 info;
    LittleFS.info64(info);
    len = sprintf(S, "%d bytes used\r\n\r\n", info.usedBytes);
    echo_all(S, len);
  }

  else if (buf[0] == 'D') {
    // DUMP COMMAND
    // followed by expected # of bytes
    if (sscanf((char *)buf, "D%d\r", &expectedWords) && expectedWords > 0 && expectedWords <= 1 << ADDRBITS) {
      flashDump(0, expectedWords);
    } else if (buf[1] == 's' && buf[2] == '\r') {
      sramDump();
    }
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

      digitalWriteFast(LED_BUILTIN, HIGH);
    } else if (buf[1] == 's' && buf[2] == '\r') {
      // program SRAM always uses full 8kb
      echo_all("Programming SRAM\r");
      isProgrammingSram = true;
      addr = 0;
      stopwatch = millis();
      databusWriteMode();
      sramSelect();
      digitalWriteFast(LED_BUILTIN, HIGH);
    }
  }

  else if (buf[0] == 'S') {
    // SAVE FILE MANIPULATION

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
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWriteFast(LED_BUILTIN, LOW);

  // USB setup
  TinyUSB_Device_Init(0);
  usb_web.setLandingPage(&landingPage);
  usb_web.setLineStateCallback(line_state_callback);
  usb_web.setStringDescriptor("Loopycart");
  usb_web.begin();

  // TODO remove this if we're purely going through webUSB, which seems to be the case now
  // TODO don't forget to update echo_all
  Serial.begin(115200);

  // Filesystem. Auto formats. Make sure to reserve size in Tools > Flash Size
  if (!LittleFS.begin()) {
    flashLed(20);
    HALT;
  }

  // Setup IO expanders
  SPI.begin();
  if (!mcpA.Init() || !mcpD.Init()) {
    flashLed(50);
    HALT;
  }
  // Rated for 10MHZ
  mcpA.setSPIClockSpeed(10000000);
  mcpD.setSPIClockSpeed(10000000);
  ioWriteMode(&mcpD);
  // Address bus always in write mode
  ioWriteMode(&mcpA);
  Serial.println("IO expanders initialized");

  pinMode(PIN_RAMCS2, OUTPUT);
  pinMode(PIN_RAMCS1, OUTPUT);
  pinMode(PIN_RAMWE, OUTPUT);

  // Setup ROM pins
  pinMode(PIN_ROMCE, OUTPUT);
  pinMode(PIN_ROMWE, OUTPUT);
  pinMode(PIN_OE, OUTPUT);

  // Setup overflow address pins
  pinMode(PIN_A0, OUTPUT);
  pinMode(PIN_A17, OUTPUT);
  pinMode(PIN_A18, OUTPUT);
  pinMode(PIN_A19, OUTPUT);
  pinMode(PIN_A20, OUTPUT);
  pinMode(PIN_A21, OUTPUT);

  sramDeselect();
  busIdle();

  flashLed(2);

  while (!TinyUSBDevice.mounted()) {
    delay(1);
  }

  flashLed(4);
}
