#include <Arduino.h>
#include <MindStreamer.h>
#include <limits.h>

// -----------------------------------------------------------------------------
// Sketch 10 - MindStreamer Internal Test vs Front-End Isolation Validation
// -----------------------------------------------------------------------------

static constexpr int MS_PIN_DRDY = 12;
static constexpr int MS_PIN_CS   = 15;
static constexpr int MS_PIN_RST  = 13;

static constexpr uint8_t  MS_NUM_CHANNELS      = 8;
static constexpr uint32_t MS_SERIAL_BAUD       = 115200;
static constexpr uint32_t MS_STARTUP_DELAY_MS  = 1500;

// Per-phase duration and reporting
static constexpr uint32_t MS_PHASE_DURATION_MS = 10000;  // 10 s per phase
static constexpr uint32_t MS_REPORT_EVERY_MS   = 2000;   // 2 s progress report
static constexpr uint8_t  MS_PRINT_FIRST_N     = 5;      // print first N samples

MindStreamer msEeg(MS_PIN_DRDY, MS_PIN_CS, MS_PIN_RST);
MindStreamerConfig msConfig;

int32_t msSamples[MS_NUM_CHANNELS];

// -----------------------------------------------------------------------------
// Phase statistics
// -----------------------------------------------------------------------------
struct PhaseStats {
    uint32_t read_success = 0;
    uint32_t read_fail = 0;
    uint32_t no_data_loops = 0;

    uint32_t consecutive_fail = 0;
    uint32_t max_consecutive_fail = 0;

    uint32_t last_success_ms = 0;
    uint32_t max_gap_ms = 0;

    int32_t min_val[MS_NUM_CHANNELS];
    int32_t max_val[MS_NUM_CHANNELS];
    uint32_t sat_count[MS_NUM_CHANNELS];

    int32_t max_abs_diff_to_ch1[MS_NUM_CHANNELS];
    uint64_t sum_abs_diff_to_ch1[MS_NUM_CHANNELS];
};

PhaseStats msPhaseStats;

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

    // Quiet mode for acquisition-focused diagnostics.
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

void msResetPhaseStats() {
    msPhaseStats.read_success = 0;
    msPhaseStats.read_fail = 0;
    msPhaseStats.no_data_loops = 0;
    msPhaseStats.consecutive_fail = 0;
    msPhaseStats.max_consecutive_fail = 0;
    msPhaseStats.last_success_ms = 0;
    msPhaseStats.max_gap_ms = 0;

    for (uint8_t ch = 0; ch < MS_NUM_CHANNELS; ++ch) {
        msPhaseStats.min_val[ch] = INT32_MAX;
        msPhaseStats.max_val[ch] = INT32_MIN;
        msPhaseStats.sat_count[ch] = 0;
        msPhaseStats.max_abs_diff_to_ch1[ch] = 0;
        msPhaseStats.sum_abs_diff_to_ch1[ch] = 0;
    }
}

void msPrintSampleRow(uint32_t index, const int32_t* samples) {
    Serial.print(F("SAMPLE "));
    Serial.print(index);
    Serial.print(F(": "));

    for (uint8_t ch = 0; ch < MS_NUM_CHANNELS; ++ch) {
        Serial.print(samples[ch]);
        if (ch + 1 < MS_NUM_CHANNELS) {
            Serial.print(F(", "));
        }
    }
    Serial.println();
}

void msUpdatePhaseStats(const int32_t* samples, bool compare_to_ch1) {
    const uint32_t now = millis();

    if (msPhaseStats.last_success_ms != 0) {
        const uint32_t gap = now - msPhaseStats.last_success_ms;
        if (gap > msPhaseStats.max_gap_ms) {
            msPhaseStats.max_gap_ms = gap;
        }
    }
    msPhaseStats.last_success_ms = now;

    const int32_t ch1 = samples[0];

    for (uint8_t ch = 0; ch < MS_NUM_CHANNELS; ++ch) {
        const int32_t v = samples[ch];

        if (v < msPhaseStats.min_val[ch]) {
            msPhaseStats.min_val[ch] = v;
        }
        if (v > msPhaseStats.max_val[ch]) {
            msPhaseStats.max_val[ch] = v;
        }

        if (v == -8388608L || v == 8388607L) {
            msPhaseStats.sat_count[ch]++;
        }

        if (compare_to_ch1) {
            int32_t diff = v - ch1;
            if (diff < 0) {
                diff = -diff;
            }
            if (diff > msPhaseStats.max_abs_diff_to_ch1[ch]) {
                msPhaseStats.max_abs_diff_to_ch1[ch] = diff;
            }
            msPhaseStats.sum_abs_diff_to_ch1[ch] += static_cast<uint32_t>(diff);
        }
    }
}

void msPrintPhaseProgress(const __FlashStringHelper* phase_name, uint32_t elapsed_ms) {
    const MindStreamerStats stats = msEeg.getStats();

    msPrintSeparator();
    Serial.print(F("PHASE="));
    Serial.println(phase_name);

    Serial.print(F("ELAPSED_MS="));
    Serial.println(elapsed_ms);

    Serial.print(F("READ_SUCCESS="));
    Serial.println(msPhaseStats.read_success);

    Serial.print(F("READ_FAIL="));
    Serial.println(msPhaseStats.read_fail);

    Serial.print(F("NO_DATA_LOOPS="));
    Serial.println(msPhaseStats.no_data_loops);

    Serial.print(F("MAX_CONSEC_FAIL="));
    Serial.println(msPhaseStats.max_consecutive_fail);

    Serial.print(F("MAX_GAP_MS="));
    Serial.println(msPhaseStats.max_gap_ms);

    Serial.print(F("LIB_TOTAL_SAMPLES="));
    Serial.println(stats.total_samples);

    Serial.print(F("LIB_DROPPED_SAMPLES="));
    Serial.println(stats.dropped_samples);

    Serial.print(F("LIB_ACTUAL_SPS="));
    Serial.println(stats.actual_sps, 3);

    Serial.print(F("LIB_LAST_ERROR="));
    Serial.println((int)stats.last_error);
}

void msPrintPhaseSummary(const __FlashStringHelper* phase_name, bool compare_to_ch1) {
    msPrintSeparator();
    Serial.print(F("SUMMARY FOR "));
    Serial.println(phase_name);

    Serial.print(F("READ_SUCCESS="));
    Serial.println(msPhaseStats.read_success);

    Serial.print(F("READ_FAIL="));
    Serial.println(msPhaseStats.read_fail);

    Serial.print(F("NO_DATA_LOOPS="));
    Serial.println(msPhaseStats.no_data_loops);

    Serial.print(F("MAX_CONSEC_FAIL="));
    Serial.println(msPhaseStats.max_consecutive_fail);

    Serial.print(F("MAX_GAP_MS="));
    Serial.println(msPhaseStats.max_gap_ms);

    for (uint8_t ch = 0; ch < MS_NUM_CHANNELS; ++ch) {
        Serial.print(F("CH"));
        Serial.print(ch + 1);
        Serial.print(F(": min="));
        Serial.print(msPhaseStats.min_val[ch]);
        Serial.print(F(", max="));
        Serial.print(msPhaseStats.max_val[ch]);
        Serial.print(F(", sat="));
        Serial.print(msPhaseStats.sat_count[ch]);

        if (compare_to_ch1) {
            uint32_t mean_abs_diff = 0;
            if (msPhaseStats.read_success > 0) {
                mean_abs_diff =
                    static_cast<uint32_t>(msPhaseStats.sum_abs_diff_to_ch1[ch] / msPhaseStats.read_success);
            }
            Serial.print(F(", maxDiffToCH1="));
            Serial.print(msPhaseStats.max_abs_diff_to_ch1[ch]);
            Serial.print(F(", meanDiffToCH1="));
            Serial.print(mean_abs_diff);
        }

        Serial.println();
    }
}

// -----------------------------------------------------------------------------
// Mode application helpers
// -----------------------------------------------------------------------------
bool msApplyNormalElectrodeMode() {
    msPrintSeparator();
    Serial.println(F("APPLY NORMAL ELECTRODE MODE"));

    if (!msEeg.setGain(PGA_GAIN_24, 0)) {
        msPrintBoolResult(F("setGain(PGA_GAIN_24, all)"), false);
        return false;
    }

    for (uint8_t ch = 1; ch <= MS_NUM_CHANNELS; ++ch) {
        if (!msEeg.disableTestSignal(ch)) {
            Serial.print(F("disableTestSignal(CH"));
            Serial.print(ch);
            Serial.println(F(") -> FAIL"));
            msPrintLastError(F("LAST_ERROR="));
            return false;
        }
        if (!msEeg.setChannelInputMux(ch, ADS1299_MUX_NORMAL)) {
            Serial.print(F("setChannelInputMux(CH"));
            Serial.print(ch);
            Serial.println(F(", NORMAL) -> FAIL"));
            msPrintLastError(F("LAST_ERROR="));
            return false;
        }
    }

    Serial.println(F("NORMAL MODE READY"));
    return true;
}

bool msApplyInternalTestSignalMode() {
    msPrintSeparator();
    Serial.println(F("APPLY INTERNAL TEST SIGNAL MODE"));

    for (uint8_t ch = 1; ch <= MS_NUM_CHANNELS; ++ch) {
        if (!msEeg.enableTestSignal(ch, PGA_GAIN_24, TEST_AMP_1x, TEST_FREQ_fCLK_21)) {
            Serial.print(F("enableTestSignal(CH"));
            Serial.print(ch);
            Serial.println(F(") -> FAIL"));
            msPrintLastError(F("LAST_ERROR="));
            return false;
        }
    }

    Serial.println(F("INTERNAL TEST SIGNAL MODE READY"));
    return true;
}

bool msApplyShortedInputMode() {
    msPrintSeparator();
    Serial.println(F("APPLY SHORTED INPUT MODE"));

    if (!msEeg.setGain(PGA_GAIN_24, 0)) {
        msPrintBoolResult(F("setGain(PGA_GAIN_24, all)"), false);
        return false;
    }

    for (uint8_t ch = 1; ch <= MS_NUM_CHANNELS; ++ch) {
        if (!msEeg.disableTestSignal(ch)) {
            Serial.print(F("disableTestSignal(CH"));
            Serial.print(ch);
            Serial.println(F(") -> FAIL"));
            msPrintLastError(F("LAST_ERROR="));
            return false;
        }
        if (!msEeg.setChannelInputMux(ch, ADS1299_MUX_SHORTED)) {
            Serial.print(F("setChannelInputMux(CH"));
            Serial.print(ch);
            Serial.println(F(", SHORTED) -> FAIL"));
            msPrintLastError(F("LAST_ERROR="));
            return false;
        }
    }

    Serial.println(F("SHORTED INPUT MODE READY"));
    return true;
}

// -----------------------------------------------------------------------------
// Phase runner
// -----------------------------------------------------------------------------
bool msRunPhase(const __FlashStringHelper* phase_name, bool compare_to_ch1) {
    msPrintSeparator();
    Serial.print(F("RUN PHASE: "));
    Serial.println(phase_name);

    msResetPhaseStats();

    bool ok = msEeg.startStreaming();
    msPrintBoolResult(F("startStreaming()"), ok);
    if (!ok) {
        return false;
    }

    const uint32_t start_ms = millis();
    uint32_t last_report_ms = start_ms;

    while ((millis() - start_ms) < MS_PHASE_DURATION_MS) {
        if (!msEeg.dataAvailable()) {
            msPhaseStats.no_data_loops++;
        } else {
            if (msEeg.readAllChannels(msSamples, MS_NUM_CHANNELS)) {
                if (msPhaseStats.read_success < MS_PRINT_FIRST_N) {
                    msPrintSampleRow(msPhaseStats.read_success, msSamples);
                }

                msPhaseStats.read_success++;
                msPhaseStats.consecutive_fail = 0;
                msUpdatePhaseStats(msSamples, compare_to_ch1);
            } else {
                msPhaseStats.read_fail++;
                msPhaseStats.consecutive_fail++;
                if (msPhaseStats.consecutive_fail > msPhaseStats.max_consecutive_fail) {
                    msPhaseStats.max_consecutive_fail = msPhaseStats.consecutive_fail;
                }
            }
        }

        const uint32_t now = millis();
        if ((now - last_report_ms) >= MS_REPORT_EVERY_MS) {
            msPrintPhaseProgress(phase_name, now - start_ms);
            last_report_ms = now;
        }
    }

    ok = msEeg.stopStreaming();
    msPrintBoolResult(F("stopStreaming()"), ok);

    msPrintPhaseSummary(phase_name, compare_to_ch1);
    return ok;
}

// -----------------------------------------------------------------------------
// Arduino entry points
// -----------------------------------------------------------------------------
void setup() {
    Serial.begin(MS_SERIAL_BAUD);
    delay(MS_STARTUP_DELAY_MS);

    msPrintSeparator();
    Serial.println(F("Sketch 10 - MindStreamer Internal Test vs Front-End Isolation Validation"));

    msConfigureLibrary();

    if (!msBeginAndReport()) {
        while (true) {
            delay(1000);
        }
    }

    if (!msApplyNormalElectrodeMode()) {
        while (true) {
            delay(1000);
        }
    }
    msRunPhase(F("PHASE_A_NORMAL_ELECTRODES"), false);

    if (!msApplyInternalTestSignalMode()) {
        while (true) {
            delay(1000);
        }
    }
    msRunPhase(F("PHASE_B_INTERNAL_TEST_SIGNAL"), true);

    if (!msApplyShortedInputMode()) {
        while (true) {
            delay(1000);
        }
    }
    msRunPhase(F("PHASE_C_SHORTED_INPUTS"), false);

    // Restore normal mode at the end
    if (!msApplyNormalElectrodeMode()) {
        msPrintBoolResult(F("restore normal mode"), false);
    }

    msDumpRegisters(F("REGISTER DUMP AFTER SKETCH 10"));
    msPrintSeparator();
    Serial.println(F("TEST COMPLETE"));
}

void loop() {
    // Nothing to do.
}