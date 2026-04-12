/**
 * @file MindStreamer.cpp
 * @brief Main implementation of MindStreamer EEG acquisition library
 * @author Dr. Hafed-Eddine Bendib
 * @version 1.0.0
 */

#include "MindStreamer.h"
#include "output/DebugStreamer.h"
#include "output/TextStreamer.h"
#include "output/BinaryStreamer.h"
#include "output/OpenEEGStreamer.h"

//=============================================================================
// Constructor & Destructor
//=============================================================================

MindStreamer::MindStreamer(int drdy_pin, int cs_pin, int rst_pin)
    : _drdy_pin(drdy_pin)
    , _cs_pin(cs_pin)
    , _rst_pin(rst_pin)
    , _spi(drdy_pin, cs_pin, rst_pin)
    , _streamer(nullptr)
    , _initialized(false)
    , _is_streaming(false)
    , _last_error(ERROR_NONE)
    , _sample_counter(0) {
    
    _stats.total_samples = 0;
    _stats.dropped_samples = 0;
    _stats.last_sample_time_ms = 0;
    _stats.actual_sps = 0;
    _stats.last_error = ERROR_NONE;
}

MindStreamer::~MindStreamer() {
    end();
    if (_streamer) {
        delete _streamer;
        _streamer = nullptr;
    }
}

//=============================================================================
// Lifecycle Methods
//=============================================================================

bool MindStreamer::begin(const MindStreamerConfig& config) {
    _config = config;
    
    // Initialize SPI
    if (!_spi.begin(_config.spi_clock_hz)) {
        _last_error = _spi.getLastError();
        return false;
    }
    
    // Create streamer based on output mode
    _updateStreamer();
    if (_streamer) {
        _streamer->begin();
    }
    
    // Configure device
    if (!_configureDevice()) {
        return false;
    }
    
    // Verify device is working
    if (!_verifyDevice()) {
        _last_error = ERROR_DEVICE_NOT_FOUND;
        return false;
    }
    
    _initialized = true;
    _last_error = ERROR_NONE;
    
    return true;
}

void MindStreamer::end() {
    if (_is_streaming) {
        stopStreaming();
    }
    _spi.end();
    _initialized = false;
}

//=============================================================================
// Configuration Methods
//=============================================================================

bool MindStreamer::setDataRate(DataRate rate) {
    if (_is_streaming) {
        _last_error = ERROR_STREAMING_ACTIVE;
        return false;
    }
    
    _config.data_rate = rate;
    
    uint8_t config1;
    if (!_spi.readRegister(ADS1299_REG_CONFIG1, &config1)) {
        return false;
    }
    
    // Clear DR bits and set new rate
    config1 &= ~ADS1299_CONFIG1_DR_MASK;
    switch (rate) {
        case DR_250:  config1 |= ADS1299_CONFIG1_DR_250; break;
        case DR_500:  config1 |= ADS1299_CONFIG1_DR_500; break;
        case DR_1K:   config1 |= ADS1299_CONFIG1_DR_1000; break;
        case DR_2K:   config1 |= ADS1299_CONFIG1_DR_2000; break;
        case DR_4K:   config1 |= ADS1299_CONFIG1_DR_4000; break;
        case DR_8K:   config1 |= ADS1299_CONFIG1_DR_8000; break;
        case DR_16K:  config1 |= ADS1299_CONFIG1_DR_16000; break;
        default: return false;
    }
    
    return _spi.writeRegister(ADS1299_REG_CONFIG1, config1);
}

bool MindStreamer::setGain(PGAGain gain, uint8_t channel) {
    if (_is_streaming) {
        _last_error = ERROR_STREAMING_ACTIVE;
        return false;
    }
    
    uint8_t gain_mask;
    switch (gain) {
        case PGA_GAIN_1:  gain_mask = ADS1299_GAIN_1; break;
        case PGA_GAIN_2:  gain_mask = ADS1299_GAIN_2; break;
        case PGA_GAIN_4:  gain_mask = ADS1299_GAIN_4; break;
        case PGA_GAIN_6:  gain_mask = ADS1299_GAIN_6; break;
        case PGA_GAIN_8:  gain_mask = ADS1299_GAIN_8; break;
        case PGA_GAIN_12: gain_mask = ADS1299_GAIN_12; break;
        case PGA_GAIN_24: gain_mask = ADS1299_GAIN_24; break;
        default: return false;
    }
    
    uint8_t start_ch, end_ch;
    if (channel == 0) {
        start_ch = 0;
        end_ch = _config.num_channels;
    } else {
        start_ch = channel - 1;
        end_ch = channel;
    }
    
    for (uint8_t ch = start_ch; ch < end_ch; ch++) {
        uint8_t chnset;
        if (!_spi.readRegister(ADS1299_REG_CH1SET + ch, &chnset)) {
            return false;
        }
        chnset &= ~ADS1299_CHNSET_GAIN_MASK;
        chnset |= gain_mask;
        if (!_spi.writeRegister(ADS1299_REG_CH1SET + ch, chnset)) {
            return false;
        }
    }
    
    _config.pga_gain = gain;
    return true;
}

bool MindStreamer::setOutputMode(OutputMode mode) {
    if (_is_streaming) {
        _last_error = ERROR_STREAMING_ACTIVE;
        return false;
    }
    
    _config.output_mode = mode;
    _updateStreamer();
    if (_streamer) {
        _streamer->begin();
    }
    return true;
}

bool MindStreamer::enableChannel(uint8_t channel, bool enable) {
    if (channel < 1 || channel > _config.num_channels) {
        _last_error = ERROR_INVALID_CHANNEL;
        return false;
    }
    
    if (_is_streaming) {
        _last_error = ERROR_STREAMING_ACTIVE;
        return false;
    }
    
    uint8_t chnset;
    if (!_spi.readRegister(ADS1299_REG_CH1SET + channel - 1, &chnset)) {
        return false;
    }
    
    if (enable) {
        chnset &= ~ADS1299_CHNSET_PD;
        // Set to normal input mode
        chnset &= ~ADS1299_CHNSET_MUX_MASK;
        chnset |= ADS1299_MUX_NORMAL;
    } else {
        chnset |= ADS1299_CHNSET_PD;
        // Set to input shorted when powered down (recommended)
        chnset &= ~ADS1299_CHNSET_MUX_MASK;
        chnset |= ADS1299_MUX_SHORTED;
    }
    
    return _spi.writeRegister(ADS1299_REG_CH1SET + channel - 1, chnset);
}

bool MindStreamer::enableDRL(bool enable) {
    if (_is_streaming) {
        _last_error = ERROR_STREAMING_ACTIVE;
        return false;
    }
    
    uint8_t config3;
    if (!_spi.readRegister(ADS1299_REG_CONFIG3, &config3)) {
        return false;
    }
    
    if (enable) {
        config3 |= ADS1299_CONFIG3_BIAS_BUFFER;
        if (_config.drl_internal_ref) {
            config3 |= ADS1299_CONFIG3_BIAS_REF_INT;
        }
    } else {
        config3 &= ~ADS1299_CONFIG3_BIAS_BUFFER;
    }
    
    _config.enable_drl = enable;
    return _spi.writeRegister(ADS1299_REG_CONFIG3, config3);
}

bool MindStreamer::addChannelToDRL(uint8_t channel) {
    if (channel < 1 || channel > _config.num_channels) {
        _last_error = ERROR_INVALID_CHANNEL;
        return false;
    }
    
    uint8_t bias_sensp, bias_sensn;
    if (!_spi.readRegister(ADS1299_REG_BIAS_SENSP, &bias_sensp)) return false;
    if (!_spi.readRegister(ADS1299_REG_BIAS_SENSN, &bias_sensn)) return false;
    
    bias_sensp |= (1 << (channel - 1));
    bias_sensn |= (1 << (channel - 1));
    
    if (!_spi.writeRegister(ADS1299_REG_BIAS_SENSP, bias_sensp)) return false;
    if (!_spi.writeRegister(ADS1299_REG_BIAS_SENSN, bias_sensn)) return false;
    
    return true;
}

void MindStreamer::enableSRB1(bool enable) {
    uint8_t misc1;
    if (_spi.readRegister(ADS1299_REG_MISC1, &misc1)) {
        if (enable) {
            misc1 |= ADS1299_MISC1_SRB1;
        } else {
            misc1 &= ~ADS1299_MISC1_SRB1;
        }
        _spi.writeRegister(ADS1299_REG_MISC1, misc1);
    }
}

void MindStreamer::enableSRB2(uint8_t channel, bool enable) {
    if (channel < 1 || channel > _config.num_channels) return;
    
    uint8_t chnset;
    if (_spi.readRegister(ADS1299_REG_CH1SET + channel - 1, &chnset)) {
        if (enable) {
            chnset |= ADS1299_CHNSET_SRB2;
        } else {
            chnset &= ~ADS1299_CHNSET_SRB2;
        }
        _spi.writeRegister(ADS1299_REG_CH1SET + channel - 1, chnset);
    }
}

//=============================================================================
// Streaming Control
//=============================================================================

bool MindStreamer::startStreaming() {
    if (!_initialized) return false;
    if (_is_streaming) return true;
    
    // Ensure in RDATAC mode
    if (!_spi.sendCommand(ADS1299_CMD_SDATAC)) return false;
    delayMicroseconds(ADS1299_T_SDECODE_US);
    
    if (!_spi.sendCommand(ADS1299_CMD_RDATAC)) return false;
    delayMicroseconds(ADS1299_T_SDECODE_US);
    
    // Start conversions
    if (!_spi.sendCommand(ADS1299_CMD_START)) return false;
    
    _is_streaming = true;
    _sample_counter = 0;
    _stats.last_sample_time_ms = millis();
    
    return true;
}

bool MindStreamer::stopStreaming() {
    if (!_initialized) return false;
    if (!_is_streaming) return true;
    
    _spi.sendCommand(ADS1299_CMD_STOP);
    delayMicroseconds(100);
    _spi.sendCommand(ADS1299_CMD_SDATAC);
    
    _is_streaming = false;
    return true;
}

bool MindStreamer::dataAvailable() {
    return _spi.isDataReady();
}

//=============================================================================
// Data Reading
//=============================================================================

bool MindStreamer::readAllChannels(int32_t* samples, uint8_t max_channels) {
    if (!_initialized) return false;
    if (!_is_streaming) return false;
    
    uint8_t num_to_read = (_config.num_channels < max_channels) ? 
                          _config.num_channels : max_channels;
    
    bool success = _spi.readSample(_status, _samples, _config.num_channels);
    
    if (success) {
        memcpy(samples, _samples, num_to_read * sizeof(int32_t));
        _sample_counter++;
        _stats.total_samples++;
        
        // Update actual SPS estimate
        uint32_t now = millis();
        if (now - _stats.last_sample_time_ms >= 1000) {
            _stats.actual_sps = _stats.total_samples;
            _stats.total_samples = 0;
            _stats.last_sample_time_ms = now;
        }
    } else {
        _stats.dropped_samples++;
        _last_error = _spi.getLastError();
    }
    
    return success;
}

int32_t MindStreamer::readChannel(uint8_t channel) {
    if (!_initialized || !_is_streaming) return 0;
    if (channel < 1 || channel > _config.num_channels) return 0;
    
    if (_spi.readSample(_status, _samples, _config.num_channels)) {
        _sample_counter++;
        return _samples[channel - 1];
    }
    
    return 0;
}

bool MindStreamer::readAndStream() {
    if (!_initialized || !_is_streaming) return false;
    if (!_streamer) return false;
    
    if (_spi.readSample(_status, _samples, _config.num_channels)) {
        _streamer->streamSample(_sample_counter, _status, _samples, _config.num_channels);
        _sample_counter++;
        _stats.total_samples++;
        return true;
    }
    
    return false;
}

//=============================================================================
// Status & Diagnostics
//=============================================================================

uint8_t MindStreamer::getDeviceID() {
    uint8_t id = 0;
    _spi.readRegister(ADS1299_REG_ID, &id);
    return id;
}

void MindStreamer::printRegisterMap() {
    uint8_t registers[ADS1299_NUM_REGISTERS];
    if (_spi.readRegisters(0x00, ADS1299_NUM_REGISTERS, registers)) {
        if (_streamer) {
            _streamer->streamRegisterDump(registers, ADS1299_NUM_REGISTERS);
        }
    }
}

//=============================================================================
// Private Methods
//=============================================================================

bool MindStreamer::_configureDevice() {
    // Reset device
    _spi.hardwareReset();
    delay(100);
    
    // Set data rate
    if (!setDataRate(_config.data_rate)) return false;
    
    // Set gain for all channels
    if (!setGain(_config.pga_gain, 0)) return false;
    
    // Configure DRL
    if (_config.enable_drl) {
        enableDRL(true);
    }
    
    // Configure daisy chain
    _spi.setDaisyChainMode(_config.daisy_chain);
    
    // Ensure continuous conversion mode (not single-shot)
    uint8_t config4;
    if (_spi.readRegister(ADS1299_REG_CONFIG4, &config4)) {
        config4 &= ~ADS1299_CONFIG4_SINGLE_SHOT;
        _spi.writeRegister(ADS1299_REG_CONFIG4, config4);
    }
    
    return true;
}

bool MindStreamer::_verifyDevice() {
    uint8_t id = getDeviceID();
    return (id == ADS1299_DEFAULT_ID);
}

void MindStreamer::_updateStreamer() {
    if (_streamer) {
        delete _streamer;
        _streamer = nullptr;
    }
    
    switch (_config.output_mode) {
        case OUTPUT_DEBUG:
            _streamer = new DebugStreamer(Serial);
            break;
        case OUTPUT_TEXT:
            _streamer = new TextStreamer(Serial);
            break;
        case OUTPUT_BINARY:
            _streamer = new BinaryStreamer(Serial);
            break;
        case OUTPUT_OPENEEG:
            _streamer = new OpenEEGStreamer(Serial);
            break;
    }
}

float MindStreamer::_convertToMicrovolts(int32_t raw_sample) {
    // Vref = 4.5V, gain = config.pga_gain
    // V_in = (raw * Vref) / (gain * 2^23)
    const float VREF = 4.5f;
    const float SCALE = VREF * 1e6f / (_config.pga_gain * 8388608.0f);
    return raw_sample * SCALE;
}