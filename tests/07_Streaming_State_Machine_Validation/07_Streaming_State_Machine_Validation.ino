#include <Arduino.h>
#include <MindStreamer.h>

// -----------------------------------------------------------------------------
// Sketch 07 - MindStreamer Streaming State Machine Validation
// -----------------------------------------------------------------------------

static constexpr int MS_PIN_DRDY = 12;
static constexpr int MS_PIN_CS   = 15;
static constexpr int MS_PIN_RST  = 13;

static constexpr uint8_t  MS_NUM_CHANNELS     = 8;
static constexpr uint32_t MS_SERIAL_BAUD      = 115200;
static constexpr uint32_t MS_STARTUP_DELAY_MS = 1500;

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

void msDumpRegisters(const __FlashStringHelper* title) {
    msPrintSeparator();
    Serial.println(title);
    msEeg.printRegisterMap();
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

void msTestStartTwice() {
    msPrintSeparator();
    Serial.println(F("TEST startStreaming() twice"));

    bool ok = msEeg.startStreaming();
    msPrintBoolResult(F("startStreaming() first"), ok);

    ok = msEeg.startStreaming();
    msPrintBoolResult(F("startStreaming() second"), ok);

    msDumpRegisters(F("REGISTER DUMP AFTER START TWICE"));
}

void msTestStopTwice() {
    msPrintSeparator();
    Serial.println(F("TEST stopStreaming() twice"));

    bool ok = msEeg.stopStreaming();
    msPrintBoolResult(F("stopStreaming() first"), ok);

    ok = msEeg.stopStreaming();
    msPrintBoolResult(F("stopStreaming() second"), ok);

    msDumpRegisters(F("REGISTER DUMP AFTER STOP TWICE"));
}

void msTestConfigWhileStreaming() {
    msPrintSeparator();
    Serial.println(F("TEST configuration calls while streaming"));

    bool ok = msEeg.startStreaming();
    msPrintBoolResult(F("startStreaming()"), ok);

    ok = msEeg.setDataRate(DR_500);
    msPrintBoolResult(F("setDataRate(DR_500) while streaming"), ok);

    ok = msEeg.setGain(PGA_GAIN_12, 1);
    msPrintBoolResult(F("setGain(PGA_GAIN_12, 1) while streaming"), ok);

    ok = msEeg.enableChannel(1, false);
    msPrintBoolResult(F("enableChannel(1, false) while streaming"), ok);

    ok = msEeg.enableDRL(true);
    msPrintBoolResult(F("enableDRL(true) while streaming"), ok);

    ok = msEeg.stopStreaming();
    msPrintBoolResult(F("stopStreaming()"), ok);

    msDumpRegisters(F("REGISTER DUMP AFTER CONFIG-WHILE-STREAMING TEST"));
}

void msTestRepeatedCycles() {
    msPrintSeparator();
    Serial.println(F("TEST repeated start/stop cycles"));

    for (uint8_t i = 0; i < 5; ++i) {
        Serial.print(F("CYCLE "));
        Serial.println(i + 1);

        bool ok = msEeg.startStreaming();
        msPrintBoolResult(F("  startStreaming()"), ok);

        ok = msEeg.stopStreaming();
        msPrintBoolResult(F("  stopStreaming()"), ok);
    }

    msDumpRegisters(F("REGISTER DUMP AFTER REPEATED CYCLES"));
}

void msTestStopBeforeStart() {
    msPrintSeparator();
    Serial.println(F("NEGATIVE TEST: stopStreaming() before start"));

    bool ok = msEeg.stopStreaming();
    msPrintBoolResult(F("stopStreaming() before start"), ok);
}

void setup() {
    Serial.begin(MS_SERIAL_BAUD);
    delay(MS_STARTUP_DELAY_MS);

    msPrintSeparator();
    Serial.println(F("Sketch 07 - MindStreamer Streaming State Machine Validation"));

    msConfigureLibrary();

    if (!msBeginAndReport()) {
        while (true) delay(1000);
    }

    msTestStopBeforeStart();
    msTestStartTwice();
    msTestStopTwice();
    msTestConfigWhileStreaming();
    msTestRepeatedCycles();

    msPrintSeparator();
    Serial.println(F("TEST COMPLETE"));
}

void loop() {
}