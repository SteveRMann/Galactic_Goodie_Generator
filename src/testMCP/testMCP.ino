// ===============================================================
// Galactic Goodie Generator - Unified Door + Turntable Controller
// V6 - Added Adafruit VL53L0X TOF sensor to detect cup or hands in the device.
//      Also, last almost working version.
// ===============================================================

#include <Wire.h>
#include <Adafruit_MCP23X17.h>
Adafruit_MCP23X17 mcp;


// =========================
// Setup
// =========================
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("\nStarting MCP Test");

  Wire.begin(D6, D5);  // Start I2C on D6 (SDA) and D5 (SCL)

  if (!mcp.begin_I2C()) {
    Serial.println("MCP NOT FOUND!");
    Serial.println();
    while (1)
      ;
  }
  Serial.println("MCP Found, initializing MCP pins.");

  for (uint8_t pin = 0; pin < 8; pin++) {
    mcp.pinMode(pin, INPUT);
    mcp.pinMode(pin, INPUT_PULLUP);
  }

  for (uint8_t pin = 8; pin < 16; pin++) {
    mcp.pinMode(pin, OUTPUT);
  }


  Serial.println("Exercize MCP Output pin");
  while (true) {
    Serial.println("High");
    mcp.digitalWrite(8, HIGH);
    delay(500);

    Serial.println("Low");
    mcp.digitalWrite(8, LOW);
    delay(500);
  }


  Serial.println("Setup complete");
}



// ==========================================================================================
// Loop
// =========================
void loop() {
}
