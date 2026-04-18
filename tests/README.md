## Tested Behavior

The current validated configuration for the 8-channel ADS1299 + ESP32 front end is:

- **ADC**: ADS1299, 8 channels
- **MCU**: ESP32
- **Default sample rate tested**: **250 SPS**
- **Validated SPI clock**: **500 kHz**
- **SPI mode**: Mode 1 (`CPOL=0`, `CPHA=1`)
- **Streaming modes validated**:
  - Debug
  - Text / CSV
  - Binary
  - OpenEEG (functional output validation)

### Startup and Initialization

Cold-boot initialization has been validated across repeated resets. In the tested configuration, `begin()` succeeded on the **first attempt** on every recorded boot, the ADS1299 ID was consistently read as `0x3E`, and the baseline register map remained stable after initialization. :contentReference[oaicite:0]{index=0}

### Register Configuration Stability

Core configuration changes were validated without unintended register drift. For example, `setDataRate()` correctly updated `CONFIG1` across supported data-rate settings while leaving the remaining register map stable, and configuration changes were correctly blocked while streaming was active. :contentReference[oaicite:1]{index=1}

### Raw Frame Integrity

Raw 27-byte ADS1299 frame acquisition was validated in shorted-input mode. After the SPI stability fixes, the corrected raw-frame integrity run showed:

- `FRAMES_OK = 7506`
- `FRAMES_FAIL = 0`
- `ANOMALIES_TOTAL = 0`

This indicates stable frame acquisition with no detected tail-channel corruption in the validated configuration. :contentReference[oaicite:2]{index=2}

### Long-Run Streaming Stability

A 10-minute continuous acquisition run completed successfully in the validated configuration with:

- `READ_SUCCESS = 149774`
- `READ_FAIL = 0`
- `LIB_TOTAL_SAMPLES = 149774`
- `LIB_DROPPED_SAMPLES = 0`
- `LIB_LAST_ERROR = 0`

This confirms stable long-run streaming without the earlier silent stall observed in a non-validated configuration. :contentReference[oaicite:3]{index=3}

### Re-begin / Recovery Support

The library now supports re-initialization on the **same object instance** in one powered session. Repeated cycles of:

- `begin()`
- `startStreaming()`
- acquisition
- `stopStreaming()`
- `begin()` again

were validated successfully across multiple cycles, each with successful reads, zero read failures, and zero dropped samples. :contentReference[oaicite:4]{index=4}

## Known Limits and Notes

- **Recommended SPI clock is 500 kHz.**  
  Testing showed that higher SPI speed settings can reduce timing margin on this hardware path. In particular, **1 MHz** showed unreliable behavior in timing-sensitivity testing, while **500 kHz, 250 kHz, and 125 kHz** were clean in the targeted validation runs. 

- **Open inputs are not a valid signal-quality test.**  
  When electrode inputs are left floating, channels can show saturated or pathological values. This is expected behavior for unconnected biopotential inputs and should not be interpreted as a library transport failure. 

- **Real electrode validation is separate from firmware robustness validation.**  
  The robustness campaign validated initialization, configuration, raw-frame integrity, long-run streaming, and recovery behavior. Actual EEG signal quality with electrodes attached should be validated separately.

- **Register dumps are most trustworthy when the device is not actively streaming.**  
  For debugging and consistency checks, prefer register inspection when streaming is stopped.

## Recommended Default Configuration

For the current 8-channel single-board setup, the recommended default is:

- `num_channels = 8`
- `data_rate = DR_250`
- `pga_gain = PGA_GAIN_24`
- `spi_clock_hz = 500000`
- `daisy_chain = false`
- `num_modules = 1`

This is the configuration that was exercised through the main robustness validation flow. 