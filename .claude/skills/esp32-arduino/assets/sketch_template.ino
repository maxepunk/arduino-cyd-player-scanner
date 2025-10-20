/*
 * ESP32 Project Template
 * 
 * Description: [Brief description of what this sketch does]
 * Board: ESP32 Dev Module (or specify your board)
 * Author: [Your name]
 * Date: [Date]
 */

#include <Arduino.h>

// Optional: Include config file
// #include "config.h"

// Pin definitions
#define LED_PIN    2     // Onboard LED (GPIO 2)
#define BUTTON_PIN 0     // Boot button (GPIO 0)

// Constants
const unsigned long BAUD_RATE = 115200;
const unsigned long LOOP_DELAY = 1000;  // milliseconds

// Global variables
unsigned long lastUpdateTime = 0;
bool ledState = false;

// Function declarations
void setupPins();
void setupSerial();
void handleButton();
void updateLED();

void setup() {
  // Initialize serial communication
  setupSerial();
  
  // Initialize pins
  setupPins();
  
  // Print startup message
  Serial.println("\n=================================");
  Serial.println("ESP32 Project Starting...");
  Serial.println("=================================");
  
  // Add your initialization code here
  
  Serial.println("Setup complete!");
}

void loop() {
  // Non-blocking timing pattern
  unsigned long currentTime = millis();
  
  if (currentTime - lastUpdateTime >= LOOP_DELAY) {
    lastUpdateTime = currentTime;
    
    // Your main loop code here
    updateLED();
  }
  
  // Check button
  handleButton();
  
  // Small delay to prevent watchdog issues
  delay(10);
}

// Function implementations

void setupSerial() {
  Serial.begin(BAUD_RATE);
  delay(1000);  // Wait for serial to initialize
  
  // Wait for serial port to connect (optional)
  while (!Serial && millis() < 3000) {
    delay(10);
  }
}

void setupPins() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);  // Boot button has external pull-up
  
  // Set initial LED state
  digitalWrite(LED_PIN, LOW);
}

void handleButton() {
  static bool lastButtonState = HIGH;
  static unsigned long lastDebounceTime = 0;
  static const unsigned long DEBOUNCE_DELAY = 50;
  
  bool buttonState = digitalRead(BUTTON_PIN);
  
  // Debounce button
  if (buttonState != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    // Button was pressed (LOW = pressed, button is active LOW)
    if (buttonState == LOW) {
      Serial.println("Button pressed!");
      
      // Toggle LED immediately
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
    }
  }
  
  lastButtonState = buttonState;
}

void updateLED() {
  // Toggle LED state
  ledState = !ledState;
  digitalWrite(LED_PIN, ledState);
  
  Serial.print("LED state: ");
  Serial.println(ledState ? "ON" : "OFF");
  
  // Print free heap
  Serial.print("Free heap: ");
  Serial.print(ESP.getFreeHeap());
  Serial.println(" bytes");
}

// Add your custom functions below
