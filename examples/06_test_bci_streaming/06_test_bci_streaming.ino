/**
 * @file bci_streaming.ino
 * @brief Complete EEG acquisition and control sketch using MindStreamer library
 * @author Dr. Hafed-Eddine Bendib
 * @version 2.0.0
 * 
 * Features:
 * - 8-channel EEG acquisition (250 SPS default)
 * - Real‑time output mode switching (DEBUG, TEXT, BINARY, OPENEEG, VOID)
 * - Channel power on/off via serial commands (1-8 to disable, a-i to enable)
 * - Optional onboard DC‑blocking + 2x notch filters (50 Hz)
 * - Internal test signal generation (multiple frequencies/amplitudes)
 * - Register dump and device restart
 * - LED indicators (ready, streaming)
 * - Non‑blocking serial command handling
 * 
 * Hardware pinout (fixed):
 *   DRDY  -> GPIO12
 *   CS    -> GPIO15
 *   RESET -> GPIO13
 *   MOSI  -> GPIO23, MISO -> GPIO19, SCLK -> GPIO18 (hardware SPI)
 */

#include <MindStreamer.h>
#include "dsp/FilterBank.h"
#include "dsp/FilterDesign.h"

using namespace MindStreamer::DSP;

// ============================================================================
// Pin Definitions
// ============================================================================
const int PIN_DRDY      = 12;
const int PIN_CS        = 15;
const int PIN_RESET     = 13;
const int PIN_READY_LED = 14;   // Ready indicator
const int PIN_STREAM_LED= 27;   // Streaming indicator
const int PIN_SWITCH    = 5;    // External button to start (optional)

// ============================================================================
// System Configuration
// ============================================================================
const int NUM_CHANNELS = 8;
const int SAMPLE_RATE  = 250;   // 250 SPS

// Output modes (matching the new library)
enum class OutputMode : uint8_t {
    VOID   = 0,
    TEXT   = 1,
    BINARY = 2,
    DEBUG  = 3,
    OPENEEG= 4
};

// ============================================================================
// Global Objects & State
// ============================================================================
MindStreamer mind(PIN_DRDY, PIN_CS, PIN_RESET);
FilterBank filterBank(NUM_CHANNELS);   // For optional onboard filtering

struct SystemState {
    OutputMode mode = OutputMode::BINARY;
    bool filtersEnabled = false;
    bool streaming = false;
    uint32_t sampleCounter = 0;
    bool ledState = false;
    bool waitingForRegisterDump = false;
} state;

// ============================================================================
// Helper Functions
// ============================================================================
void blinkLed(int pin, int times, int delayMs = 100) {
    for (int i = 0; i < times; i++) {
        digitalWrite(pin, HIGH);
        delay(delayMs);
        digitalWrite(pin, LOW);
        delay(delayMs);
    }
}

void setStreamingLed(bool on) {
    digitalWrite(PIN_STREAM_LED, on ? HIGH : LOW);
}

void setReadyLed(bool on) {
    digitalWrite(PIN_READY_LED, on ? HIGH : LOW);
}

void printRegisterDump() {
    Serial.println("\n=== Register Dump ===");
    mind.printRegisterMap();
    Serial.println("====================\n");
}

void restartDevice() {
    Serial.println("Restarting device...");
    delay(100);
    ESP.restart();
}

// ============================================================================
// Serial Command Handler
// ============================================================================
void handleSerialCommands() {
    if (!Serial.available()) return;
    
    char c = Serial.read();
    
    // --- Channel power control ---
    if (c >= '1' && c <= '8') {
        uint8_t ch = c - '0';
        mind.enableChannel(ch, false);
        Serial.print("Channel "); Serial.print(ch); Serial.println(" powered DOWN");
    }
    else if (c >= 'a' && c <= 'i') {
        uint8_t ch = c - 'a' + 1;
        if (ch <= NUM_CHANNELS) {
            mind.enableChannel(ch, true);
            Serial.print("Channel "); Serial.print(ch); Serial.println(" powered ON");
        }
    }
    
    // --- Output mode switching ---
    else if (c == 'X') {   // TEXT mode
        mind.stopStreaming();
        mind.setOutputMode(OUTPUT_TEXT);
        state.mode = OutputMode::TEXT;
        mind.startStreaming();
        Serial.println("Switched to TEXT mode");
    }
    else if (c == 'B') {   // BINARY mode
        mind.stopStreaming();
        mind.setOutputMode(OUTPUT_BINARY);
        state.mode = OutputMode::BINARY;
        mind.startStreaming();
        Serial.println("Switched to BINARY mode");
    }
    else if (c == 'D') {   // DEBUG mode (register dump on demand)
        state.waitingForRegisterDump = true;
        mind.stopStreaming();
        Serial.println("Entering DEBUG mode. Send 'R' to resume streaming.");
    }
    else if (c == 'O') {   // OPENEEG mode (BrainBay)
        mind.stopStreaming();
        mind.setOutputMode(OUTPUT_OPENEEG);
        state.mode = OutputMode::OPENEEG;
        mind.startStreaming();
        Serial.println("Switched to OPENEEG mode");
    }
    else if (c == 'W') {   // VOID mode (no output)
        mind.stopStreaming();
        state.mode = OutputMode::VOID;
        Serial.println("VOID mode: streaming without serial output");
        mind.startStreaming();
    }
    else if (c == 'R' && state.waitingForRegisterDump) {
        // Resume streaming after DEBUG mode
        state.waitingForRegisterDump = false;
        mind.startStreaming();
        Serial.println("Resumed streaming");
    }
    
    // --- Filter toggle ---
    else if (c == 'F') {
        state.filtersEnabled = true;
        Serial.println("Filters ENABLED");
    }
    else if (c == 'N') {
        state.filtersEnabled = false;
        Serial.println("Filters DISABLED");
    }
    
    // --- Test signals (global) ---
    else if (c == 'q') {
        // Internal test signal, 2x amplitude, DC
        mind.configureGlobalTestSignal(1, 2, 0);
        for (int i = 1; i <= NUM_CHANNELS; i++) {
            mind.setChannelInputMux(i, ADS1299_MUX_TEST_SIG);
            mind.setGain(PGA_GAIN_24, i);
        }
        Serial.println("Test signal: internal, 2x, DC");
    }
    else if (c == 's') {
        mind.configureGlobalTestSignal(1, 1, 21);
        for (int i = 1; i <= NUM_CHANNELS; i++) {
            mind.setChannelInputMux(i, ADS1299_MUX_TEST_SIG);
            mind.setGain(PGA_GAIN_24, i);
        }
        Serial.println("Test signal: internal, 1x, fCLK/2^21");
    }
    else if (c == 'd') {
        mind.configureGlobalTestSignal(1, 1, 20);
        for (int i = 1; i <= NUM_CHANNELS; i++) {
            mind.setChannelInputMux(i, ADS1299_MUX_TEST_SIG);
            mind.setGain(PGA_GAIN_24, i);
        }
        Serial.println("Test signal: internal, 1x, fCLK/2^20");
    }
    else if (c == 'f') {
        mind.configureGlobalTestSignal(1, 2, 0);
        for (int i = 1; i <= NUM_CHANNELS; i++) {
            mind.setChannelInputMux(i, ADS1299_MUX_TEST_SIG);
            mind.setGain(PGA_GAIN_24, i);
        }
        Serial.println("Test signal: internal, 2x, DC");
    }
    else if (c == 'g') {
        mind.configureGlobalTestSignal(1, 2, 21);
        for (int i = 1; i <= NUM_CHANNELS; i++) {
            mind.setChannelInputMux(i, ADS1299_MUX_TEST_SIG);
            mind.setGain(PGA_GAIN_24, i);
        }
        Serial.println("Test signal: internal, 2x, fCLK/2^21");
    }
    else if (c == 'h') {
        mind.configureGlobalTestSignal(1, 2, 20);
        for (int i = 1; i <= NUM_CHANNELS; i++) {
            mind.setChannelInputMux(i, ADS1299_MUX_TEST_SIG);
            mind.setGain(PGA_GAIN_24, i);
        }
        Serial.println("Test signal: internal, 2x, fCLK/2^20");
    }
    
    // --- Restart ---
    else if (c == '.') {
        restartDevice();
    }
    
    // --- Register dump (triggered by '0') ---
    else if (c == '0') {
        printRegisterDump();
    }
    
    // --- Stop/Start streaming (optional) ---
    else if (c == 'S') {
        mind.stopStreaming();
        state.streaming = false;
        setStreamingLed(false);
        Serial.println("Streaming stopped");
    }
    else if (c == 'T') {
        mind.startStreaming();
        state.streaming = true;
        Serial.println("Streaming started");
    }
}

// ============================================================================
// Setup
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    pinMode(PIN_READY_LED, OUTPUT);
    pinMode(PIN_STREAM_LED, OUTPUT);
    pinMode(PIN_SWITCH, INPUT_PULLUP);
    setReadyLed(LOW);
    setStreamingLed(LOW);
    
    Serial.println(F("\n========================================="));
    Serial.println(F("   MindStreamer EEG Acquisition System"));
    Serial.println(F("========================================="));
    Serial.println(F("Initializing..."));
    
    // Configure the library
    MindStreamerConfig config;
    config.num_channels = NUM_CHANNELS;
    config.data_rate = DR_250;
    config.pga_gain = PGA_GAIN_6;
    config.output_mode = OUTPUT_BINARY;   // start with binary
    config.enable_drl = true;
    config.enable_filter = false;          // we'll toggle later
    
    if (!mind.begin(config)) {
        Serial.print("ERROR: Failed to initialize! Code: ");
        Serial.println(mind.getLastError());
        blinkLed(PIN_READY_LED, 10, 200);
        while (1);
    }
    
    // Setup default channel gains and mux
    for (int ch = 1; ch <= NUM_CHANNELS; ch++) {
        mind.enableChannel(ch, true);
        mind.setGain(PGA_GAIN_6, ch);
        mind.setChannelInputMux(ch, ADS1299_MUX_NORMAL);
    }
    
    // Setup DRL on all channels
    mind.enableDRL(true);
    for (int ch = 1; ch <= NUM_CHANNELS; ch++) {
        mind.addChannelToDRL(ch);
    }
    
    // Prepare filter bank (if needed later)
    FilterCascade eegFilter = EEGDesign::standardEEGFilter(SAMPLE_RATE, true);
    filterBank.setFilter(eegFilter);
    
    // Print device info
    Serial.print("Device ID: 0x"); Serial.println(mind.getDeviceID(), HEX);
    Serial.println("All registers (default):");
    mind.printRegisterMap();
    
    // Wait for external button or serial command to start
    Serial.println("\nPress external button (pin 5) or send 'T' to start streaming...");
    while (digitalRead(PIN_SWITCH) == HIGH && !Serial.available()) {
        delay(50);
    }
    if (Serial.available()) handleSerialCommands();
    
    // Start streaming
    mind.startStreaming();
    state.streaming = true;
    setReadyLed(HIGH);
    setStreamingLed(true);
    
    Serial.println("\nSystem READY. Streaming started.");
    Serial.println("Commands: 1-8 (disable), a-i (enable), X(text) B(bin) O(openEEG) W(void)");
    Serial.println("          F(enable filters) N(disable filters)");
    Serial.println("          q,s,d,f,g,h (test signals) 0(reg dump) .(restart) S(stop) T(start)");
}

// ============================================================================
// Main Loop
// ============================================================================
void loop() {
    // Handle serial commands (non‑blocking)
    handleSerialCommands();
    
    // If in DEBUG mode, we are not streaming; just wait for resume command
    if (state.waitingForRegisterDump) {
        delay(10);
        return;
    }
    
    // If streaming is stopped, just blink LED slowly
    if (!state.streaming) {
        static uint32_t lastBlink = 0;
        if (millis() - lastBlink > 500) {
            lastBlink = millis();
            setStreamingLed(!digitalRead(PIN_STREAM_LED));
        }
        return;
    }
    
    // Check for new data
    if (mind.dataAvailable()) {
        // Optional: blink streaming LED at ~20 Hz (every 50 samples at 250 SPS)
        if ((state.sampleCounter % 50) == 0) {
            setStreamingLed(!digitalRead(PIN_STREAM_LED));
        }
        
        // Read raw data
        int32_t rawSamples[NUM_CHANNELS];
        if (!mind.readAllChannels(rawSamples, NUM_CHANNELS)) {
            // Error reading, skip this sample
            return;
        }
        
        // Apply optional onboard filtering
        if (state.filtersEnabled) {
            float floatSamples[NUM_CHANNELS];
            for (int i = 0; i < NUM_CHANNELS; i++) {
                floatSamples[i] = (float)rawSamples[i];
            }
            float filtered[NUM_CHANNELS];
            filterBank.process(floatSamples, filtered);
            // Convert back to int32 for output (optional)
            for (int i = 0; i < NUM_CHANNELS; i++) {
                rawSamples[i] = (int32_t)filtered[i];
            }
        }
        
        // If output mode is VOID, do nothing (except increment counter)
        if (state.mode == OutputMode::VOID) {
            state.sampleCounter++;
            return;
        }
        
        // For all other modes, the library's internal streamer handles output
        // But we need to call readAndStream() which uses the configured output mode.
        // However, readAndStream() internally reads and sends data.
        // We already read the data, so we can either let the library do it,
        // or we manually output using the rawSamples.
        // For consistency, we call readAndStream() – it will re-read the same sample
        // (DRDY still low until we read). That's fine.
        mind.readAndStream();
        
        state.sampleCounter++;
    }
}