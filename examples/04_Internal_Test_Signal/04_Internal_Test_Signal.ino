#include <Arduino.h>
#include <MindStreamer.h>

// -----------------------------------------------------------------------------
// MindStreamer Example 04
// Internal Test Signal
//
// What it does:
// - Initializes MindStreamer with the validated operating point
// - Routes the ADS1299 internal test signal to all channels
// - Starts streaming
// - Reads all channels and prints raw values as CSV
//
// Why this is useful:
// - Validates acquisition without electrodes
// - Helps confirm that all channels respond consistently
// - Useful for bring-up, factory checks, and regression testing
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
    config.output_mode      = OUTPUT_BINARY;
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

    for (uint8_t ch = 1; ch <= NUM_CHANNELS; ++ch) {
        if (!eeg.enableTestSignal(ch, PGA_GAIN_24, TEST_AMP_1x, TEST_FREQ_fCLK_21)) {
            Serial.print("ENABLE_TEST_FAIL_CH,");
            Serial.print(ch);
            Serial.print(',');
            Serial.println((int)eeg.getLastError());
            while (true) {
                delay(1000);
            }
        }
    }

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