// Keep Adafruit VID, custom PID for debugging (different from programming)
#define PID_DEBUG 0x239A
#define VID_DEBUG 0xF10D

void loop_debugging() {
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
  Serial.begin(38400);

  // Serial1; // UART0 on alternative pins
  Serial1.setRX(13);
  Serial1.setTX(12);
  Serial1.begin(38400);

  // Wait for USB connection
  while (!Serial) {
    delay(1);
  }

  ledColor(0x008000);

  delay(500);
  Serial.print("∞ FLOOPY DRIVE ready to forward serial\r\n");
}
