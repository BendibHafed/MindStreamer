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

void msPrintRateResult(const __FlashStringHelper* rateName, bool ok) {
    Serial.print(F("setDataRate("));
    Serial.print(rateName);
    Serial.print(F(") -> "));
    Serial.println(ok ? F("OK") : F("FAIL"));

    if (!ok) {
        msPrintLastError(F("LAST_ERROR="));
    }
}

void msTestDataRate(DataRate rate, const __FlashStringHelper* rateName) {
    msPrintSeparator();
    Serial.print(F("TESTING RATE: "));
    Serial.println(rateName);

    const bool ok = msEeg.setDataRate(rate);
    msPrintRateResult(rateName, ok);

    msDumpRegisters(F("REGISTER DUMP"));
}

void msTestDataRateWhileStreaming() {
    msPrintSeparator();
    Serial.println(F("NEGATIVE TEST: setDataRate() while streaming"));

    bool ok = msEeg.startStreaming();
    Serial.print(F("startStreaming() -> "));
    Serial.println(ok ? F("OK") : F("FAIL"));
    if (!ok) {
        msPrintLastError(F("LAST_ERROR="));
    }

    ok = msEeg.setDataRate(DR_500);
    Serial.print(F("setDataRate(DR_500) while streaming -> "));
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

void msConfigureLibrary() {
    msConfig.num_channels            = MS_NUM_CHANNELS;
    msConfig.data_rate               = DR_250;
    msConfig.pga_gain                = PGA_GAIN_24;
    msConfig.output_mode             = OUTPUT_DEBUG;
    msConfig.enable_drl              = false;
    msConfig.drl_internal_ref        = false;
    msConfig.enable_filter           = false;
    msConfig.daisy_chain             = false;
    msConfig.num_modules             = 1;
    msConfig.spi_clock_hz            = 500000;
    msConfig.drdy_timeout_ms         = 100;

    // Keep internal reference enabled by default in your updated library.
    // If your MindStreamerConfig now includes this field, leave it true.
    // Uncomment this line if the field exists in your current version:
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
// Arduino entry points
// -----------------------------------------------------------------------------
void setup() {
    Serial.begin(MS_SERIAL_BAUD);
    delay(MS_STARTUP_DELAY);

    msPrintSeparator();
    Serial.println(F("MindStreamer setDataRate() Robustness Test"));

    msConfigureLibrary();

    if (!msBeginAndReport()) {
        while (true) {
            delay(1000);
        }
    }

    // Sweep all supported data rates and inspect register integrity.
    msTestDataRate(DR_500,  F("DR_500"));
    msTestDataRate(DR_1K,   F("DR_1K"));
    msTestDataRate(DR_2K,   F("DR_2K"));
    msTestDataRate(DR_4K,   F("DR_4K"));
    msTestDataRate(DR_8K,   F("DR_8K"));
    msTestDataRate(DR_16K,  F("DR_16K"));
    msTestDataRate(DR_250,  F("DR_250"));

    // Negative-path test
    msTestDataRateWhileStreaming();

    msPrintSeparator();
    Serial.println(F("TEST COMPLETE"));
}

void loop() {
    // Nothing to do.
}