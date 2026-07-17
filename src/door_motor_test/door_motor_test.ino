#include <Wire.h>
#include <Adafruit_MCP23X17.h>

Adafruit_MCP23X17 mcp;

// Door motor pins
const int DOOR_PWM_PIN = D2;     // ESP8266 PWM pin
const uint8_t DOOR_DIR_PIN = 9;  // MCP23017 GPB1

// Motor speeds
const int OPEN_SPEED = 260;
const int CLOSE_SPEED = 110;

void setup() {
  Serial.begin(115200);
  Serial.println("\nDoor Motor Test Starting");

  Wire.begin(D6, D5);

  if (!mcp.begin_I2C()) {
    Serial.println("MCP NOT FOUND!");
    while (1)
      ;
  }

  // Force GPB pins to OUTPUT (8–15)
  for (uint8_t pin = 8; pin < 16; pin++) {
    mcp.pinMode(pin, OUTPUT);
    mcp.digitalWrite(pin, LOW);  // clear OLAT
  }

  analogWriteFreq(1000);

  // Ensure motor is stopped at boot
  mcp.digitalWrite(DOOR_DIR_PIN, LOW);
  analogWrite(DOOR_PWM_PIN, 0);

  Serial.println("Setup complete");
}

void loop() {

  // OPEN
  Serial.println("OPEN- L+speed");
  mcp.digitalWrite(DOOR_DIR_PIN, LOW);
  analogWrite(DOOR_PWM_PIN, OPEN_SPEED);
  delay(2000);

  // STOP
  Serial.println("STOP- L+0");
  mcp.digitalWrite(DOOR_DIR_PIN, LOW);
  analogWrite(DOOR_PWM_PIN, 0);
  delay(2000);

  // CLOSE
  Serial.println("CLOSE- H+Speed");
  mcp.digitalWrite(DOOR_DIR_PIN, HIGH);
  analogWrite(DOOR_PWM_PIN, CLOSE_SPEED);
  delay(2000);

  // STOP
  Serial.println("STOP- L+0");
  mcp.digitalWrite(DOOR_DIR_PIN, LOW);
  analogWrite(DOOR_PWM_PIN, 0);
  delay(2000);
}
