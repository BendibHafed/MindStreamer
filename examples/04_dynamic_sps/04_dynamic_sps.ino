/**
 * @file 04_dynamic_sps.ino
 * @brief Demonstrates changing sampling rate at runtime
 * @author Dr. Hafed-Eddine Bendib
 * @version 1.0.0
 * 
 * Commands:
 *   '1' - 250 SPS
 *   '2' - 500 SPS
 *   '3' - 1000 SPS
 *   '4' - 2000 SPS
 *   '5' - 4000 SPS
 *   '6' - 8000 SPS
 *   '7' - 16000 SPS
 *   's' - Show statistics
 */

#include <MindStreamer.h>

MindStreamer mind(12, 15, 13);

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("=== MindStreamer Dynamic SPS Demo ===");
    Serial.println("Commands: 1(250) 2(500) 3(1k) 4(2k) 5(4k) 6(8k) 7(16k) s(stats)");
    
    MindStreamerConfig config;
    config.num_channels = 8;
    config.data_rate = DR_250;
    config.output_mode = OUTPUT_TEXT;
    config.enable_drl = true;
    
    if (!mind.begin(config)) {
        Serial.println("Failed to initialize!");
        while (1);
    }
    
    mind.startStreaming();
    Serial.println("Streaming at 250 SPS");
}

void loop() {
    if (Serial.available()) {
        char cmd = Serial.read();
        DataRate newRate = DR_250;
        switch (cmd) {
            case '1': newRate = DR_250; break;
            case '2': newRate = DR_500; break;
            case '3': newRate = DR_1K; break;
            case '4': newRate = DR_2K; break;
            case '5': newRate = DR_4K; break;
            case '6': newRate = DR_8K; break;
            case '7': newRate = DR_16K; break;
            case 's': {
                auto stats = mind.getStats();
                Serial.print("Actual SPS: "); Serial.print(stats.actual_sps);
                Serial.print(", Total: "); Serial.print(stats.total_samples);
                Serial.print(", Dropped: "); Serial.println(stats.dropped_samples);
                break;
            }
        }
        if (cmd >= '1' && cmd <= '7') {
            mind.stopStreaming();
            if (mind.setDataRate(newRate)) {
                Serial.print("Data rate changed to "); Serial.print(newRate); Serial.println(" SPS");
            } else {
                Serial.println("Failed to change data rate");
            }
            mind.startStreaming();
        }
    }
    
    if (mind.dataAvailable()) {
        mind.readAndStream();
    }
}