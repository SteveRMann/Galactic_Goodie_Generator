

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("\nTEST.INO");
}


void loop() {
    static int i;
    Serial.println(i++);
  delay(250);
}
