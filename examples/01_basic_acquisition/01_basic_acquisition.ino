/**
 * @file 01_basic_acquisition.ino
 * @brief Minimal example: 8-channel EEG acquisition with binary output
 * @author Dr. Hafed-Eddine Bendib
 * @version 1.0.0
 * 
 * Hardware Connections:
 * - DRDY -> GPIO12
 * - CS   -> GPIO15
 * - RST  -> GPIO13
 * - MOSI -> GPIO23
 * - MISO -> GPIO19
 * - SCK  -> GPIO18
 */

#include <MindStreamer.h>

// Create instance with explicit pins (optional, defaults are 12,15,13)
MindStreamer mind(12, 15, 13);

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("=== MindStreamer Basic Acquisition ===");
    Serial.println("Initializing...");
    
    // Configure the system
    MindStreamerConfig config;
    config.num_channels = 8;
    config.data_rate = DR_250;
    config.pga_gain = PGA_GAIN_6;
    config.output_mode = OUTPUT_BINARY;
    config.enable_drl = true;
    
    if (!mind.begin(config)) {
        Serial.print("Failed to initialize! Error: ");
        Serial.println(mind.getLastError());
        while (1) delay(1000);
    }
    
    Serial.println("MindStreamer initialized successfully!");
    Serial.print("Device ID: 0x");
    Serial.println(mind.getDeviceID(), HEX);
    
    mind.startStreaming();
    Serial.println("Streaming EEG data in BINARY mode...");
}

void loop() {
    if (mind.dataAvailable()) {
        mind.readAndStream();
    }
}