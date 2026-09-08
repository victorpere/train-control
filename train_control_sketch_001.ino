int voltage = 0;
int targetVoltage = 0;
const int maxVoltage = 128;
const int changeRate = 1;

unsigned long lastUpdateTime = 0;
const unsigned long updateInterval = 100;  // Update every 100ms

void setup() {
  Serial.begin(9600);
  pinMode(5, OUTPUT);   // IN1 (PWM)
  pinMode(6, OUTPUT);   // IN2 (PWM)

  // Increase PWM frequency on Timer 3 (pins 5, 6)
  // Shift frequency from ~490 Hz to ~3900 Hz (less noticeable)
  TCCR3B = (TCCR3B & 0b11111000) | 0b001;

  // Serial.println("Enter voltage (0-255). 0=stop, 128=half, 255=full");
  // Serial.println("Positive=forward, Negative=backward");
}

void loop() {
  if (Serial.available()) {
    targetVoltage = Serial.parseInt();
    while (Serial.available() && Serial.read() != '\n');  // Clear the newline

    // Cap voltage
    if (targetVoltage > maxVoltage) {
      targetVoltage = maxVoltage;
    } else if (targetVoltage < -maxVoltage) {
      targetVoltage = -maxVoltage;
    }

    // Serial.print("Target voltage: ");
    // Serial.println(targetVoltage);
    // Serial.print("Current voltage: ");
    // Serial.println(voltage);
  }

  unsigned long now = millis();
  if (now - lastUpdateTime >= updateInterval) {
    if (voltage != targetVoltage) {
      if (voltage < targetVoltage) {
        voltage += changeRate;
        if (voltage > targetVoltage) {
          voltage = targetVoltage;
        }
      } else {
        voltage -= changeRate;
        if (voltage < targetVoltage) {
          voltage = targetVoltage;
        }
      }

      if (voltage > 0) {
        // Forward
        analogWrite(5, voltage);    // IN1 gets PWM
        analogWrite(6, 0);        // IN2 stays off
        Serial.write((byte)voltage);
      } else if (voltage < 0) {
        // Backward
        analogWrite(5, 0);
        analogWrite(6, -voltage);   // IN2 gets PWM (negate to positive)
        Serial.write((byte)(256-voltage));
      } else {
        // Stop
        analogWrite(5, 0);
        analogWrite(6, 0);
        Serial.write((byte)0);
      }

      if (voltage == targetVoltage) {
        // Serial.print("Target voltage reached: ");
        // Serial.println(voltage);
      }
    }

    lastUpdateTime = now;
  }
}
