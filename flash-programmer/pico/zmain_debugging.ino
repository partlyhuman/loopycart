// Keep Adafruit VID, custom PID for debugging (different from programming)
#define PID_DEBUG 0x239A
#define VID_DEBUG 0xF10D

int debugBaud = 38400;

void loop_debugging() {
  // Check for changed baud rate
  // Better to do this with callbacks if possible?
  int requestedBaud = Serial.baud();
  if(requestedBaud != debugBaud) {
    debugBaud = requestedBaud;
    Serial1.end(); // Maybe flush is enough?
    Serial1.begin(debugBaud);
  }

  // USB -> Loopy
  if (Serial.available()) {
    uint8_t b = Serial.read();
    Serial1.write(b);
  }

  // Loopy -> USB
  if (Serial1.available()) {
    uint8_t b = Serial1.read();
    Serial.write(b);
  }
}

void setup_debugging() {
  ledColor(0x001000);

  // Serial; // USB serial
  TinyUSBDevice.setID(VID_DEBUG, PID_DEBUG);
  //TinyUSBDevice.setProductDescriptor("Floopy Drive (Debug)"); // Adafruit_USBD_CDC annoyingly overrides this, fix with custom class?
  Serial.begin(debugBaud);

  // Serial1; // UART0 on alternative pins
  Serial1.setRX(13);
  Serial1.setTX(12);
  Serial1.setFIFOSize(256);
  Serial1.setPollingMode(true);
  Serial1.begin(debugBaud);

  // Wait for USB connection
  while (!Serial) {
    delay(1);
  }

  ledColor(0x008000);
}
