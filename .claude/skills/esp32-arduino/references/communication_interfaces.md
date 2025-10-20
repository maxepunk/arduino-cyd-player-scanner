# ESP32 Communication Interfaces

This guide covers SPI, I2C, UART, and other communication protocols on ESP32, including pin assignments, configuration, and best practices.

## SPI (Serial Peripheral Interface)

ESP32 has **three SPI controllers**, but only two are available for external devices:

### HSPI (SPI2) - Hardware SPI Bus 2

**Default pins:**
- MISO: GPIO 12
- MOSI: GPIO 13
- CLK: GPIO 14
- CS: GPIO 15

**Considerations:**
- GPIO 12 is strapping pin (**MUST BE LOW at boot**)
- GPIO 15 is strapping pin (has internal pull-up)
- Use with caution due to boot mode conflicts

### VSPI (SPI3) - Hardware SPI Bus 3 (Recommended)

**Default pins:**
- MISO: GPIO 19
- MOSI: GPIO 23
- CLK: GPIO 18
- CS: GPIO 5

**Advantages:**
- Safer pin assignments (no critical strapping pins)
- Standard choice for most ESP32 projects
- Default SPI bus in Arduino framework

### SPI1 - Unavailable
- Reserved for internal flash chip
- Pins GPIO 6-11 must never be used
- Attempting to use causes immediate crashes

### SPI Configuration Example

```cpp
#include <SPI.h>

// Use VSPI (default)
SPIClass spi(VSPI);

void setup() {
  // Initialize with default pins
  spi.begin();
  
  // Or specify custom pins
  spi.begin(
    18,  // SCK
    19,  // MISO
    23,  // MOSI
    5    // CS
  );
  
  // Configure SPI settings
  spi.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
}
```

### Multiple SPI Devices

```cpp
#define CS_DEVICE1  5
#define CS_DEVICE2  15
#define CS_DEVICE3  16

void setup() {
  pinMode(CS_DEVICE1, OUTPUT);
  pinMode(CS_DEVICE2, OUTPUT);
  pinMode(CS_DEVICE3, OUTPUT);
  
  digitalWrite(CS_DEVICE1, HIGH);
  digitalWrite(CS_DEVICE2, HIGH);
  digitalWrite(CS_DEVICE3, HIGH);
  
  SPI.begin();
}

void readDevice1() {
  digitalWrite(CS_DEVICE1, LOW);
  byte data = SPI.transfer(0x00);
  digitalWrite(CS_DEVICE1, HIGH);
}
```

### Custom Pin Assignment

While hardware defaults provide best performance, pins can be remapped:

```cpp
// Custom SPI pins via GPIO matrix
SPIClass mySPI(VSPI);
mySPI.begin(
  14,  // Custom SCK
  12,  // Custom MISO
  13,  // Custom MOSI
  15   // Custom CS
);
```

**Trade-off:** Slight performance reduction, but gains flexibility.

## I2C (Inter-Integrated Circuit)

ESP32 has **two I2C hardware controllers** (I2C0 and I2C1), both fully remappable.

### Default Pins

- **SDA**: GPIO 21
- **SCL**: GPIO 22

### I2C Configuration

```cpp
#include <Wire.h>

void setup() {
  // Use default pins
  Wire.begin();
  
  // Or specify custom pins
  Wire.begin(21, 22);  // SDA, SCL
  
  // Set I2C clock speed
  Wire.setClock(100000);  // 100kHz (standard)
  Wire.setClock(400000);  // 400kHz (fast mode)
}
```

### Multiple I2C Buses

```cpp
#include <Wire.h>

TwoWire I2C_1 = TwoWire(0);  // I2C controller 0
TwoWire I2C_2 = TwoWire(1);  // I2C controller 1

void setup() {
  I2C_1.begin(21, 22, 100000);  // SDA, SCL, frequency
  I2C_2.begin(16, 17, 100000);  // Different pins for second bus
}

void loop() {
  I2C_1.beginTransmission(0x50);  // Device on bus 1
  I2C_1.write(0x00);
  I2C_1.endTransmission();
  
  I2C_2.beginTransmission(0x50);  // Same address, different bus
  I2C_2.write(0x00);
  I2C_2.endTransmission();
}
```

### Critical I2C Requirements

**External pull-up resistors are mandatory:**
- Resistance: 2.2kΩ - 10kΩ (4.7kΩ typical)
- Internal ESP32 pull-ups (~45kΩ) are **insufficient**
- Without proper pull-ups: unreliable communication, random failures

**Resistor value selection:**
- **2.2kΩ**: Long traces, high capacitance, faster speeds
- **4.7kΩ**: Standard value, good for most applications
- **10kΩ**: Low-power applications, shorter traces

### I2C Scanning

```cpp
#include <Wire.h>

void scanI2C() {
  Serial.println("Scanning I2C bus...");
  byte count = 0;
  
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    byte error = Wire.endTransmission();
    
    if (error == 0) {
      Serial.printf("Device found at 0x%02X\n", addr);
      count++;
    }
  }
  
  Serial.printf("Found %d device(s)\n", count);
}
```

### Common I2C Issues

**Symptoms:** Bus lockup, random failures, missing ACKs

**Solutions:**
1. Check pull-up resistors (measure voltage on SDA/SCL)
2. Verify power supply to I2C devices
3. Check address conflicts (each device needs unique address)
4. Reduce bus speed to 100kHz
5. Shorten wire lengths (under 30cm for reliability)

## UART (Serial Communication)

ESP32 has **three UART controllers** (UART0, UART1, UART2).

### UART0 - USB Serial (Primary)

**Default pins:**
- TX: GPIO 1
- RX: GPIO 3

**Usage:**
- Connected to USB-to-serial chip
- Used for programming and `Serial.print()`
- Difficult to use for external devices

```cpp
void setup() {
  Serial.begin(115200);  // UART0
  Serial.println("Hello from UART0");
}
```

### UART1 - Generally Avoid

**Default pins:**
- TX: GPIO 9
- RX: GPIO 10

**Problem:** These pins connect to flash chip
- Must be remapped to use
- Better to use UART2 instead

### UART2 - Best for External Devices

**Default pins:**
- TX: GPIO 16
- RX: GPIO 17

**Note:** On ESP32-WROVER, GPIO 16/17 conflict with PSRAM

```cpp
void setup() {
  Serial2.begin(9600, SERIAL_8N1, 16, 17);  // Baud, config, RX, TX
  Serial2.println("Hello from UART2");
}
```

### Custom UART Pins

All UART signals can be remapped to almost any GPIO:

```cpp
#include <HardwareSerial.h>

HardwareSerial MySerial(2);  // Use UART2

void setup() {
  MySerial.begin(
    9600,        // Baud rate
    SERIAL_8N1,  // Config (8 data bits, no parity, 1 stop bit)
    26,          // RX pin
    27           // TX pin
  );
}
```

### UART Configurations

```cpp
// Common configurations
SERIAL_8N1  // 8 data, no parity, 1 stop (most common)
SERIAL_8N2  // 8 data, no parity, 2 stop
SERIAL_8E1  // 8 data, even parity, 1 stop
SERIAL_8O1  // 8 data, odd parity, 1 stop
SERIAL_7E1  // 7 data, even parity, 1 stop
```

### Multiple Serial Ports Example

```cpp
void setup() {
  Serial.begin(115200);   // USB debug (UART0)
  Serial1.begin(9600);    // GPS module (remapped UART1)
  Serial2.begin(115200);  // External device (UART2)
  
  // Remap UART1 to safe pins
  Serial1.begin(9600, SERIAL_8N1, 25, 26);
}

void loop() {
  // Read from GPS (Serial1)
  if (Serial1.available()) {
    String gpsData = Serial1.readStringUntil('\n');
    Serial.println("GPS: " + gpsData);  // Debug to USB
  }
  
  // Communicate with external device (Serial2)
  if (Serial2.available()) {
    byte data = Serial2.read();
    Serial2.write(data + 1);  // Echo back incremented
  }
}
```

## PWM (Pulse Width Modulation)

ESP32 has **16 independent PWM channels** (LED PWM controller).

### PWM Configuration

```cpp
// PWM parameters
#define PWM_FREQ      5000   // 5 kHz
#define PWM_CHANNEL   0
#define PWM_RESOLUTION 8     // 8-bit (0-255)
#define PWM_PIN       13

void setup() {
  // Configure PWM
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  
  // Attach pin to channel
  ledcAttachPin(PWM_PIN, PWM_CHANNEL);
  
  // Set duty cycle (0-255 for 8-bit)
  ledcWrite(PWM_CHANNEL, 128);  // 50% duty cycle
}
```

### Servo Control

```cpp
#include <ESP32Servo.h>

Servo myServo;

void setup() {
  myServo.attach(13);  // Attach to pin
  myServo.write(90);   // Move to 90 degrees
}
```

### Multiple PWM Channels

```cpp
// Configure multiple channels for RGB LED
#define RED_PIN     13
#define GREEN_PIN   14
#define BLUE_PIN    27
#define RED_CH      0
#define GREEN_CH    1
#define BLUE_CH     2

void setup() {
  ledcSetup(RED_CH, 5000, 8);
  ledcSetup(GREEN_CH, 5000, 8);
  ledcSetup(BLUE_CH, 5000, 8);
  
  ledcAttachPin(RED_PIN, RED_CH);
  ledcAttachPin(GREEN_PIN, GREEN_CH);
  ledcAttachPin(BLUE_PIN, BLUE_CH);
}

void setColor(int r, int g, int b) {
  ledcWrite(RED_CH, r);
  ledcWrite(GREEN_CH, g);
  ledcWrite(BLUE_CH, b);
}
```

## Quick Pin Assignment Reference

### Recommended Pin Assignments

```cpp
// I2C - use defaults
#define I2C_SDA  21
#define I2C_SCL  22

// SPI - use VSPI defaults
#define SPI_MISO 19
#define SPI_MOSI 23
#define SPI_CLK  18
#define SPI_CS   5

// UART2 - for external serial
#define UART2_RX 16
#define UART2_TX 17

// General GPIO - safe choices
#define GPIO1    13
#define GPIO2    14
#define GPIO3    25
#define GPIO4    26
#define GPIO5    27

// Analog inputs - ADC1 (WiFi safe)
#define ADC1     32
#define ADC2     33
#define ADC3     34  // Input-only
#define ADC4     35  // Input-only
```

## Performance Considerations

### SPI Speed
- Maximum: 40 MHz (80 MHz possible with limitations)
- Recommended: 10-20 MHz for reliable operation
- Use DMA for high-throughput transfers

### I2C Speed
- Standard: 100 kHz (most compatible)
- Fast: 400 kHz (shorter cables, good pull-ups)
- Maximum: 1 MHz (rarely practical)

### UART Baud Rates
- Standard: 9600, 19200, 38400, 57600, 115200
- High speed: 230400, 460800, 921600
- Maximum: ~5 Mbaud (varies by cable/conditions)

## Next Steps

- See **hardware_constraints.md** for GPIO restrictions
- See **debugging_troubleshooting.md** for common communication issues
