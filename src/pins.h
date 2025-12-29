#pragma once

// --- LOKATA HARDWARE MAP (T-Beam Supreme V3.0) ---

// I2C Bus 0: Sensors (Mag, Pressure, RTC) & OLED
// The Magnetometer is on the standard I2C bus.
#define SENSOR_SDA          17
#define SENSOR_SCL          18

// I2C Bus 1: Power Management (AXP2101)
// The PMU is on a separate, private I2C bus.
#define PMU_SDA             42
#define PMU_SCL             41
#define PMU_IRQ             40

// SPI Bus: IMU (QMI8658) & SD Card
// The IMU is connected via SPI, NOT I2C.
#define SPI_MOSI            35
#define SPI_MISO            37
#define SPI_SCK             36
#define IMU_CS              34  // Chip Select for IMU
#define SD_CS               47  // Chip Select for SD Card

// GNSS (u-blox MAX-M10S)
// Mapped from IO09/IO08.
// Note: In PlatformIO, Serial pins are usually defined as relative to the ESP32.
#define GPS_RX_PIN          9  // ESP32 RX pin
#define GPS_TX_PIN          8  // ESP32 TX pin
#define GPS_PPS_PIN         6

// LoRa Radio (SX1262) - Separate SPI Bus
#define RADIO_SCLK_PIN      12
#define RADIO_MISO_PIN      13
#define RADIO_MOSI_PIN      11
#define RADIO_CS_PIN        10
#define RADIO_RST_PIN       5
#define RADIO_DIO1_PIN      1
#define RADIO_BUSY_PIN      4