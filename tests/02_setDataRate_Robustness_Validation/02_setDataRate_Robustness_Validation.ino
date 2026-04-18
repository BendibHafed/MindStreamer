#include <Arduino.h>
#include <MindStreamer.h>

static constexpr int PIN_DRDY = 12;
static constexpr int PIN_CS   = 15;
static constexpr int PIN_RST  = 13;
static constexpr uint8_t NUM_CHANNELS = 8;

MindStreamer eeg(PIN_DRDY, PIN_CS, PIN_RST);
MindStreamerConfig config;

void setup() {
    Serial.begin(115200);
    delay(1500);

    config.num_channels             = NUM_CHANNELS;
    config.data_rate                = DR_250;
    config.pga_gain                 = PGA_GAIN_24;
    config.output_mode              = OUTPUT_DEBUG;
    config.use_internal_reference   = true;
    config.enable_drl               = false;
    config.drl_internal_ref         = false;
    config.enable_filter            = false;
    config.daisy_chain              = false;
    config.num_modules              = 1;
    config.spi_clock_hz             = 500000;
    config.drdy_timeout_ms          = 100;

    if (!eeg.begin(config)) {
        Serial.print("BEGIN_FAIL,");
        Serial.println((int)eeg.getLastError());
        while (true) delay(1000);
    }

    Serial.print("ID,0x");
    Serial.println(eeg.getDeviceID(), HEX);

    Serial.println("=== REGISTER DUMP AFTER BEGIN ===");
    eeg.printRegisterMap();
}

void loop() {}