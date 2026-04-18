#include <Arduino.h>
#include <MindStreamer.h>

// -----------------------------------------------------------------------------
// MindStreamer Example 02
// Raw Read All Channels
//
// What it does:
// - Initializes MindStreamer at the validated operating point
// - Starts streaming
// - Reads all channels into a raw int32_t buffer
// - Prints one CSV line per sample
//
// Notes:
// - Values are raw ADS1299 signed 24-bit samples sign-extended to int32_t
// - This example is useful when you want to process data yourself instead of
//   using readAndStream() with a built-in output formatter
// -----------------------------------------------------------------------------

static constexpr int PIN_DRDY = 12;
static constexpr int PIN_CS   = 15;
static constexpr int PIN_RST  = 13;

static constexpr uint8_t  NUM_CHANNELS = 8;
static constexpr uint32_t SERIAL_BAUD  = 115200;

MindStreamer eeg(PIN_DRDY, PIN_CS, PIN_RST);
MindStreamerConfig config;

int32_t samples[NUM_CHANNELS];
uint32_t sampleIndex = 0;

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(1500);

    config.num_channels     = NUM_CHANNELS;
    config.data_rate        = DR_250;
    config.pga_gain         = PGA_GAIN_24;
    config.output_mode      = OUTPUT_BINARY;   // formatter unused in this example
    config.enable_filter    = false;
    config.enable_drl       = false;
    config.drl_internal_ref = false;
    config.daisy_chain      = false;
    config.num_modules      = 1;
    config.spi_clock_hz     = 500000;
    config.drdy_timeout_ms  = 100;

    if (!eeg.begin(config)) {
        Serial.print("BEGIN_FAIL,");
        Serial.println((int)eeg.getLastError());
        while (true) {
            delay(1000);
        }
    }

    Serial.print("ID,0x");
    Serial.println(eeg.getDeviceID(), HEX);

    if (!eeg.startStreaming()) {
        Serial.print("START_FAIL,");
        Serial.println((int)eeg.getLastError());
        while (true) {
            delay(1000);
        }
    }

    Serial.println("sample,ch1,ch2,ch3,ch4,ch5,ch6,ch7,ch8");
}

void loop() {
    if (!eeg.dataAvailable()) {
        return;
    }

    if (!eeg.readAllChannels(samples, NUM_CHANNELS)) {
        Serial.print("READ_FAIL,");
        Serial.println((int)eeg.getLastError());
        return;
    }

    Serial.print(sampleIndex++);
    for (uint8_t ch = 0; ch < NUM_CHANNELS; ++ch) {
        Serial.print(',');
        Serial.print(samples[ch]);
    }
    Serial.println();
}