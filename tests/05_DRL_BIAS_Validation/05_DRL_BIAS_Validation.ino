#include <Arduino.h>
#include <MindStreamer.h>

// -----------------------------------------------------------------------------
// Sketch 05 - MindStreamer DRL / BIAS Configuration Validation
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
void msTestEnableDRL(bool enable, const __FlashStringHelper* label) {
    msPrintSeparator();
    Serial.print(F("TEST "));
    Serial.println(label);

    const bool ok = msEeg.enableDRL(enable);
    msPrintBoolResult(F("enableDRL(...)"), ok);

    msDumpRegisters(F("REGISTER DUMP"));
}

void msTestAddChannelToDRL(uint8_t channel) {
    msPrintSeparator();
    Serial.print(F("TEST addChannelToDRL(CH"));
    Serial.print(channel);
    Serial.println(F(")"));

    const bool ok = msEeg.addChannelToDRL(channel);
    msPrintBoolResult(F("addChannelToDRL(...)"), ok);

    msDumpRegisters(F("REGISTER DUMP"));
}

void msTestDRLAccumulation() {
    msPrintSeparator();
    Serial.println(F("TEST DRL CHANNEL ACCUMULATION"));

    bool ok = msEeg.enableDRL(true);
    msPrintBoolResult(F("enableDRL(true)"), ok);

    ok = msEeg.addChannelToDRL(1);
    msPrintBoolResult(F("addChannelToDRL(1)"), ok);

    ok = msEeg.addChannelToDRL(2);
    msPrintBoolResult(F("addChannelToDRL(2)"), ok);

    ok = msEeg.addChannelToDRL(8);
    msPrintBoolResult(F("addChannelToDRL(8)"), ok);

    msDumpRegisters(F("REGISTER DUMP AFTER DRL ACCUMULATION"));
}

void msTestDisableDRLAfterSelection() {
    msPrintSeparator();
    Serial.println(F("TEST DRL DISABLE AFTER CHANNEL SELECTION"));

    bool ok = msEeg.enableDRL(true);
    msPrintBoolResult(F("enableDRL(true)"), ok);

    ok = msEeg.addChannelToDRL(3);
    msPrintBoolResult(F("addChannelToDRL(3)"), ok);

    ok = msEeg.addChannelToDRL(5);
    msPrintBoolResult(F("addChannelToDRL(5)"), ok);

    msDumpRegisters(F("REGISTER DUMP BEFORE DRL DISABLE"));

    ok = msEeg.enableDRL(false);
    msPrintBoolResult(F("enableDRL(false)"), ok);

    msDumpRegisters(F("REGISTER DUMP AFTER DRL DISABLE"));
}

void msTestInvalidDRLChannel() {
    msPrintSeparator();
    Serial.println(F("NEGATIVE TEST: invalid DRL channel"));

    bool ok = msEeg.addChannelToDRL(0);
    Serial.print(F("addChannelToDRL(0) -> "));
    Serial.println(ok ? F("OK") : F("FAIL"));
    if (!ok) {
        msPrintLastError(F("LAST_ERROR="));
    }

    ok = msEeg.addChannelToDRL(9);
    Serial.print(F("addChannelToDRL(9) -> "));
    Serial.println(ok ? F("OK") : F("FAIL"));
    if (!ok) {
        msPrintLastError(F("LAST_ERROR="));
    }

    msDumpRegisters(F("REGISTER DUMP AFTER INVALID DRL CHANNEL TEST"));
}

void msTestDRLWhileStreaming() {
    msPrintSeparator();
    Serial.println(F("NEGATIVE TEST: DRL operations while streaming"));

    bool ok = msEeg.startStreaming();
    Serial.print(F("startStreaming() -> "));
    Serial.println(ok ? F("OK") : F("FAIL"));
    if (!ok) {
        msPrintLastError(F("LAST_ERROR="));
    }

    ok = msEeg.enableDRL(true);
    Serial.print(F("enableDRL(true) while streaming -> "));
    Serial.println(ok ? F("OK") : F("FAIL"));
    if (!ok) {
        msPrintLastError(F("LAST_ERROR="));
    }

    ok = msEeg.addChannelToDRL(1);
    Serial.print(F("addChannelToDRL(1) while streaming -> "));
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
    Serial.println(F("Sketch 05 - MindStreamer DRL / BIAS Configuration Validation"));

    msConfigureLibrary();

    if (!msBeginAndReport()) {
        while (true) {
            delay(1000);
        }
    }

    // Basic DRL on/off checks.
    msTestEnableDRL(true,  F("ENABLE DRL"));
    msTestEnableDRL(false, F("DISABLE DRL"));

    // Add channels one at a time.
    msTestAddChannelToDRL(1);
    msTestAddChannelToDRL(4);
    msTestAddChannelToDRL(8);

    // Accumulation behavior.
    msTestDRLAccumulation();

    // Disable after channels have been selected.
    msTestDisableDRLAfterSelection();

    // Negative tests.
    msTestInvalidDRLChannel();
    msTestDRLWhileStreaming();

    msPrintSeparator();
    Serial.println(F("TEST COMPLETE"));
}

void loop() {
    // Nothing to do.
}