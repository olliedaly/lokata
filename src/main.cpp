#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <XPowersLib.h>               
#include <TinyGPS++.h>
#include <SensorQMI8658.hpp>          
#include <SensorQMC6310.hpp>          
#include <U8g2lib.h>                  
#include "pins.h"

// --- Objects ---
XPowersAXP2101 PMU;
TinyGPSPlus gps;             
SensorQMI8658 imu;
SensorQMC6310 mag;

TwoWire PMU_I2C = TwoWire(1);      
SPIClass IMU_SPI = SPIClass(HSPI); 

// OLED (Software I2C for stability)
U8G2_SH1106_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, SENSOR_SCL, SENSOR_SDA, U8X8_PIN_NONE);

uint32_t bootTime = 0;

void setup() {
    // Debug Serial
    Serial.begin(115200);
    delay(1000);
    
    // Init Power Management (AXP2101)
    PMU_I2C.begin(PMU_SDA, PMU_SCL, 400000);
    if (!PMU.begin(PMU_I2C, AXP2101_SLAVE_ADDRESS, PMU_SDA, PMU_SCL)) {
         Serial.println("❌ PMU Fail");
         while(1); 
    }

    // Power Button Logic
    
    // Set Shutdown Time (Hold button to turn OFF)
    // 0 = 4s, 1 = 6s, 2 = 8s, 3 = 10s
    PMU.setPowerKeyPressOffTime(0); 

    // Set Startup Time (Hold button to turn ON)
    // 0 = 128ms, 1 = 512ms, 2 = 1s, 3 = 2s
    PMU.setPowerKeyPressOnTime(2);

    // Turn ON the Voltage Rails
    PMU.setALDO1Voltage(3300); PMU.enableALDO1(); // Sensors/OLED
    PMU.setALDO4Voltage(3300); PMU.enableALDO4(); // GPS
    Serial.println("✅ Power Rails Active");
    delay(1000); 

    // Init OLED (Software I2C)
    u8g2.begin();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 15, "LOKATA: FUSION");
    u8g2.sendBuffer();

    // Init Sensors
    // Magnetometer (I2C)
    Wire.begin(SENSOR_SDA, SENSOR_SCL); 
    if(mag.begin(Wire, QMC6310_SLAVE_ADDRESS, SENSOR_SDA, SENSOR_SCL)) {
        mag.configMagnetometer(SensorQMC6310::MODE_CONTINUOUS, SensorQMC6310::RANGE_8G, SensorQMC6310::DATARATE_200HZ, SensorQMC6310::OSR_8, SensorQMC6310::DSR_8);
    }
    
    // IMU (SPI)
    IMU_SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, IMU_CS);
    if(imu.begin(IMU_CS, SPI_MOSI, SPI_MISO, SPI_SCK, IMU_SPI)) {
        imu.configAccelerometer(SensorQMI8658::ACC_RANGE_4G, SensorQMI8658::ACC_ODR_125Hz, SensorQMI8658::LPF_MODE_0);
        imu.configGyroscope(SensorQMI8658::GYR_RANGE_1024DPS, SensorQMI8658::GYR_ODR_112_1Hz, SensorQMI8658::LPF_MODE_0);
        imu.enableGyroscope(); imu.enableAccelerometer();
    }

    // Init GPS (Passive Listener)
    Serial1.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    
    bootTime = millis();
    Serial.println("✅ System Ready");
}

void loop() {
    static uint32_t lastScreenUpdate = 0;
    static uint32_t lastSerial = 0;

    // Parse GPS Data
    while (Serial1.available() > 0) {
        gps.encode(Serial1.read());
    }

    // Get Fusion Metrics
    // If HDOP is 0 (invalid), we default to 99.9 (high uncertainty)
    float hdop = gps.hdop.isValid() ? gps.hdop.hdop() : 99.9;
    
    // Convert HDOP to Estimated Accuracy in Meters (Sigma)
    // Convert HDOP to Estimated Accuracy in Meters (Sigma)
    // Formula: Sigma = HDOP * 2.0m (Standard UERE for u-blox)
    float est_accuracy = hdop * 2.0;

    // Sensors
    IMUdata acc = {0,0,0}, gyr = {0,0,0};
    float mx=0, my=0, mz=0;
    if (imu.getDataReady()) { imu.getAccelerometer(acc.x, acc.y, acc.z); imu.getGyroscope(gyr.x, gyr.y, gyr.z); }
    mag.readData(); mx = mag.getX(); my = mag.getY(); mz = mag.getZ();

    // Data Logging (CSV)
    // Columns: Time, Sats, Lat, Lon, HDOP, AccuracyEst, Ax, Ay, Az, Gx, Gy, Gz, Mx, My, Mz
    if (millis() - lastSerial > 50) { 
        lastSerial = millis();
        long lat = gps.location.isValid() ? (long)(gps.location.lat() * 10000000) : 0;
        long lon = gps.location.isValid() ? (long)(gps.location.lng() * 10000000) : 0;
        int siv = gps.satellites.value();

        Serial.printf("DATA,%lu,%d,%ld,%ld,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n", 
            millis(), siv, lat, lon, hdop, est_accuracy,
            acc.x, acc.y, acc.z, gyr.x, gyr.y, gyr.z, mx, my, mz);
    }

    // OLED Update
    if (millis() - lastScreenUpdate > 500) {
        lastScreenUpdate = millis();
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x12_tr); 

        // Status
        if (gps.location.isValid()) u8g2.drawStr(0, 10, "✅ GPS LOCKED");
        else if (gps.satellites.value() > 0) u8g2.drawStr(0, 10, "📡 ACQUIRING...");
        else u8g2.drawStr(0, 10, "⚠️ NO SATS");

        // Stats
        char buf[30];
        sprintf(buf, "Sats: %d  HDOP: %.1f", gps.satellites.value(), hdop);
        u8g2.drawStr(0, 25, buf);
        
        // ESTIMATED ACCURACY LINE
        if (hdop < 50) {
            sprintf(buf, "Est Err: +/- %.1fm", est_accuracy);
            u8g2.drawStr(0, 38, buf);
        } else {
            u8g2.drawStr(0, 38, "Est Err: HIGH");
        }

        // Location or Timer
        if (gps.location.isValid()) {
            u8g2.setCursor(0, 52); u8g2.print(gps.location.lat(), 5);
        } else {
             sprintf(buf, "Time: %ds", (millis()-bootTime)/1000);
             u8g2.drawStr(0, 52, buf);
        }
        u8g2.sendBuffer();
    }
}