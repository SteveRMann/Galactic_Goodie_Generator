#define SKETCH "cupDetector.ino"
// V2 = Pololu library

#include <Wire.h>
#include <VL53L0X.h>
VL53L0X sensor;



void setup() {
  // wait until serial port opens for native USB devices
  Serial.begin(115200);
  while (!Serial) {
    delay(1);
  }

  Serial.println();
  Serial.println(SKETCH);
  Serial.println("VL53L0X test");

  // Start I2C on D2 (SDA) and D1 (SCL)
  Wire.begin(D2, D1);

  if (!sensor.init()) {
    Serial.println("Failed to detect and initialize sensor!");
    while (1);
  }

  sensor.setTimeout(500);
  sensor.startContinuous(100);  // 100 ms between readings
}


void loop() {
  uint16_t distance = sensor.readRangeContinuousMillimeters();

  if (sensor.timeoutOccurred()) {
    Serial.println("Timeout!");
  } else {
    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.println(" mm");
  }

  delay(100);
}
