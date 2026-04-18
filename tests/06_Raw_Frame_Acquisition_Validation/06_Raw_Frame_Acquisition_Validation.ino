#include <Arduino.h>
#include <MindStreamer.h>

// -----------------------------------------------------------------------------
// Sketch 06 - MindStreamer Raw Frame Acquisition Validation
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Hardware mapping
// -----------------------------------------------------------------------------
static constexpr int MS_PIN_DRDY = 12;
static constexpr int MS_PIN_CS   = 15;
static constexpr int MS_PIN_RST  = 13;

// -----------------------------------------------------------------------------
// Test configuration
// -----------------------------------------------------------------------------
static constexpr uint8_t  MS_NUM_CHANNELS      = 8;
static constexpr uint32_t MS_SERIAL_BAUD       = 115200;
static constexpr uint32_t MS_STARTUP_DELAY_MS  = 1500;
static constexpr uint8_t  MS_FRAME_SIZE_BYTES  = 3 + (3 * MS_NUM_CHANNELS);
static constexpr uint8_t  MS_NUM_TEST_FRAMES   = 5;

// -----------------------------------------------------------------------------
// Global objects
// -----------------------------------------------------------------------------
MindStreamer msEeg(MS_PIN_DRDY, MS_PIN_CS, MS_PIN_RST);
MindStreamerConfig msConfig;

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------
void msPrintSeparator() {
    Serial.println(F("============================================================"));
}

void msPrintLastError(const __FlashStringHelper* prefix) {
    Serial.print(prefix);
    Serial.println((int)msEeg.getLastError());
}

void msDumpRegisters(const __FlashStringHelper* title) {
    msPrintSeparator();
    Serial.println(title);
    msEeg.printRegisterMap();
}

void msPrintBoolResult(const __FlashStringHelper* label, bool ok) {
    Serial.print(label);
    Serial.print(F(" -> "));
    Serial.println(ok ? F("OK") : F("FAIL"));

    if (!ok) {
        msPrintLastError(F("LAST_ERROR="));
    }
}

void msPrintRawFrameHex(const uint8_t* frame, uint8_t length) {
    if (!frame) {
        Serial.println(F("(null frame)"));
        return;
    }

    for (uint8_t i = 0; i < length; ++i) {
        if (frame[i] < 0x10) {
            Serial.print('0');
        }
        Serial.print(frame[i], HEX);

        if (i + 1 < length) {
            Serial.print(' ');
        }
    }
    Serial.println();
}

void msPrintStatusBytes(const uint8_t* frame) {
    if (!frame) {
        Serial.println(F("STATUS: (null)"));
        return;
    }

    Serial.print(F("STATUS BYTES: "));
    if (frame[0] < 0x10) Serial.print('0');
    Serial.print(frame[0], HEX);
    Serial.print(' ');
    if (frame[1] < 0x10) Serial.print('0');
    Serial.print(frame[1], HEX);
    Serial.print(' ');
    if (frame[2] < 0x10) Serial.print('0');
    Serial.println(frame[2], HEX);
}

void msConfigureLibrary() {
    msConfig.num_channels     = MS_NUM_CHANNELS;
    msConfig.data_rate        = DR_250;
    msConfig.pga_gain         = PGA_GAIN_24;
    msConfig.output_mode      = OUTPUT_DEBUG;
    msConfig.enable_drl       = false;
    msConfig.drl_internal_ref = false;
    msConfig.enable_filter    = false;
    msConfig.daisy_chain      = false;
    msConfig.num_modules      = 1;
    msConfig.spi_clock_hz     = 500000;
    msConfig.drdy_timeout_ms  = 100;

    // Uncomment if present in your current config type:
    // msConfig.use_internal_reference = true;
}

bool msBeginAndReport() {
    if (!msEeg.begin(msConfig)) {
        Serial.print(F("BEGIN_FAIL,"));
        Serial.println((int)msEeg.getLastError());
        return false;
    }

    Serial.print(F("ID,0x"));
    Serial.println(msEeg.getDeviceID(), HEX);

    msDumpRegisters(F("BASELINE REGISTER DUMP AFTER BEGIN"));
    return true;
}

// -----------------------------------------------------------------------------
// Tests
// -----------------------------------------------------------------------------
void msTestReadRawFrameWithoutStreaming() {
    msPrintSeparator();
    Serial.println(F("NEGATIVE TEST: debugReadFrameRaw() before startStreaming()"));

    uint8_t frame[MS_FRAME_SIZE_BYTES] = {0};
    const bool ok = msEeg.debugReadFrameRaw(frame, MS_NUM_CHANNELS);

    msPrintBoolResult(F("debugReadFrameRaw(...) before streaming"), ok);
}

void msTestStartStreaming() {
    msPrintSeparator();
    Serial.println(F("TEST startStreaming()"));

    const bool ok = msEeg.startStreaming();
    msPrintBoolResult(F("startStreaming()"), ok);
}

void msTestReadSingleRawFrame() {
    msPrintSeparator();
    Serial.println(F("TEST read single raw frame"));

    uint8_t frame[MS_FRAME_SIZE_BYTES] = {0};
    const bool ok = msEeg.debugReadFrameRaw(frame, MS_NUM_CHANNELS);

    msPrintBoolResult(F("debugReadFrameRaw(...)"), ok);

    if (ok) {
        Serial.print(F("EXPECTED FRAME SIZE = "));
        Serial.println(MS_FRAME_SIZE_BYTES);

        Serial.print(F("RAW FRAME HEX: "));
        msPrintRawFrameHex(frame, MS_FRAME_SIZE_BYTES);

        msPrintStatusBytes(frame);
    }
}

void msTestReadMultipleRawFrames() {
    msPrintSeparator();
    Serial.println(F("TEST read multiple raw frames"));

    for (uint8_t i = 0; i < MS_NUM_TEST_FRAMES; ++i) {
        uint8_t frame[MS_FRAME_SIZE_BYTES] = {0};
        const bool ok = msEeg.debugReadFrameRaw(frame, MS_NUM_CHANNELS);

        Serial.print(F("FRAME "));
        Serial.print(i + 1);
        Serial.print(F(" -> "));
        Serial.println(ok ? F("OK") : F("FAIL"));

        if (!ok) {
            msPrintLastError(F("LAST_ERROR="));
            continue;
        }

        msPrintStatusBytes(frame);

        Serial.print(F("RAW FRAME HEX: "));
        msPrintRawFrameHex(frame, MS_FRAME_SIZE_BYTES);
    }
}

void msTestStopStreaming() {
    msPrintSeparator();
    Serial.println(F("TEST stopStreaming()"));

    const bool ok = msEeg.stopStreaming();
    msPrintBoolResult(F("stopStreaming()"), ok);
}

void msTestReadAfterStopStreaming() {
    msPrintSeparator();
    Serial.println(F("NEGATIVE TEST: debugReadFrameRaw() after stopStreaming()"));

    uint8_t frame[MS_FRAME_SIZE_BYTES] = {0};
    const bool ok = msEeg.debugReadFrameRaw(frame, MS_NUM_CHANNELS);

    msPrintBoolResult(F("debugReadFrameRaw(...) after stop"), ok);
}

void msTestRestartStreamingAndRead() {
    msPrintSeparator();
    Serial.println(F("TEST restart streaming and read one frame"));

    bool ok = msEeg.startStreaming();
    msPrintBoolResult(F("startStreaming()"), ok);

    if (!ok) {
        return;
    }

    uint8_t frame[MS_FRAME_SIZE_BYTES] = {0};
    ok = msEeg.debugReadFrameRaw(frame, MS_NUM_CHANNELS);
    msPrintBoolResult(F("debugReadFrameRaw(...) after restart"), ok);

    if (ok) {
        msPrintStatusBytes(frame);
        Serial.print(F("RAW FRAME HEX: "));
        msPrintRawFrameHex(frame, MS_FRAME_SIZE_BYTES);
    }

    ok = msEeg.stopStreaming();
    msPrintBoolResult(F("stopStreaming()"), ok);
}

// -----------------------------------------------------------------------------
// Arduino entry points
// -----------------------------------------------------------------------------
void setup() {
    Serial.begin(MS_SERIAL_BAUD);
    delay(MS_STARTUP_DELAY_MS);

    msPrintSeparator();
    Serial.println(F("Sketch 06 - MindStreamer Raw Frame Acquisition Validation"));

    msConfigureLibrary();

    if (!msBeginAndReport()) {
        while (true) {
            delay(1000);
        }
    }

    msTestReadRawFrameWithoutStreaming();
    msTestStartStreaming();
    msTestReadSingleRawFrame();
    msTestReadMultipleRawFrames();
    msTestStopStreaming();
    msTestReadAfterStopStreaming();
    msTestRestartStreamingAndRead();

    msPrintSeparator();
    Serial.println(F("TEST COMPLETE"));
}

void loop() {
    // Nothing to do.
}