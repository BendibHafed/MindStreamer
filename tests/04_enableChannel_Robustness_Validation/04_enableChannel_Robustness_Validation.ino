#include <Arduino.h>
#include <MindStreamer.h>

// -----------------------------------------------------------------------------
// Sketch 04 - MindStreamer enableChannel() Robustness Validation
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

void msPrintBoolResult(const __FlashStringHelper* label, bool ok) {
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
// Tests
// -----------------------------------------------------------------------------
void msTestDisableChannel(uint8_t channel) {
    msPrintSeparator();
    Serial.print(F("TEST DISABLE CHANNEL: CH"));
    Serial.println(channel);

    const bool ok = msEeg.enableChannel(channel, false);
    msPrintBoolResult(F("enableChannel(channel, false)"), ok);

    msDumpRegisters(F("REGISTER DUMP"));
}

void msTestEnableChannel(uint8_t channel) {
    msPrintSeparator();
    Serial.print(F("TEST ENABLE CHANNEL: CH"));
    Serial.println(channel);

    const bool ok = msEeg.enableChannel(channel, true);
    msPrintBoolResult(F("enableChannel(channel, true)"), ok);

    msDumpRegisters(F("REGISTER DUMP"));
}

void msTestDisableEnableSequence(uint8_t channel) {
    msPrintSeparator();
    Serial.print(F("TEST DISABLE/ENABLE SEQUENCE: CH"));
    Serial.println(channel);

    bool ok = msEeg.enableChannel(channel, false);
    msPrintBoolResult(F("disable"), ok);

    ok = msEeg.enableChannel(channel, true);
    msPrintBoolResult(F("enable"), ok);

    msDumpRegisters(F("REGISTER DUMP AFTER DISABLE/ENABLE"));
}

void msTestEnableChannelWithCustomGain(uint8_t channel, PGAGain gain, const __FlashStringHelper* gainName) {
    msPrintSeparator();
    Serial.print(F("TEST GAIN PRESERVATION ON CH"));
    Serial.print(channel);
    Serial.print(F(" WITH "));
    Serial.println(gainName);

    bool ok = msEeg.setGain(gain, channel);
    msPrintBoolResult(F("setGain(channel-specific)"), ok);

    ok = msEeg.enableChannel(channel, false);
    msPrintBoolResult(F("enableChannel(channel, false)"), ok);

    ok = msEeg.enableChannel(channel, true);
    msPrintBoolResult(F("enableChannel(channel, true)"), ok);

    msDumpRegisters(F("REGISTER DUMP AFTER GAIN PRESERVATION TEST"));
}

void msTestInvalidChannelNumbers() {
    msPrintSeparator();
    Serial.println(F("NEGATIVE TEST: invalid channel numbers"));

    bool ok = msEeg.enableChannel(0, false);
    Serial.print(F("enableChannel(0, false) -> "));
    Serial.println(ok ? F("OK") : F("FAIL"));
    if (!ok) {
        msPrintLastError(F("LAST_ERROR="));
    }

    ok = msEeg.enableChannel(9, false);
    Serial.print(F("enableChannel(9, false) -> "));
    Serial.println(ok ? F("OK") : F("FAIL"));
    if (!ok) {
        msPrintLastError(F("LAST_ERROR="));
    }

    msDumpRegisters(F("REGISTER DUMP AFTER INVALID CHANNEL TEST"));
}

void msTestEnableChannelWhileStreaming() {
    msPrintSeparator();
    Serial.println(F("NEGATIVE TEST: enableChannel() while streaming"));

    bool ok = msEeg.startStreaming();
    Serial.print(F("startStreaming() -> "));
    Serial.println(ok ? F("OK") : F("FAIL"));
    if (!ok) {
        msPrintLastError(F("LAST_ERROR="));
    }

    ok = msEeg.enableChannel(1, false);
    Serial.print(F("enableChannel(1, false) while streaming -> "));
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
    Serial.println(F("Sketch 04 - MindStreamer enableChannel() Robustness Validation"));

    msConfigureLibrary();

    if (!msBeginAndReport()) {
        while (true) {
            delay(1000);
        }
    }

    // Basic single-channel disable/enable checks.
    msTestDisableChannel(1);
    msTestEnableChannel(1);

    msTestDisableChannel(4);
    msTestEnableChannel(4);

    // Disable/enable sequence on another channel.
    msTestDisableEnableSequence(8);

    // Gain preservation checks:
    // Channel gain should remain unchanged except for the PD bit toggle.
    msTestEnableChannelWithCustomGain(2, PGA_GAIN_6,  F("PGA_GAIN_6"));
    msTestEnableChannelWithCustomGain(5, PGA_GAIN_12, F("PGA_GAIN_12"));

    // Negative-path tests.
    msTestInvalidChannelNumbers();
    msTestEnableChannelWhileStreaming();

    msPrintSeparator();
    Serial.println(F("TEST COMPLETE"));
}

void loop() {
    // Nothing to do.
}