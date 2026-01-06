#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <XPowersLib.h>               
#include <TinyGPS++.h>
#include <SensorQMI8658.hpp>          
#include <SensorQMC6310.hpp>          
#include <U8g2lib.h>                  
#include "pins.h"

// --- Configuration ---
#define TELEMETRY_BAUD      921600   // Fast enough for 100Hz text
#define GPS_BAUD            9600
#define LOG_INTERVAL_US     10000    // 10,000us = 10ms = 100Hz

// --- Global Objects ---
XPowersAXP2101 PMU;
TinyGPSPlus gps;             
SensorQMI8658 imu;
SensorQMC6310 mag;

TwoWire PMU_I2C = TwoWire(1);      
SPIClass IMU_SPI = SPIClass(HSPI); 
U8G2_SH1106_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, SENSOR_SCL, SENSOR_SDA, U8X8_PIN_NONE);

uint32_t bootTime = 0;

// Variables to hold sensor data between reads
float mx = 0, my = 0, mz = 0;

void setup() {
    // 1. Serial & Power Init
    Serial.begin(TELEMETRY_BAUD);
    delay(500);
    
    PMU_I2C.begin(PMU_SDA, PMU_SCL, 400000);
    if (!PMU.begin(PMU_I2C, AXP2101_SLAVE_ADDRESS, PMU_SDA, PMU_SCL)) {
        Serial.println("PMU Failed");
        while(1); 
    }
    
    // Enable Power Rails
    PMU.setALDO1Voltage(3300); PMU.enableALDO1(); 
    PMU.setALDO4Voltage(3300); PMU.enableALDO4();
    delay(200); 

    // 2. Display Init
    u8g2.begin();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 15, "Status: 100Hz Log");
    u8g2.sendBuffer();

    // 3. Magnetometer Init (200Hz)
    Wire.begin(SENSOR_SDA, SENSOR_SCL); 
    if(mag.begin(Wire, QMC6310_SLAVE_ADDRESS, SENSOR_SDA, SENSOR_SCL)) {
        mag.configMagnetometer(SensorQMC6310::MODE_CONTINUOUS, SensorQMC6310::RANGE_8G, SensorQMC6310::DATARATE_200HZ, SensorQMC6310::OSR_8, SensorQMC6310::DSR_8);
    }
    
    // 4. IMU Init (FIXED ODR CONSTANTS)
    IMU_SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, IMU_CS);
    if(imu.begin(IMU_CS, SPI_MOSI, SPI_MISO, SPI_SCK, IMU_SPI)) {
        
        // ACC: 125Hz is valid
        imu.configAccelerometer(SensorQMI8658::ACC_RANGE_4G, SensorQMI8658::ACC_ODR_125Hz, SensorQMI8658::LPF_MODE_3);
        
        // GYR: 125Hz does not exist. Use 112.1Hz instead.
        imu.configGyroscope(SensorQMI8658::GYR_RANGE_1024DPS, SensorQMI8658::GYR_ODR_112_1Hz, SensorQMI8658::LPF_MODE_3);
        
        imu.enableGyroscope(); imu.enableAccelerometer();
    }

    // 5. GPS Init
    Serial1.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    
    bootTime = millis();
}

void loop() {
    static uint32_t lastLogTime = 0;
    static uint32_t lastDisplayUpdate = 0;

    // --- CRITICAL: Always process GPS bytes ---
    while (Serial1.available() > 0) {
        gps.encode(Serial1.read());
    }

    // --- 100Hz High Priority Logging Loop ---
    if (micros() - lastLogTime >= LOG_INTERVAL_US) { 
        lastLogTime = micros();
        
        // 1. Get IMU Data (Accelerometer & Gyro)
        // IMU has a dedicated check, so we keep it to ensure data integrity
        IMUdata acc = {0,0,0}, gyr = {0,0,0};
        if (imu.getDataReady()) { 
            imu.getAccelerometer(acc.x, acc.y, acc.z); 
            imu.getGyroscope(gyr.x, gyr.y, gyr.z); 
        }

        // 2. Get Mag Data (FIXED)
        // Since Mag is 200Hz and loop is 100Hz, data is always fresh.
        // We just read it directly.
        mag.readData();
        mx = mag.getX(); 
        my = mag.getY(); 
        mz = mag.getZ();

        // 3. Get GPS Snapshot
        long lat = 0, lon = 0;
        float hdop = 99.9;
        int siv = 0;
        
        if (gps.location.isValid()) {
            lat = (long)(gps.location.lat() * 10000000);
            lon = (long)(gps.location.lng() * 10000000);
            siv = gps.satellites.value();
            hdop = gps.hdop.isValid() ? gps.hdop.hdop() : 99.9;
        }

        // 4. Output CSV Line
        Serial.printf("DATA,%lu,%d,%ld,%ld,%.2f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.2f,%.2f,%.2f\n", 
            micros(), siv, lat, lon, hdop, 
            acc.x, acc.y, acc.z, gyr.x, gyr.y, gyr.z, 
            mx, my, mz);
    }

    // --- Low Priority UI Update (2Hz) ---
    if (millis() - lastDisplayUpdate > 500) {
        lastDisplayUpdate = millis();
        
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x12_tr); 
        
        if (gps.location.isValid()) u8g2.drawStr(0, 10, "GPS: LOCKED");
        else u8g2.drawStr(0, 10, "GPS: SEARCH");
        
        char buf[32];
        sprintf(buf, "Sats: %d", gps.satellites.value());
        u8g2.drawStr(0, 25, buf);
        
        u8g2.sendBuffer();
    }
}