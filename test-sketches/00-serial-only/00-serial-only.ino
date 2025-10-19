void setup() {
  Serial.begin(115200);
  pinMode(2, OUTPUT);  // Use LED pin as indicator
}

void loop() {
  Serial.println("Serial test - If you see this, serial works!");
  digitalWrite(2, HIGH);
  delay(500);
  digitalWrite(2, LOW);
  delay(500);
}