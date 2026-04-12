/**
 * @file MindStreamer.h
 * @brief Main API for MindStreamer EEG acquisition library
 * @author Dr. Hafed-Eddine Bendib
 * @version 1.0.0
 */

#ifndef MINDSTREAMER_H
#define MINDSTREAMER_H

#include "include/MindStreamer_Types.h"
#include "core/ADS1299_Registers.h"
#include "core/ADS1299_SPI.h"
#include "output/DataStreamer.h"

class MindStreamer {
public:
    /**
     * @brief Constructor with default pins
     * @param drdy_pin Data ready pin (default: 16)
     * @param cs_pin Chip select pin (default: 15)
     * @param rst_pin Reset pin (default: 13)
     */
    MindStreamer(int drdy_pin = 16, int cs_pin = 15, int rst_pin = 13);
    
    /**
     * @brief Destructor
     */
    ~MindStreamer();
    
    //=========================================================================
    // Lifecycle Methods
    //=========================================================================
    
    /**
     * @brief Initialize MindStreamer with configuration
     * @param config Configuration structure
     * @return true if successful
     */
    bool begin(const MindStreamerConfig& config);
    
    /**
     * @brief Shutdown and release resources
     */
    void end();
    
    /**
     * @brief Check if initialized
     */
    bool isInitialized() const { return _initialized; }
    
    //=========================================================================
    // Configuration Methods
    //=========================================================================
    
    /**
     * @brief Change sampling rate (restarts streaming if active)
     * @param rate New data rate
     * @return true if successful
     */
    bool setDataRate(DataRate rate);
    
    /**
     * @brief Set PGA gain for channel(s)
     * @param gain Gain value
     * @param channel 0 = all channels, 1-8 = specific channel
     * @return true if successful
     */
    bool setGain(PGAGain gain, uint8_t channel = 0);
    
    /**
     * @brief Change output format
     * @param mode New output mode
     * @return true if successful
     */
    bool setOutputMode(OutputMode mode);
    
    /**
     * @brief Enable/disable a specific channel
     * @param channel Channel number (1-8)
     * @param enable true = enable, false = disable
     * @return true if successful
     */
    bool enableChannel(uint8_t channel, bool enable);
    
    /**
     * @brief Enable/disable Right-Leg Drive (DRL)
     * @param enable true = enable DRL
     * @return true if successful
     */
    bool enableDRL(bool enable);
    
    /**
     * @brief Add channel to DRL derivation
     * @param channel Channel number (1-8)
     * @return true if successful
     */
    bool addChannelToDRL(uint8_t channel);
    
    /**
     * @brief Enable SRB1 (connects all inverting inputs)
     * @param enable true = close SRB1 switch
     */
    void enableSRB1(bool enable);
    
    /**
     * @brief Enable SRB2 for specific channel
     * @param channel Channel number (1-8)
     * @param enable true = close SRB2 switch
     */
    void enableSRB2(uint8_t channel, bool enable);
    
    //=========================================================================
    // Streaming Control
    //=========================================================================
    
    /**
     * @brief Start continuous acquisition
     * @return true if successful
     */
    bool startStreaming();
    
    /**
     * @brief Stop acquisition
     * @return true if successful
     */
    bool stopStreaming();
    
    /**
     * @brief Check if streaming is active
     */
    bool isStreaming() const { return _is_streaming; }
    
    /**
     * @brief Check if new data is available
     * @return true if DRDY is low (data ready)
     */
    bool dataAvailable();
    
    //=========================================================================
    // Data Reading
    //=========================================================================
    
    /**
     * @brief Read all active channels
     * @param samples Output buffer (must hold at least num_channels)
     * @param max_channels Size of output buffer
     * @return true if successful
     */
    bool readAllChannels(int32_t* samples, uint8_t max_channels);
    
    /**
     * @brief Read a single channel
     * @param channel Channel number (1-8)
     * @return Sample value (24-bit, two's complement)
     */
    int32_t readChannel(uint8_t channel);
    
    /**
     * @brief Read and automatically output based on configured mode
     * @return true if successful
     */
    bool readAndStream();
    
    //=========================================================================
    // Status & Diagnostics
    //=========================================================================
    
    /**
     * @brief Get ADS1299 device ID
     * @return Device ID (should be 0x3E)
     */
    uint8_t getDeviceID();
    
    /**
     * @brief Get last error code
     */
    ErrorCode getLastError() const { return _last_error; }
    
    /**
     * @brief Get performance statistics
     */
    MindStreamerStats getStats() const { return _stats; }
    
    /**
     * @brief Print all register values (debug)
     */
    void printRegisterMap();
    
private:
    // Pin assignments
    int _drdy_pin;
    int _cs_pin;
    int _rst_pin;
    
    // Core components
    ADS1299_SPI _spi;
    MindStreamerConfig _config;
    DataStreamer* _streamer;
    
    // State
    bool _initialized;
    bool _is_streaming;
    ErrorCode _last_error;
    MindStreamerStats _stats;
    
    // Data buffers
    int32_t _samples[16];  // Support up to 16 channels
    uint8_t _status[3];
    uint32_t _sample_counter;
    
    // Internal methods
    bool _configureDevice();
    bool _verifyDevice();
    void _updateStreamer();
    float _convertToMicrovolts(int32_t raw_sample);
};

#endif // MINDSTREAMER_H