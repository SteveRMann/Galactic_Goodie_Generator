//mcp_read_test
//Reads mcp pins 0-5

#include <Wire.h>
#include <Adafruit_MCP23X17.h>

Adafruit_MCP23X17 mcp;


// ######################################## setup() ########################################
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println();
  Serial.println("=== MCP23017 GPA0–GPA5 INPUT TEST ===");

  // ESP8266 I2C pins
  Wire.begin(D6, D5);

  // Init MCP at address 0x20
  if (!mcp.begin_I2C()) {
    Serial.println("MCP NOT FOUND!");
    while (1);
  }
  Serial.println("MCP FOUND at 0x20");

  // Force IOCON into BANK=0 mode (important!)
  mcp.setupInterrupts(false, false, LOW);

  // Configure GPA0–GPA5 as INPUT_PULLUP
  for (int pin = 0; pin <= 5; pin++) {
    mcp.pinMode(pin, INPUT);          // ensure direction is set
    mcp.pinMode(pin, INPUT_PULLUP);   // enable pull-up
  }

  Serial.println("Pins GPA0–GPA5 set to INPUT_PULLUP");
  Serial.println("Press buttons or ground pins to test.");
}



// ######################################## loop() ########################################
void loop() {
  Serial.print("GPA0–5: ");

  for (int pin = 0; pin <= 5; pin++) {
    Serial.print(mcp.digitalRead(pin));
    if (pin < 5) Serial.print(",");
  }

  Serial.println();
  delay(200);
}
