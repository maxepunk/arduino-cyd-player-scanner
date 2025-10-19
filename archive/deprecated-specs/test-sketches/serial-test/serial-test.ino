// Minimal Serial Test
void setup() {
  Serial.begin(115200);
  delay(2000); // Give time for serial monitor to connect
  Serial.println("SERIAL TEST START");
  Serial.println("If you see this, serial is working!");
  Serial.println("Entering loop...");
}

void loop() {
  static int counter = 0;
  Serial.print("Loop iteration: ");
  Serial.println(counter++);
  delay(1000); // Print every second
}