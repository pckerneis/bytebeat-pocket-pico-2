const uint8_t DRIVE = 5;   // what you previously called "columns"
const uint8_t SENSE = 4;   // what you previously called "rows"

// DRIVE side = diode ANODES (must be driven LOW)
uint8_t drivePins[DRIVE] = {6, 7, 8, 9, 10};

// SENSE side = diode CATHODES (ring) via button
uint8_t sensePins[SENSE] = {2, 3, 4, 5};

void setup() {
  Serial.begin(115200);
  Serial.println("5x4 MATRIX — CORRECTED TOPOLOGY");
  Serial.println();

  // Drive side: outputs, idle HIGH
  for (uint8_t d = 0; d < DRIVE; d++) {
    pinMode(drivePins[d], OUTPUT);
    digitalWrite(drivePins[d], HIGH);
  }

  // Sense side: inputs with pullups
  for (uint8_t s = 0; s < SENSE; s++) {
    pinMode(sensePins[s], INPUT_PULLUP);
  }
}

void loop() {
  for (uint8_t d = 0; d < DRIVE; d++) {
    digitalWrite(drivePins[d], LOW);   // activate line

    for (uint8_t s = 0; s < SENSE; s++) {
      if (digitalRead(sensePins[s]) == LOW) {
        Serial.print("D");
        Serial.print(d);
        Serial.print(" S");
        Serial.println(s);
        delay(150); // crude debounce
      }
    }

    digitalWrite(drivePins[d], HIGH);  // deactivate line
  }
}
