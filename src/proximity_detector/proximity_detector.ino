/****************************************************************
 Phidgets IR Reflective Sensor 10cm (1103_1)
 Test sketch for Wemos D1 Mini (ESP8266)
 Assumes 5V sensor output scaled to 0–1V using a 4k/1k divider

 With nothing in front of the sensor:
 readings will be low (typically 50–150)
 With an object close:
 readings will jump higher (200–300 depending on reflectivity)
***************************************************************/

const int SENSOR_PIN = A0;
const int LED_PIN = LED_BUILTIN;
const uint8_t MOTOR_PIN = D1;
int raw;
const uint16_t PWM_VALUE = (uint16_t)(1023 * 0.35);


// Returns true when the black tape is detected
bool proximity() {
  raw = analogRead(SENSOR_PIN);

  // Your sensor gives ~200 on black tape,  ~20 on the turntable bottom.
  bool tapeSeen = (raw > 100);

  digitalWrite(LED_PIN, tapeSeen ? LOW : HIGH);
  return tapeSeen;
}



void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(LED_PIN, OUTPUT);
  pinMode(MOTOR_PIN, OUTPUT);
  analogWriteRange(1023);

  Serial.println("Phidgets 1103_1 Reflective Sensor Test");
  Serial.println("Reading analog values...");

  //Test the motor
  analogWrite(MOTOR_PIN, PWM_VALUE);
}


void loop() {
  proximity();

  Serial.print("Analog: ");
  Serial.println(raw);
}
