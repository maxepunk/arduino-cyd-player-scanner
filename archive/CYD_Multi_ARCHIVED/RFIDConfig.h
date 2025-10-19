#ifndef RFID_CONFIG_H
#define RFID_CONFIG_H

#include <Arduino.h>

/**
 * RFIDConfig - RFID reader configuration and state
 * 
 * Validation Rules:
 * - Pin numbers must match constitution requirements
 * - clockDelayUs typically 2-10
 * - timeoutMs typically 50-200
 * - maxRetries typically 3-5
 */
struct RFIDConfig {
  // Software SPI pins
  struct RFIDPins {
    uint8_t sck;                 // Clock (GPIO22)
    uint8_t mosi;                // Data out (GPIO27)
    uint8_t miso;                // Data in (GPIO35)
    uint8_t ss;                  // Slave select (GPIO3/RX)
  };
  RFIDPins pins;
  
  // Timing parameters
  struct SPITiming {
    uint8_t clockDelayUs;        // Microseconds between clock edges
    uint8_t operationDelayUs;    // Delay after operations
    uint16_t timeoutMs;          // Operation timeout
    uint8_t maxRetries;          // Retry count for failed ops
  };
  SPITiming timing;
  
  // Runtime state
  bool cardPresent;
  uint8_t lastUID[10];           // Last card UID
  uint8_t uidLength;             // UID length (4, 7, or 10)
  uint32_t lastReadTime;
  
  // Statistics
  uint32_t successfulReads;
  uint32_t failedReads;
  uint32_t timeouts;
  uint32_t collisions;
};

#endif // RFID_CONFIG_H