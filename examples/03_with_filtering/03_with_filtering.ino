/**
 * @file 03_with_filtering.ino
 * @brief Demonstrates optional onboard DSP filtering (requires FilterBank)
 * @author Dr. Hafed-Eddine Bendib
 * @version 1.0.0
 * 
 * Note: Filtering is optional and increases CPU load.
 * Enable only if needed.
 */

#include <MindStreamer.h>
#include "dsp/FilterDesign.h"

using namespace MindStreamer::DSP;

MindStreamer mind(12, 15, 13);
FilterBank filterBank(8);

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("=== MindStreamer with Onboard Filtering ===");
    
    MindStreamerConfig config;
    config.num_channels = 8;
    config.data_rate = DR_250;
    config.output_mode = OUTPUT_TEXT;
    config.enable_drl = true;
    config.enable_filter = true;   // Enable filter bank
    
    if (!mind.begin(config)) {
        Serial.println("Failed to initialize!");
        while (1);
    }
    
    // Create standard EEG filter (0.5-40 Hz bandpass + 50 Hz notch)
    FilterCascade eegFilter = EEGDesign::standardEEGFilter(250, true);
    filterBank.setFilter(eegFilter);
    
    Serial.println("Filter bank configured. Streaming filtered data...");
    mind.startStreaming();
}

void loop() {
    if (mind.dataAvailable()) {
        int32_t raw[8];
        mind.readAllChannels(raw, 8);
        
        // Convert to float for filtering
        float floatSamples[8];
        for (int i = 0; i < 8; i++) floatSamples[i] = (float)raw[i];
        
        float filtered[8];
        filterBank.process(floatSamples, filtered);
        
        // Output filtered data as CSV
        static uint32_t sampleCount = 0;
        Serial.print(sampleCount++);
        for (int i = 0; i < 8; i++) {
            Serial.print(",");
            Serial.print((int32_t)filtered[i]);
        }
        Serial.println();
    }
}