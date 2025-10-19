// Backlight Pin Test for CYD
// Tests which GPIO actually controls the backlight

#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n========================================");
  Serial.println("CYD Backlight Pin Test");
  Serial.println("========================================");
  
  // Initialize display
  tft.begin();
  tft.fillScreen(TFT_WHITE); // White screen to see backlight clearly
  tft.setTextColor(TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("BACKLIGHT TEST");
  
  // Configure both pins as OUTPUT but start with both LOW (backlight OFF)
  pinMode(21, OUTPUT);
  pinMode(27, OUTPUT);
  digitalWrite(21, LOW);
  digitalWrite(27, LOW);
  
  Serial.println("\nStarting backlight pin test...");
  Serial.println("Both pins LOW - backlight should be OFF");
  delay(2000);
  
  // Test GPIO21
  Serial.println("\n[TEST 1] GPIO21 HIGH, GPIO27 LOW");
  digitalWrite(21, HIGH);
  digitalWrite(27, LOW);
  
  tft.setCursor(10, 50);
  tft.println("GPIO21 HIGH");
  
  Serial.println("If backlight is ON now, GPIO21 controls it (single USB variant)");
  delay(3000);
  
  // Turn off GPIO21
  digitalWrite(21, LOW);
  Serial.println("\n[TEST 2] Both pins LOW again");
  Serial.println("Backlight should be OFF");
  delay(2000);
  
  // Test GPIO27
  Serial.println("\n[TEST 3] GPIO21 LOW, GPIO27 HIGH");
  digitalWrite(21, LOW);
  digitalWrite(27, HIGH);
  
  tft.fillScreen(TFT_WHITE);
  tft.setCursor(10, 50);
  tft.println("GPIO27 HIGH");
  
  Serial.println("If backlight is ON now, GPIO27 controls it (dual USB variant)");
  delay(3000);
  
  // Test both HIGH
  Serial.println("\n[TEST 4] Both GPIO21 and GPIO27 HIGH");
  digitalWrite(21, HIGH);
  digitalWrite(27, HIGH);
  
  tft.fillScreen(TFT_WHITE);
  tft.setCursor(10, 50);
  tft.println("BOTH HIGH");
  
  Serial.println("Backlight should definitely be ON");
  delay(3000);
  
  // PWM test on GPIO27
  Serial.println("\n[TEST 5] PWM test on GPIO27");
  digitalWrite(21, LOW);
  
  for(int i = 0; i <= 255; i += 51) {
    analogWrite(27, i);
    Serial.printf("GPIO27 PWM: %d/255\n", i);
    
    tft.fillScreen(TFT_WHITE);
    tft.setCursor(10, 50);
    tft.printf("PWM: %d", i);
    
    delay(1000);
  }
  
  // Final state - both HIGH
  digitalWrite(21, HIGH);
  digitalWrite(27, HIGH);
  
  Serial.println("\n========================================");
  Serial.println("Test Complete - Check which pin controls backlight");
  Serial.println("========================================");
  
  tft.fillScreen(TFT_GREEN);
  tft.setCursor(10, 100);
  tft.setTextColor(TFT_BLACK);
  tft.println("TEST DONE");
}

void loop() {
  // Blink to show sketch is running
  static uint32_t lastBlink = 0;
  static bool state = false;
  
  if(millis() - lastBlink > 500) {
    lastBlink = millis();
    state = !state;
    
    if(state) {
      tft.fillRect(220, 10, 10, 10, TFT_RED);
    } else {
      tft.fillRect(220, 10, 10, 10, TFT_GREEN);
    }
  }
}