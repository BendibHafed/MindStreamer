#include <Arduino.h>
#include <MindStreamer.h>

// -----------------------------------------------------------------------------
// Hardware mapping
// -----------------------------------------------------------------------------
static constexpr int MS_PIN_DRDY = 12;
static constexpr int MS_PIN_CS   = 15;
static constexpr int MS_PIN_RST  = 13;

// -----------------------------------------------------------------------------
// Test configuration
// -----------------------------------------------------------------------------
static constexpr uint8_t  MS_NUM_CHANNELS   = 8;
static constexpr uint32_t MS_SERIAL_BAUD    = 115200;
static constexpr uint32_t MS_STARTUP_DELAY  = 1500;

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

void msPrintGainResult(const __FlashStringHelper* label, bool ok) {
    Serial.print(label);
    Serial.print(F(" -> "));
    Serial.println(ok ? F("OK") : F("FAIL"));

    if (!ok) {
        msPrintLastError(F("LAST_ERROR="));
    }
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
// Gain tests
// -----------------------------------------------------------------------------
void msTestSingleChannelGain(uint8_t channel,
                             PGAGain gain,
                             const __FlashStringHelper* gainName) {
    msPrintSeparator();
    Serial.print(F("TEST SINGLE CHANNEL GAIN: CH"));
    Serial.print(channel);
    Serial.print(F(" -> "));
    Serial.println(gainName);

    const bool ok = msEeg.setGain(gain, channel);
    msPrintGainResult(F("setGain(single-channel)"), ok);

    msDumpRegisters(F("REGISTER DUMP"));
}

void msTestAllChannelGain(PGAGain gain, const __FlashStringHelper* gainName) {
    msPrintSeparator();
    Serial.print(F("TEST ALL CHANNEL GAIN -> "));
    Serial.println(gainName);

    const bool ok = msEeg.setGain(gain, 0);
    msPrintGainResult(F("setGain(all-channels)"), ok);

    msDumpRegisters(F("REGISTER DUMP"));
}

void msTestInvalidChannelGain() {
    msPrintSeparator();
    Serial.println(F("NEGATIVE TEST: invalid channel number"));

    const bool ok = msEeg.setGain(PGA_GAIN_24, 9);
    msPrintGainResult(F("setGain(PGA_GAIN_24, 9)"), ok);
}

void msTestGainWhileStreaming() {
    msPrintSeparator();
    Serial.println(F("NEGATIVE TEST: setGain() while streaming"));

    bool ok = msEeg.startStreaming();
    Serial.print(F("startStreaming() -> "));
    Serial.println(ok ? F("OK") : F("FAIL"));
    if (!ok) {
        msPrintLastError(F("LAST_ERROR="));
    }

    ok = msEeg.setGain(PGA_GAIN_12, 1);
    Serial.print(F("setGain(PGA_GAIN_12, 1) while streaming -> "));
    Serial.println(ok ? F("OK") : F("FAIL"));
    if (!ok) {
        msPrintLastError(F("LAST_ERROR="));
    }

    ok = msEeg.stopStreaming();
    Serial.print(F("stopStreaming() -> "));
    Serial.println(ok ? F("OK") : F("FAIL"));
    if (!ok) {
        msPrintLastError(F("LAST_ERROR="));
    }

    msDumpRegisters(F("REGISTER DUMP AFTER STREAMING NEGATIVE TEST"));
}

// -----------------------------------------------------------------------------
// Arduino entry points
// -----------------------------------------------------------------------------
void setup() {
    Serial.begin(MS_SERIAL_BAUD);
    delay(MS_STARTUP_DELAY);

    msPrintSeparator();
    Serial.println(F("MindStreamer setGain() Robustness Test"));

    msConfigureLibrary();

    if (!msBeginAndReport()) {
        while (true) {
            delay(1000);
        }
    }

    // Single-channel checks: only target channel should change.
    msTestSingleChannelGain(1, PGA_GAIN_1,  F("PGA_GAIN_1"));
    msTestSingleChannelGain(1, PGA_GAIN_2,  F("PGA_GAIN_2"));
    msTestSingleChannelGain(1, PGA_GAIN_4,  F("PGA_GAIN_4"));
    msTestSingleChannelGain(1, PGA_GAIN_6,  F("PGA_GAIN_6"));
    msTestSingleChannelGain(1, PGA_GAIN_8,  F("PGA_GAIN_8"));
    msTestSingleChannelGain(1, PGA_GAIN_12, F("PGA_GAIN_12"));
    msTestSingleChannelGain(1, PGA_GAIN_24, F("PGA_GAIN_24"));

    // Cross-check another channel.
    msTestSingleChannelGain(4, PGA_GAIN_6,  F("PGA_GAIN_6"));
    msTestSingleChannelGain(4, PGA_GAIN_24, F("PGA_GAIN_24"));

    // All-channel checks: every CHnSET should update together.
    msTestAllChannelGain(PGA_GAIN_1,  F("PGA_GAIN_1"));
    msTestAllChannelGain(PGA_GAIN_12, F("PGA_GAIN_12"));
    msTestAllChannelGain(PGA_GAIN_24, F("PGA_GAIN_24"));

    // Negative-path tests
    msTestInvalidChannelGain();
    msTestGainWhileStreaming();

    msPrintSeparator();
    Serial.println(F("TEST COMPLETE"));
}

void loop() {
    // Nothing to do.
}