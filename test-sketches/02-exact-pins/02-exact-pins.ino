#include <TFT_eSPI.h>

// Exact same pin definitions as ALNScanner
#define SOFT_SPI_SCK  22
#define SOFT_SPI_MOSI 27
#define SOFT_SPI_MISO 35
#define RFID_SS       3
#define TOUCH_IRQ  36

TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== Pin Test - Before TFT ===");
  
  // Initialize TFT exactly like ALNScanner
  tft.init();
  Serial.println("TFT initialized");
  
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(0, 0);
  tft.println("Serial Test");
  Serial.println("Display configured");
  
  // Configure exact same pins as ALNScanner
  pinMode(SOFT_SPI_SCK, OUTPUT);
  pinMode(SOFT_SPI_MOSI, OUTPUT);
  pinMode(SOFT_SPI_MISO, INPUT);
  pinMode(RFID_SS, OUTPUT);
  digitalWrite(RFID_SS, HIGH);
  digitalWrite(SOFT_SPI_SCK, LOW);
  Serial.println("RFID pins configured");
  
  pinMode(TOUCH_IRQ, INPUT_PULLUP);
  Serial.println("Touch pin configured");
  
  Serial.println("=== All pins configured ===");
}

void loop() {
  Serial.println("Loop running with all pins configured!");
  tft.setCursor(0, 40);
  tft.printf("Loop: %lu", millis()/1000);
  delay(1000);
}