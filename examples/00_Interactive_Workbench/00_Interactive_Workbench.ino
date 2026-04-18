#include <Arduino.h>
#include <MindStreamer.h>

// -----------------------------------------------------------------------------
// MindStreamer Example 07
// Interactive Workbench
//
// What it does:
// - Initializes MindStreamer with the validated operating point
// - Waits for a startup mode selection from button or serial
// - Provides a serial command console for runtime control
// - Supports text, binary, debug, and silent acquisition modes
// - Supports channel enable/disable, shorted inputs, internal test signal,
//   temperature reading, gain changes, register dump, and optional filtering
//
// Design goals:
// - Keep one acquisition loop
// - Stop streaming before reconfiguration, then resume if needed
// - Stay close in spirit to the legacy all-in-one interactive sketch
// -----------------------------------------------------------------------------

static constexpr int PIN_DRDY   = 12;
static constexpr int PIN_CS     = 15;
static constexpr int PIN_RST    = 13;
static constexpr int PIN_LED1   = 14;
static constexpr int PIN_LED2   = 27;
static constexpr int PIN_BUTTON = 5;

static constexpr uint8_t  NUM_CHANNELS        = 8;
static constexpr uint32_t SERIAL_BAUD         = 115200;
static constexpr uint32_t STARTUP_DELAY_MS    = 1500;
static constexpr uint32_t DEFAULT_SPI_HZ      = 500000;
static constexpr uint32_t DEFAULT_INTERVAL_MS = 1000;

enum AppOutputMode : uint8_t {
    APP_OUTPUT_VOID = 0,
    APP_OUTPUT_TEXT,
    APP_OUTPUT_BINARY,
    APP_OUTPUT_DEBUG
};

struct AppState {
    bool streaming = false;
    bool filtersEnabled = false;
    bool led2State = false;
    bool startupComplete = false;
    AppOutputMode outputMode = APP_OUTPUT_TEXT;
    uint32_t sampleCounter = 0;
    uint32_t lastStatusPrintMs = 0;
    uint32_t lastLedToggleSample = 0;
};

MindStreamer eeg(PIN_DRDY, PIN_CS, PIN_RST);
MindStreamerConfig config;
AppState app;

int32_t rawSamples[NUM_CHANNELS];
float processedSamples[NUM_CHANNELS];
float filterPrevX[NUM_CHANNELS];
float filterPrevY[NUM_CHANNELS];

// -----------------------------------------------------------------------------
// Utility helpers
// -----------------------------------------------------------------------------
void printSeparator() {
    Serial.println(F("============================================================"));
}

void printLastError(const __FlashStringHelper* prefix) {
    Serial.print(prefix);
    Serial.println((int)eeg.getLastError());
}

void printBoolResult(const __FlashStringHelper* label, bool ok) {
    Serial.print(label);
    Serial.print(F(" -> "));
    Serial.println(ok ? F("OK") : F("FAIL"));
    if (!ok) {
        printLastError(F("LAST_ERROR="));
    }
}

void resetFilterState() {
    for (uint8_t ch = 0; ch < NUM_CHANNELS; ++ch) {
        filterPrevX[ch] = 0.0f;
        filterPrevY[ch] = 0.0f;
    }
}

void updateLed2State() {
    if (!app.streaming) {
        digitalWrite(PIN_LED2, LOW);
        app.led2State = false;
        return;
    }

    if ((app.sampleCounter - app.lastLedToggleSample) >= 20) {
        app.led2State = !app.led2State;
        digitalWrite(PIN_LED2, app.led2State ? HIGH : LOW);
        app.lastLedToggleSample = app.sampleCounter;
    }
}

void printHelp() {
    printSeparator();
    Serial.println(F("MindStreamer Interactive Workbench"));
    Serial.println(F("Startup:"));
    Serial.println(F("  Press button on GPIO5 for default TEXT mode"));
    Serial.println(F("  Or send one of: X, B, W, D"));
    Serial.println(F(""));
    Serial.println(F("Streaming control:"));
    Serial.println(F("  X = text mode and start/resume"));
    Serial.println(F("  B = binary mode and start/resume"));
    Serial.println(F("  W = silent acquisition and start/resume"));
    Serial.println(F("  D = debug text mode and start/resume"));
    Serial.println(F("  S = stop streaming"));
    Serial.println(F("  R = resume current mode"));
    Serial.println(F(""));
    Serial.println(F("Channel control:"));
    Serial.println(F("  1..8 = disable channels 1..8"));
    Serial.println(F("  a z e r t y u i = enable channels 1..8"));
    Serial.println(F(""));
    Serial.println(F("Input routing and tests:"));
    Serial.println(F("  n = all channels normal input"));
    Serial.println(F("  q = all channels shorted"));
    Serial.println(F("  s = all channels internal test signal (1x, fCLK/2^21)"));
    Serial.println(F("  g = all channels internal test signal (2x, fCLK/2^21)"));
    Serial.println(F(""));
    Serial.println(F("Gain presets:"));
    Serial.println(F("  j = all channels gain 1"));
    Serial.println(F("  k = all channels gain 6"));
    Serial.println(F("  l = all channels gain 24"));
    Serial.println(F(""));
    Serial.println(F("Diagnostics:"));
    Serial.println(F("  0 = print register map"));
    Serial.println(F("  T = read temperature"));
    Serial.println(F("  P = print stats"));
    Serial.println(F("  H = print this help"));
    Serial.println(F(""));
    Serial.println(F("Processing:"));
    Serial.println(F("  F = enable simple DC-blocking filter"));
    Serial.println(F("  N = disable filters"));
    Serial.println(F(""));
    Serial.println(F("System:"));
    Serial.println(F("  . = restart ESP32"));
    printSeparator();
}

void printTextHeader() {
    Serial.println(F("sample,ch1,ch2,ch3,ch4,ch5,ch6,ch7,ch8"));
}

void printStartupPrompt() {
    printSeparator();
    Serial.println(F("Select startup mode:"));
    Serial.println(F("  X = TEXT"));
    Serial.println(F("  B = BINARY"));
    Serial.println(F("  W = SILENT"));
    Serial.println(F("  D = DEBUG"));
    Serial.println(F("Or press the button on GPIO5 for default TEXT mode."));
    printSeparator();
}

void printCurrentMode() {
    Serial.print(F("CURRENT_MODE="));
    switch (app.outputMode) {
        case APP_OUTPUT_VOID:   Serial.println(F("SILENT")); break;
        case APP_OUTPUT_TEXT:   Serial.println(F("TEXT"));   break;
        case APP_OUTPUT_BINARY: Serial.println(F("BINARY")); break;
        case APP_OUTPUT_DEBUG:  Serial.println(F("DEBUG"));  break;
        default:                Serial.println(F("UNKNOWN")); break;
    }
}

void printStats() {
    const MindStreamerStats stats = eeg.getStats();
    printSeparator();
    Serial.print(F("STREAMING="));
    Serial.println(app.streaming ? F("YES") : F("NO"));
    Serial.print(F("SAMPLE_COUNTER="));
    Serial.println(app.sampleCounter);
    Serial.print(F("FILTERS_ENABLED="));
    Serial.println(app.filtersEnabled ? F("YES") : F("NO"));
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
    printCurrentMode();
    printSeparator();
}

// -----------------------------------------------------------------------------
// Filtering
// -----------------------------------------------------------------------------
void applyOptionalProcessing() {
    if (!app.filtersEnabled) {
        for (uint8_t ch = 0; ch < NUM_CHANNELS; ++ch) {
            processedSamples[ch] = static_cast<float>(rawSamples[ch]);
        }
        return;
    }

    const float alpha = 0.995f;

    for (uint8_t ch = 0; ch < NUM_CHANNELS; ++ch) {
        const float x = static_cast<float>(rawSamples[ch]);
        const float y = x - filterPrevX[ch] + alpha * filterPrevY[ch];

        filterPrevX[ch] = x;
        filterPrevY[ch] = y;
        processedSamples[ch] = y;
    }
}

// -----------------------------------------------------------------------------
// Output helpers
// -----------------------------------------------------------------------------
void emitTextFrame() {
    Serial.print(app.sampleCounter);
    for (uint8_t ch = 0; ch < NUM_CHANNELS; ++ch) {
        Serial.print(',');
        Serial.print(static_cast<int32_t>(processedSamples[ch]));
    }
    Serial.println();
}

void emitDebugFrame() {
    Serial.print(F("Sample: "));
    Serial.println(app.sampleCounter);
    for (uint8_t ch = 0; ch < NUM_CHANNELS; ++ch) {
        Serial.print(F("CH"));
        Serial.print(ch + 1);
        Serial.print(F(": "));
        Serial.println(static_cast<int32_t>(processedSamples[ch]));
    }
    Serial.println(F("---"));
}

void emitBinaryFrame() {
    const uint8_t header = MINDSTREAMER_PACKET_HEADER;
    const uint8_t footer = MINDSTREAMER_PACKET_FOOTER;

    Serial.write(&header, 1);
    Serial.write(reinterpret_cast<const uint8_t*>(&app.sampleCounter), sizeof(app.sampleCounter));

    for (uint8_t ch = 0; ch < NUM_CHANNELS; ++ch) {
        const int32_t value = static_cast<int32_t>(processedSamples[ch]);
        Serial.write(reinterpret_cast<const uint8_t*>(&value), sizeof(value));
    }

    Serial.write(&footer, 1);
}

void emitCurrentFrame() {
    switch (app.outputMode) {
        case APP_OUTPUT_VOID:
            return;

        case APP_OUTPUT_TEXT:
            emitTextFrame();
            return;

        case APP_OUTPUT_BINARY:
            emitBinaryFrame();
            return;

        case APP_OUTPUT_DEBUG:
            emitDebugFrame();
            return;

        default:
            return;
    }
}

// -----------------------------------------------------------------------------
// Safe reconfiguration helpers
// -----------------------------------------------------------------------------
bool pauseForReconfiguration(bool& resumeAfter) {
    resumeAfter = app.streaming;

    if (resumeAfter) {
        if (!eeg.stopStreaming()) {
            printBoolResult(F("stopStreaming()"), false);
            return false;
        }
        app.streaming = false;
    }

    digitalWrite(PIN_LED2, LOW);
    app.led2State = false;
    return true;
}

bool resumeAfterReconfiguration(bool resumeAfter) {
    if (!resumeAfter) {
        return true;
    }

    if (!eeg.startStreaming()) {
        printBoolResult(F("startStreaming()"), false);
        return false;
    }

    app.streaming = true;
    app.lastLedToggleSample = app.sampleCounter;
    return true;
}

bool setAppOutputMode(AppOutputMode mode) {
    app.outputMode = mode;

    if (mode == APP_OUTPUT_TEXT) {
        printTextHeader();
    }

    printCurrentMode();
    return true;
}

bool startInCurrentMode() {
    if (app.streaming) {
        return true;
    }

    if (!eeg.startStreaming()) {
        printBoolResult(F("startStreaming()"), false);
        return false;
    }

    app.streaming = true;
    app.lastLedToggleSample = app.sampleCounter;

    if (app.outputMode == APP_OUTPUT_TEXT) {
        printTextHeader();
    }

    printCurrentMode();
    return true;
}

bool stopCurrentStreaming() {
    if (!app.streaming) {
        return true;
    }

    if (!eeg.stopStreaming()) {
        printBoolResult(F("stopStreaming()"), false);
        return false;
    }

    app.streaming = false;
    digitalWrite(PIN_LED2, LOW);
    app.led2State = false;
    return true;
}

bool setAllChannelsNormal() {
    bool resumeAfter = false;
    if (!pauseForReconfiguration(resumeAfter)) {
        return false;
    }

    bool ok = true;
    for (uint8_t ch = 1; ch <= NUM_CHANNELS; ++ch) {
        ok &= eeg.enableChannel(ch, true);
        ok &= eeg.disableTestSignal(ch);
        ok &= eeg.setChannelInputMux(ch, ADS1299_MUX_NORMAL);
    }

    if (!ok) {
        printLastError(F("NORMAL_MODE_FAIL, LAST_ERROR="));
    } else {
        Serial.println(F("ALL_CHANNELS_NORMAL"));
    }

    if (!resumeAfterReconfiguration(resumeAfter)) {
        return false;
    }

    return ok;
}

bool setAllChannelsShorted() {
    bool resumeAfter = false;
    if (!pauseForReconfiguration(resumeAfter)) {
        return false;
    }

    bool ok = true;
    for (uint8_t ch = 1; ch <= NUM_CHANNELS; ++ch) {
        ok &= eeg.enableChannel(ch, true);
        ok &= eeg.disableTestSignal(ch);
        ok &= eeg.setChannelInputMux(ch, ADS1299_MUX_SHORTED);
    }

    if (!ok) {
        printLastError(F("SHORTED_MODE_FAIL, LAST_ERROR="));
    } else {
        Serial.println(F("ALL_CHANNELS_SHORTED"));
    }

    if (!resumeAfterReconfiguration(resumeAfter)) {
        return false;
    }

    return ok;
}

bool setAllChannelsTestSignal(TestSignalAmp amplitude, TestSignalFreq frequency) {
    bool resumeAfter = false;
    if (!pauseForReconfiguration(resumeAfter)) {
        return false;
    }

    bool ok = true;
    for (uint8_t ch = 1; ch <= NUM_CHANNELS; ++ch) {
        ok &= eeg.enableChannel(ch, true);
        ok &= eeg.enableTestSignal(ch, PGA_GAIN_24, amplitude, frequency);
    }

    if (!ok) {
        printLastError(F("TEST_SIGNAL_FAIL, LAST_ERROR="));
    } else {
        Serial.println(F("ALL_CHANNELS_TEST_SIGNAL"));
    }

    if (!resumeAfterReconfiguration(resumeAfter)) {
        return false;
    }

    return ok;
}

bool setAllChannelsGain(PGAGain gain) {
    bool resumeAfter = false;
    if (!pauseForReconfiguration(resumeAfter)) {
        return false;
    }

    const bool ok = eeg.setGain(gain, 0);

    if (!ok) {
        printLastError(F("SET_GAIN_FAIL, LAST_ERROR="));
    } else {
        Serial.print(F("ALL_CHANNELS_GAIN="));
        Serial.println((int)gain);
    }

    if (!resumeAfterReconfiguration(resumeAfter)) {
        return false;
    }

    return ok;
}

bool setChannelEnabled(uint8_t channel, bool enable) {
    bool resumeAfter = false;
    if (!pauseForReconfiguration(resumeAfter)) {
        return false;
    }

    const bool ok = eeg.enableChannel(channel, enable);

    if (!ok) {
        printLastError(F("CHANNEL_STATE_FAIL, LAST_ERROR="));
    } else {
        Serial.print(F("CHANNEL "));
        Serial.print(channel);
        Serial.print(F(" -> "));
        Serial.println(enable ? F("ENABLED") : F("DISABLED"));
    }

    if (!resumeAfterReconfiguration(resumeAfter)) {
        return false;
    }

    return ok;
}

bool dumpRegistersSafely() {
    bool resumeAfter = false;
    if (!pauseForReconfiguration(resumeAfter)) {
        return false;
    }

    eeg.printRegisterMap();

    if (!resumeAfterReconfiguration(resumeAfter)) {
        return false;
    }

    return true;
}

bool readTemperatureNow() {
    float temperatureC = 0.0f;
    if (!eeg.readTemperature(1, temperatureC)) {
        printLastError(F("TEMP_READ_FAIL, LAST_ERROR="));
        return false;
    }

    Serial.print(F("TEMPERATURE_C="));
    Serial.println(temperatureC, 2);
    return true;
}

// -----------------------------------------------------------------------------
// Command handling
// -----------------------------------------------------------------------------
void handleChannelDisableCommand(char c) {
    if (c >= '1' && c <= '8') {
        const uint8_t channel = static_cast<uint8_t>(c - '0');
        setChannelEnabled(channel, false);
    }
}

void handleChannelEnableCommand(char c) {
    switch (c) {
        case 'a': setChannelEnabled(1, true); break;
        case 'z': setChannelEnabled(2, true); break;
        case 'e': setChannelEnabled(3, true); break;
        case 'r': setChannelEnabled(4, true); break;
        case 't': setChannelEnabled(5, true); break;
        case 'y': setChannelEnabled(6, true); break;
        case 'u': setChannelEnabled(7, true); break;
        case 'i': setChannelEnabled(8, true); break;
        default: break;
    }
}

void handleCommand(char c) {
    handleChannelDisableCommand(c);
    handleChannelEnableCommand(c);

    switch (c) {
        case 'X':
        case 'x':
            setAppOutputMode(APP_OUTPUT_TEXT);
            startInCurrentMode();
            break;

        case 'B':
        case 'b':
            setAppOutputMode(APP_OUTPUT_BINARY);
            startInCurrentMode();
            break;

        case 'W':
        case 'w':
            setAppOutputMode(APP_OUTPUT_VOID);
            startInCurrentMode();
            break;

        case 'D':
        case 'd':
            setAppOutputMode(APP_OUTPUT_DEBUG);
            startInCurrentMode();
            break;

        case 'S':
            stopCurrentStreaming();
            Serial.println(F("STREAMING_STOPPED"));
            break;

        case 'R':
            startInCurrentMode();
            Serial.println(F("STREAMING_RESUMED"));
            break;

        case 'n':
            setAllChannelsNormal();
            break;

        case 'q':
            setAllChannelsShorted();
            break;

        case 's':
            setAllChannelsTestSignal(TEST_AMP_1x, TEST_FREQ_fCLK_21);
            break;

        case 'g':
            setAllChannelsTestSignal(TEST_AMP_2x, TEST_FREQ_fCLK_21);
            break;

        case 'j':
            setAllChannelsGain(PGA_GAIN_1);
            break;

        case 'k':
            setAllChannelsGain(PGA_GAIN_6);
            break;

        case 'l':
            setAllChannelsGain(PGA_GAIN_24);
            break;

        case '0':
            dumpRegistersSafely();
            break;

        case 'T':
            readTemperatureNow();
            break;

        case 'P':
        case 'p':
            printStats();
            break;

        case 'F':
            app.filtersEnabled = true;
            resetFilterState();
            Serial.println(F("FILTERS_ENABLED"));
            break;

        case 'N':
            app.filtersEnabled = false;
            resetFilterState();
            Serial.println(F("FILTERS_DISABLED"));
            break;

        case 'H':
        case 'h':
            printHelp();
            break;

        case '.':
            Serial.println(F("RESTARTING..."));
            delay(100);
            ESP.restart();
            break;

        default:
            break;
    }
}

void serviceSerialCommands() {
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        handleCommand(c);
    }
}

// -----------------------------------------------------------------------------
// Startup selection
// -----------------------------------------------------------------------------
AppOutputMode waitForStartupSelection() {
    digitalWrite(PIN_LED1, HIGH);
    printStartupPrompt();

    while (true) {
        if (digitalRead(PIN_BUTTON) == LOW) {
            Serial.println(F("BUTTON_START -> TEXT"));
            delay(200);
            return APP_OUTPUT_TEXT;
        }

        if (Serial.available() > 0) {
            const char c = static_cast<char>(Serial.read());
            switch (c) {
                case 'X':
                case 'x':
                    return APP_OUTPUT_TEXT;

                case 'B':
                case 'b':
                    return APP_OUTPUT_BINARY;

                case 'W':
                case 'w':
                    return APP_OUTPUT_VOID;

                case 'D':
                case 'd':
                    return APP_OUTPUT_DEBUG;

                case 'H':
                case 'h':
                    printHelp();
                    break;

                default:
                    Serial.println(F("INVALID_START_COMMAND"));
                    printStartupPrompt();
                    break;
            }
        }

        delay(10);
    }
}

// -----------------------------------------------------------------------------
// Acquisition
// -----------------------------------------------------------------------------
void acquireAndOutput() {
    if (!app.streaming) {
        updateLed2State();
        return;
    }

    if (!eeg.dataAvailable()) {
        return;
    }

    if (!eeg.readAllChannels(rawSamples, NUM_CHANNELS)) {
        printLastError(F("READ_FAIL, LAST_ERROR="));
        return;
    }

    applyOptionalProcessing();
    emitCurrentFrame();

    app.sampleCounter++;
    updateLed2State();
}

// -----------------------------------------------------------------------------
// Arduino entry points
// -----------------------------------------------------------------------------
void setup() {
    Serial.begin(SERIAL_BAUD);

    pinMode(PIN_LED1, OUTPUT);
    pinMode(PIN_LED2, OUTPUT);
    pinMode(PIN_BUTTON, INPUT_PULLUP);

    digitalWrite(PIN_LED1, LOW);
    digitalWrite(PIN_LED2, LOW);

    delay(STARTUP_DELAY_MS);

    config.num_channels     = NUM_CHANNELS;
    config.data_rate        = DR_250;
    config.pga_gain         = PGA_GAIN_24;
    config.output_mode      = OUTPUT_BINARY;
    config.enable_filter    = false;
    config.enable_drl       = false;
    config.drl_internal_ref = false;
    config.daisy_chain      = false;
    config.num_modules      = 1;
    config.spi_clock_hz     = DEFAULT_SPI_HZ;
    config.drdy_timeout_ms  = 100;

    printSeparator();
    Serial.println(F("Sketch 07 - MindStreamer Interactive Workbench"));

    if (!eeg.begin(config)) {
        Serial.print(F("BEGIN_FAIL,"));
        Serial.println((int)eeg.getLastError());
        while (true) {
            delay(1000);
        }
    }

    Serial.print(F("ID,0x"));
    Serial.println(eeg.getDeviceID(), HEX);

    resetFilterState();
    printHelp();

    app.outputMode = waitForStartupSelection();
    app.startupComplete = true;

    // ***** FIX: Do NOT automatically start streaming after mode selection *****
    // The user must explicitly send X, B, W, D, or R to start streaming.
    Serial.println(F("Ready. Send X (text), B (binary), W (void), D (debug), or R (resume) to start streaming."));
    // ------------------------------------------------------------------------
}

void loop() {
    serviceSerialCommands();
    acquireAndOutput();
}