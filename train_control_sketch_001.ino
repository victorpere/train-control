int speed = 0;
int targetSpeed = 0;
const int maxSpeed = 128;
const int acceleration = 1;

unsigned long lastUpdateTime = 0;
const unsigned long updateInterval = 100;  // Update every 100ms

void setup() {
  Serial.begin(9600);
  pinMode(5, OUTPUT);   // IN1 (PWM)
  pinMode(6, OUTPUT);   // IN2 (PWM)

  // Increase PWM frequency on Timer 3 (pins 5, 6)
  // Shift frequency from ~490 Hz to ~3900 Hz (less noticeable)
  TCCR3B = (TCCR3B & 0b11111000) | 0b001;

  // Serial.println("Enter speed (0-255). 0=stop, 128=half, 255=full");
  // Serial.println("Positive=forward, Negative=backward");
}

void loop() {
  if (Serial.available()) {
    targetSpeed = Serial.parseInt();
    while (Serial.available() && Serial.read() != '\n');  // Clear the newline

    // Cap speed
    if (targetSpeed > maxSpeed) {
      targetSpeed = maxSpeed;
    } else if (targetSpeed < -maxSpeed) {
      targetSpeed = -maxSpeed;
    }

    // Serial.print("Target speed: ");
    // Serial.println(targetSpeed);
    // Serial.print("Current speed: ");
    // Serial.println(speed);
  }

  unsigned long now = millis();
  if (now - lastUpdateTime >= updateInterval) {
    if (speed != targetSpeed) {
      if (speed < targetSpeed) {
        speed += acceleration;
      } else {
        speed -= acceleration;
      }

      if (speed > 0) {
        // Forward
        analogWrite(5, speed);    // IN1 gets PWM
        analogWrite(6, 0);        // IN2 stays off
      } else if (speed < 0) {
        // Backward
        analogWrite(5, 0);
        analogWrite(6, -speed);   // IN2 gets PWM (negate to positive)
      } else {
        // Stop
        analogWrite(5, 0);
        analogWrite(6, 0);
      }

      // Write speed to serial
      Serial.write((byte)speed);

      if (speed == targetSpeed) {
        // Serial.print("Target speed reached: ");
        // Serial.println(speed);
      }
    }

    lastUpdateTime = now;
  }
}
