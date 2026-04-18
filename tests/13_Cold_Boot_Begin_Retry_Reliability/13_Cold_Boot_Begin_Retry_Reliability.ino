#include <Arduino.h>
#include <MindStreamer.h>

// -----------------------------------------------------------------------------
// Sketch 13 - Cold-Boot / Begin-Retry Reliability
// -----------------------------------------------------------------------------

static constexpr int MS_PIN_DRDY = 12;
static constexpr int MS_PIN_CS   = 15;
static constexpr int MS_PIN_RST  = 13;

static constexpr uint8_t  MS_NUM_CHANNELS      = 8;
static constexpr uint32_t MS_SERIAL_BAUD       = 115200;
static constexpr uint32_t MS_STARTUP_DELAY_MS  = 1500;

// Retry behavior
static constexpr uint8_t  MS_MAX_BEGIN_ATTEMPTS   = 5;
static constexpr uint32_t MS_RETRY_DELAY_MS       = 200;
static constexpr uint32_t MS_POST_SUCCESS_DELAY_MS = 500;

MindStreamer msEeg(MS_PIN_DRDY, MS_PIN_CS, MS_PIN_RST);
MindStreamerConfig msConfig;

void msPrintSeparator() {
    Serial.println(F("============================================================"));
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

    // Use library default if you already changed it globally.
    // Or set explicitly here if you want to force the validated rate.
    msConfig.spi_clock_hz     = 500000;

    msConfig.drdy_timeout_ms  = 100;
}

bool msTryBeginWithRetries(uint8_t& success_attempt) {
    for (uint8_t attempt = 1; attempt <= MS_MAX_BEGIN_ATTEMPTS; ++attempt) {
        msPrintSeparator();
        Serial.print(F("BEGIN ATTEMPT "));
        Serial.print(attempt);
        Serial.print(F(" / "));
        Serial.println(MS_MAX_BEGIN_ATTEMPTS);

        if (msEeg.begin(msConfig)) {
            success_attempt = attempt;
            return true;
        }

        Serial.print(F("BEGIN_FAIL, LAST_ERROR="));
        Serial.println((int)msEeg.getLastError());

        if (attempt < MS_MAX_BEGIN_ATTEMPTS) {
            delay(MS_RETRY_DELAY_MS);
        }
    }

    success_attempt = 0;
    return false;
}

void setup() {
    Serial.begin(MS_SERIAL_BAUD);
    delay(MS_STARTUP_DELAY_MS);

    msPrintSeparator();
    Serial.println(F("Sketch 13 - Cold-Boot / Begin-Retry Reliability"));

    msConfigureLibrary();

    uint8_t success_attempt = 0;
    const bool ok = msTryBeginWithRetries(success_attempt);

    msPrintSeparator();
    if (!ok) {
        Serial.println(F("BEGIN_FAIL_ALL_ATTEMPTS"));
        while (true) {
            delay(1000);
        }
    }

    Serial.print(F("BEGIN_SUCCESS_ON_ATTEMPT="));
    Serial.println(success_attempt);

    Serial.print(F("ID,0x"));
    Serial.println(msEeg.getDeviceID(), HEX);

    delay(MS_POST_SUCCESS_DELAY_MS);

    msPrintSeparator();
    Serial.println(F("REGISTER DUMP AFTER SUCCESSFUL BEGIN"));
    msEeg.printRegisterMap();

    msPrintSeparator();
    Serial.println(F("BOOT TEST COMPLETE"));
}

void loop() {
    // Nothing to do.
}