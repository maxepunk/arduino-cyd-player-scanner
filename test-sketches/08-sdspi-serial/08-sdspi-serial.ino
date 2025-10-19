// Senior Engineer's test - SDSPI.begin() might be the culprit
#include <SPI.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();
SPIClass SDSPI(VSPI);

static const uint8_t SDSPI_SCK   = 18;
static const uint8_t SDSPI_MISO  = 19;
static const uint8_t SDSPI_MOSI  = 23;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== Before TFT init ===");
  
  tft.init();
  Serial.println("After TFT init");
  
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(0, 0);
  tft.println("Testing SDSPI");
  
  Serial.println("Before SDSPI.begin()...");
  Serial.flush(); // Force output before potential issue
  
  // This is line 927 in ALNScanner - might kill serial!
  SDSPI.begin(SDSPI_SCK, SDSPI_MISO, SDSPI_MOSI);
  
  Serial.println("After SDSPI.begin() - if you see this, SDSPI is OK");
  tft.println("SDSPI OK!");
}

void loop() {
  Serial.println("Still alive!");
  delay(1000);
}