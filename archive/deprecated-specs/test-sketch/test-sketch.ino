// Simple test sketch for WSL2 Arduino CLI wrapper
// Tests basic compilation without complex dependencies

#define LED_PIN 2  // ESP32 built-in LED on GPIO2

void setup() {
  Serial.begin(115200);
  Serial.println("WSL2 Arduino CLI Test");
  Serial.println("Compilation successful!");
  
  // Basic ESP32 LED test
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_PIN, HIGH);
  delay(1000);
  digitalWrite(LED_PIN, LOW);
  delay(1000);
  
  Serial.println("Heartbeat...");
}