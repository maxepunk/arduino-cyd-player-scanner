// Test 02: Does TFT_eSPI global object break serial?
// Purpose: Test if global TFT_eSPI constructor interferes

#include <TFT_eSPI.h>

// GLOBAL OBJECT - constructor runs before main()
TFT_eSPI tft = TFT_eSPI();

void setup() {
    // Try serial FIRST, before any TFT operations
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("TEST02: Serial after TFT_eSPI global");
    Serial.println("If you see this, global TFT object doesn't break serial");
    
    // Now try to init display
    Serial.println("About to call tft.init()...");
    tft.init();
    Serial.println("tft.init() completed!");
    
    // Visual feedback
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(0, 0);
    tft.println("TEST02 Running");
    
    Serial.println("Display initialized");
}

void loop() {
    static int counter = 0;
    Serial.print("Loop with TFT: ");
    Serial.println(counter++);
    delay(1000);
}