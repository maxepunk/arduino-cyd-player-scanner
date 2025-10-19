// Test 03: Touch IRQ Detection - Test interrupt-based touch detection
// Uses GPIO36 for touch interrupt (no coordinate reading)

#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

// Touch IRQ pin (interrupt only, no SPI communication)
#define TOUCH_IRQ 36

// Touch state tracking
volatile bool touchDetected = false;
unsigned long lastTouchTime = 0;
int touchCount = 0;
const unsigned long debounceDelay = 200; // 200ms debounce

// Interrupt handler
void IRAM_ATTR handleTouch() {
  touchDetected = true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== CYD Touch IRQ Test ===");
  Serial.println("Testing touch interrupt detection on GPIO36...");
  
  // Initialize display
  tft.init();
  tft.setRotation(0);  // Portrait
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);
  
  // Clear screen
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(50, 50);
  tft.println("Touch Screen Test");
  
  tft.setTextSize(1);
  tft.setCursor(50, 100);
  tft.println("Tap the screen to test touch");
  tft.setCursor(50, 120);
  tft.println("Touch count: 0");
  
  // Configure touch IRQ pin
  pinMode(TOUCH_IRQ, INPUT);
  
  // Attach interrupt (FALLING edge when touched)
  attachInterrupt(digitalPinToInterrupt(TOUCH_IRQ), handleTouch, FALLING);
  
  Serial.println("Touch IRQ configured on GPIO36");
  Serial.println("Tap the screen to generate interrupts");
  Serial.println("Note: Each tap may generate 2 interrupts ~80-130ms apart");
  Serial.println();
}

void loop() {
  // Check if touch was detected
  if (touchDetected) {
    unsigned long now = millis();
    
    // Apply debounce
    if (now - lastTouchTime > debounceDelay) {
      touchCount++;
      lastTouchTime = now;
      
      Serial.print("Touch detected! Count: ");
      Serial.print(touchCount);
      Serial.print(" at ");
      Serial.print(now);
      Serial.println(" ms");
      
      // Update display
      tft.fillRect(50, 120, 200, 20, TFT_BLACK);
      tft.setCursor(50, 120);
      tft.print("Touch count: ");
      tft.print(touchCount);
      
      // Flash the screen
      tft.fillRect(0, 180, 320, 60, TFT_GREEN);
      tft.setCursor(100, 200);
      tft.setTextSize(2);
      tft.setTextColor(TFT_BLACK);
      tft.print("TOUCHED!");
      
      delay(100);
      
      // Clear flash
      tft.fillRect(0, 180, 320, 60, TFT_BLACK);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setTextSize(1);
    }
    
    touchDetected = false;
  }
  
  // Show IRQ pin state
  static unsigned long lastStatusUpdate = 0;
  if (millis() - lastStatusUpdate > 1000) {
    lastStatusUpdate = millis();
    
    int irqState = digitalRead(TOUCH_IRQ);
    
    tft.fillRect(50, 140, 200, 10, TFT_BLACK);
    tft.setCursor(50, 140);
    tft.print("IRQ Pin: ");
    tft.print(irqState ? "HIGH" : "LOW");
    
    Serial.print("IRQ pin state: ");
    Serial.println(irqState);
  }
}