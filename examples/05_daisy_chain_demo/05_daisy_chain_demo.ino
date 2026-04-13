/**
 * @file 05_daisy_chain_demo.ino
 * @brief Template for daisy chain configuration (2 ADS1299 modules, 16 channels)
 * @author Dr. Hafed-Eddine Bendib
 * @version 1.0.0
 * 
 * Hardware: Connect DOUT of first to DIN of second; share SCLK, CS, DRDY, RESET.
 * 
 * NOTE: Daisy chain support requires additional testing.
 */

#include <MindStreamer.h>

MindStreamer mind(12, 15, 13);  // DRDY, CS, RST

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("=== MindStreamer Daisy Chain Demo (16 Channels) ===");
    Serial.println("WARNING: Verify hardware connections!");
    
    MindStreamerConfig config;
    config.num_channels = 16;
    config.daisy_chain = true;
    config.num_modules = 2;
    config.data_rate = DR_250;
    config.pga_gain = PGA_GAIN_6;
    config.output_mode = OUTPUT_TEXT;
    config.enable_drl = true;
    
    if (!mind.begin(config)) {
        Serial.print("Initialization failed! Error: ");
        Serial.println(mind.getLastError());
        while (1);
    }
    
    Serial.println("Daisy chain initialized. Streaming 16 channels...");
    mind.startStreaming();
}

void loop() {
    if (mind.dataAvailable()) {
        int32_t samples[16];
        if (mind.readAllChannels(samples, 16)) {
            static uint32_t sampleCount = 0;
            Serial.print(sampleCount++);
            for (int i = 0; i < 16; i++) {
                Serial.print(",");
                Serial.print(samples[i]);
            }
            Serial.println();
        }
    }
}