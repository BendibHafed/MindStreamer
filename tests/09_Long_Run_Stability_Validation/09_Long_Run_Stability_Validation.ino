#include <Arduino.h>
#include <MindStreamer.h>
#include <limits.h>

// -----------------------------------------------------------------------------
// Sketch 09 - MindStreamer Long-Run Stability Validation
// -----------------------------------------------------------------------------

static constexpr int MS_PIN_DRDY = 12;
static constexpr int MS_PIN_CS   = 15;
static constexpr int MS_PIN_RST  = 13;

static constexpr uint8_t  MS_NUM_CHANNELS      = 8;
static constexpr uint32_t MS_SERIAL_BAUD       = 115200;
static constexpr uint32_t MS_STARTUP_DELAY_MS  = 1500;

// Adjust these as needed
static constexpr uint32_t MS_TEST_DURATION_MS  = 10UL * 60UL * 1000UL; // 10 minutes
static constexpr uint32_t MS_REPORT_EVERY_MS   = 10UL * 1000UL;        // 10 seconds

MindStreamer msEeg(MS_PIN_DRDY, MS_PIN_CS, MS_PIN_RST);
MindStreamerConfig msConfig;

// -----------------------------------------------------------------------------
// Tracking
// -----------------------------------------------------------------------------
int32_t msSamples[MS_NUM_CHANNELS];

int32_t msMin[MS_NUM_CHANNELS];
int32_t msMax[MS_NUM_CHANNELS];
uint32_t msSaturationCount[MS_NUM_CHANNELS];

uint32_t msReadSuccessCount = 0;
uint32_t msReadFailCount = 0;
uint32_t msNoDataLoops = 0;

uint32_t msConsecutiveFailCount = 0;
uint32_t msMaxConsecutiveFailCount = 0;

uint32_t msLastSuccessMillis = 0;
uint32_t msMaxInterSampleGapMs = 0;

uint32_t msLastReportMillis = 0;

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

void msResetTracking() {
    for (uint8_t ch = 0; ch < MS_NUM_CHANNELS; ++ch) {
        msMin[ch] = INT32_MAX;
        msMax[ch] = INT32_MIN;
        msSaturationCount[ch] = 0;
    }

    msReadSuccessCount = 0;
    msReadFailCount = 0;
    msNoDataLoops = 0;
    msConsecutiveFailCount = 0;
    msMaxConsecutiveFailCount = 0;
    msLastSuccessMillis = 0;
    msMaxInterSampleGapMs = 0;
    msLastReportMillis = 0;
}

void msConfigureLibrary() {
    msConfig.num_channels     = MS_NUM_CHANNELS;
    msConfig.data_rate        = DR_250;
    msConfig.pga_gain         = PGA_GAIN_24;

    // Long-run stability test should avoid verbose output bottlenecks.
    msConfig.output_mode      = OUTPUT_BINARY;

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

void msUpdateSampleStats(const int32_t* samples) {
    const uint32_t now = millis();

    if (msLastSuccessMillis != 0) {
        const uint32_t gap = now - msLastSuccessMillis;
        if (gap > msMaxInterSampleGapMs) {
            msMaxInterSampleGapMs = gap;
        }
    }
    msLastSuccessMillis = now;

    for (uint8_t ch = 0; ch < MS_NUM_CHANNELS; ++ch) {
        if (samples[ch] < msMin[ch]) {
            msMin[ch] = samples[ch];
        }
        if (samples[ch] > msMax[ch]) {
            msMax[ch] = samples[ch];
        }

        if (samples[ch] == -8388608L || samples[ch] == 8388607L) {
            msSaturationCount[ch]++;
        }
    }
}

void msPrintPeriodicReport(uint32_t elapsedMs) {
    const MindStreamerStats stats = msEeg.getStats();

    msPrintSeparator();
    Serial.print(F("ELAPSED_MS="));
    Serial.println(elapsedMs);

    Serial.print(F("READ_SUCCESS="));
    Serial.println(msReadSuccessCount);

    Serial.print(F("READ_FAIL="));
    Serial.println(msReadFailCount);

    Serial.print(F("NO_DATA_LOOPS="));
    Serial.println(msNoDataLoops);

    Serial.print(F("MAX_CONSEC_FAIL="));
    Serial.println(msMaxConsecutiveFailCount);

    Serial.print(F("MAX_GAP_MS="));
    Serial.println(msMaxInterSampleGapMs);

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

    for (uint8_t ch = 0; ch < MS_NUM_CHANNELS; ++ch) {
        Serial.print(F("CH"));
        Serial.print(ch + 1);
        Serial.print(F(": min="));
        Serial.print(msMin[ch]);
        Serial.print(F(", max="));
        Serial.print(msMax[ch]);
        Serial.print(F(", sat="));
        Serial.println(msSaturationCount[ch]);
    }
}

void msPrintFinalSummary(uint32_t elapsedMs) {
    const MindStreamerStats stats = msEeg.getStats();

    msPrintSeparator();
    Serial.println(F("FINAL SUMMARY"));

    Serial.print(F("TEST_DURATION_MS="));
    Serial.println(elapsedMs);

    Serial.print(F("READ_SUCCESS="));
    Serial.println(msReadSuccessCount);

    Serial.print(F("READ_FAIL="));
    Serial.println(msReadFailCount);

    Serial.print(F("NO_DATA_LOOPS="));
    Serial.println(msNoDataLoops);

    Serial.print(F("MAX_CONSEC_FAIL="));
    Serial.println(msMaxConsecutiveFailCount);

    Serial.print(F("MAX_GAP_MS="));
    Serial.println(msMaxInterSampleGapMs);

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

    for (uint8_t ch = 0; ch < MS_NUM_CHANNELS; ++ch) {
        Serial.print(F("CH"));
        Serial.print(ch + 1);
        Serial.print(F(": min="));
        Serial.print(msMin[ch]);
        Serial.print(F(", max="));
        Serial.print(msMax[ch]);
        Serial.print(F(", sat="));
        Serial.println(msSaturationCount[ch]);
    }
}

void msRunLongStabilityTest() {
    msPrintSeparator();
    Serial.println(F("LONG-RUN ACQUISITION TEST START"));

    bool ok = msEeg.startStreaming();
    msPrintBoolResult(F("startStreaming()"), ok);
    if (!ok) {
        return;
    }

    msResetTracking();

    const uint32_t testStart = millis();
    msLastReportMillis = testStart;

    while ((millis() - testStart) < MS_TEST_DURATION_MS) {
        if (!msEeg.dataAvailable()) {
            msNoDataLoops++;
        } else {
            if (msEeg.readAllChannels(msSamples, MS_NUM_CHANNELS)) {
                msReadSuccessCount++;
                msConsecutiveFailCount = 0;
                msUpdateSampleStats(msSamples);
            } else {
                msReadFailCount++;
                msConsecutiveFailCount++;
                if (msConsecutiveFailCount > msMaxConsecutiveFailCount) {
                    msMaxConsecutiveFailCount = msConsecutiveFailCount;
                }
            }
        }

        const uint32_t now = millis();
        if ((now - msLastReportMillis) >= MS_REPORT_EVERY_MS) {
            msPrintPeriodicReport(now - testStart);
            msLastReportMillis = now;
        }
    }

    ok = msEeg.stopStreaming();
    msPrintBoolResult(F("stopStreaming()"), ok);

    const uint32_t elapsed = millis() - testStart;
    msPrintFinalSummary(elapsed);
    msDumpRegisters(F("REGISTER DUMP AFTER LONG-RUN TEST"));
}

void setup() {
    Serial.begin(MS_SERIAL_BAUD);
    delay(MS_STARTUP_DELAY_MS);

    msPrintSeparator();
    Serial.println(F("Sketch 09 - MindStreamer Long-Run Stability Validation"));

    msConfigureLibrary();

    if (!msBeginAndReport()) {
        while (true) {
            delay(1000);
        }
    }

    msRunLongStabilityTest();

    msPrintSeparator();
    Serial.println(F("TEST COMPLETE"));
}

void loop() {
    // Nothing to do.
}