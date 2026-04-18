#include <Arduino.h>
#include <MindStreamer.h>

// -----------------------------------------------------------------------------
// Sketch 11 - Tail-Channel Raw Frame Integrity Validation
//
// Goal:
//   Put all channels in SHORTED-input mode, then read raw 27-byte ADS1299 frames.
//   Only print frames when CH6/CH7/CH8 look abnormal.
//   This helps determine whether the bad values are already present in raw bytes.
//
// Interpretation:
//   - If raw bytes decode to bad CH6/CH7/CH8 values, the issue is before/at raw frame
//     acquisition (SPI tail, frame integrity, or channel path).
//   - If raw bytes stay sane here, but higher-level reads go bad elsewhere, the issue is
//     more likely above raw acquisition.
// -----------------------------------------------------------------------------

static constexpr int MS_PIN_DRDY = 12;
static constexpr int MS_PIN_CS   = 15;
static constexpr int MS_PIN_RST  = 13;

static constexpr uint8_t  MS_NUM_CHANNELS       = 8;
static constexpr uint32_t MS_SERIAL_BAUD        = 115200;
static constexpr uint32_t MS_STARTUP_DELAY_MS   = 1500;
static constexpr uint32_t MS_TEST_DURATION_MS   = 30000;   // 30 s
static constexpr uint32_t MS_REPORT_EVERY_MS    = 5000;    // 5 s

static constexpr uint8_t  MS_RAW_FRAME_BYTES    = 3 + (3 * MS_NUM_CHANNELS);

// Shorted-input thresholds:
// Shorted channels should be near zero and close to one another.
// Keep thresholds loose enough to catch only clear anomalies.
static constexpr int32_t  MS_ABS_LIMIT_SHORTED  = 50000;
static constexpr int32_t  MS_DIFF_LIMIT_TO_CH1  = 50000;

// Limit anomaly print volume.
static constexpr uint16_t MS_MAX_ANOMALY_PRINTS = 30;

MindStreamer msEeg(MS_PIN_DRDY, MS_PIN_CS, MS_PIN_RST);
MindStreamerConfig msConfig;

// -----------------------------------------------------------------------------
// Tracking
// -----------------------------------------------------------------------------
uint8_t  msRaw[MS_RAW_FRAME_BYTES];
int32_t  msDecoded[MS_NUM_CHANNELS];

uint32_t msFramesOk = 0;
uint32_t msFramesFail = 0;
uint32_t msNoDataLoops = 0;

uint32_t msAnomalyCountTotal = 0;
uint32_t msAnomalyCountCH6 = 0;
uint32_t msAnomalyCountCH7 = 0;
uint32_t msAnomalyCountCH8 = 0;

uint32_t msPrintedAnomalies = 0;

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

void msConfigureLibrary() {
    msConfig.num_channels     = MS_NUM_CHANNELS;
    msConfig.data_rate        = DR_250;
    msConfig.pga_gain         = PGA_GAIN_24;
    msConfig.output_mode      = OUTPUT_BINARY;  // quiet
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

int32_t msDecode24(const uint8_t* p) {
    uint32_t sample = (static_cast<uint32_t>(p[0]) << 16) |
                      (static_cast<uint32_t>(p[1]) << 8)  |
                      (static_cast<uint32_t>(p[2]));

    if (sample & 0x00800000UL) {
        sample |= 0xFF000000UL;
    }

    return static_cast<int32_t>(sample);
}

void msDecodeFrame(const uint8_t* raw, int32_t* out_samples) {
    for (uint8_t ch = 0; ch < MS_NUM_CHANNELS; ++ch) {
        const uint8_t* p = &raw[3 + (3 * ch)];
        out_samples[ch] = msDecode24(p);
    }
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

bool msChannelLooksBad(uint8_t ch_index, const int32_t* s) {
    const int32_t v = s[ch_index];
    if (v > MS_ABS_LIMIT_SHORTED || v < -MS_ABS_LIMIT_SHORTED) {
        return true;
    }

    int32_t diff = v - s[0];
    if (diff < 0) diff = -diff;
    return (diff > MS_DIFF_LIMIT_TO_CH1);
}

void msHandleAnomaly(uint32_t frame_index) {
    msAnomalyCountTotal++;

    const bool bad6 = msChannelLooksBad(5, msDecoded);
    const bool bad7 = msChannelLooksBad(6, msDecoded);
    const bool bad8 = msChannelLooksBad(7, msDecoded);

    if (bad6) msAnomalyCountCH6++;
    if (bad7) msAnomalyCountCH7++;
    if (bad8) msAnomalyCountCH8++;

    if (msPrintedAnomalies >= MS_MAX_ANOMALY_PRINTS) {
        return;
    }

    msPrintedAnomalies++;

    msPrintSeparator();
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

void msPrintProgress(uint32_t elapsed_ms) {
    msPrintSeparator();
    Serial.print(F("ELAPSED_MS="));
    Serial.println(elapsed_ms);

    Serial.print(F("FRAMES_OK="));
    Serial.println(msFramesOk);

    Serial.print(F("FRAMES_FAIL="));
    Serial.println(msFramesFail);

    Serial.print(F("NO_DATA_LOOPS="));
    Serial.println(msNoDataLoops);

    Serial.print(F("ANOMALIES_TOTAL="));
    Serial.println(msAnomalyCountTotal);

    Serial.print(F("ANOMALIES_CH6="));
    Serial.println(msAnomalyCountCH6);

    Serial.print(F("ANOMALIES_CH7="));
    Serial.println(msAnomalyCountCH7);

    Serial.print(F("ANOMALIES_CH8="));
    Serial.println(msAnomalyCountCH8);
}

void msPrintFinalSummary(uint32_t elapsed_ms) {
    msPrintSeparator();
    Serial.println(F("FINAL SUMMARY"));

    Serial.print(F("TEST_DURATION_MS="));
    Serial.println(elapsed_ms);

    Serial.print(F("FRAMES_OK="));
    Serial.println(msFramesOk);

    Serial.print(F("FRAMES_FAIL="));
    Serial.println(msFramesFail);

    Serial.print(F("NO_DATA_LOOPS="));
    Serial.println(msNoDataLoops);

    Serial.print(F("ANOMALIES_TOTAL="));
    Serial.println(msAnomalyCountTotal);

    Serial.print(F("ANOMALIES_CH6="));
    Serial.println(msAnomalyCountCH6);

    Serial.print(F("ANOMALIES_CH7="));
    Serial.println(msAnomalyCountCH7);

    Serial.print(F("ANOMALIES_CH8="));
    Serial.println(msAnomalyCountCH8);

    Serial.print(F("PRINTED_ANOMALIES="));
    Serial.println(msPrintedAnomalies);
}

void msRunTailIntegrityTest() {
    msPrintSeparator();
    Serial.println(F("TAIL-CHANNEL RAW FRAME TEST START"));

    bool ok = msEeg.startStreaming();
    msPrintBoolResult(F("startStreaming()"), ok);
    if (!ok) {
        return;
    }

    const uint32_t start_ms = millis();
    uint32_t last_report_ms = start_ms;
    uint32_t frame_index = 0;

    while ((millis() - start_ms) < MS_TEST_DURATION_MS) {
        if (!msEeg.dataAvailable()) {
            msNoDataLoops++;
            continue;
        }

        if (!msEeg.debugReadFrameRaw(msRaw, MS_NUM_CHANNELS)) {
            msFramesFail++;
            continue;
        }

        msFramesOk++;
        frame_index++;

        msDecodeFrame(msRaw, msDecoded);

        const bool bad6 = msChannelLooksBad(5, msDecoded);
        const bool bad7 = msChannelLooksBad(6, msDecoded);
        const bool bad8 = msChannelLooksBad(7, msDecoded);

        if (bad6 || bad7 || bad8) {
            msHandleAnomaly(frame_index);
        }

        const uint32_t now = millis();
        if ((now - last_report_ms) >= MS_REPORT_EVERY_MS) {
            msPrintProgress(now - start_ms);
            last_report_ms = now;
        }
    }

    ok = msEeg.stopStreaming();
    msPrintBoolResult(F("stopStreaming()"), ok);

    msPrintFinalSummary(millis() - start_ms);
}

void setup() {
    Serial.begin(MS_SERIAL_BAUD);
    delay(MS_STARTUP_DELAY_MS);

    msPrintSeparator();
    Serial.println(F("Sketch 11 - Tail-Channel Raw Frame Integrity Validation"));

    msConfigureLibrary();

    if (!msBeginAndReport()) {
        while (true) {
            delay(1000);
        }
    }

    if (!msApplyShortedInputMode()) {
        while (true) {
            delay(1000);
        }
    }

    msRunTailIntegrityTest();

    msPrintSeparator();
    Serial.println(F("TEST COMPLETE"));
}

void loop() {
    // Nothing to do.
}