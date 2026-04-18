#include <Arduino.h>
#include <MindStreamer.h>

// -----------------------------------------------------------------------------
// Sketch 08 - MindStreamer Output Mode Validation (Single-Mode Template)
// Change TEST_OUTPUT_MODE and re-upload once per mode:
//   OUTPUT_DEBUG
//   OUTPUT_TEXT
//   OUTPUT_BINARY
//   OUTPUT_OPENEEG
// -----------------------------------------------------------------------------

static constexpr int MS_PIN_DRDY = 12;
static constexpr int MS_PIN_CS   = 15;
static constexpr int MS_PIN_RST  = 13;

static constexpr uint8_t  MS_NUM_CHANNELS       = 8;
static constexpr uint32_t MS_SERIAL_BAUD        = 115200; 
static constexpr uint32_t MS_STARTUP_DELAY_MS   = 1500;
static constexpr uint32_t MS_STREAM_DURATION_MS = 500;      // keep short

// ------------------------- SELECT ONE MODE HERE -------------------------------
// #define TEST_OUTPUT_MODE OUTPUT_DEBUG
#define TEST_OUTPUT_MODE OUTPUT_TEXT
// #define TEST_OUTPUT_MODE OUTPUT_BINARY
// #define TEST_OUTPUT_MODE OUTPUT_OPENEEG
// -----------------------------------------------------------------------------

MindStreamer msEeg(MS_PIN_DRDY, MS_PIN_CS, MS_PIN_RST);
MindStreamerConfig msConfig;

void msPrintSeparator() {
    Serial.println(F("============================================================"));
}

void msPrintLastError(const __FlashStringHelper* prefix) {
    Serial.print(prefix);
    Serial.println((int)msEeg.getLastError());
}

void msPrintBoolResult(const __FlashStringHelper* label, bool ok) {
    Serial.print(label);
    Serial.print(F(" -> "));
    Serial.println(ok ? F("OK") : F("FAIL"));
    if (!ok) {
        msPrintLastError(F("LAST_ERROR="));
    }
}

const __FlashStringHelper* msModeName() {
#if TEST_OUTPUT_MODE == OUTPUT_DEBUG
    return F("OUTPUT_DEBUG");
#elif TEST_OUTPUT_MODE == OUTPUT_TEXT
    return F("OUTPUT_TEXT");
#elif TEST_OUTPUT_MODE == OUTPUT_BINARY
    return F("OUTPUT_BINARY");
#elif TEST_OUTPUT_MODE == OUTPUT_OPENEEG
    return F("OUTPUT_OPENEEG");
#else
    return F("UNKNOWN_MODE");
#endif
}

void msConfigureLibrary() {
    msConfig.num_channels     = MS_NUM_CHANNELS;
    msConfig.data_rate        = DR_250;
    msConfig.pga_gain         = PGA_GAIN_24;
    msConfig.output_mode      = TEST_OUTPUT_MODE;
    msConfig.enable_drl       = false;
    msConfig.drl_internal_ref = false;
    msConfig.enable_filter    = false;
    msConfig.daisy_chain      = false;
    msConfig.num_modules      = 1;
    msConfig.spi_clock_hz     = 500000;
    msConfig.drdy_timeout_ms  = 100;
}

bool msBeginAndReport() {
    msPrintSeparator();
    Serial.print(F("BEGIN WITH MODE: "));
    Serial.println(msModeName());

    if (!msEeg.begin(msConfig)) {
        msPrintBoolResult(F("begin(...)"), false);
        return false;
    }

    msPrintBoolResult(F("begin(...)"), true);

    Serial.print(F("ID,0x"));
    Serial.println(msEeg.getDeviceID(), HEX);

    return true;
}

void msPrintModeExpectation() {
    msPrintSeparator();

#if TEST_OUTPUT_MODE == OUTPUT_DEBUG
    Serial.println(F("EXPECTATION: human-readable debug sample blocks"));
#elif TEST_OUTPUT_MODE == OUTPUT_TEXT
    Serial.println(F("EXPECTATION: CSV header then CSV sample rows"));
#elif TEST_OUTPUT_MODE == OUTPUT_BINARY
    Serial.println(F("EXPECTATION: raw binary packets framed with 0xBB ... 0xEE"));
    Serial.println(F("NOTE: use a raw terminal / hex logger, not Serial Monitor text view"));
#elif TEST_OUTPUT_MODE == OUTPUT_OPENEEG
    Serial.println(F("EXPECTATION: 17-byte OpenEEG/BrainBay packets"));
    Serial.println(F("NOTE: OpenEEG mode carries at most 6 channels"));
    Serial.println(F("NOTE: use a raw terminal / hex logger, not Serial Monitor text view"));
#endif
}

void msRunStreamWindow() {
    msPrintSeparator();
    Serial.println(F("STREAM WINDOW START"));

    bool ok = msEeg.startStreaming();
    msPrintBoolResult(F("startStreaming()"), ok);
    if (!ok) {
        return;
    }

    uint32_t success_count = 0;
    uint32_t fail_count = 0;

    const uint32_t t0 = millis();
    while ((millis() - t0) < MS_STREAM_DURATION_MS) {
        if (msEeg.dataAvailable()) {
            if (msEeg.readAndStream()) {
                ++success_count;
            } else {
                ++fail_count;
            }
        }
    }

    // Make text output finish before printing summary lines.
    Serial.flush();

    Serial.println();
    Serial.print(F("STREAMED FRAMES = "));
    Serial.println(success_count);
    Serial.print(F("FAILED READS   = "));
    Serial.println(fail_count);

    ok = msEeg.stopStreaming();
    msPrintBoolResult(F("stopStreaming()"), ok);

    Serial.flush();
    delay(200);
}

void setup() {
    Serial.begin(MS_SERIAL_BAUD);
    delay(MS_STARTUP_DELAY_MS);

    msPrintSeparator();
    Serial.println(F("Sketch 08 - MindStreamer Output Mode Validation (Single-Mode)"));

    msConfigureLibrary();

    if (!msBeginAndReport()) {
        while (true) {
            delay(1000);
        }
    }

    msPrintModeExpectation();
    msRunStreamWindow();

    msPrintSeparator();
    Serial.println(F("TEST COMPLETE"));
}

void loop() {
}