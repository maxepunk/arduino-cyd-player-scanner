/**
 * SoftwareSPI.cpp - Software SPI implementation with precise timing
 * 
 * Implements bit-bang SPI with critical section protection, GPIO27 management,
 * and optimized timing for RFID communication on CYD devices.
 */

#include "SoftwareSPI.h"
#include "GPIO27Manager.h"

SoftwareSPI::SoftwareSPI() : initialized(false), pinsClaimed(false), 
                           usesGPIO27(false), gpio27Acquired(false) {
    // Initialize statistics
    memset(&stats, 0, sizeof(stats));
    stats.minTransferTimeUs = UINT32_MAX;
    
    // Set default timing
    timing = SPITimingPresets::STANDARD;
    mode = SOFT_SPI_MODE0;
    
    // Initialize pins to invalid values
    pins.sck = 0;
    pins.mosi = 0;
    pins.miso = 0;
    pins.ss = 0;
}

SoftwareSPI::~SoftwareSPI() {
    if (initialized) {
        end();
    }
}

bool SoftwareSPI::initialize(const SPIPins& pinConfig, SoftSPIMode spiMode) {
    if (initialized) {
        Serial.println(F("[WARNING][SoftSPI]: Already initialized"));
        return true;
    }
    
    Serial.println(F("[INFO][SoftSPI]: Initializing software SPI"));
    
    // Validate pin configuration
    if (!SPIHelper::isValidPin(pinConfig.sck) || 
        !SPIHelper::isValidPin(pinConfig.mosi) || 
        !SPIHelper::isValidPin(pinConfig.miso)) {
        Serial.println(F("[ERROR][SoftSPI]: Invalid pin configuration"));
        return false;
    }
    
    if (!arePinsAvailable(pinConfig)) {
        Serial.println(F("[ERROR][SoftSPI]: Pins not available"));
        return false;
    }
    
    // Check if GPIO27 is used
    usesGPIO27 = (pinConfig.mosi == 27 || pinConfig.sck == 27 || 
                  pinConfig.miso == 27 || pinConfig.ss == 27);
    
    // Store configuration
    pins = pinConfig;
    mode = spiMode;
    
    // Configure pins
    configurePins();
    
    // Calibrate timing if needed
    calibrateTiming();
    
    initialized = true;
    pinsClaimed = true;
    
    Serial.print(F("[INFO][SoftSPI]: Initialized - SCK="));
    Serial.print(pins.sck);
    Serial.print(F(", MOSI="));
    Serial.print(pins.mosi);
    Serial.print(F(", MISO="));
    Serial.print(pins.miso);
    Serial.print(F(", Mode="));
    Serial.println(SPIHelper::modeToString(mode));
    
    return true;
}

void SoftwareSPI::configurePins() {
    // Configure SCK (Clock) - always output
    pinMode(pins.sck, OUTPUT);
    setClock(mode == SPI_MODE2 || mode == SPI_MODE3); // Idle state based on CPOL
    
    // Configure MOSI (Master Out, Slave In) - always output
    pinMode(pins.mosi, OUTPUT);
    digitalWrite(pins.mosi, LOW);
    
    // Configure MISO (Master In, Slave Out) - always input
    pinMode(pins.miso, INPUT);
    
    // Configure SS (Slave Select) if specified
    if (pins.ss != 0) {
        pinMode(pins.ss, OUTPUT);
        digitalWrite(pins.ss, HIGH); // Idle high
    }
}

void SoftwareSPI::releasePins() {
    if (pinsClaimed) {
        // Set all pins to input to release them
        if (pins.sck != 0) pinMode(pins.sck, INPUT);
        if (pins.mosi != 0) pinMode(pins.mosi, INPUT);
        if (pins.ss != 0) pinMode(pins.ss, INPUT);
        
        pinsClaimed = false;
    }
}

bool SoftwareSPI::setTiming(const SPITiming& newTiming) {
    if (!validateTiming(newTiming)) {
        Serial.println(F("[ERROR][SoftSPI]: Invalid timing configuration"));
        return false;
    }
    
    timing = newTiming;
    
    Serial.print(F("[INFO][SoftSPI]: Timing updated - ClockDelay="));
    Serial.print(timing.clockDelayUs);
    Serial.print(F("us, OpDelay="));
    Serial.print(timing.operationDelayUs);
    Serial.println(F("us"));
    
    return true;
}

bool SoftwareSPI::validateTiming(const SPITiming& newTiming) const {
    return (newTiming.clockDelayUs >= MIN_CLOCK_DELAY_US && 
            newTiming.clockDelayUs <= MAX_CLOCK_DELAY_US &&
            newTiming.operationDelayUs <= 50 &&
            newTiming.timeoutMs > 0 && 
            newTiming.timeoutMs <= 10000 &&
            newTiming.maxRetries <= 10);
}

void SoftwareSPI::begin() {
    if (!initialized) {
        Serial.println(F("[ERROR][SoftSPI]: Not initialized"));
        return;
    }
    
    // Acquire GPIO27 if needed
    if (usesGPIO27 && !gpio27Acquired) {
        gpio27Acquired = acquireGPIO27();
        if (!gpio27Acquired) {
            Serial.println(F("[WARNING][SoftSPI]: Could not acquire GPIO27"));
        }
    }
}

void SoftwareSPI::end() {
    // Release GPIO27 if acquired
    if (gpio27Acquired) {
        releaseGPIO27();
        gpio27Acquired = false;
    }
    
    // Release pins
    releasePins();
    
    initialized = false;
    
    Serial.println(F("[INFO][SoftSPI]: SPI ended"));
}

bool SoftwareSPI::acquireGPIO27() {
    if (!usesGPIO27) return true;
    
    GPIO27Manager& gpio27 = GPIO27Manager::getInstance();
    return gpio27.beginRFIDOperation();
}

void SoftwareSPI::releaseGPIO27() {
    if (!usesGPIO27) return;
    
    GPIO27Manager& gpio27 = GPIO27Manager::getInstance();
    gpio27.endRFIDOperation();
}

void SoftwareSPI::setClock(bool high) {
    digitalWrite(pins.sck, high ? HIGH : LOW);
}

void SoftwareSPI::setData(bool high) {
    digitalWrite(pins.mosi, high ? HIGH : LOW);
}

bool SoftwareSPI::readData() {
    return digitalRead(pins.miso) == HIGH;
}

void SoftwareSPI::delayMicroseconds(uint8_t us) {
    if (us > 0) {
        ::delayMicroseconds(us);
    }
}

uint8_t SoftwareSPI::transfer(uint8_t data) {
    if (!initialized) {
        Serial.println(F("[ERROR][SoftSPI]: Not initialized"));
        return 0;
    }
    
    uint32_t startTime = micros();
    uint8_t result = transferByte(data);
    uint32_t transferTime = micros() - startTime;
    
    updateStatistics(transferTime, true);
    
    return result;
}

uint8_t SoftwareSPI::transferByte(uint8_t data) {
    uint8_t result = 0;
    
    // Use critical section if enabled
    if (timing.useInterruptProtection) {
        enterCriticalSection();
    }
    
    // Transfer 8 bits
    for (int bit = 7; bit >= 0; bit--) {
        // Set data bit on MOSI
        setData((data >> bit) & 0x01);
        
        // Clock timing based on SPI mode
        if (mode == SPI_MODE0 || mode == SPI_MODE1) {
            // CPOL = 0: Clock starts low
            delayMicroseconds(timing.clockDelayUs);
            setClock(true);  // Rising edge
            
            if (mode == SPI_MODE0) {
                // CPHA = 0: Sample on rising edge
                if (readData()) {
                    result |= (1 << bit);
                }
            }
            
            delayMicroseconds(timing.clockDelayUs);
            setClock(false); // Falling edge
            
            if (mode == SPI_MODE1) {
                // CPHA = 1: Sample on falling edge
                if (readData()) {
                    result |= (1 << bit);
                }
            }
        } else {
            // CPOL = 1: Clock starts high
            delayMicroseconds(timing.clockDelayUs);
            setClock(false); // Falling edge
            
            if (mode == SPI_MODE2) {
                // CPHA = 0: Sample on falling edge
                if (readData()) {
                    result |= (1 << bit);
                }
            }
            
            delayMicroseconds(timing.clockDelayUs);
            setClock(true);  // Rising edge
            
            if (mode == SPI_MODE3) {
                // CPHA = 1: Sample on rising edge
                if (readData()) {
                    result |= (1 << bit);
                }
            }
        }
    }
    
    // Return clock to idle state
    setClock(mode == SPI_MODE2 || mode == SPI_MODE3);
    
    if (timing.useInterruptProtection) {
        exitCriticalSection();
    }
    
    // Operation delay
    if (timing.operationDelayUs > 0) {
        delayMicroseconds(timing.operationDelayUs);
    }
    
    return result;
}

bool SoftwareSPI::transfer(uint8_t* txData, uint8_t* rxData, size_t length) {
    if (!initialized) {
        Serial.println(F("[ERROR][SoftSPI]: Not initialized"));
        return false;
    }
    
    if (length == 0 || length > SPIConfig::MAX_TRANSFER_SIZE) {
        Serial.println(F("[ERROR][SoftSPI]: Invalid transfer length"));
        return false;
    }
    
    uint32_t startTime = micros();
    bool success = true;
    
    for (size_t i = 0; i < length; i++) {
        uint8_t txByte = txData ? txData[i] : 0;
        uint8_t rxByte = transferByte(txByte);
        
        if (rxData) {
            rxData[i] = rxByte;
        }
        
        // Check timeout
        if ((micros() - startTime) / 1000 > timing.timeoutMs) {
            Serial.println(F("[ERROR][SoftSPI]: Transfer timeout"));
            success = false;
            break;
        }
    }
    
    uint32_t transferTime = micros() - startTime;
    updateStatistics(transferTime, success);
    
    return success;
}

void SoftwareSPI::enterCriticalSection() {
    // ESP32 critical section
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    portENTER_CRITICAL(&mux);
}

void SoftwareSPI::exitCriticalSection() {
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    portEXIT_CRITICAL(&mux);
}

void SoftwareSPI::updateStatistics(uint32_t transferTime, bool success) {
    stats.totalTransfers++;
    stats.totalBytes++;
    
    if (success) {
        stats.successfulTransfers++;
    } else {
        stats.failedTransfers++;
    }
    
    // Update timing statistics
    if (transferTime > stats.maxTransferTimeUs) {
        stats.maxTransferTimeUs = transferTime;
    }
    
    if (transferTime < stats.minTransferTimeUs) {
        stats.minTransferTimeUs = transferTime;
    }
    
    // Calculate running average
    if (stats.successfulTransfers > 0) {
        stats.avgTransferTimeUs = ((stats.avgTransferTimeUs * (stats.successfulTransfers - 1)) + 
                                  transferTime) / stats.successfulTransfers;
    }
    
    stats.lastTransferTime = millis();
}

bool SoftwareSPI::arePinsAvailable(const SPIPins& pinConfig) {
    // Check if pins are in forbidden list
    for (size_t i = 0; i < SPIConfig::FORBIDDEN_PIN_COUNT; i++) {
        uint8_t forbiddenPin = SPIConfig::FORBIDDEN_PINS[i];
        if (pinConfig.sck == forbiddenPin || 
            pinConfig.mosi == forbiddenPin || 
            pinConfig.miso == forbiddenPin || 
            pinConfig.ss == forbiddenPin) {
            Serial.print(F("[ERROR][SoftSPI]: Pin "));
            Serial.print(forbiddenPin);
            Serial.println(F(" is forbidden"));
            return false;
        }
    }
    
    return SPIHelper::isPinAvailable(pinConfig.sck) &&
           SPIHelper::isPinAvailable(pinConfig.mosi) &&
           SPIHelper::isPinAvailable(pinConfig.miso) &&
           (pinConfig.ss == 0 || SPIHelper::isPinAvailable(pinConfig.ss));
}

bool SoftwareSPI::setClockFrequency(uint32_t frequencyHz) {
    if (frequencyHz < SPIConfig::MIN_FREQUENCY_HZ || 
        frequencyHz > SPIConfig::MAX_FREQUENCY_HZ) {
        Serial.println(F("[ERROR][SoftSPI]: Frequency out of range"));
        return false;
    }
    
    timing.clockDelayUs = SPIHelper::calculateClockDelay(frequencyHz);
    
    Serial.print(F("[INFO][SoftSPI]: Clock frequency set to "));
    Serial.print(frequencyHz);
    Serial.print(F(" Hz (delay = "));
    Serial.print(timing.clockDelayUs);
    Serial.println(F(" us)"));
    
    return true;
}

uint32_t SoftwareSPI::getClockFrequency() const {
    return SPIHelper::calculateFrequency(timing.clockDelayUs);
}

bool SoftwareSPI::testConnection() {
    if (!initialized) {
        return false;
    }
    
    Serial.println(F("[INFO][SoftSPI]: Testing SPI connection"));
    
    // Test basic pin control
    setClock(false);
    delayMicroseconds(10);
    bool clockLow = (digitalRead(pins.sck) == LOW);
    
    setClock(true);
    delayMicroseconds(10);
    bool clockHigh = (digitalRead(pins.sck) == HIGH);
    
    setData(false);
    delayMicroseconds(10);
    bool dataLow = (digitalRead(pins.mosi) == LOW);
    
    setData(true);
    delayMicroseconds(10);
    bool dataHigh = (digitalRead(pins.mosi) == HIGH);
    
    bool testPassed = clockLow && clockHigh && dataLow && dataHigh;
    
    Serial.print(F("[INFO][SoftSPI]: Connection test "));
    Serial.println(testPassed ? F("PASSED") : F("FAILED"));
    
    return testPassed;
}

void SoftwareSPI::resetStatistics() {
    memset(&stats, 0, sizeof(stats));
    stats.minTransferTimeUs = UINT32_MAX;
    Serial.println(F("[INFO][SoftSPI]: Statistics reset"));
}

void SoftwareSPI::calibrateTiming() {
    // Simple timing calibration
    uint32_t startTime = micros();
    delayMicroseconds(timing.clockDelayUs);
    uint32_t actualDelay = micros() - startTime;
    
    if (actualDelay > timing.clockDelayUs * 2) {
        Serial.println(F("[WARNING][SoftSPI]: Timing may be inaccurate"));
    }
}

void SoftwareSPI::printDiagnostics() const {
    Serial.println(F("=== Software SPI Diagnostics ==="));
    
    Serial.print(F("Initialized: "));
    Serial.println(initialized ? F("Yes") : F("No"));
    
    Serial.print(F("Pins - SCK:"));
    Serial.print(pins.sck);
    Serial.print(F(", MOSI:"));
    Serial.print(pins.mosi);
    Serial.print(F(", MISO:"));
    Serial.print(pins.miso);
    Serial.print(F(", SS:"));
    Serial.println(pins.ss);
    
    Serial.print(F("Mode: "));
    Serial.println(SPIHelper::modeToString(mode));
    
    Serial.print(F("Clock frequency: "));
    Serial.print(getClockFrequency());
    Serial.println(F(" Hz"));
    
    Serial.print(F("Uses GPIO27: "));
    Serial.println(usesGPIO27 ? F("Yes") : F("No"));
    
    SPIHelper::printStatistics(stats);
    
    Serial.println(F("==============================="));
}

// Helper function implementations
namespace SPIHelper {
    bool isValidPin(uint8_t pin) {
        return pin > 0 && pin <= 39; // ESP32 GPIO range
    }
    
    bool isPinAvailable(uint8_t pin) {
        // Basic availability check - could be enhanced with actual GPIO register reading
        return isValidPin(pin);
    }
    
    bool canPinBeOutput(uint8_t pin) {
        return isValidPin(pin) && pin != 34 && pin != 35 && pin != 36 && pin != 39; // Input-only pins
    }
    
    bool canPinBeInput(uint8_t pin) {
        return isValidPin(pin);
    }
    
    uint32_t calculateFrequency(uint8_t clockDelayUs) {
        if (clockDelayUs == 0) return SPIConfig::MAX_FREQUENCY_HZ;
        return 1000000 / (clockDelayUs * 2); // Two delays per clock cycle
    }
    
    uint8_t calculateClockDelay(uint32_t frequencyHz) {
        if (frequencyHz >= SPIConfig::MAX_FREQUENCY_HZ) return 1;
        uint32_t delayUs = 1000000 / (frequencyHz * 2);
        return constrain(delayUs, 1, 100); // 1us to 100us range
    }
    
    const char* modeToString(SPIMode mode) {
        switch (mode) {
            case SPI_MODE0: return "MODE0 (CPOL=0,CPHA=0)";
            case SPI_MODE1: return "MODE1 (CPOL=0,CPHA=1)";
            case SPI_MODE2: return "MODE2 (CPOL=1,CPHA=0)";
            case SPI_MODE3: return "MODE3 (CPOL=1,CPHA=1)";
            default: return "UNKNOWN";
        }
    }
    
    bool isValidMode(SPIMode mode) {
        return mode >= SPI_MODE0 && mode <= SPI_MODE3;
    }
    
    void printStatistics(const SPIStats& stats) {
        Serial.println(F("--- SPI Statistics ---"));
        Serial.print(F("Total transfers: "));
        Serial.println(stats.totalTransfers);
        
        Serial.print(F("Successful: "));
        Serial.println(stats.successfulTransfers);
        
        Serial.print(F("Failed: "));
        Serial.println(stats.failedTransfers);
        
        Serial.print(F("Total bytes: "));
        Serial.println(stats.totalBytes);
        
        if (stats.totalTransfers > 0) {
            float successRate = 100.0f * stats.successfulTransfers / stats.totalTransfers;
            Serial.print(F("Success rate: "));
            Serial.print(successRate);
            Serial.println(F("%"));
        }
        
        Serial.print(F("Avg transfer time: "));
        Serial.print(stats.avgTransferTimeUs);
        Serial.println(F(" us"));
        
        Serial.print(F("Min/Max time: "));
        Serial.print(stats.minTransferTimeUs);
        Serial.print(F("/"));
        Serial.print(stats.maxTransferTimeUs);
        Serial.println(F(" us"));
        
        Serial.println(F("--------------------"));
    }
    
    void formatTransferRate(uint32_t bytesPerSecond, char* buffer, size_t bufferSize) {
        if (bytesPerSecond >= 1000000) {
            snprintf(buffer, bufferSize, "%.1f MB/s", bytesPerSecond / 1000000.0f);
        } else if (bytesPerSecond >= 1000) {
            snprintf(buffer, bufferSize, "%.1f KB/s", bytesPerSecond / 1000.0f);
        } else {
            snprintf(buffer, bufferSize, "%lu B/s", bytesPerSecond);
        }
    }
}

// RAII Transaction Implementation
SPITransaction::SPITransaction(SoftwareSPI* spiInstance) : spi(spiInstance), active(false) {
    if (spi && spi->isInitialized()) {
        spi->begin();
        active = true;
        startTime = millis();
    }
}

SPITransaction::~SPITransaction() {
    if (spi && active) {
        spi->end();
    }
}