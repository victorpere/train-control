/*
N-guage model railway control system using Arduino.
2-way communication with upstream control via serial.

Protocol (3 bytes):
  Byte 0: [type:1 | device_type:3 | device_id:4]
  Byte 1: [value:8] (signed for voltage: -128 to +127)
  Byte 2: [CRC8]

Device types:
  000 - Actual voltage (read-only)
  001 - Target voltage (write and read)

Request types:
  0 - Query (request device state)
  1 - Set (write value to device)
*/

// Device Types
const uint8_t DEVICE_TYPE_ACTUAL_VOLTAGE = 0b000;
const uint8_t DEVICE_TYPE_TARGET_VOLTAGE = 0b001;
const uint8_t DEVICE_TYPE_BLOCK = 0b010;
const uint8_t DEVICE_TYPE_SENSOR = 0b011;

// Device IDs
const uint8_t DEVICE_ID_TRACK = 0b0001;
const uint8_t DEVICE_ID_SENSOR01 = 0b0001;

// Message Types
const uint8_t MSG_TYPE_QUERY = 0;
const uint8_t MSG_TYPE_SET = 1;

// Pin numbers
const int PIN_SENSOR01 = 2;

// Track control constants
const int MAX_VOLTAGE = 128;
const int VOLTAGE_CHANGE_RATE = 1;
const unsigned long SENSOR_DEBOUNCE_TIME = 50;  // Debounce time for sensor in milliseconds
const unsigned long VOLTAGE_UPDATE_INTERVAL = 100;  // Update every 100ms

// Track control variables
int voltage = 0;
int targetVoltage = 0;
int sensorState01 = 0;
unsigned long lastSensorOnTime = 0;
unsigned long lastSensorOffTime = 0;
unsigned long lastVoltageUpdateTime = 0;


// Calculate CRC8 for two bytes (CRC8-ATM polynomial)
uint8_t calculateCRC8(uint8_t byte0, uint8_t byte1) {
  uint8_t crc = 0;
  
  crc ^= byte0;
  for (int i = 0; i < 8; i++) {
    crc = (crc << 1) ^ ((crc & 0x80) ? 0x07 : 0x00);
  }
  
  crc ^= byte1;
  for (int i = 0; i < 8; i++) {
    crc = (crc << 1) ^ ((crc & 0x80) ? 0x07 : 0x00);
  }
  
  return crc;
}

// Extract fields from header byte
uint8_t getMessageType(uint8_t header) {
  return (header >> 7) & 0x01;
}

uint8_t getDeviceType(uint8_t header) {
  return (header >> 4) & 0x07;
}

uint8_t getDeviceID(uint8_t header) {
  return header & 0x0F;
}

// Build header byte
uint8_t buildHeader(uint8_t msgType, uint8_t deviceType, uint8_t deviceID) {
  return (msgType << 7) | (deviceType << 4) | (deviceID & 0x0F);
}

// Send a message with header, value, and CRC
void sendMessage(uint8_t msgType, uint8_t deviceType, uint8_t deviceID, int msgValue) {
  uint8_t header = buildHeader(msgType, deviceType, deviceID);
  uint8_t value = (uint8_t)msgValue;
  uint8_t crc = calculateCRC8(header, value);
  
  uint8_t buf[3] = {header, value, crc};
  Serial.write(buf, 3);
}

void setup() {
  Serial.begin(9600);
  pinMode(5, OUTPUT);   // IN1 (PWM)
  pinMode(6, OUTPUT);   // IN2 (PWM)
  pinMode(PIN_SENSOR01, INPUT_PULLUP);

  // Increase PWM frequency on Timer 3 and 4 (pins 5, 6)
  // Shift frequency from ~490 Hz to ~3900 Hz (less noticeable)
  TCCR3B = (TCCR3B & 0b11111000) | 0b001;
  TCCR4B = (TCCR4B & 0b11111000) | 0b001;
}

void loop() {
  // Detect sensor
  if (digitalRead(PIN_SENSOR01) == LOW) {
    if (sensorState01 == 0 && (millis() - lastSensorOffTime > SENSOR_DEBOUNCE_TIME)) {
      // Sensor turned on
      sensorState01 = 1;
      lastSensorOnTime = millis();
      sendMessage(MSG_TYPE_SET, DEVICE_TYPE_SENSOR, DEVICE_ID_SENSOR01, 1);
    }
  } else {
    if (sensorState01 == 1 && (millis() - lastSensorOnTime > SENSOR_DEBOUNCE_TIME)) {
      // Sensor turned off
      sensorState01 = 0;
      lastSensorOffTime = millis();
      sendMessage(MSG_TYPE_SET, DEVICE_TYPE_SENSOR, DEVICE_ID_SENSOR01, 0);
    }
  }

  // Read incoming 3-byte messages
  if (Serial.available() >= 3) {
    uint8_t msgBuf[3];
    int bytesRead = Serial.readBytes(msgBuf, 3);
    uint8_t header = msgBuf[0];
    uint8_t value = msgBuf[1];
    uint8_t crc = msgBuf[2];
    
    uint8_t calcCRC = calculateCRC8(header, value);
    
    // Verify CRC
    if (calcCRC == crc) {
      uint8_t msgType = getMessageType(header);
      uint8_t deviceType = getDeviceType(header);
      uint8_t deviceID = getDeviceID(header);
      
      if (deviceID == DEVICE_ID_TRACK) {
        if (msgType == MSG_TYPE_SET && deviceType == DEVICE_TYPE_TARGET_VOLTAGE) {
          // Set target voltage
          targetVoltage = (int8_t)value;
          
          // Cap voltage
          if (targetVoltage > MAX_VOLTAGE) {
            targetVoltage = MAX_VOLTAGE;
          } else if (targetVoltage < -MAX_VOLTAGE) {
            targetVoltage = -MAX_VOLTAGE;
          }
        } else if (msgType == MSG_TYPE_QUERY && deviceType == DEVICE_TYPE_TARGET_VOLTAGE) {
          // Query target voltage
          sendMessage(MSG_TYPE_SET, DEVICE_TYPE_TARGET_VOLTAGE, DEVICE_ID_TRACK, targetVoltage);  
        } else if (msgType == MSG_TYPE_QUERY && deviceType == DEVICE_TYPE_ACTUAL_VOLTAGE) {
          // Query actual voltage
          sendMessage(MSG_TYPE_SET, DEVICE_TYPE_ACTUAL_VOLTAGE, DEVICE_ID_TRACK, voltage);
        }
      }
    }
  }

  unsigned long now = millis();
  if (now - lastVoltageUpdateTime >= VOLTAGE_UPDATE_INTERVAL) {
    if (voltage != targetVoltage) {
      if (voltage < targetVoltage) {
        voltage += VOLTAGE_CHANGE_RATE;
        if (voltage > targetVoltage) {
          voltage = targetVoltage;
        }
      } else {
        voltage -= VOLTAGE_CHANGE_RATE;
        if (voltage < targetVoltage) {
          voltage = targetVoltage;
        }
      }

      if (voltage > 0) {
        // Forward
        analogWrite(5, voltage);    // IN1 gets PWM
        analogWrite(6, 0);          // IN2 stays off
      } else if (voltage < 0) {
        // Backward
        analogWrite(5, 0);
        analogWrite(6, -voltage);   // IN2 gets PWM (negate to positive)
      } else {
        // Stop
        analogWrite(5, 0);
        analogWrite(6, 0);
      }
      
      // Send status update
      sendMessage(MSG_TYPE_SET, DEVICE_TYPE_ACTUAL_VOLTAGE, DEVICE_ID_TRACK, voltage);
    }

    lastVoltageUpdateTime = now;
  }
}
