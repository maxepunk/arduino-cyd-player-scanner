// Audio Test Sketch for CYD
// Simple I2S test with tone generation

#include <TFT_eSPI.h>
#include <driver/i2s.h>

// Backlight pins
#define BACKLIGHT_PIN_SINGLE 21
#define BACKLIGHT_PIN_DUAL   27

// I2S pins for CYD
#define I2S_BCK  26
#define I2S_WS   25
#define I2S_DOUT 17

TFT_eSPI tft = TFT_eSPI();

// I2S configuration
#define SAMPLE_RATE 8000
#define I2S_NUM I2S_NUM_0

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\nCYD Audio Test v1.0");
  Serial.println("===================");
  
  // Initialize display
  tft.begin();
  
  // Enable backlight (try both pins)
  pinMode(BACKLIGHT_PIN_SINGLE, OUTPUT);
  pinMode(BACKLIGHT_PIN_DUAL, OUTPUT);
  digitalWrite(BACKLIGHT_PIN_SINGLE, HIGH);
  digitalWrite(BACKLIGHT_PIN_DUAL, HIGH);
  
  // Display header
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Audio Test");
  
  // Initialize I2S
  tft.setTextSize(1);
  tft.setCursor(10, 40);
  Serial.println("Initializing I2S audio...");
  
  if (initI2S()) {
    Serial.println("I2S initialized successfully");
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("Audio: Initialized");
  } else {
    Serial.println("I2S initialization failed");
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.println("Audio: Failed");
    return;
  }
  
  Serial.print("I2S Pins: BCK=");
  Serial.print(I2S_BCK);
  Serial.print(", WS=");
  Serial.print(I2S_WS);
  Serial.print(", DOUT=");
  Serial.println(I2S_DOUT);
  
  tft.setCursor(10, 55);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print("BCK=");
  tft.print(I2S_BCK);
  tft.print(" WS=");
  tft.print(I2S_WS);
  tft.print(" OUT=");
  tft.println(I2S_DOUT);
  
  // Display test info
  tft.setCursor(10, 80);
  tft.setTextSize(2);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.println("Tone Tests:");
  
  tft.setTextSize(1);
  tft.setCursor(10, 110);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.println("Generating test tones");
  tft.println("Connect speaker to hear");
  
  Serial.println("\nAudio test ready");
  Serial.println("Generating test tones...");
}

bool initI2S() {
  // I2S configuration
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };
  
  // I2S pin configuration
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };
  
  // Install and start I2S driver
  esp_err_t err = i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.print("Failed to install I2S driver: ");
    Serial.println(err);
    return false;
  }
  
  err = i2s_set_pin(I2S_NUM, &pin_config);
  if (err != ESP_OK) {
    Serial.print("Failed to set I2S pins: ");
    Serial.println(err);
    return false;
  }
  
  return true;
}

void generateTone(int frequency, int duration_ms) {
  int samples = (SAMPLE_RATE * duration_ms) / 1000;
  int16_t* buffer = (int16_t*)malloc(samples * sizeof(int16_t) * 2); // Stereo
  
  if (buffer == NULL) {
    Serial.println("Failed to allocate audio buffer");
    return;
  }
  
  // Generate sine wave
  for (int i = 0; i < samples; i++) {
    float angle = 2.0 * PI * frequency * i / SAMPLE_RATE;
    int16_t sample = (int16_t)(sin(angle) * 10000); // Amplitude
    buffer[i * 2] = sample;     // Left channel
    buffer[i * 2 + 1] = sample; // Right channel
  }
  
  // Write to I2S
  size_t bytes_written;
  i2s_write(I2S_NUM, buffer, samples * sizeof(int16_t) * 2, &bytes_written, portMAX_DELAY);
  
  free(buffer);
}

void loop() {
  static uint32_t testTimer = 0;
  static int testPhase = 0;
  
  if (millis() - testTimer > 2000) {
    testTimer = millis();
    
    // Clear test area
    tft.fillRect(0, 140, 240, 100, TFT_BLACK);
    tft.setCursor(10, 145);
    tft.setTextSize(2);
    
    switch(testPhase) {
      case 0:
        Serial.println("Playing 440Hz tone (A4)");
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.println("Tone: 440Hz");
        generateTone(440, 500);
        break;
        
      case 1:
        Serial.println("Playing 880Hz tone (A5)");
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.println("Tone: 880Hz");
        generateTone(880, 500);
        break;
        
      case 2:
        Serial.println("Playing 1000Hz tone");
        tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
        tft.println("Tone: 1000Hz");
        generateTone(1000, 500);
        break;
        
      case 3:
        Serial.println("Playing 262Hz tone (C4)");
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.println("Tone: 262Hz");
        generateTone(262, 500);
        break;
    }
    
    testPhase = (testPhase + 1) % 4;
  }
  
  // Update status
  static uint32_t statusTimer = 0;
  if (millis() - statusTimer > 1000) {
    statusTimer = millis();
    
    tft.fillRect(0, 200, 240, 40, TFT_BLACK);
    tft.drawRect(0, 200, 240, 40, TFT_WHITE);
    
    tft.setCursor(10, 205);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.print("I2S Status: Active");
    
    tft.setCursor(10, 220);
    tft.print("Test cycle: ");
    tft.print(testPhase + 1);
    tft.print("/4");
  }
}