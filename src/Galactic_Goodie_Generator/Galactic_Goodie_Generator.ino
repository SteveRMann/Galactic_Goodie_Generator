// ===============================================================
// Galactic Goodie Generator - Unified Door + Turntable Controller
// V6 - Added Adafruit VL53L0X TOF sensor to detect cup or hands in the device.
//      Also, last almost working version.
// V7 - Various experiments
// V8 - Moved bump from setup to a function.
// V9 - Added door open failsafe timeout
// V10- Moved door failsafes to the updateDoorMotor() function
// V11- Removed stress stuff, added cup detector
// V12- If cupPresent == true,  do not close the door.
// V13- DOWN limit switch logic added, Remove all PWM.
// V14- Added safety logic. If a hand reaches into the opening while the door is closing,
//      reverse the close (open). When the object is removed, resume closing the door.
// V15- In ttStop(), momentarily reverse the motor.
// V16- Trigger starts everything - firs
// V18- Cleaning up the code and simplifying the turntable state machine
// V19- Stable
// V20- Keep the door open during loading.
// V21- Stable, door stays open during loading.
// ===============================================================

/*
Expected performance:
Idle: Wait for user to press the trigger button. (Or the Load button).
User presses Trigger button: Rotate turntable until the index switch -> FALSE (clearing. 
Index FALSE: Continue rotating the turntable until index switch -> TRUE.
Index TRUE: Wait one second then open door
DoorOpen TRUE: Wait for cupPresent to go FALSE
cupPresent FALSE: Wait for one second then close the door
Door close switch TRUS: Idle. all motors and lights off.
The loading switch TRUE: The turntable rotates five index positions with a 1.5 second delay between movements, then return to idle.

*/





#define USE_TOF 1  // Set to 1 when you're ready to enable the VL53L0X
#define USE_LED 1  // Set to 1 to use Adafruit Neopixel

#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#if USE_LED
#include <Adafruit_NeoPixel.h>
#endif
#include "Adafruit_VL53L0X.h"
#if USE_TOF
Adafruit_VL53L0X lox = Adafruit_VL53L0X();
#endif

Adafruit_MCP23X17 mcp;

// =========================
// Tuneable Constants
// =========================
//const uint32_t PAUSE_TIME = 1000;  // (legacy, no longer used in TT_LOADING)
const uint32_t CUP_LOAD_DELAY = 2000;  // Delay after cup detected before rotating to next index
const uint32_t CLEARING_TIME = 500;
const uint32_t CUP_DISTANCE = 200;  // Distance in mm that is an object in the chamber. Anything more is false.
const uint32_t TT_BRAKE_TIME = 50;  // 50 ms reverse
const uint32_t OPEN_TIME = 10000;   // safety only
const uint32_t CLOSE_TIME = 10000;  // safety only


// =========================
// NeoPixel Strip
// =========================
#if USE_LED
#define STRIP_PIN D7
#define STRIP_LED_COUNT 30
Adafruit_NeoPixel strip(STRIP_LED_COUNT, STRIP_PIN, NEO_GRB + NEO_KHZ800);
#endif


// =========================
// Indicator LEDs
// =========================
const uint8_t HEARTBEAT_LED = 10;  // Blue LED on MCP Pin 3 (GPB2)
const uint8_t CUP_LED = 11;        // Red LED on MCP Pin 4 (GPB3), ON when cup is present
const uint8_t OPEN_SW_LED = 12;    // Yellow LED on MCP Pin 5 (GPB4), ON when door is open
const uint8_t CLOSE_SW_LED = 13;   // Green LED on MCP Pin 6 (GPB5), ON when door is closed


// =========================
// Motor control pins
// =========================
const int IA_A = D1;  // Door
const uint8_t IB_A = 8;
const int IA_B = D2;  // Turntable
const uint8_t IB_B = 9;


// =========================
// MCP23017 Inputs (GPA)
// =========================
const uint8_t TRIGGER_PIN = 0;
const uint8_t LOAD_PIN = 1;
const uint8_t INDEX_PIN = 2;

const uint8_t UP_SWITCH = 4;
const uint8_t DN_SWITCH = 6;


// =========================
// Debounce System
// =========================
const uint16_t DEBOUNCE_MS = 30;

struct Debounce {
  uint8_t pin;
  bool stableState;
  bool lastRaw;
  uint32_t lastChange;
};

Debounce dbTrigger = { TRIGGER_PIN, HIGH, HIGH, 0 };
Debounce dbLoad = { LOAD_PIN, HIGH, HIGH, 0 };
Debounce dbIndex = { INDEX_PIN, HIGH, HIGH, 0 };

bool debounceRead(Debounce &b) {
  bool raw = mcp.digitalRead(b.pin);
  uint32_t now = millis();

  if (raw != b.lastRaw) {
    b.lastRaw = raw;
    b.lastChange = now;
  }

  if ((now - b.lastChange) > DEBOUNCE_MS) {
    b.stableState = raw;
  }

  return b.stableState;
}


// =========================
// Turntable State Machine
// =========================
enum TTState {
  TT_IDLE,
  TT_CLEARING,
  TT_SEEKING,
  TT_LOADING
};
TTState ttState = TT_IDLE;


// =========================
// Turntable constants
// =========================
bool ttBrakeActive = false;
uint32_t ttBrakeStart = 0;

unsigned long stateStartTime = 0;
int loadCount = 0;
const int LOAD_TARGET = 5;

bool lastTrig = HIGH;
bool lastLoad = HIGH;
bool firstTurntableIdle = true;
bool ttLoadingWaitForDoor = false;  // TT_LOADING: block rotation until door fully open
bool ttLoadingForceClose = false;   // TT_LOADING: bypass cup-safety reversal for the post-load close


// =========================
// Door Motor State Machine
// =========================
enum DoorMotorState { MOTOR_OPEN,
                      MOTOR_CLOSE,
                      MOTOR_STOP };
DoorMotorState doorMotor = MOTOR_STOP;
DoorMotorState lastDoorMotor = MOTOR_STOP;

uint32_t openStartTime = 0;
uint32_t closeStartTime = 0;

bool cupPresent = false;
bool closeRequested = false;


// after-index → open door
bool waitingAfterIndex = false;
uint32_t afterIndexTime = 0;

// cup removed → delayed close
bool waitingCupDelay = false;
uint32_t cupGoneTime = 0;


// =========================
// NeoPixel helpers
// =========================
#if USE_LED
uint32_t rainbowHue = 0;
unsigned long lastRainbowUpdate = 0;
const int rainbowWait = 10;

void rainbowStep() {
  if (millis() - lastRainbowUpdate < rainbowWait) return;
  lastRainbowUpdate = millis();

  for (int i = 0; i < strip.numPixels(); i++) {
    int pixelHue = rainbowHue + (i * 65536L / strip.numPixels());
    strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
  }
  strip.show();
  rainbowHue += 256;
}

void colorFill(uint32_t color) {
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();
}

void flashRedStep() {
  static bool isRed = false;
  static uint32_t lastToggle = 0;
  const uint32_t flashInterval = 250;  //ms

  uint32_t now = millis();
  if (now - lastToggle < flashInterval) return;
  lastToggle = now;

  if (isRed) {
    colorFill(strip.Color(0, 0, 0));  // Black
  } else {
    colorFill(strip.Color(255, 0, 0));  // Red
  }
  isRed = !isRed;
}
#endif


// =========================
// Door Functions
// =========================
void doorOpen() {
  digitalWrite(IA_A, HIGH);
  mcp.digitalWrite(IB_A, LOW);
}
void doorClose() {
  digitalWrite(IA_A, LOW);
  mcp.digitalWrite(IB_A, HIGH);
}
void doorStop() {
  digitalWrite(IA_A, LOW);
  mcp.digitalWrite(IB_A, LOW);
}


// =========================
// Turntable Functions
// =========================
void ttForward() {
  digitalWrite(IA_B, HIGH);
  mcp.digitalWrite(IB_B, LOW);
}
void ttReverse() {
  digitalWrite(IA_B, LOW);
  mcp.digitalWrite(IB_B, HIGH);
}
void ttStopMotor() {
  digitalWrite(IA_B, LOW);
  mcp.digitalWrite(IB_B, LOW);
}
void ttStop() {
  // Enable reverse brake. Loop will call ttStopMotor.
  if (!ttBrakeActive) {  // only start brake if not already braking
    ttBrakeActive = true;
    ttBrakeStart = millis();
  }
}


// =========================
// Cup detector
// =========================
#if USE_TOF
bool cupDetector() {
  VL53L0X_RangingMeasurementData_t measure;
  lox.rangingTest(&measure, false);
  return (measure.RangeMilliMeter < CUP_DISTANCE);
}
#else
bool cupDetector() {
  return false;
}
#endif


// =========================
// Door motor update
// =========================
void updateDoorMotor() {

  if (doorMotor != lastDoorMotor) {
    switch (doorMotor) {
      case MOTOR_OPEN: Serial.println("Door: OPEN"); break;
      case MOTOR_CLOSE: Serial.println("Door: CLOSE"); break;
      case MOTOR_STOP: Serial.println("Door: STOPPED"); break;
    }
    lastDoorMotor = doorMotor;
  }

  switch (doorMotor) {
    case MOTOR_OPEN:
      doorOpen();

      // UP limit switch
      if (mcp.digitalRead(UP_SWITCH) == LOW) {
        doorStop();
        Serial.println("OPEN: UP limit switch hit");
        doorMotor = MOTOR_STOP;
        break;
      }

      // Safety timeout
      if (millis() - openStartTime >= OPEN_TIME) {
        doorStop();
        Serial.println("Open Timeout elapsed");
        doorMotor = MOTOR_STOP;
      }
      break;


    case MOTOR_CLOSE:
      doorClose();

      // SAFETY: reverse if cup appears mid‑close
      // (skipped when force-closing after a loading cycle, since the
      //  "cup" present is just the dispensed cup, not a door obstruction)
      if (cupPresent && !ttLoadingForceClose) {
        if (!closeRequested) {
          Serial.println("CLOSE interrupted: cup detected → reversing");
        }
        closeRequested = true;     // remember that we still want to close
        doorMotor = MOTOR_OPEN;    // reverse immediately
        openStartTime = millis();  // start open timer
        break;
      }

      // DOWN limit switch
      if (mcp.digitalRead(DN_SWITCH) == LOW) {
        doorStop();
        Serial.println("CLOSE: DOWN limit switch hit");
        doorMotor = MOTOR_STOP;
        ttLoadingForceClose = false;
        break;
      }

      // Safety timeout
      if (millis() - closeStartTime >= CLOSE_TIME) {
        doorStop();
        Serial.println("Close Timeout elapsed");
        doorMotor = MOTOR_STOP;
        ttLoadingForceClose = false;
      }
      break;


    case MOTOR_STOP:
    default:
      doorStop();
      break;
  }
}


// =========================
// Turntable index switch
// =========================
bool indexSwitch() {
  return mcp.digitalRead(INDEX_PIN) == LOW;
}



// ----------------------------------- updateTurntable -----------------------------------------
void updateTurntable() {

  // ------------------------------------------------------------
  // Handle non-blocking reverse brake
  // ------------------------------------------------------------
  if (ttBrakeActive) {
    if (millis() - ttBrakeStart < TT_BRAKE_TIME) {
      ttReverse();
      return;
    } else {
      ttBrakeActive = false;
      ttStopMotor();
      return;
    }
  }



  // ------------------------------------------------------------
  // Turntable State Machine
  // ------------------------------------------------------------
  switch (ttState) {

    // ============================================================
    // TT_IDLE — wait for trigger OR load button
    // ============================================================
    case TT_IDLE:
      {
        ttStopMotor();

        bool trig = (debounceRead(dbTrigger) == LOW);
        bool load = (debounceRead(dbLoad) == LOW);

        // Trigger event → normal cycle
        if (trig && !lastTrig) {
          ttForward();
          ttState = TT_CLEARING;
          Serial.println("Normal cycle- ttState=TT_CLEARING");
        }

        // Load event → loading cycle
        if (load && !lastLoad) {
          loadCount = 0;
          ttLoadingWaitForDoor = true;  // don't spin until door is fully open
          ttState = TT_LOADING;
          // open door for entire loading cycle
          doorMotor = MOTOR_OPEN;
          openStartTime = millis();
        }

        lastTrig = trig;
        lastLoad = load;
        break;
      }

    // ============================================================
    // TT_CLEARING — rotate until indexSwitch == FALSE
    // ============================================================
    case TT_CLEARING:
      {
        ttForward();

        if (!indexSwitch()) {  // index FALSE → cleared
          ttState = TT_SEEKING;
          Serial.println("Clearing finished -> ttState=TT_SEEKING");
        }
        break;
      }

    // ============================================================
    // TT_SEEKING — rotate until indexSwitch == TRUE
    // ============================================================
    case TT_SEEKING:
      {
        ttForward();

        if (indexSwitch()) {         // index TRUE → found
          ttStop();                  // includes reverse brake
          waitingAfterIndex = true;  // door opens after 1 second
          afterIndexTime = millis();
          ttState = TT_IDLE;
          Serial.println("Seeking finished -> ttState=TT_IDLE");
        }
        break;
      }

      // ============================================================
      // TT_LOADING — rotate through 5 index hits with 1.5s pauses
      /*
      Behavior:
        Rotate until index TRUE
          Stop
          Wait 1.5 seconds
          Resume rotation
        Repeat 5 times
        Return to IDLE
      */
    // ============================================================
    case TT_LOADING:
      {
        static bool lastIdx = false;
        static bool waitingForCup = false;
        static bool waitingAfterCup = false;
        static uint32_t afterCupStart = 0;

        // Don't start rotating until the door has finished opening
        if (ttLoadingWaitForDoor) {
          bool doorFullyOpenNow = (mcp.digitalRead(UP_SWITCH) == LOW && doorMotor == MOTOR_STOP);
          if (doorFullyOpenNow) {
            ttLoadingWaitForDoor = false;
            lastIdx = indexSwitch();  // avoid a false "index hit" on the first read
            ttForward();              // door is open, now safe to start spinning
          }
          break;
        }

        bool idx = indexSwitch();

        // Stage 1: stopped at index, waiting for a cup to be placed
        if (waitingForCup) {
          if (cupPresent) {
            waitingForCup = false;
            waitingAfterCup = true;
            afterCupStart = millis();
            Serial.println("Cup detected -> waiting 1s before next rotation");
          }
          break;
        }

        // Stage 2: cup detected, wait 1 second before moving on
        if (waitingAfterCup) {
          if (millis() - afterCupStart >= CUP_LOAD_DELAY) {
            waitingAfterCup = false;

            if (loadCount >= LOAD_TARGET) {
              ttStopMotor();
              ttState = TT_IDLE;
              Serial.println("Loading complete -> closing door");
              ttLoadingForceClose = true;  // ignore cup-present safety reversal for this close
              doorMotor = MOTOR_CLOSE;
              closeStartTime = millis();
            } else {
              ttForward();  // continue to next index
            }
          }
          break;
        }

        // Index hit event
        if (idx && !lastIdx) {
          ttStop();  // includes brake
          loadCount++;
          waitingForCup = true;
          Serial.println("Index hit -> waiting for cup");

          if (loadCount >= LOAD_TARGET) {
            // Will return to IDLE after cup + 1s delay finishes
          }
        }

        lastIdx = idx;
        break;
      }
  }
}  // end updateTurntable()



// -----------------------------------------------------------------
// =========================
// LED update
// =========================
#if USE_LED
void updateLEDs() {

  // Door fully open → solid green
  if (mcp.digitalRead(UP_SWITCH) == LOW && doorMotor == MOTOR_STOP) {
    colorFill(strip.Color(0, 255, 0));
    return;
  }

  // Door opening → Solid blue
  if (doorMotor == MOTOR_OPEN) {
    colorFill(strip.Color(0, 0, 255));
    return;
  }

  // Door closing → flash red
  if (doorMotor == MOTOR_CLOSE) {
    flashRedStep();
    return;
  }

  // Turntable brake → solid red
  if (ttBrakeActive) {
    colorFill(strip.Color(255, 0, 0));
    return;
  }

  // Otherwise: turntable state
  switch (ttState) {
    case TT_IDLE: colorFill(strip.Color(0, 0, 0)); break;
    case TT_LOADING: colorFill(strip.Color(255, 0, 0)); break;
    case TT_CLEARING:
    case TT_SEEKING: rainbowStep(); break;
  }
}
#endif

// =========================
// Setup
// =========================
void setup() {
  delay(500);  // give USB/IDE time to attach
  Serial.begin(115200);
  delay(200);
  Serial.println("\nStarting Galactic Goodie Generator V17");

  // Configure the ESP pins
  pinMode(IA_A, OUTPUT);
  pinMode(IA_B, OUTPUT);

  // Make sure motor doesn't start before the MCP is initialized/
  // (IB_A and IB_B have a 10K pulldown).
  digitalWrite(IA_A, LOW);
  digitalWrite(IA_B, LOW);

  // --- Start I2C ---
  Wire.begin(D6, D5);  // SDA, SCL
  delay(50);           // Give the MCP a few more ms to start up.

#if USE_TOF
  if (!lox.begin()) {
    Serial.println(F("Failed to boot VL53L0X"));
  }
#endif

  bool mcp_ok = false;
  // Retry MCP detection
  for (int i = 0; i < 5; i++) {
    if (mcp.begin_I2C()) {
      mcp_ok = true;
      break;
    }
    Serial.println("MCP NOT FOUND, retrying...");
    delay(200);
  }
  // --- If MCP still missing, reboot ---
  if (!mcp_ok) {
    Serial.println("MCP STILL NOT FOUND — REBOOTING");
    delay(500);
    ESP.restart();
  }
  Serial.println("MCP Found, continuing.");

  // --- Now that MCP is alive, configure its pins ---
  for (uint8_t pin = 0; pin < 8; pin++) {
    mcp.pinMode(pin, INPUT);
    mcp.pinMode(pin, INPUT_PULLUP);
  }
  for (uint8_t pin = 8; pin < 16; pin++) {
    mcp.pinMode(pin, OUTPUT);
  }

  // Stop the motors
  doorStop();
  ttStopMotor();

  // Initial states
  doorMotor = MOTOR_STOP;
  ttState = TT_IDLE;

#if USE_LED
  strip.begin();
  strip.show();
  strip.setBrightness(50);
  analogWriteFreq(1000);
#endif

  firstTurntableIdle = true;
  lastTrig = HIGH;
  lastLoad = HIGH;

  dbTrigger.stableState = HIGH;
  dbTrigger.lastRaw = HIGH;
  dbTrigger.lastChange = millis();

  dbLoad.stableState = HIGH;
  dbLoad.lastRaw = HIGH;
  dbLoad.lastChange = millis();


  // Flash the LEDS to indicate that the MCP is initialized
  for (int i = 0; i < 3; i++) {
    mcp.digitalWrite(HEARTBEAT_LED, HIGH);
    delay(150);
    mcp.digitalWrite(CUP_LED, HIGH);
    delay(150);
    mcp.digitalWrite(OPEN_SW_LED, HIGH);
    delay(150);
    mcp.digitalWrite(CLOSE_SW_LED, HIGH);
    delay(250);

    mcp.digitalWrite(HEARTBEAT_LED, LOW);
    delay(150);
    mcp.digitalWrite(CUP_LED, LOW);
    delay(150);
    mcp.digitalWrite(OPEN_SW_LED, LOW);
    delay(150);
    mcp.digitalWrite(CLOSE_SW_LED, LOW);
    delay(250);
  }

  Serial.println("Setup complete");
}



// ====================================================================================================
// Loop
// ====================================================================================================
void loop() {
  uint32_t now = millis();

  // Heartbeat ----------------------------
  static uint32_t lastBeat = 0;
  static bool hbOn = false;
  static uint32_t hbStart = 0;

  if (!hbOn && now - lastBeat >= 1000) {
    mcp.digitalWrite(HEARTBEAT_LED, HIGH);
    hbStart = now;
    hbOn = true;
    lastBeat = now;
  }
  if (hbOn && now - hbStart >= 50) {
    mcp.digitalWrite(HEARTBEAT_LED, LOW);
    hbOn = false;
  }
  //-------------------------------------


  // Door Limit switch LEDs
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
  // ---------------------------------------


#if USE_TOF
  /*the behavior is:
    OPEN pressed → door opens, any pending close is canceled.
    CLOSE pressed with cup present → closeRequested = true, door waits.
    Cup removed → auto‑close runs once, then closeRequested goes false.
*/
  static uint32_t lastTOF = 0;
  if (millis() - lastTOF > 50) {
    lastTOF = millis();
    cupPresent = cupDetector();
  }
  if (cupPresent) {
    mcp.digitalWrite(CUP_LED, HIGH);
  } else {
    mcp.digitalWrite(CUP_LED, LOW);
  }
#endif


  // ------------------------------------------------------------
  // After turntable reaches index, wait 1 second then open door
  if (waitingAfterIndex) {
    if (millis() - afterIndexTime >= 1000) {
      waitingAfterIndex = false;
      Serial.println("Opening door after index delay");
      doorMotor = MOTOR_OPEN;
      openStartTime = millis();
    }
  }

  // Auto-close: door fully open, cup removed → wait 1s → close
  // (suppressed during TT_LOADING — door must stay open for the whole loading cycle)
  bool doorFullyOpen = (mcp.digitalRead(UP_SWITCH) == LOW && doorMotor == MOTOR_STOP);

  if (ttState == TT_LOADING) {
    waitingCupDelay = false;  // cancel any pending auto-close
  } else {
    if (doorFullyOpen && cupPresent) {
      waitingCupDelay = false;
    }

    if (doorFullyOpen && !cupPresent && !waitingCupDelay) {
      waitingCupDelay = true;
      cupGoneTime = millis();
      Serial.println("Cup removed → starting 1-second close delay");
    }

    if (waitingCupDelay && cupPresent) {
      waitingCupDelay = false;
      Serial.println("Cup returned → canceling auto-close");
    }

    if (waitingCupDelay && !cupPresent && millis() - cupGoneTime >= 1000) {
      waitingCupDelay = false;
      Serial.println("Cup gone for 1 second → closing door");
      doorMotor = MOTOR_CLOSE;
      closeStartTime = millis();
    }
  }

  updateDoorMotor();
  updateTurntable();

#if USE_LED
  updateLEDs();
#endif

  delay(1);
}
