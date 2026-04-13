/**
 * @file MindStreamer_Types.h
 * @brief Core types and configurations for MindStreamer library
 * @author Dr. Hafed-Eddine Bendib
 * @version 1.0.0
 */

#ifndef MINDSTREAMER_TYPES_H
#define MINDSTREAMER_TYPES_H

#include <stdint.h>
#include <stddef.h>

//=============================================================================
// Data Rates (Samples per second)
//=============================================================================

enum DataRate : uint32_t {
    DR_250  = 250,
    DR_500  = 500,
    DR_1K   = 1000,
    DR_2K   = 2000,
    DR_4K   = 4000,
    DR_8K   = 8000,
    DR_16K  = 16000
};

//=============================================================================
// PGA Gain Settings
//=============================================================================

enum PGAGain : uint8_t {
    PGA_GAIN_1  = 1,
    PGA_GAIN_2  = 2,
    PGA_GAIN_4  = 4,
    PGA_GAIN_6  = 6,
    PGA_GAIN_8  = 8,
    PGA_GAIN_12 = 12,
    PGA_GAIN_24 = 24
};

//=============================================================================
// Output Data Formats
//=============================================================================

enum OutputMode : uint8_t {
    OUTPUT_DEBUG,    // Register dump and verbose output
    OUTPUT_TEXT,     // Human-readable CSV
    OUTPUT_BINARY,   // Binary packet format (0xBB...0xEE)
    OUTPUT_OPENEEG   // BrainBay P2 protocol
};

//=============================================================================
// Error Codes
//=============================================================================

enum ErrorCode : uint8_t {
    ERROR_NONE = 0,
    ERROR_SPI_TIMEOUT,
    ERROR_DEVICE_NOT_FOUND,
    ERROR_INVALID_CHANNEL,
    ERROR_INVALID_DATA_RATE,
    ERROR_STREAMING_ACTIVE,
    ERROR_STREAMING_INACTIVE,
    ERROR_REGISTER_READ,
    ERROR_REGISTER_WRITE,
    ERROR_DRDY_TIMEOUT,
    ERROR_INVALID_CONFIG
};

//=============================================================================
// Lead-Off Current Settings
//=============================================================================

enum LeadOffCurrent : uint8_t {
    LEAD_OFF_OFF   = 0,  // Disabled
    LEAD_OFF_6nA   = 1,  // 6 nA
    LEAD_OFF_12nA  = 2,  // 12 nA (recommended)
    LEAD_OFF_18nA  = 3,  // 18 nA
    LEAD_OFF_24nA  = 4   // 24 nA
};

//=============================================================================
// Test Signal Configuration
//=============================================================================

enum TestSignalFreq : uint8_t {
    TEST_FREQ_DC     = 0,   // DC
    TEST_FREQ_fCLK_20 = 20, // fCLK / 2^20
    TEST_FREQ_fCLK_21 = 21  // fCLK / 2^21 (default)
};

enum TestSignalAmp : uint8_t {
    TEST_AMP_1x = 1,  // 1x amplitude
    TEST_AMP_2x = 2   // 2x amplitude
};

//=============================================================================
// Main Configuration Structure
//=============================================================================

struct MindStreamerConfig {
    // Channel configuration
    uint8_t num_channels = 8;           // 1-8 (more with daisy chain)
    PGAGain pga_gain = PGA_GAIN_6;      // Default gain = 6
    
    // Sampling configuration
    DataRate data_rate = DR_250;        // Default 250 SPS
    
    // Output configuration
    OutputMode output_mode = OUTPUT_BINARY;
    
    // Optional DSP filtering (disabled by default)
    bool enable_filter = false;
    float low_cutoff_hz = 0.5;
    float high_cutoff_hz = 40.0;
    bool enable_notch_50hz = true;
    
    // Daisy chain (for multi-module systems)
    bool daisy_chain = false;
    uint8_t num_modules = 1;            // 1 = 8CH, 2 = 16CH
    
    // DRL (Right-Leg Drive) configuration
    bool enable_drl = true;
    bool drl_internal_ref = true;       // Use internal BIASREF signal
    
    // Test signal (for debugging)
    bool enable_test_signal = false;
    uint8_t test_signal_channel = 1;
    
    // Performance tuning
    uint32_t spi_clock_hz = 1000000;    // 1 MHz default
    uint16_t drdy_timeout_ms = 100;     // DRDY timeout in ms
};

//=============================================================================
// Performance Statistics Structure
//=============================================================================

struct MindStreamerStats {
    uint32_t total_samples;      // Total samples acquired since last reset
    uint32_t dropped_samples;    // Samples dropped due to buffer overflow
    uint32_t last_sample_time_ms;// Timestamp of last sample (milliseconds)
    float actual_sps;            // Actual achieved sampling rate
    ErrorCode last_error;        // Last error encountered
};

//=============================================================================
// Channel-Specific Settings (Advanced)
//=============================================================================

struct ChannelConfig {
    bool enabled = true;
    PGAGain gain = PGA_GAIN_6;
    bool srb2_enabled = false;
    uint8_t input_select = 0;    // 0=normal, 1=shorted, 2=bias meas, 4=temp, 5=test
};

#endif // MINDSTREAMER_TYPES_H