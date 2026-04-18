#include <Arduino.h>
#include <MindStreamer.h>

// -----------------------------------------------------------------------------
// MindStreamer Example 03
// Binary Stream
//
// What it does:
// - Initializes MindStreamer with the validated operating point
// - Starts continuous acquisition
// - Streams framed binary packets over Serial
//
// Notes:
// - This example is intended for host software, loggers, or custom parsers
// - Serial Monitor text view will show unreadable characters
// - Use a raw serial logger or host-side parser to decode the stream
// -----------------------------------------------------------------------------

static constexpr int PIN_DRDY = 12;
static constexpr int PIN_CS   = 15;
static constexpr int PIN_RST  = 13;

static constexpr uint8_t  NUM_CHANNELS = 8;
static constexpr uint32_t SERIAL_BAUD  = 115200;

MindStreamer eeg(PIN_DRDY, PIN_CS, PIN_RST);
MindStreamerConfig config;

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(1500);

    config.num_channels     = NUM_CHANNELS;
    config.data_rate        = DR_250;
    config.pga_gain         = PGA_GAIN_24;
    config.output_mode      = OUTPUT_BINARY;

    config.enable_filter    = false;
    config.enable_drl       = false;
    config.drl_internal_ref = false;

    config.daisy_chain      = false;
    config.num_modules      = 1;

    // Validated stable SPI setting
    config.spi_clock_hz     = 500000;
    config.drdy_timeout_ms  = 100;

    if (!eeg.begin(config)) {
        Serial.print("BEGIN_FAIL,");
        Serial.println((int)eeg.getLastError());
        while (true) {
            delay(1000);
        }
    }

    if (!eeg.startStreaming()) {
        Serial.print("START_FAIL,");
        Serial.println((int)eeg.getLastError());
        while (true) {
            delay(1000);
        }
    }
}

void loop() {
    if (!eeg.dataAvailable()) {
        return;
    }

    if (!eeg.readAndStream()) {
        Serial.print("READ_FAIL,");
        Serial.println((int)eeg.getLastError());
    }
}