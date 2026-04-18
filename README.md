# MindStreamer
A clean, modern, production-ready library for EEG acquisition using ESP32 and ADS1299 analog front-end.
# MindStreamer

MindStreamer is an Arduino/ESP32 library for EEG acquisition with the **ADS1299** biopotential front end. It focuses on a clean acquisition lifecycle, explicit device configuration, multiple output formats, and practical validation tools for bring-up, debugging, and host-side integration.

It is designed first for a **single 8-channel ADS1299 board**, with future expansion toward multi-board daisy-chain setups.

---

## Features

- ADS1299 8-channel acquisition
- ESP32 hardware SPI communication
- Multiple output formats:
  - `OUTPUT_DEBUG`
  - `OUTPUT_TEXT`
  - `OUTPUT_BINARY`
  - `OUTPUT_OPENEEG`
- Per-channel controls:
  - enable / disable
  - gain
  - input mux
  - test signal
- DRL / bias support
- Lead-off support
- Temperature reading helpers
- Shorted-input and internal test-signal diagnostics
- Recovery-friendly lifecycle:
  - `begin()`
  - `startStreaming()`
  - `stopStreaming()`
  - `end()`
  - `begin()` again on the same object

---

## Current Target Hardware

Validated target:

- **ADC**: ADS1299, 8 channels
- **Host**: ESP32
- **Primary operating point**: `DR_250`
- **Recommended SPI clock**: `500000` Hz

> Recommendation: use **500 kHz SPI** as the default operating point for the currently validated 8-channel setup.

---

## Validated Pinout

### ADS1299 ↔ ESP32

- `DRDY  -> GPIO12`
- `CS    -> GPIO15`
- `RESET -> GPIO13`
- `MOSI  -> GPIO23`
- `MISO  -> GPIO19`
- `SCLK  -> GPIO18`

### Optional GPIOs

- `LED1 (ready)     -> GPIO14`
- `LED2 (streaming) -> GPIO27`
- `BUTTON           -> GPIO5`

---

## Installation

### Arduino IDE

1. Copy the `MindStreamer` folder into your Arduino libraries directory.
2. Restart the Arduino IDE.
3. Open **File > Examples** and look for **MindStreamer**.

### PlatformIO

Add the library as a local library or place it inside your project `lib/` folder.

---

## Supported Configuration

### Data Rates

- `DR_250`
- `DR_500`
- `DR_1K`
- `DR_2K`
- `DR_4K`
- `DR_8K`
- `DR_16K`

### PGA Gain

- `PGA_GAIN_1`
- `PGA_GAIN_2`
- `PGA_GAIN_4`
- `PGA_GAIN_6`
- `PGA_GAIN_8`
- `PGA_GAIN_12`
- `PGA_GAIN_24`

### Output Modes

- `OUTPUT_DEBUG`
- `OUTPUT_TEXT`
- `OUTPUT_BINARY`
- `OUTPUT_OPENEEG`

### Common Error Codes

- `ERROR_NONE`
- `ERROR_SPI_TIMEOUT`
- `ERROR_DEVICE_NOT_FOUND`
- `ERROR_INVALID_CHANNEL`
- `ERROR_INVALID_DATA_RATE`
- `ERROR_STREAMING_ACTIVE`
- `ERROR_STREAMING_INACTIVE`
- `ERROR_REGISTER_READ`
- `ERROR_REGISTER_WRITE`
- `ERROR_DRDY_TIMEOUT`
- `ERROR_INVALID_CONFIG`

---

## Recommended Default Configuration

For the current single-board 8-channel setup:

```cpp
MindStreamerConfig config;
config.num_channels     = 8;
config.data_rate        = DR_250;
config.pga_gain         = PGA_GAIN_24;
config.output_mode      = OUTPUT_BINARY;
config.enable_filter    = false;
config.enable_drl       = false;
config.drl_internal_ref = false;
config.daisy_chain      = false;
config.num_modules      = 1;
config.spi_clock_hz     = 500000;
config.drdy_timeout_ms  = 100;
```

---

## Quick Start

```cpp
#include <Arduino.h>
#include <MindStreamer.h>

static constexpr int PIN_DRDY = 12;
static constexpr int PIN_CS   = 15;
static constexpr int PIN_RST  = 13;

MindStreamer eeg(PIN_DRDY, PIN_CS, PIN_RST);
MindStreamerConfig config;

void setup() {
    Serial.begin(115200);
    delay(1500);

    config.num_channels     = 8;
    config.data_rate        = DR_250;
    config.pga_gain         = PGA_GAIN_24;
    config.output_mode      = OUTPUT_TEXT;
    config.enable_filter    = false;
    config.enable_drl       = false;
    config.drl_internal_ref = false;
    config.daisy_chain      = false;
    config.num_modules      = 1;
    config.spi_clock_hz     = 500000;
    config.drdy_timeout_ms  = 100;

    if (!eeg.begin(config)) {
        Serial.print("BEGIN_FAIL,");
        Serial.println((int)eeg.getLastError());
        while (true) delay(1000);
    }

    if (!eeg.startStreaming()) {
        Serial.print("START_FAIL,");
        Serial.println((int)eeg.getLastError());
        while (true) delay(1000);
    }
}

void loop() {
    if (eeg.dataAvailable()) {
        eeg.readAndStream();
    }
}
```

---

## Lifecycle

MindStreamer is intended to support this flow safely:

1. `begin(config)`
2. `startStreaming()`
3. `readAllChannels(...)` or `readAndStream()`
4. `stopStreaming()`
5. `end()`
6. `begin(config)` again if needed

This allows reconfiguration and recovery without requiring a board power cycle.

---

## Tutorials

### Tutorial 1 — Stream CSV to the Serial Monitor

Use this when you want a simple human-readable data stream.

```cpp
#include <Arduino.h>
#include <MindStreamer.h>

MindStreamer eeg(12, 15, 13);
MindStreamerConfig config;

void setup() {
    Serial.begin(115200);
    delay(1500);

    config.num_channels = 8;
    config.data_rate = DR_250;
    config.pga_gain = PGA_GAIN_24;
    config.output_mode = OUTPUT_TEXT;
    config.spi_clock_hz = 500000;

    if (!eeg.begin(config) || !eeg.startStreaming()) {
        while (true) delay(1000);
    }
}

void loop() {
    if (eeg.dataAvailable()) {
        eeg.readAndStream();
    }
}
```

Expected output:
- one CSV header line
- one row per sample

---

### Tutorial 2 — Read Raw Samples Yourself

Use this when you want to process samples inside the microcontroller or send them to your own protocol.

```cpp
#include <Arduino.h>
#include <MindStreamer.h>

MindStreamer eeg(12, 15, 13);
MindStreamerConfig config;
int32_t samples[8];

void setup() {
    Serial.begin(115200);
    delay(1500);

    config.num_channels = 8;
    config.data_rate = DR_250;
    config.pga_gain = PGA_GAIN_24;
    config.output_mode = OUTPUT_BINARY;
    config.spi_clock_hz = 500000;

    if (!eeg.begin(config) || !eeg.startStreaming()) {
        while (true) delay(1000);
    }
}

void loop() {
    if (eeg.dataAvailable()) {
        if (eeg.readAllChannels(samples, 8)) {
            Serial.print("CH1=");
            Serial.print(samples[0]);
            Serial.print(", CH2=");
            Serial.println(samples[1]);
        }
    }
}
```

---

### Tutorial 3 — Internal Test Signal Check

Use this before connecting electrodes. It helps separate acquisition problems from front-end or wiring problems.

```cpp
#include <Arduino.h>
#include <MindStreamer.h>

MindStreamer eeg(12, 15, 13);
MindStreamerConfig config;

void setup() {
    Serial.begin(115200);
    delay(1500);

    config.num_channels = 8;
    config.data_rate = DR_250;
    config.pga_gain = PGA_GAIN_24;
    config.output_mode = OUTPUT_TEXT;
    config.spi_clock_hz = 500000;

    if (!eeg.begin(config)) {
        while (true) delay(1000);
    }

    for (uint8_t ch = 1; ch <= 8; ++ch) {
        eeg.enableTestSignal(ch, PGA_GAIN_24, TEST_AMP_1x, TEST_FREQ_fCLK_21);
    }

    if (!eeg.startStreaming()) {
        while (true) delay(1000);
    }
}

void loop() {
    if (eeg.dataAvailable()) {
        eeg.readAndStream();
    }
}
```

If all channels behave similarly here, the acquisition path is probably healthy.

---

### Tutorial 4 — Shorted-Input Noise Check

Use this to verify stable low-amplitude readings with no external electrodes attached.

```cpp
#include <Arduino.h>
#include <MindStreamer.h>

MindStreamer eeg(12, 15, 13);
MindStreamerConfig config;
int32_t samples[8];

void setup() {
    Serial.begin(115200);
    delay(1500);

    config.num_channels = 8;
    config.data_rate = DR_250;
    config.pga_gain = PGA_GAIN_24;
    config.output_mode = OUTPUT_BINARY;
    config.spi_clock_hz = 500000;

    if (!eeg.begin(config)) {
        while (true) delay(1000);
    }

    for (uint8_t ch = 1; ch <= 8; ++ch) {
        eeg.disableTestSignal(ch);
        eeg.setChannelInputMux(ch, ADS1299_MUX_SHORTED);
    }

    if (!eeg.startStreaming()) {
        while (true) delay(1000);
    }
}

void loop() {
    if (eeg.dataAvailable() && eeg.readAllChannels(samples, 8)) {
        Serial.println(samples[0]);
    }
}
```

This mode is useful for checking raw-frame integrity and low-noise behavior.

---

## Output Modes

### `OUTPUT_DEBUG`
Verbose human-readable output for development and troubleshooting.

### `OUTPUT_TEXT`
CSV-style text output for quick inspection and logging.

### `OUTPUT_BINARY`
Framed binary output for efficient host-side decoding.

### `OUTPUT_OPENEEG`
Compatibility-oriented OpenEEG / BrainBay output mode.

> Note: OpenEEG should be treated as a compatibility transport mode. Document channel expectations clearly when using it with an 8-channel front end.

---

## Diagnostics and Debugging

Useful methods include:

- `getDeviceID()`
- `getLastError()`
- `printRegisterMap()`
- `getStats()`
- `debugReadFrameRaw(...)`

Recommended practice:

- use `printRegisterMap()` mainly when **not actively streaming**
- use `getStats()` for long-run monitoring
- use **shorted-input mode** and **internal test signal mode** before connecting electrodes
- treat floating inputs as a front-end condition, not as a valid signal-quality benchmark

---

## Validation Summary

The current validation campaign covered:

- cold-boot initialization reliability
- register configuration stability
- raw-frame integrity
- output mode validation
- long-run streaming stability
- same-object re-begin / recovery behavior

Validated behavior in the corrected configuration includes:

- stable repeated `begin()` on cold boot
- stable raw-frame acquisition in corrected shorted-input tests
- successful 10-minute streaming without dropped samples
- successful repeated re-begin / recovery cycles on the same object

---

## Known Limits and Notes

- **Recommended SPI clock is 500 kHz** for the currently validated 8-channel hardware path.
- **Floating inputs are not a valid signal-quality test.**
  Open biopotential inputs can produce saturated or pathological values.
- **Real electrode validation is separate from firmware robustness validation.**
- If you later enable multi-board daisy chain for 16 / 24 channels, repeat timing and framing validation before claiming the same operating limits.

---

## Troubleshooting

### `begin()` fails
Check:
- wiring
- power rails
- reset pin
- SPI pin mapping
- SPI clock setting

Recommended first fallback:
- use `spi_clock_hz = 500000`

### Data looks saturated with no electrodes attached
That can be normal for floating biopotential inputs. Use:
- internal test signal mode
- shorted-input mode

before concluding there is a transport bug.

### Streaming stops unexpectedly
Check:
- DRDY wiring
- host serial bottlenecks
- current SPI clock
- whether you are mixing heavy debug output with real-time acquisition

### Register dump looks wrong while streaming
Prefer register inspection when streaming is stopped.

---

## API Overview

Common methods:

- `begin(const MindStreamerConfig& config)`
- `end()`
- `startStreaming()`
- `stopStreaming()`
- `dataAvailable()`
- `readAllChannels(...)`
- `readAndStream()`
- `setDataRate(...)`
- `setGain(...)`
- `enableChannel(...)`
- `enableDRL(...)`
- `setChannelInputMux(...)`
- `enableTestSignal(...)`
- `disableTestSignal(...)`
- `printRegisterMap()`
- `getStats()`
- `getDeviceID()`
- `getLastError()`

---

## Roadmap

Planned cleanup and expansion items:

- add official examples under `examples/`
- add formal docs under `docs/`
- add automated tests under `tests/`
- complete multi-board daisy-chain validation
- document host-side parsers for binary and OpenEEG modes
- add a dedicated validation document with bring-up procedures

---

## Project Status

Current status for the single-board 8-channel path:

- robust initialization
- robust raw acquisition
- robust long-run streaming
- robust same-object recovery
- ready for the next phase: **real electrode validation**
