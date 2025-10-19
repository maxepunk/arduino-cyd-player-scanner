/*
 * Hardware_Probe.ino
 * 
 * Low-level hardware probing sketch for CYD (Cheap Yellow Display) variant detection
 * Performs raw GPIO tests, SPI responses, and hardware signature detection
 * without relying on library initialization.
 * 
 * This sketch tests:
 * - GPIO pin states and pull-up/down characteristics
 * - SPI bus responses and device signatures
 * - I2C device detection
 * - ADC characteristics
 * - Flash memory identification
 * - Board-specific hardware signatures
 * 
 * Compatible with ESP32-2432S028, ESP32-2432S032, and variants
 */

#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <driver/i2c.h>
#include <esp_flash.h>
#include <esp_system.h>
#include <soc/gpio_reg.h>
#include <soc/io_mux_reg.h>

// Common CYD pin definitions for testing
#define TFT_CS     15
#define TFT_DC     2
#define TFT_MOSI   13
#define TFT_MISO   12
#define TFT_SCLK   14
#define TFT_RST    -1
#define TFT_BL     21

#define TOUCH_CS   33
#define TOUCH_IRQ  36

#define SD_CS      5
#define SD_MOSI    23
#define SD_MISO    19
#define SD_SCLK    18

// RGB connector pins (P3)
#define RGB_R1     4
#define RGB_G1     16
#define RGB_B1     17
#define RGB_R2     25
#define RGB_G2     26
#define RGB_B2     27

// Audio pins
#define AUDIO_OUT  26
#define MIC_IN     34

// Extension connector pins
#define EXT_IO1    35
#define EXT_IO2    0
#define EXT_IO3    22

// SPI handles
spi_device_handle_t tft_spi;
spi_device_handle_t touch_spi;
spi_device_handle_t sd_spi;

struct HardwareSignature {
  String board_type;
  uint32_t flash_size;
  uint8_t display_id[4];
  uint8_t touch_id[4];
  bool has_sd_card;
  bool has_rgb_connector;
  bool has_audio;
  uint16_t gpio_states;
  float vcc_voltage;
  String unique_signature;
};

HardwareSignature hw_sig;

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("=== CYD Hardware Probe v1.0 ===");
  Serial.println("Low-level hardware detection starting...\n");
  
  // Initialize hardware signature structure
  memset(&hw_sig, 0, sizeof(hw_sig));
  
  // Perform comprehensive hardware probing
  probeGPIOCharacteristics();
  probeFlashMemory();
  probeSPIDevices();
  probeI2CDevices();
  probeADCCharacteristics();
  probeSpecialPins();
  
  // Generate hardware signature
  generateHardwareSignature();
  
  // Display results
  displayProbeResults();
  
  Serial.println("\n=== Hardware Probe Complete ===");
}

void loop() {
  // Continuous monitoring mode
  delay(5000);
  
  // Monitor dynamic characteristics
  Serial.println("\n--- Dynamic Monitoring ---");
  monitorDynamicCharacteristics();
}

void probeGPIOCharacteristics() {
  Serial.println("1. GPIO Characteristics Probe");
  Serial.println("==============================");
  
  // Test GPIO pins for pull-up/down characteristics and default states
  int test_pins[] = {0, 2, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33};
  int num_pins = sizeof(test_pins) / sizeof(test_pins[0]);
  
  hw_sig.gpio_states = 0;
  
  for (int i = 0; i < num_pins; i++) {
    int pin = test_pins[i];
    
    // Configure as input with no pull
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << pin);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    
    delay(10);
    
    // Read floating state
    int floating_state = gpio_get_level((gpio_num_t)pin);
    
    // Test with pull-up
    gpio_pullup_en((gpio_num_t)pin);
    delay(10);
    int pullup_state = gpio_get_level((gpio_num_t)pin);
    
    // Test with pull-down
    gpio_pullup_dis((gpio_num_t)pin);
    gpio_pulldown_en((gpio_num_t)pin);
    delay(10);
    int pulldown_state = gpio_get_level((gpio_num_t)pin);
    
    // Reset to floating
    gpio_pulldown_dis((gpio_num_t)pin);
    
    Serial.printf("GPIO %2d: Float=%d, PullUp=%d, PullDown=%d", 
                  pin, floating_state, pullup_state, pulldown_state);
    
    // Check for external connections (unusual behavior)
    if (floating_state == pullup_state && floating_state == pulldown_state) {
      Serial.print(" [DRIVEN]");
      hw_sig.gpio_states |= (1 << i);
    } else if (floating_state != 0 && floating_state != 1) {
      Serial.print(" [ANALOG]");
    }
    
    Serial.println();
  }
  
  Serial.println();
}

void probeFlashMemory() {
  Serial.println("2. Flash Memory Probe");
  Serial.println("=====================");
  
  // Get flash chip info
  esp_flash_t* flash = esp_flash_default_chip;
  uint32_t flash_size;
  esp_flash_get_size(flash, &flash_size);
  hw_sig.flash_size = flash_size;
  
  Serial.printf("Flash Size: %u bytes (%.1f MB)\n", flash_size, flash_size / (1024.0 * 1024.0));
  
  // Read flash ID
  uint32_t flash_id;
  esp_err_t ret = esp_flash_read_id(flash, &flash_id);
  if (ret == ESP_OK) {
    Serial.printf("Flash ID: 0x%06X\n", flash_id & 0xFFFFFF);
    Serial.printf("Manufacturer: 0x%02X\n", (flash_id >> 16) & 0xFF);
    Serial.printf("Memory Type: 0x%02X\n", (flash_id >> 8) & 0xFF);
    Serial.printf("Capacity: 0x%02X\n", flash_id & 0xFF);
  }
  
  Serial.println();
}

void probeSPIDevices() {
  Serial.println("3. SPI Device Probe");
  Serial.println("===================");
  
  // Initialize SPI bus
  spi_bus_config_t buscfg = {
    .mosi_io_num = TFT_MOSI,
    .miso_io_num = TFT_MISO,
    .sclk_io_num = TFT_SCLK,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = 4096
  };
  
  esp_err_t ret = spi_bus_initialize(HSPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
  if (ret != ESP_OK) {
    Serial.printf("SPI bus init failed: %s\n", esp_err_to_name(ret));
    return;
  }
  
  // Probe TFT display
  probeTFTDisplay();
  
  // Probe touch controller
  probeTouchController();
  
  // Probe SD card
  probeSDCard();
  
  // Clean up SPI
  spi_bus_free(HSPI_HOST);
  Serial.println();
}

void probeTFTDisplay() {
  Serial.println("3a. TFT Display Probe:");
  
  spi_device_interface_config_t devcfg = {
    .command_bits = 0,
    .address_bits = 0,
    .dummy_bits = 0,
    .mode = 0,
    .duty_cycle_pos = 128,
    .cs_ena_pretrans = 0,
    .cs_ena_posttrans = 0,
    .clock_speed_hz = 1000000,  // 1MHz for probing
    .input_delay_ns = 0,
    .spics_io_num = TFT_CS,
    .flags = 0,
    .queue_size = 1,
    .pre_cb = NULL,
    .post_cb = NULL,
  };
  
  esp_err_t ret = spi_bus_add_device(HSPI_HOST, &devcfg, &tft_spi);
  if (ret != ESP_OK) {
    Serial.printf("  Failed to add TFT device: %s\n", esp_err_to_name(ret));
    return;
  }
  
  // Configure DC pin
  gpio_set_direction((gpio_num_t)TFT_DC, GPIO_MODE_OUTPUT);
  
  // Try to read display ID (ILI9341 command 0x04)
  uint8_t cmd = 0x04;
  uint8_t id_data[4] = {0};
  
  spi_transaction_t t = {};
  t.length = 8;
  t.tx_buffer = &cmd;
  
  gpio_set_level((gpio_num_t)TFT_DC, 0);  // Command mode
  ret = spi_device_transmit(tft_spi, &t);
  
  if (ret == ESP_OK) {
    memset(&t, 0, sizeof(t));
    t.length = 32;
    t.rx_buffer = id_data;
    
    gpio_set_level((gpio_num_t)TFT_DC, 1);  // Data mode
    ret = spi_device_transmit(tft_spi, &t);
    
    if (ret == ESP_OK) {
      Serial.printf("  Display ID: 0x%02X 0x%02X 0x%02X 0x%02X\n", 
                    id_data[0], id_data[1], id_data[2], id_data[3]);
      memcpy(hw_sig.display_id, id_data, 4);
      
      // Identify display type
      if (id_data[1] == 0x93 && id_data[2] == 0x41) {
        Serial.println("  Detected: ILI9341");
      } else if (id_data[1] == 0x54 && id_data[2] == 0x80) {
        Serial.println("  Detected: ST7789");
      } else if (id_data[1] == 0x76 && id_data[2] == 0x63) {
        Serial.println("  Detected: ST7796");
      } else {
        Serial.println("  Unknown display controller");
      }
    }
  } else {
    Serial.printf("  SPI communication failed: %s\n", esp_err_to_name(ret));
  }
  
  spi_bus_remove_device(tft_spi);
}

void probeTouchController() {
  Serial.println("3b. Touch Controller Probe:");
  
  spi_device_interface_config_t devcfg = {
    .command_bits = 0,
    .address_bits = 0,
    .dummy_bits = 0,
    .mode = 0,
    .duty_cycle_pos = 128,
    .cs_ena_pretrans = 0,
    .cs_ena_posttrans = 0,
    .clock_speed_hz = 500000,  // 500kHz for touch
    .input_delay_ns = 0,
    .spics_io_num = TOUCH_CS,
    .flags = 0,
    .queue_size = 1,
    .pre_cb = NULL,
    .post_cb = NULL,
  };
  
  esp_err_t ret = spi_bus_add_device(HSPI_HOST, &devcfg, &touch_spi);
  if (ret != ESP_OK) {
    Serial.printf("  Failed to add touch device: %s\n", esp_err_to_name(ret));
    return;
  }
  
  // Try XPT2046 identification
  uint8_t cmd = 0x90;  // Read X position command
  uint8_t touch_data[2] = {0};
  
  spi_transaction_t t = {};
  t.length = 8;
  t.tx_buffer = &cmd;
  
  ret = spi_device_transmit(touch_spi, &t);
  
  if (ret == ESP_OK) {
    memset(&t, 0, sizeof(t));
    t.length = 16;
    t.rx_buffer = touch_data;
    ret = spi_device_transmit(touch_spi, &t);
    
    if (ret == ESP_OK) {
      uint16_t x_val = (touch_data[0] << 8) | touch_data[1];
      Serial.printf("  Touch response: 0x%04X\n", x_val);
      
      if (x_val > 0 && x_val < 0xFFF0) {
        Serial.println("  Detected: XPT2046 touch controller");
        hw_sig.touch_id[0] = 0XPT;
        hw_sig.touch_id[1] = 0x20;
        hw_sig.touch_id[2] = 0x46;
      }
    }
  }
  
  spi_bus_remove_device(touch_spi);
}

void probeSDCard() {
  Serial.println("3c. SD Card Probe:");
  
  // Check if SD CS pin is connected/pulled
  gpio_set_direction((gpio_num_t)SD_CS, GPIO_MODE_INPUT);
  gpio_set_pull_mode((gpio_num_t)SD_CS, GPIO_PULLUP_ONLY);
  delay(10);
  
  int cs_state = gpio_get_level((gpio_num_t)SD_CS);
  Serial.printf("  SD_CS state: %d\n", cs_state);
  
  if (cs_state == 1) {
    // Try basic SD card communication
    spi_device_interface_config_t devcfg = {
      .command_bits = 0,
      .address_bits = 0,
      .dummy_bits = 0,
      .mode = 0,
      .duty_cycle_pos = 128,
      .cs_ena_pretrans = 0,
      .cs_ena_posttrans = 0,
      .clock_speed_hz = 400000,  // 400kHz for SD init
      .input_delay_ns = 0,
      .spics_io_num = SD_CS,
      .flags = 0,
      .queue_size = 1,
      .pre_cb = NULL,
      .post_cb = NULL,
    };
    
    esp_err_t ret = spi_bus_add_device(HSPI_HOST, &devcfg, &sd_spi);
    if (ret == ESP_OK) {
      // Send CMD0 (reset)
      uint8_t cmd0[] = {0x40, 0x00, 0x00, 0x00, 0x00, 0x95};
      uint8_t response[1];
      
      spi_transaction_t t = {};
      t.length = 48;
      t.tx_buffer = cmd0;
      ret = spi_device_transmit(sd_spi, &t);
      
      if (ret == ESP_OK) {
        memset(&t, 0, sizeof(t));
        t.length = 8;
        t.rx_buffer = response;
        ret = spi_device_transmit(sd_spi, &t);
        
        if (ret == ESP_OK && response[0] == 0x01) {
          Serial.println("  SD card detected and responsive");
          hw_sig.has_sd_card = true;
        } else {
          Serial.println("  SD card slot present but no card detected");
        }
      }
      
      spi_bus_remove_device(sd_spi);
    }
  } else {
    Serial.println("  SD card slot not detected");
  }
}

void probeI2CDevices() {
  Serial.println("4. I2C Device Scan");
  Serial.println("==================");
  
  // Initialize I2C
  i2c_config_t conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = 21,  // Common SDA pin
    .scl_io_num = 22,  // Common SCL pin
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master = {.clk_speed = 100000}
  };
  
  esp_err_t ret = i2c_param_config(I2C_NUM_0, &conf);
  if (ret == ESP_OK) {
    ret = i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
  }
  
  if (ret != ESP_OK) {
    Serial.printf("I2C init failed: %s\n", esp_err_to_name(ret));
    return;
  }
  
  Serial.println("Scanning I2C addresses...");
  bool found_device = false;
  
  for (uint8_t addr = 0x08; addr < 0x78; addr++) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    
    ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    
    if (ret == ESP_OK) {
      Serial.printf("  Device found at 0x%02X\n", addr);
      found_device = true;
    }
  }
  
  if (!found_device) {
    Serial.println("  No I2C devices found");
  }
  
  i2c_driver_delete(I2C_NUM_0);
  Serial.println();
}

void probeADCCharacteristics() {
  Serial.println("5. ADC Characteristics");
  Serial.println("======================");
  
  // Read VCC voltage via internal reference
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);
  
  // Read multiple samples for stability
  uint32_t adc_sum = 0;
  for (int i = 0; i < 100; i++) {
    adc_sum += adc1_get_raw(ADC1_CHANNEL_0);
    delay(1);
  }
  
  float adc_avg = adc_sum / 100.0;
  float voltage = (adc_avg / 4095.0) * 3.3;  // Approximate conversion
  
  Serial.printf("ADC Average: %.1f (%.2fV equivalent)\n", adc_avg, voltage);
  hw_sig.vcc_voltage = voltage;
  
  // Test specific analog pins that might indicate board variant
  int analog_pins[] = {34, 35, 36, 39};
  for (int i = 0; i < 4; i++) {
    // Configure pin for ADC reading
    adc1_config_channel_atten((adc1_channel_t)(analog_pins[i] - 30), ADC_ATTEN_DB_11);
    
    uint32_t reading = 0;
    for (int j = 0; j < 10; j++) {
      reading += adc1_get_raw((adc1_channel_t)(analog_pins[i] - 30));
    }
    reading /= 10;
    
    Serial.printf("GPIO%d ADC: %u\n", analog_pins[i], reading);
  }
  
  Serial.println();
}

void probeSpecialPins() {
  Serial.println("6. Special Pin Characteristics");
  Serial.println("==============================");
  
  // Test RGB connector presence
  Serial.println("RGB Connector Test:");
  int rgb_pins[] = {RGB_R1, RGB_G1, RGB_B1, RGB_R2, RGB_G2, RGB_B2};
  bool rgb_connected = true;
  
  for (int i = 0; i < 6; i++) {
    gpio_set_direction((gpio_num_t)rgb_pins[i], GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)rgb_pins[i], GPIO_PULLUP_ONLY);
    delay(5);
    
    int state = gpio_get_level((gpio_num_t)rgb_pins[i]);
    Serial.printf("  RGB pin %d: %d\n", rgb_pins[i], state);
    
    if (state == 0) {  // Pin pulled low, might be connected
      rgb_connected = false;
    }
  }
  
  hw_sig.has_rgb_connector = rgb_connected;
  Serial.printf("  RGB connector likely %s\n", rgb_connected ? "present" : "absent/connected");
  
  // Test audio pins
  Serial.println("Audio Test:");
  gpio_set_direction((gpio_num_t)AUDIO_OUT, GPIO_MODE_INPUT);
  gpio_set_pull_mode((gpio_num_t)AUDIO_OUT, GPIO_PULLUP_ONLY);
  delay(5);
  
  int audio_state = gpio_get_level((gpio_num_t)AUDIO_OUT);
  Serial.printf("  Audio out pin %d: %d\n", AUDIO_OUT, audio_state);
  
  hw_sig.has_audio = (audio_state == 1);
  
  Serial.println();
}

void generateHardwareSignature() {
  Serial.println("7. Hardware Signature Generation");
  Serial.println("=================================");
  
  // Create unique signature based on all probed characteristics
  String signature = "";
  
  // Flash size signature
  signature += "F" + String(hw_sig.flash_size / (1024 * 1024)) + "MB_";
  
  // Display signature
  signature += "D";
  for (int i = 0; i < 4; i++) {
    signature += String(hw_sig.display_id[i], HEX);
  }
  signature += "_";
  
  // GPIO characteristics
  signature += "G" + String(hw_sig.gpio_states, HEX) + "_";
  
  // Board features
  if (hw_sig.has_sd_card) signature += "SD_";
  if (hw_sig.has_rgb_connector) signature += "RGB_";
  if (hw_sig.has_audio) signature += "AUD_";
  
  // Voltage signature
  signature += "V" + String((int)(hw_sig.vcc_voltage * 100)) + "_";
  
  hw_sig.unique_signature = signature;
  
  // Determine board type based on signature
  if (signature.indexOf("F4MB") >= 0 && signature.indexOf("D0093041") >= 0) {
    if (signature.indexOf("RGB") >= 0) {
      hw_sig.board_type = "ESP32-2432S028R";
    } else {
      hw_sig.board_type = "ESP32-2432S028";
    }
  } else if (signature.indexOf("F8MB") >= 0) {
    hw_sig.board_type = "ESP32-2432S032";
  } else {
    hw_sig.board_type = "Unknown CYD variant";
  }
  
  Serial.printf("Generated signature: %s\n", signature.c_str());
  Serial.printf("Detected board type: %s\n", hw_sig.board_type.c_str());
  Serial.println();
}

void displayProbeResults() {
  Serial.println("8. Hardware Probe Summary");
  Serial.println("=========================");
  
  Serial.printf("Board Type: %s\n", hw_sig.board_type.c_str());
  Serial.printf("Flash Size: %u MB\n", hw_sig.flash_size / (1024 * 1024));
  Serial.printf("Display ID: 0x%02X%02X%02X%02X\n", 
                hw_sig.display_id[0], hw_sig.display_id[1], 
                hw_sig.display_id[2], hw_sig.display_id[3]);
  Serial.printf("Touch ID: 0x%02X%02X%02X%02X\n", 
                hw_sig.touch_id[0], hw_sig.touch_id[1], 
                hw_sig.touch_id[2], hw_sig.touch_id[3]);
  Serial.printf("SD Card: %s\n", hw_sig.has_sd_card ? "Present" : "Not detected");
  Serial.printf("RGB Connector: %s\n", hw_sig.has_rgb_connector ? "Present" : "Not detected");
  Serial.printf("Audio: %s\n", hw_sig.has_audio ? "Present" : "Not detected");
  Serial.printf("GPIO States: 0x%04X\n", hw_sig.gpio_states);
  Serial.printf("VCC Voltage: %.2fV\n", hw_sig.vcc_voltage);
  Serial.printf("Unique Signature: %s\n", hw_sig.unique_signature.c_str());
  
  // Provide recommendations
  Serial.println("\nRecommendations:");
  if (hw_sig.board_type.indexOf("2432S028") >= 0) {
    Serial.println("- Use TFT_eSPI library with ILI9341 driver");
    Serial.println("- Touch controller: XPT2046");
    Serial.println("- Resolution: 240x320 pixels");
  } else if (hw_sig.board_type.indexOf("2432S032") >= 0) {
    Serial.println("- Use TFT_eSPI library with ST7789 driver");
    Serial.println("- Touch controller: CST816S (I2C)");
    Serial.println("- Resolution: 240x320 pixels");
  }
  
  if (hw_sig.has_sd_card) {
    Serial.println("- SD card functionality available");
  }
  
  if (hw_sig.has_rgb_connector) {
    Serial.println("- RGB connector available for external displays");
  }
}

void monitorDynamicCharacteristics() {
  // Monitor characteristics that might change over time
  
  // Check for temperature effects on ADC
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);
  
  uint32_t current_adc = adc1_get_raw(ADC1_CHANNEL_0);
  float current_voltage = (current_adc / 4095.0) * 3.3;
  
  Serial.printf("Current VCC: %.2fV (was %.2fV)\n", current_voltage, hw_sig.vcc_voltage);
  
  // Monitor touch IRQ pin for activity
  int touch_irq = gpio_get_level((gpio_num_t)TOUCH_IRQ);
  Serial.printf("Touch IRQ: %d\n", touch_irq);
  
  // Check for any GPIO state changes
  Serial.printf("GPIO states: 0x%04X\n", hw_sig.gpio_states);
}