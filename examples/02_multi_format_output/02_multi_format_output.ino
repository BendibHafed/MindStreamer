/**
 * @file 02_multi_format_output.ino
 * @brief Demonstrates switching output formats at runtime
 * @author Dr. Hafed-Eddine Bendib
 * @version 1.0.0
 * 
 * Commands via Serial Monitor:
 *   'd' - DEBUG mode
 *   't' - TEXT mode (CSV)
 *   'b' - BINARY mode
 *   'o' - OPENEEG mode (BrainBay P2)
 *   's' - Stop streaming
 *   'r' - Resume streaming
 */

#include <MindStreamer.h>

MindStreamer mind(12, 15, 13);
OutputMode currentMode = OUTPUT_BINARY;

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("=== MindStreamer Multi-Format Demo ===");
    Serial.println("Commands: d(debug) t(text) b(binary) o(openEEG) s(stop) r(resume)");
    
    MindStreamerConfig config;
    config.num_channels = 8;
    config.data_rate = DR_250;
    config.pga_gain = PGA_GAIN_6;
    config.output_mode = OUTPUT_BINARY;
    config.enable_drl = true;
    
    if (!mind.begin(config)) {
        Serial.println("Failed to initialize!");
        while (1);
    }
    
    Serial.println("Ready. Starting in BINARY mode...");
    mind.startStreaming();
}

void loop() {
    if (Serial.available()) {
        char cmd = Serial.read();
        switch (cmd) {
            case 'd':
                mind.stopStreaming();
                mind.setOutputMode(OUTPUT_DEBUG);
                mind.startStreaming();
                Serial.println("Switched to DEBUG mode");
                break;
            case 't':
                mind.stopStreaming();
                mind.setOutputMode(OUTPUT_TEXT);
                mind.startStreaming();
                Serial.println("Switched to TEXT mode");
                break;
            case 'b':
                mind.stopStreaming();
                mind.setOutputMode(OUTPUT_BINARY);
                mind.startStreaming();
                Serial.println("Switched to BINARY mode");
                break;
            case 'o':
                mind.stopStreaming();
                mind.setOutputMode(OUTPUT_OPENEEG);
                mind.startStreaming();
                Serial.println("Switched to OPENEEG mode");
                break;
            case 's':
                mind.stopStreaming();
                Serial.println("Streaming stopped");
                break;
            case 'r':
                mind.startStreaming();
                Serial.println("Streaming resumed");
                break;
        }
    }
    
    if (mind.dataAvailable()) {
        mind.readAndStream();
    }
}