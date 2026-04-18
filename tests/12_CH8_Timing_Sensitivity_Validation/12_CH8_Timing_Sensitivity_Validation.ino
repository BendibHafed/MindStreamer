#include <Arduino.h>
#include <MindStreamer.h>

// -----------------------------------------------------------------------------
// Sketch 12 - CH8 Timing Sensitivity Validation
//
// Goal:
//   Re-run the shorted-input raw-frame integrity test at multiple SPI clock rates
//   and compare anomaly counts, especially on CH8.
//
// Interpretation:
//   - If CH8 anomalies drop or disappear at lower SPI clock, that strongly points
//     to an SPI/end-of-frame timing issue.
//   - If CH8 anomalies remain even at very low SPI clock, that points more toward
//     a CH8-specific hardware/channel-path issue.
// -----------------------------------------------------------------------------

static constexpr int MS_PIN_DRDY = 12;
static constexpr int MS_PIN_CS   = 15;
static constexpr int MS_PIN_RST  = 13;

static constexpr uint8_t  MS_NUM_CHANNELS        = 8;
static constexpr uint32_t MS_SERIAL_BAUD         = 115200;
static constexpr uint32_t MS_STARTUP_DELAY_MS    = 1500;

static constexpr uint32_t MS_TEST_DURATION_MS    = 15000;   // 15 s per SPI rate
static constexpr uint32_t MS_REPORT_EVERY_MS     = 5000;    // 5 s
static constexpr uint16_t MS_MAX_ANOMALY_PRINTS  = 10;

static constexpr uint8_t  MS_RAW_FRAME_BYTES     = 3 + (3 * MS_NUM_CHANNELS);

// Shorted inputs should stay near zero and close together.
static constexpr int32_t  MS_ABS_LIMIT_SHORTED   = 50000;
static constexpr int32_t  MS_DIFF_LIMIT_TO_CH1   = 50000;

// Test speeds
static constexpr uint32_t MS_SPI_SPEEDS[] = {
    1000000UL,
    500000UL,
    250000UL,
    125000UL
};
static constexpr uint8_t MS_NUM_SPEEDS = sizeof(MS_SPI_SPEEDS) / sizeof(MS_SPI_SPEEDS[0]);

// -----------------------------------------------------------------------------
// Per-run statistics
// -----------------------------------------------------------------------------
struct RunStats {
    uint32_t frames_ok = 0;
    uint32_t frames_fail = 0;
    uint32_t no_data_loops = 0;

    uint32_t anomaly_total = 0;
    uint32_t anomaly_ch6 = 0;
    uint32_t anomaly_ch7 = 0;
    uint32_t anomaly_ch8 = 0;

    uint32_t printed_anomalies = 0;
};

uint8_t msRaw[MS_RAW_FRAME_BYTES];
int32_t msDecoded[MS_NUM_CHANNELS];

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------
void msPrintSeparator() {
    Serial.println(F("============================================================"));
}

void msPrintBoolResult(const __FlashStringHelper* label, bool ok, MindStreamer& eeg) {
    Serial.print(label);
    Serial.print(F(" -> "));
    Serial.println(ok ? F("OK") : F("FAIL"));
    if (!ok) {
        Serial.print(F("LAST_ERROR="));
        Serial.println((int)eeg.getLastError());
    }
}

MindStreamerConfig msMakeConfig(uint32_t spi_clock_hz) {
    MindStreamerConfig cfg;
    cfg.num_channels     = MS_NUM_CHANNELS;
    cfg.data_rate        = DR_250;
    cfg.pga_gain         = PGA_GAIN_24;
    cfg.output_mode      = OUTPUT_BINARY;   // quiet
    cfg.enable_drl       = false;
    cfg.drl_internal_ref = false;
    cfg.enable_filter    = false;
    cfg.daisy_chain      = false;
    cfg.num_modules      = 1;
    cfg.spi_clock_hz     = spi_clock_hz;
    cfg.drdy_timeout_ms  = 100;
    return cfg;
}

int32_t msDecode24(const uint8_t* p) {
    uint32_t sample =
        (static_cast<uint32_t>(p[0]) << 16) |
        (static_cast<uint32_t>(p[1]) << 8)  |
        (static_cast<uint32_t>(p[2]));

    if (sample & 0x00800000UL) {
        sample |= 0xFF000000UL;
    }

    return static_cast<int32_t>(sample);
}

void msDecodeFrame(const uint8_t* raw, int32_t* out_samples) {
    for (uint8_t ch = 0; ch < MS_NUM_CHANNELS; ++ch) {
        out_samples[ch] = msDecode24(&raw[3 + (3 * ch)]);
    }
}

int32_t msAbs32(int32_t v) {
    return (v < 0) ? -v : v;
}

bool msChannelLooksBad(uint8_t ch_index, const int32_t* s) {
    const int32_t v = s[ch_index];
    if (msAbs32(v) > MS_ABS_LIMIT_SHORTED) {
        return true;
    }

    const int32_t diff = msAbs32(v - s[0]);
    return (diff > MS_DIFF_LIMIT_TO_CH1);
}

void msPrintRawHex(const uint8_t* raw, uint8_t len) {
    for (uint8_t i = 0; i < len; ++i) {
        if (raw[i] < 0x10) {
            Serial.print('0');
        }
        Serial.print(raw[i], HEX);
        if (i + 1 < len) {
            Serial.print(' ');
        }
    }
    Serial.println();
}

void msPrintDecodedSamples(const int32_t* s) {
    for (uint8_t ch = 0; ch < MS_NUM_CHANNELS; ++ch) {
        Serial.print(F("CH"));
        Serial.print(ch + 1);
        Serial.print('=');
        Serial.print(s[ch]);
        if (ch + 1 < MS_NUM_CHANNELS) {
            Serial.print(F(", "));
        }
    }
    Serial.println();
}

bool msApplyShortedInputMode(MindStreamer& eeg) {
    msPrintSeparator();
    Serial.println(F("APPLY SHORTED INPUT MODE"));

    if (!eeg.setGain(PGA_GAIN_24, 0)) {
        msPrintBoolResult(F("setGain(PGA_GAIN_24, all)"), false, eeg);
        return false;
    }

    for (uint8_t ch = 1; ch <= MS_NUM_CHANNELS; ++ch) {
        if (!eeg.disableTestSignal(ch)) {
            Serial.print(F("disableTestSignal(CH"));
            Serial.print(ch);
            Serial.println(F(") -> FAIL"));
            Serial.print(F("LAST_ERROR="));
            Serial.println((int)eeg.getLastError());
            return false;
        }

        if (!eeg.setChannelInputMux(ch, ADS1299_MUX_SHORTED)) {
            Serial.print(F("setChannelInputMux(CH"));
            Serial.print(ch);
            Serial.println(F(", SHORTED) -> FAIL"));
            Serial.print(F("LAST_ERROR="));
            Serial.println((int)eeg.getLastError());
            return false;
        }
    }

    Serial.println(F("SHORTED INPUT MODE READY"));
    return true;
}

void msPrintProgress(uint32_t spi_hz, const RunStats& st, uint32_t elapsed_ms) {
    msPrintSeparator();
    Serial.print(F("SPI_HZ="));
    Serial.println(spi_hz);

    Serial.print(F("ELAPSED_MS="));
    Serial.println(elapsed_ms);

    Serial.print(F("FRAMES_OK="));
    Serial.println(st.frames_ok);

    Serial.print(F("FRAMES_FAIL="));
    Serial.println(st.frames_fail);

    Serial.print(F("NO_DATA_LOOPS="));
    Serial.println(st.no_data_loops);

    Serial.print(F("ANOMALIES_TOTAL="));
    Serial.println(st.anomaly_total);

    Serial.print(F("ANOMALIES_CH6="));
    Serial.println(st.anomaly_ch6);

    Serial.print(F("ANOMALIES_CH7="));
    Serial.println(st.anomaly_ch7);

    Serial.print(F("ANOMALIES_CH8="));
    Serial.println(st.anomaly_ch8);
}

void msPrintAnomaly(uint32_t spi_hz, uint32_t frame_index, const RunStats& st) {
    if (st.printed_anomalies >= MS_MAX_ANOMALY_PRINTS) {
        return;
    }

    msPrintSeparator();
    Serial.print(F("SPI_HZ="));
    Serial.println(spi_hz);

    Serial.print(F("ANOMALY FRAME #"));
    Serial.println(frame_index);

    Serial.print(F("STATUS="));
    if (msRaw[0] < 0x10) Serial.print('0');
    Serial.print(msRaw[0], HEX);
    Serial.print(' ');
    if (msRaw[1] < 0x10) Serial.print('0');
    Serial.print(msRaw[1], HEX);
    Serial.print(' ');
    if (msRaw[2] < 0x10) Serial.print('0');
    Serial.println(msRaw[2], HEX);

    const bool bad6 = msChannelLooksBad(5, msDecoded);
    const bool bad7 = msChannelLooksBad(6, msDecoded);
    const bool bad8 = msChannelLooksBad(7, msDecoded);

    Serial.print(F("BAD_CH6="));
    Serial.print(bad6 ? F("1") : F("0"));
    Serial.print(F(", BAD_CH7="));
    Serial.print(bad7 ? F("1") : F("0"));
    Serial.print(F(", BAD_CH8="));
    Serial.println(bad8 ? F("1") : F("0"));

    msPrintDecodedSamples(msDecoded);

    Serial.println(F("RAW_FRAME_HEX:"));
    msPrintRawHex(msRaw, MS_RAW_FRAME_BYTES);
}

void msPrintFinalSummary(uint32_t spi_hz, const RunStats& st, uint32_t elapsed_ms) {
    msPrintSeparator();
    Serial.print(F("FINAL SUMMARY FOR SPI_HZ="));
    Serial.println(spi_hz);

    Serial.print(F("TEST_DURATION_MS="));
    Serial.println(elapsed_ms);

    Serial.print(F("FRAMES_OK="));
    Serial.println(st.frames_ok);

    Serial.print(F("FRAMES_FAIL="));
    Serial.println(st.frames_fail);

    Serial.print(F("NO_DATA_LOOPS="));
    Serial.println(st.no_data_loops);

    Serial.print(F("ANOMALIES_TOTAL="));
    Serial.println(st.anomaly_total);

    Serial.print(F("ANOMALIES_CH6="));
    Serial.println(st.anomaly_ch6);

    Serial.print(F("ANOMALIES_CH7="));
    Serial.println(st.anomaly_ch7);

    Serial.print(F("ANOMALIES_CH8="));
    Serial.println(st.anomaly_ch8);

    Serial.print(F("PRINTED_ANOMALIES="));
    Serial.println(st.printed_anomalies);
}

void msRunOneSpeed(uint32_t spi_hz) {
    msPrintSeparator();
    Serial.print(F("BEGIN TEST AT SPI_HZ="));
    Serial.println(spi_hz);

    MindStreamer eeg(MS_PIN_DRDY, MS_PIN_CS, MS_PIN_RST);
    MindStreamerConfig cfg = msMakeConfig(spi_hz);

    if (!eeg.begin(cfg)) {
        msPrintBoolResult(F("begin(...)"), false, eeg);
        return;
    }
    msPrintBoolResult(F("begin(...)"), true, eeg);

    Serial.print(F("ID,0x"));
    Serial.println(eeg.getDeviceID(), HEX);

    if (!msApplyShortedInputMode(eeg)) {
        return;
    }

    bool ok = eeg.startStreaming();
    msPrintBoolResult(F("startStreaming()"), ok, eeg);
    if (!ok) {
        return;
    }

    RunStats st;
    uint32_t frame_index = 0;

    const uint32_t start_ms = millis();
    uint32_t last_report_ms = start_ms;

    while ((millis() - start_ms) < MS_TEST_DURATION_MS) {
        if (!eeg.dataAvailable()) {
            st.no_data_loops++;
            continue;
        }

        if (!eeg.debugReadFrameRaw(msRaw, MS_NUM_CHANNELS)) {
            st.frames_fail++;
            continue;
        }

        st.frames_ok++;
        frame_index++;

        msDecodeFrame(msRaw, msDecoded);

        const bool bad6 = msChannelLooksBad(5, msDecoded);
        const bool bad7 = msChannelLooksBad(6, msDecoded);
        const bool bad8 = msChannelLooksBad(7, msDecoded);

        if (bad6 || bad7 || bad8) {
            st.anomaly_total++;
            if (bad6) st.anomaly_ch6++;
            if (bad7) st.anomaly_ch7++;
            if (bad8) st.anomaly_ch8++;

            if (st.printed_anomalies < MS_MAX_ANOMALY_PRINTS) {
                msPrintAnomaly(spi_hz, frame_index, st);
                st.printed_anomalies++;
            }
        }

        const uint32_t now = millis();
        if ((now - last_report_ms) >= MS_REPORT_EVERY_MS) {
            msPrintProgress(spi_hz, st, now - start_ms);
            last_report_ms = now;
        }
    }

    ok = eeg.stopStreaming();
    msPrintBoolResult(F("stopStreaming()"), ok, eeg);

    msPrintFinalSummary(spi_hz, st, millis() - start_ms);

    Serial.flush();
    delay(300);
}

void setup() {
    Serial.begin(MS_SERIAL_BAUD);
    delay(MS_STARTUP_DELAY_MS);

    msPrintSeparator();
    Serial.println(F("Sketch 12 - CH8 Timing Sensitivity Validation"));

    for (uint8_t i = 0; i < MS_NUM_SPEEDS; ++i) {
        msRunOneSpeed(MS_SPI_SPEEDS[i]);
    }

    msPrintSeparator();
    Serial.println(F("TEST COMPLETE"));
}

void loop() {
    // Nothing to do.
}