static bool PROGRAM_DEBUGGING = false;

void onInserted() {
  bool isInserted = digitalRead(PIN_INSERTED) == LOW;
  if (PROGRAM_DEBUGGING != isInserted) {
    ledColor(0x80ff80);
    rp2040.reboot();
  }
}

void loop() {
  if (PROGRAM_DEBUGGING) {
    loop_debugging();
  } else {
    loop_programming();
  }
}

void setup() {
  led.begin();

  // Externally pulled up, active/inserted low
  pinMode(PIN_INSERTED, INPUT);
  PROGRAM_DEBUGGING = digitalRead(PIN_INSERTED) == LOW;

  attachInterrupt(digitalPinToInterrupt(PIN_INSERTED), onInserted, CHANGE);

  if (PROGRAM_DEBUGGING) {
    setup_debugging();
  } else {
    setup_programming();
  }
}
