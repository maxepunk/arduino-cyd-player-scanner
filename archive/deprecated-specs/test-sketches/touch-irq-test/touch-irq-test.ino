/*
 * Touch IRQ Test for CYD
 * Tests IRQ-only touch detection (tap and double-tap)
 * No coordinate reading - XPT2046 SPI not connected on CYD boards
 */

#include <Arduino.h>

// Touch IRQ Pin (same for all CYD variants)
#define TOUCH_IRQ_PIN 36

// Touch detection parameters
volatile bool touchInterruptOccurred = false;
uint32_t lastTouchTime = 0;
bool lastTouchWasValid = false;
const uint32_t DOUBLE_TAP_TIMEOUT = 500;  // Max ms between taps for double-tap
const uint32_t TOUCH_DEBOUNCE = 200;      // Increased to ignore release interrupt
uint32_t lastTouchDebounce = 0;
bool waitingForRelease = false;

// Statistics
uint32_t tapCount = 0;
uint32_t doubleTapCount = 0;
uint32_t interruptCount = 0;

// Touch Interrupt Service Routine
void IRAM_ATTR touchISR() {
    touchInterruptOccurred = true;
    interruptCount++;
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("=================================");
    Serial.println("CYD Touch IRQ Test v1.0");
    Serial.println("=================================");
    Serial.println("Touch detection using IRQ-only mode");
    Serial.println("No coordinate reading available");
    Serial.println("(XPT2046 SPI not connected on CYD)");
    Serial.println();
    
    // Configure Touch IRQ pin
    pinMode(TOUCH_IRQ_PIN, INPUT_PULLUP);
    
    // Test initial pin state
    int initialState = digitalRead(TOUCH_IRQ_PIN);
    Serial.printf("Touch IRQ Pin: GPIO%d\n", TOUCH_IRQ_PIN);
    Serial.printf("Initial state: %s\n", initialState == HIGH ? "HIGH (not pressed)" : "LOW (pressed)");
    
    // Attach interrupt on falling edge (touch detected)
    attachInterrupt(digitalPinToInterrupt(TOUCH_IRQ_PIN), touchISR, FALLING);
    
    Serial.println("\nTouch interrupt configured");
    Serial.println("Tap the screen to test single tap");
    Serial.println("Double-tap quickly to test double-tap detection");
    Serial.println();
    Serial.println("Commands:");
    Serial.println("  STATS - Show touch statistics");
    Serial.println("  RESET - Reset statistics");
    Serial.println("  TEST - Run automated test sequence");
    Serial.println();
}

void loop() {
    // Handle serial commands
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        
        if (command == "STATS") {
            showStatistics();
        } else if (command == "RESET") {
            resetStatistics();
        } else if (command == "TEST") {
            runTestSequence();
        } else {
            Serial.println("Unknown command: " + command);
        }
    }
    
    // Handle touch interrupt
    if (touchInterruptOccurred) {
        touchInterruptOccurred = false;
        uint32_t now = millis();
        
        // Debounce check - ignores the release interrupt from same tap
        if (now - lastTouchDebounce < TOUCH_DEBOUNCE) {
            // This is likely the release interrupt from the same tap
            return;
        }
        
        lastTouchDebounce = now;
        
        // This is a new tap (not the release from previous tap)
        tapCount++;
        
        // Check for double-tap
        if (lastTouchWasValid && (now - lastTouchTime) < DOUBLE_TAP_TIMEOUT) {
            // This tap came soon after the last tap = double-tap
            doubleTapCount++;
            Serial.printf("[%lu ms] Double-tap detected! (total taps: %lu, double-taps: %lu)\n", 
                         now, tapCount, doubleTapCount);
            lastTouchWasValid = false;  // Reset double-tap detection
        } else {
            // Single tap (first tap or timeout expired)
            Serial.printf("[%lu ms] Tap detected (tap #%lu)\n", now, tapCount);
            lastTouchTime = now;
            lastTouchWasValid = true;  // Start double-tap window
        }
        
        // Show current pin state for debugging
        int currentState = digitalRead(TOUCH_IRQ_PIN);
        Serial.printf("  Pin state: %s (debounce: %lums)\n", 
                     currentState == HIGH ? "HIGH" : "LOW", TOUCH_DEBOUNCE);
    }
    
    // Clear double-tap window if timeout expired
    if (lastTouchWasValid && (millis() - lastTouchTime) >= DOUBLE_TAP_TIMEOUT) {
        lastTouchWasValid = false;
    }
}

void showStatistics() {
    Serial.println("\n=== Touch Statistics ===");
    Serial.printf("Total interrupts: %lu\n", interruptCount);
    Serial.printf("Valid taps: %lu\n", tapCount);
    Serial.printf("Double-taps: %lu\n", doubleTapCount);
    Serial.printf("Debounce time: %lu ms (filters release interrupt)\n", TOUCH_DEBOUNCE);
    Serial.printf("Double-tap window: %lu ms\n", DOUBLE_TAP_TIMEOUT);
    Serial.printf("Current pin state: %s\n", 
                 digitalRead(TOUCH_IRQ_PIN) == HIGH ? "HIGH (not pressed)" : "LOW (pressed)");
    Serial.println("Note: Each physical tap generates 2 interrupts (press + release)");
    Serial.println("========================\n");
}

void resetStatistics() {
    tapCount = 0;
    doubleTapCount = 0;
    interruptCount = 0;
    lastTouchWasValid = false;
    Serial.println("Statistics reset");
}

void runTestSequence() {
    Serial.println("\n=== Running Test Sequence ===");
    
    // Test 1: Check pin state
    Serial.println("Test 1: Pin State Check");
    int state = digitalRead(TOUCH_IRQ_PIN);
    Serial.printf("  Current state: %s\n", state == HIGH ? "HIGH (PASS)" : "LOW (FAIL - screen pressed?)");
    
    // Test 2: Interrupt configuration
    Serial.println("Test 2: Interrupt Configuration");
    Serial.println("  Interrupt attached to GPIO36");
    Serial.println("  Trigger: FALLING edge");
    Serial.println("  Status: CONFIGURED");
    
    // Test 3: Detection parameters
    Serial.println("Test 3: Detection Parameters");
    Serial.printf("  Debounce: %lu ms (filters duplicate interrupts)\n", TOUCH_DEBOUNCE);
    Serial.printf("  Double-tap window: %lu ms\n", DOUBLE_TAP_TIMEOUT);
    Serial.println("  Note: Each tap triggers 2 interrupts (press + release)");
    Serial.println("  Status: OK");
    
    // Test 4: Response test
    Serial.println("Test 4: Response Test");
    Serial.println("  Please tap the screen within 5 seconds...");
    uint32_t startTime = millis();
    uint32_t initialTaps = tapCount;
    
    while (millis() - startTime < 5000) {
        if (touchInterruptOccurred) {
            touchInterruptOccurred = false;
            uint32_t now = millis();
            
            if (now - lastTouchDebounce >= TOUCH_DEBOUNCE) {
                lastTouchDebounce = now;
                tapCount++;
                Serial.printf("  Tap detected at %lu ms\n", now - startTime);
                break;
            }
        }
    }
    
    if (tapCount > initialTaps) {
        Serial.println("  Status: PASS - Touch detected");
    } else {
        Serial.println("  Status: TIMEOUT - No touch detected");
    }
    
    Serial.println("\nTest sequence complete");
    Serial.println("===========================\n");
}