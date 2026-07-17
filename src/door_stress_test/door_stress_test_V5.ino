// ===============================================================
// Door Stress Tester (Safe for bench testing)
// Opens for OPEN_TIME, brakes, waits, closes for CLOSE_TIME, brakes, waits.
// V3: Added DOWN limit switch on GPA6.
// V4: Added open/close switch LEDs
// V5: No longer using PWM to control speed.
// ===============================================================

#include <Wire.h>
#include <Adafruit_MCP23X17.h>
Adafruit_MCP23X17 mcp;

// Door motor pins
const uint8_t IA_A = D1;  // ESP pin D1
const uint8_t IB_A = 8;   // MCP pin GPB0

// Limit switches (MCP23017 inputs)
const uint8_t UP_SWITCH = 4;  // GPA4
const uint8_t DN_SWITCH = 6;  // GPA6



// =========================
// Tuneable parameters
// =========================
const uint32_t OPEN_TIME = 9000;
const uint32_t CLOSE_TIME = 9000;
const uint32_t WAIT_TIME = 1000;  // Wait times between cycles



// =========================
// Indicator LEDs
// =========================
const uint8_t HEARTBEAT_LED = 10;  // Blue LED on MCP Pin 3 (GPB2)
const uint8_t CUP_LED = 11;        // Red LED on MCP Pin 4 (GPB3), ON when cup is present
const uint8_t OPEN_SW_LED = 12;    // Yellow LED on MCP Pin 5 (GPB4), ON when door is open
const uint8_t CLOSE_SW_LED = 13;   // Green LED on MCP Pin 6 (GPB5), ON when door is closed



enum Phase { OPENING,
             WAIT1,
             CLOSING,
             WAIT2 };
Phase phase = OPENING;

uint32_t timer = 0;


// ###################
// Door Functions
// ###################
void doorOpen() {
  digitalWrite(IA_A, HIGH);  // UP direction
  mcp.digitalWrite(IB_A, LOW);
}
void doorClose() {
  digitalWrite(IA_A, LOW);  // DOWN direction
  mcp.digitalWrite(IB_A, HIGH);
}
void doorStop() {
  digitalWrite(IA_A, LOW);  // stop
  mcp.digitalWrite(IB_A, LOW);
}



// This forces any stuck slave to release SDA void i2c_bus_recover() {
void i2c_bus_recover() {
  pinMode(D5, OUTPUT);        // SCL
  pinMode(D6, INPUT_PULLUP);  // SDA

  for (int i = 0; i < 9; i++) {
    digitalWrite(D5, LOW);
    delayMicroseconds(5);
    digitalWrite(D5, HIGH);
    delayMicroseconds(5);
  }
}




// ===================================================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\nDoor Stress Tester Starting");

  // Configure the ESP pins
  pinMode(IA_A, OUTPUT);

  // Make sure motor doesn't start before the MCP is initialized/
  // (IA_A has a 10K pulldown on it).
  digitalWrite(IA_A, LOW);



  // --- Start I2C ---
  //i2c_bus_recover();
  Wire.begin(D6, D5);  // SDA, SCL
  delay(50);           // Give the MCP a few more ms to start up.

  // --- Retry MCP detection ---
  const int retries = 5;
  bool mcp_ok = false;

  for (int i = 0; i < retries; i++) {
    if (mcp.begin_I2C()) {
      mcp_ok = true;
      break;
    }
    Serial.println("MCP NOT FOUND, retrying...");
    delay(200);
  }

  // --- If MCP still missing, reboot ---
  if (!mcp_ok) {
    Serial.println("MCP STILL NOT FOUND — MOTOR DISABLED — REBOOTING");
    delay(500);
    ESP.restart();
  }

  Serial.println("MCP Found, continuing.");

  // --- Now that MCP is alive, configure its pins ---
  mcp.pinMode(IB_A, OUTPUT);
  mcp.pinMode(HEARTBEAT_LED, OUTPUT);
  mcp.pinMode(CUP_LED, OUTPUT);
  mcp.pinMode(OPEN_SW_LED, OUTPUT);
  mcp.pinMode(CLOSE_SW_LED, OUTPUT);
  mcp.pinMode(UP_SWITCH, INPUT_PULLUP);
  mcp.pinMode(DN_SWITCH, INPUT_PULLUP);




  // Flash the LEDS to indicate that the MCP is initialized
  for (int i = 0; i < 3; i++) {
    mcp.digitalWrite(HEARTBEAT_LED, HIGH);  // Blue
    delay(150);
    mcp.digitalWrite(CUP_LED, HIGH);  // Red
    delay(150);
    mcp.digitalWrite(OPEN_SW_LED, HIGH);  // Yellow
    delay(150);
    mcp.digitalWrite(CLOSE_SW_LED, HIGH);  // Green
    delay(250);

    mcp.digitalWrite(HEARTBEAT_LED, LOW);  // Blue
    delay(150);
    mcp.digitalWrite(CUP_LED, LOW);  // Red
    delay(150);
    mcp.digitalWrite(OPEN_SW_LED, LOW);  // Yellow
    delay(150);
    mcp.digitalWrite(CLOSE_SW_LED, LOW);  // Green
    delay(250);
  }

  timer = millis();
  Serial.println("Setup complete.");
}



// ===================================================
void loop() {
  // ---------- Heartbeat ----------
  static uint32_t lastBeat = 0;
  static bool hbOn = false;
  static uint32_t hbStart = 0;
  uint32_t now = millis();
  if (!hbOn && now - lastBeat >= 1000) {
    //Serial.println("In loop");
    mcp.digitalWrite(HEARTBEAT_LED, HIGH);
    hbStart = now;
    hbOn = true;
    lastBeat = now;
  }
  if (hbOn && now - hbStart >= 50) {
    mcp.digitalWrite(HEARTBEAT_LED, LOW);
    hbOn = false;
  }
  // -------------------------------


  // ----- Show door limit switch LEDs -----
  if (mcp.digitalRead(UP_SWITCH) == LOW) {
    mcp.digitalWrite(OPEN_SW_LED, HIGH);
  } else {
    mcp.digitalWrite(OPEN_SW_LED, LOW);
  }
  if (mcp.digitalRead(DN_SWITCH) == LOW) {
    mcp.digitalWrite(CLOSE_SW_LED, HIGH);
  } else {
    mcp.digitalWrite(CLOSE_SW_LED, LOW);
  }


  // ---------------------------------------------------
  switch (phase) {
    case OPENING:
      doorOpen();

      // UP limit switch
      if (mcp.digitalRead(UP_SWITCH) == LOW) {
        doorStop();
        Serial.println("OPEN: UP limit switch hit");
        phase = WAIT1;
        timer = millis();
        break;
      }

      // Timeout
      if (millis() - timer >= OPEN_TIME) {
        doorStop();
        Serial.println("OPEN: Timeout");
        phase = WAIT1;
        timer = millis();
      }
      break;


      // ---------------------------------------------------
    case WAIT1:
      if (millis() - timer >= WAIT_TIME) {
        phase = CLOSING;
        timer = millis();  // start CLOSING timer
      }
      break;


    // ---------------------------------------------------
    case CLOSING:
      doorClose();

      // DOWN limit switch
      if (mcp.digitalRead(DN_SWITCH) == LOW) {
        doorStop();
        Serial.println("CLOSE: DOWN limit switch hit");
        phase = WAIT2;
        timer = millis();
        break;
      }

      // Timeout
      if (millis() - timer >= CLOSE_TIME) {
        doorStop();
        Serial.println("CLOSE: Timeout");
        phase = WAIT2;
        timer = millis();
      }
      break;


    // ---------------------------------------------------
    case WAIT2:
      if (millis() - timer >= WAIT_TIME) {
        phase = OPENING;
        timer = millis();  // Start the opening timer
      }
      break;
  }

  delay(1);  // Feed ESP8266 watchdog
}
