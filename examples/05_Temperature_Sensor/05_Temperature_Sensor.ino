#include <Arduino.h>
#include <MindStreamer.h>

// -----------------------------------------------------------------------------
// MindStreamer Example 05
// Temperature Sensor
//
// What it does:
// - Initializes MindStreamer with the validated operating point
// - Periodically reads the ADS1299 internal temperature sensor
// - Prints temperature in degrees Celsius once per second
//
// Important notes:
// - The ADS1299 has one internal temperature sensor source
// - Any channel can be used as the temporary mux path for this measurement
// - The reported value reflects chip/package temperature, not exact ambient air
// -----------------------------------------------------------------------------

static constexpr int PIN_DRDY = 12;
static constexpr int PIN_CS   = 15;
static constexpr int PIN_RST  = 13;

static constexpr uint8_t  NUM_CHANNELS        = 8;
static constexpr uint8_t  TEMP_CHANNEL        = 1;
static constexpr uint32_t SERIAL_BAUD         = 115200;
static constexpr uint32_t MEASURE_INTERVAL_MS = 1000;

MindStreamer eeg(PIN_DRDY, PIN_CS, PIN_RST);
MindStreamerConfig config;

uint32_t lastMeasureMs = 0;

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

    Serial.println("timestamp_ms,temperature_c");
}

void loop() {
    const uint32_t now = millis();
    if ((now - lastMeasureMs) < MEASURE_INTERVAL_MS) {
        return;
    }
    lastMeasureMs = now;

    float temperatureC = 0.0f;
    if (!eeg.readTemperature(TEMP_CHANNEL, temperatureC)) {
        Serial.print("TEMP_READ_FAIL,");
        Serial.println((int)eeg.getLastError());
        return;
    }

    Serial.print(now);
    Serial.print(',');
    Serial.println(temperatureC, 2);
}