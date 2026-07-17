// L9110_test.ino

// =========================
// Motor control pins
// =========================
// Motor control pins
const uint8_t IA_B = D2;  // Motor B (Turntable) - already wired
const uint8_t IB_B = D7;  // NEW: use D7 for IB_B

const uint8_t IA_A = D1;  // Motor A (Door) - already wired
const uint8_t IB_A = D0;  // NEW: use D0 for IB_A


// Motor A
void motorA_fwd() {
  digitalWrite(IA_A, HIGH);
  digitalWrite(IB_A, LOW);
}
void motorA_rev() {
  digitalWrite(IA_A, LOW);
  digitalWrite(IB_A, HIGH);
}
void motorA_stop() {
  digitalWrite(IA_A, LOW);
  digitalWrite(IB_A, LOW);
}
// Motor B
void motorB_fwd() {
  digitalWrite(IA_B, HIGH);
  digitalWrite(IB_B, LOW);
}
void motorB_rev() {
  digitalWrite(IA_B, LOW);
  digitalWrite(IB_B, HIGH);
}
void motorB_stop() {
  digitalWrite(IA_B, LOW);
  digitalWrite(IB_B, LOW);
}



void setup() {
  // Configure the ESP pins
  pinMode(IA_A, OUTPUT);
  pinMode(IB_A, OUTPUT);
  pinMode(IA_B, OUTPUT);
  pinMode(IB_B, OUTPUT);

  // Make sure motor doesn't start before we are ready.
  digitalWrite(IA_A, LOW);
  digitalWrite(IB_A, LOW);
  digitalWrite(IA_B, LOW);
  digitalWrite(IB_B, LOW);
}



void loop() {
  while (true) {
    // Motor B
    motorB_fwd();
    delay(500);
    motorB_stop();
    delay(500);
    motorB_rev();
    delay(500);
    motorB_stop();
    delay(500);

    // Motor A
    motorA_fwd();
    delay(500);
    motorA_stop();
    delay(500);
    motorA_rev();
    delay(500);
    motorA_stop();
    delay(1000);
  }
}
