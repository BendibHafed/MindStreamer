#include <Arduino.h>
#include <MindStreamer.h>

// -----------------------------------------------------------------------------
// Sketch 14 - Re-begin / Recovery Validation
// -----------------------------------------------------------------------------

static constexpr int MS_PIN_DRDY = 12;
static constexpr int MS_PIN_CS   = 15;
static constexpr int MS_PIN_RST  = 13;

static constexpr uint8_t  MS_NUM_CHANNELS        = 8;
static constexpr uint32_t MS_SERIAL_BAUD         = 115200;
static constexpr uint32_t MS_STARTUP_DELAY_MS    = 1500;

static constexpr uint8_t  MS_NUM_CYCLES          = 5;
static constexpr uint32_t MS_STREAM_WINDOW_MS    = 3000;
static constexpr uint32_t MS_REPORT_EVERY_MS     = 1000;
static constexpr uint32_t MS_BETWEEN_CYCLES_MS   = 250;

MindStreamer msEeg(MS_PIN_DRDY, MS_PIN_CS, MS_PIN_RST);
MindStreamerConfig msConfig;

int32_t msSamples[MS_NUM_CHANNELS];

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
    msConfig.output_mode      = OUTPUT_BINARY;   // quiet test
    msConfig.enable_drl       = false;
    msConfig.drl_internal_ref = false;
    msConfig.enable_filter    = false;
    msConfig.daisy_chain      = false;
    msConfig.num_modules      = 1;
    msConfig.spi_clock_hz     = 500000;          // validated stable rate
    msConfig.drdy_timeout_ms  = 100;
}

bool msBeginAndReport(const __FlashStringHelper* title) {
    msPrintSeparator();
    Serial.println(title);

    const bool ok = msEeg.begin(msConfig);
    msPrintBoolResult(F("begin(...)"), ok);
    if (!ok) {
        return false;
    }

    Serial.print(F("ID,0x"));
    Serial.println(msEeg.getDeviceID(), HEX);
    return true;
}

bool msAcquireWindow(uint8_t cycle_index) {
    msPrintSeparator();
    Serial.print(F("CYCLE "));
    Serial.print(cycle_index);
    Serial.println(F(" ACQUISITION WINDOW"));

    bool ok = msEeg.startStreaming();
    msPrintBoolResult(F("startStreaming()"), ok);
    if (!ok) {
        return false;
    }

    uint32_t read_success = 0;
    uint32_t read_fail = 0;
    uint32_t no_data_loops = 0;

    const uint32_t start_ms = millis();
    uint32_t last_report_ms = start_ms;

    while ((millis() - start_ms) < MS_STREAM_WINDOW_MS) {
        if (!msEeg.dataAvailable()) {
            no_data_loops++;
        } else {
            if (msEeg.readAllChannels(msSamples, MS_NUM_CHANNELS)) {
                read_success++;
            } else {
                read_fail++;
            }
        }

        const uint32_t now = millis();
        if ((now - last_report_ms) >= MS_REPORT_EVERY_MS) {
            Serial.print(F("READ_SUCCESS="));
            Serial.print(read_success);
            Serial.print(F(", READ_FAIL="));
            Serial.print(read_fail);
            Serial.print(F(", NO_DATA_LOOPS="));
            Serial.println(no_data_loops);
            last_report_ms = now;
        }
    }

    ok = msEeg.stopStreaming();
    msPrintBoolResult(F("stopStreaming()"), ok);

    const MindStreamerStats stats = msEeg.getStats();

    Serial.print(F("FINAL_READ_SUCCESS="));
    Serial.println(read_success);

    Serial.print(F("FINAL_READ_FAIL="));
    Serial.println(read_fail);

    Serial.print(F("FINAL_NO_DATA_LOOPS="));
    Serial.println(no_data_loops);

    Serial.print(F("LIB_TOTAL_SAMPLES="));
    Serial.println(stats.total_samples);

    Serial.print(F("LIB_DROPPED_SAMPLES="));
    Serial.println(stats.dropped_samples);

    Serial.print(F("LIB_ACTUAL_SPS="));
    Serial.println(stats.actual_sps, 3);

    Serial.print(F("LIB_LAST_SAMPLE_MS="));
    Serial.println(stats.last_sample_time_ms);

    Serial.print(F("LIB_LAST_ERROR="));
    Serial.println((int)stats.last_error);

    return ok && (read_success > 0) && (read_fail == 0);
}

void setup() {
    Serial.begin(MS_SERIAL_BAUD);
    delay(MS_STARTUP_DELAY_MS);

    msPrintSeparator();
    Serial.println(F("Sketch 14 - Re-begin / Recovery Validation"));

    msConfigureLibrary();

    // -------------------------------------------------------------------------
    // Step 1: initial begin
    // -------------------------------------------------------------------------
    if (!msBeginAndReport(F("INITIAL BEGIN"))) {
        while (true) delay(1000);
    }
    msDumpRegisters(F("REGISTER DUMP AFTER INITIAL BEGIN"));

    // -------------------------------------------------------------------------
    // Step 2: immediate re-begin while idle
    // -------------------------------------------------------------------------
    if (!msBeginAndReport(F("IDLE RE-BEGIN WITHOUT POWER CYCLE"))) {
        while (true) delay(1000);
    }
    msDumpRegisters(F("REGISTER DUMP AFTER IDLE RE-BEGIN"));

    // -------------------------------------------------------------------------
    // Step 3: repeated begin -> stream -> stop -> begin cycles
    // -------------------------------------------------------------------------
    for (uint8_t cycle = 1; cycle <= MS_NUM_CYCLES; ++cycle) {
        msPrintSeparator();
        Serial.print(F("RECOVERY CYCLE "));
        Serial.println(cycle);

        if (!msBeginAndReport(F("RE-BEGIN BEFORE STREAMING"))) {
            while (true) delay(1000);
        }

        msDumpRegisters(F("REGISTER DUMP AFTER RE-BEGIN"));

        const bool ok = msAcquireWindow(cycle);
        if (!ok) {
            Serial.println(F("CYCLE FAILED"));
            while (true) delay(1000);
        }

        delay(MS_BETWEEN_CYCLES_MS);
    }

    msPrintSeparator();
    Serial.println(F("TEST COMPLETE"));
}

void loop() {
    // Nothing to do.
}