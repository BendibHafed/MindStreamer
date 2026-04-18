/**
 * @file MindStreamer.cpp
 * @brief Main implementation of MindStreamer EEG acquisition library
 * @author Dr. Hafed-Eddine Bendib
 * @version 1.0.0
 */

#include "MindStreamer.h"
#include "core/ADS1299_Registers.h"
#include "output/DebugStreamer.h"
#include "output/TextStreamer.h"
#include "output/BinaryStreamer.h"
#include "output/OpenEEGStreamer.h"
#include "math.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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
    , _sample_counter(0)
    , _sps_window_start_ms(0)
    , _sps_window_samples(0) {

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

bool MindStreamer::debugReadFrameRaw(uint8_t* raw_bytes, uint8_t num_channels) {
    if (!_initialized) {
        _last_error = ERROR_INVALID_CONFIG;
        return false;
    }

    if (!_is_streaming) {
        _last_error = ERROR_STREAMING_INACTIVE;
        return false;
    }

    const bool ok = _spi.debugReadFrameRaw(raw_bytes, num_channels);
    if (!ok) {
        _last_error = _spi.getLastError();
        return false;
    }

    _last_error = ERROR_NONE;
    return true;
}

bool MindStreamer::begin(const MindStreamerConfig& config) {
    // Allow re-begin on the same object instance.
    if (_initialized || _is_streaming) {
        end();
    }

    _config = config;

    _initialized = false;
    _is_streaming = false;
    _sample_counter = 0;
    _sps_window_start_ms = 0;
    _sps_window_samples = 0;

    _stats.total_samples = 0;
    _stats.dropped_samples = 0;
    _stats.last_sample_time_ms = 0;
    _stats.actual_sps = 0.0f;
    _stats.last_error = ERROR_NONE;

    _last_error = ERROR_NONE;

    if (!_spi.begin(_config.spi_clock_hz)) {
        _last_error = _spi.getLastError();
        return false;
    }

    _updateStreamer();
    if (_streamer) {
        _streamer->begin();
    }

    if (!_configureDevice()) {
        return false;
    }

    if (!_verifyDevice()) {
        _last_error = _spi.getLastError();
        if (_last_error == ERROR_NONE) {
            _last_error = ERROR_DEVICE_NOT_FOUND;
        }
        return false;
    }

    _initialized = true;
    _last_error = ERROR_NONE;
    _stats.last_error = ERROR_NONE;

    return true;
}

void MindStreamer::end() {
    if (_is_streaming) {
        stopStreaming();
    }

    _spi.end();

    if (_streamer) {
        delete _streamer;
        _streamer = nullptr;
    }

    _is_streaming = false;
    _initialized = false;
    _sample_counter = 0;
    _sps_window_start_ms = 0;
    _sps_window_samples = 0;

    _stats.total_samples = 0;
    _stats.dropped_samples = 0;
    _stats.last_sample_time_ms = 0;
    _stats.actual_sps = 0.0f;
    _stats.last_error = ERROR_NONE;

    _last_error = ERROR_NONE;
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
    if (channel > _config.num_channels && channel != 0) {
        _last_error = ERROR_INVALID_CHANNEL;
        return false;
    }

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
        chnset &= ~ADS1299_CHNSET_MUX_MASK;
        chnset |= ADS1299_MUX_NORMAL;
    } else {
        chnset |= ADS1299_CHNSET_PD;
        chnset &= ~ADS1299_CHNSET_MUX_MASK;
        chnset |= ADS1299_MUX_SHORTED;
    }

    return _spi.writeRegister(ADS1299_REG_CH1SET + channel - 1, chnset);
}

/**
 * @brief Enable or disable the ADS1299 bias-drive amplifier (DRL).
 */
bool MindStreamer::enableDRL(bool enable) {
    if (_is_streaming) {
        _last_error = ERROR_STREAMING_ACTIVE;
        return false;
    }

    uint8_t config3 = 0;
    if (!_spi.readRegister(ADS1299_REG_CONFIG3, &config3)) {
        _last_error = ERROR_REGISTER_READ;
        return false;
    }

    if (enable) {
        config3 |= ADS1299_CONFIG3_BIAS_BUFFER;

        if (_config.drl_internal_ref) {
            config3 |= ADS1299_CONFIG3_BIAS_REF_INT;
        } else {
            config3 &= ~ADS1299_CONFIG3_BIAS_REF_INT;
        }

        config3 |= ADS1299_CONFIG3_BIAS_SENS;
        config3 &= ~ADS1299_CONFIG3_BIAS_MEAS;
    } else {
        config3 &= ~ADS1299_CONFIG3_BIAS_BUFFER;
        config3 &= ~ADS1299_CONFIG3_BIAS_SENS;
        config3 &= ~ADS1299_CONFIG3_BIAS_MEAS;
        config3 &= ~ADS1299_CONFIG3_BIAS_LOFF_SENS;
        config3 &= ~ADS1299_CONFIG3_BIAS_REF_INT;
    }

    if (!_spi.writeRegister(ADS1299_REG_CONFIG3, config3)) {
        _last_error = ERROR_REGISTER_WRITE;
        return false;
    }

    // When DRL is disabled, also clear the selected BIAS source channels
    // so the register state stays logically consistent.
    if (!enable) {
        if (!_spi.writeRegister(ADS1299_REG_BIAS_SENSP, 0x00)) {
            _last_error = ERROR_REGISTER_WRITE;
            return false;
        }

        if (!_spi.writeRegister(ADS1299_REG_BIAS_SENSN, 0x00)) {
            _last_error = ERROR_REGISTER_WRITE;
            return false;
        }
    }

    _config.enable_drl = enable;
    _last_error = ERROR_NONE;
    return true;
}

/**
 * @brief Route one channel into the ADS1299 DRL/bias derivation network.
 */
bool MindStreamer::addChannelToDRL(uint8_t channel) {
    if (channel < 1 || channel > _config.num_channels) {
        _last_error = ERROR_INVALID_CHANNEL;
        return false;
    }

    if (_is_streaming) {
        _last_error = ERROR_STREAMING_ACTIVE;
        return false;
    }

    if (!_config.enable_drl) {
        if (!enableDRL(true)) {
            return false;
        }
    }

    uint8_t bias_sensp = 0;
    uint8_t bias_sensn = 0;

    if (!_spi.readRegister(ADS1299_REG_BIAS_SENSP, &bias_sensp)) {
        _last_error = ERROR_REGISTER_READ;
        return false;
    }

    if (!_spi.readRegister(ADS1299_REG_BIAS_SENSN, &bias_sensn)) {
        _last_error = ERROR_REGISTER_READ;
        return false;
    }

    bias_sensp |= (1u << (channel - 1));
    bias_sensn |= (1u << (channel - 1));

    if (!_spi.writeRegister(ADS1299_REG_BIAS_SENSP, bias_sensp)) {
        _last_error = ERROR_REGISTER_WRITE;
        return false;
    }

    if (!_spi.writeRegister(ADS1299_REG_BIAS_SENSN, bias_sensn)) {
        _last_error = ERROR_REGISTER_WRITE;
        return false;
    }

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
    if (!_initialized) {
        _last_error = ERROR_INVALID_CONFIG;
        return false;
    }

    if (_is_streaming) {
        _last_error = ERROR_STREAMING_ACTIVE;
        return false;
    }

    if (!_spi.sendCommand(ADS1299_CMD_SDATAC)) {
        _last_error = _spi.getLastError();
        return false;
    }
    delayMicroseconds(ADS1299_T_SDECODE_US);

    if (!_spi.sendCommand(ADS1299_CMD_START)) {
        _last_error = _spi.getLastError();
        return false;
    }
    delayMicroseconds(ADS1299_T_SDECODE_US);

    if (!_spi.sendCommand(ADS1299_CMD_RDATAC)) {
        _last_error = _spi.getLastError();
        return false;
    }
    delayMicroseconds(ADS1299_T_SDECODE_US);

    _is_streaming = true;
    _sample_counter = 0;

    const uint32_t now = millis();
    _sps_window_start_ms = now;
    _sps_window_samples = 0;

    _stats.last_sample_time_ms = 0;
    _stats.actual_sps = 0.0f;
    _stats.last_error = ERROR_NONE;

    _last_error = ERROR_NONE;
    return true;
}

bool MindStreamer::stopStreaming() {
    if (!_initialized) {
        _last_error = ERROR_INVALID_CONFIG;
        return false;
    }

    if (!_is_streaming) {
        _last_error = ERROR_STREAMING_INACTIVE;
        return false;
    }

    if (!_spi.sendCommand(ADS1299_CMD_STOP)) {
        _last_error = _spi.getLastError();
        return false;
    }
    delayMicroseconds(ADS1299_T_SDECODE_US);

    if (!_spi.sendCommand(ADS1299_CMD_SDATAC)) {
        _last_error = _spi.getLastError();
        return false;
    }
    delayMicroseconds(ADS1299_T_SDECODE_US);

    _is_streaming = false;
    _last_error = ERROR_NONE;
    return true;
}

bool MindStreamer::dataAvailable() {
    return _spi.isDataReady();
}

//=============================================================================
// Data Reading
//=============================================================================

bool MindStreamer::readAllChannels(int32_t* samples, uint8_t max_channels) {
    if (!_initialized) {
        _last_error = ERROR_INVALID_CONFIG;
        return false;
    }

    if (!_is_streaming) {
        _last_error = ERROR_STREAMING_INACTIVE;
        return false;
    }

    if (samples == nullptr || max_channels == 0) {
        _last_error = ERROR_INVALID_CONFIG;
        return false;
    }

    const uint8_t num_to_read =
        (_config.num_channels < max_channels) ? _config.num_channels : max_channels;

    const bool success = _spi.readSample(_status, _samples, _config.num_channels);

    if (success) {
        memcpy(samples, _samples, num_to_read * sizeof(int32_t));
        _sample_counter++;
    } else {
        _last_error = _spi.getLastError();
    }

    _updateAcquisitionStats(success);
    return success;
}

int32_t MindStreamer::readChannel(uint8_t channel) {
    if (!_initialized || !_is_streaming) {
        _last_error = ERROR_STREAMING_INACTIVE;
        return 0;
    }

    if (channel < 1 || channel > _config.num_channels) {
        _last_error = ERROR_INVALID_CHANNEL;
        return 0;
    }

    const bool success = _spi.readSample(_status, _samples, _config.num_channels);

    if (success) {
        _sample_counter++;
    } else {
        _last_error = _spi.getLastError();
    }

    _updateAcquisitionStats(success);

    if (!success) {
        return 0;
    }

    return _samples[channel - 1];
}

bool MindStreamer::readAndStream() {
    if (!_initialized || !_is_streaming) {
        _last_error = ERROR_STREAMING_INACTIVE;
        return false;
    }

    if (!_streamer) {
        _last_error = ERROR_INVALID_CONFIG;
        return false;
    }

    const bool success = _spi.readSample(_status, _samples, _config.num_channels);

    if (success) {
        _streamer->streamSample(_sample_counter, _status, _samples, _config.num_channels);
        _sample_counter++;
    } else {
        _last_error = _spi.getLastError();
    }

    _updateAcquisitionStats(success);
    return success;
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

/**
 * @brief Configure the ADS1299 into a known acquisition-ready state.
 */
bool MindStreamer::_configureDevice() {
    _spi.hardwareReset();
    delay(10);

    uint8_t config4 = 0;
    if (!_spi.readRegister(ADS1299_REG_CONFIG4, &config4)) {
        _last_error = ERROR_REGISTER_READ;
        return false;
    }
    config4 &= ~ADS1299_CONFIG4_SINGLE_SHOT;
    config4 &= ~ADS1299_CONFIG4_PD_LOFF_COMP;
    if (!_spi.writeRegister(ADS1299_REG_CONFIG4, config4)) {
        _last_error = ERROR_REGISTER_WRITE;
        return false;
    }

    // Force internal ADC reference buffer ON by default
    uint8_t config3 = 0;
    if (!_spi.readRegister(ADS1299_REG_CONFIG3, &config3)) {
        _last_error = ERROR_REGISTER_READ;
        return false;
    }

    if (_config.use_internal_reference) {
        config3 |= ADS1299_CONFIG3_REFBUF_EN;
    } else {
        config3 &= ~ADS1299_CONFIG3_REFBUF_EN;
    }

    if (!_spi.writeRegister(ADS1299_REG_CONFIG3, config3)) {
        _last_error = ERROR_REGISTER_WRITE;
        return false;
    }

    if (!setDataRate(_config.data_rate)) {
        return false;
    }

    if (!setGain(_config.pga_gain, 0)) {
        return false;
    }

    for (uint8_t ch = 0; ch < _config.num_channels; ++ch) {
        uint8_t chnset = 0;
        if (!_spi.readRegister(ADS1299_REG_CH1SET + ch, &chnset)) {
            _last_error = ERROR_REGISTER_READ;
            return false;
        }

        chnset &= ~ADS1299_CHNSET_PD;
        chnset &= ~ADS1299_CHNSET_MUX_MASK;
        chnset |= ADS1299_MUX_NORMAL;
        chnset &= ~ADS1299_CHNSET_SRB2;

        if (!_spi.writeRegister(ADS1299_REG_CH1SET + ch, chnset)) {
            _last_error = ERROR_REGISTER_WRITE;
            return false;
        }
    }

    if (!_spi.writeRegister(ADS1299_REG_BIAS_SENSP, 0x00)) {
        _last_error = ERROR_REGISTER_WRITE;
        return false;
    }
    if (!_spi.writeRegister(ADS1299_REG_BIAS_SENSN, 0x00)) {
        _last_error = ERROR_REGISTER_WRITE;
        return false;
    }

    uint8_t config2 = 0;
    if (!_spi.readRegister(ADS1299_REG_CONFIG2, &config2)) {
        _last_error = ERROR_REGISTER_READ;
        return false;
    }
    config2 &= ~ADS1299_CONFIG2_TEST_SIG_SRC;
    config2 &= ~ADS1299_CONFIG2_TEST_SIG_AMP;
    config2 &= ~ADS1299_CONFIG2_TEST_SIG_FREQ;
    config2 |= ADS1299_CONFIG2_RESERVED;
    if (!_spi.writeRegister(ADS1299_REG_CONFIG2, config2)) {
        _last_error = ERROR_REGISTER_WRITE;
        return false;
    }

    _spi.setDaisyChainMode(_config.daisy_chain);

    if (_config.enable_drl) {
        if (!enableDRL(true)) {
            return false;
        }
    } else {
        if (!enableDRL(false)) {
            return false;
        }
    }

    return true;
}

bool MindStreamer::_verifyDevice() {
    return _spi.verifyDevice();
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

        default:
            _streamer = new DebugStreamer(Serial);
            break;
    }
}

void MindStreamer::_updateAcquisitionStats(bool success) {
    const uint32_t now = millis();

    if (success) {
        _stats.total_samples++;
        _stats.last_sample_time_ms = now;
        _stats.last_error = ERROR_NONE;

        _sps_window_samples++;

        const uint32_t elapsed = now - _sps_window_start_ms;
        if (elapsed >= 1000) {
            _stats.actual_sps =
                (1000.0f * static_cast<float>(_sps_window_samples)) / static_cast<float>(elapsed);
            _sps_window_start_ms = now;
            _sps_window_samples = 0;
        }
    } else {
        _stats.dropped_samples++;
        _stats.last_error = _last_error;
    }
}

float MindStreamer::_convertToMicrovolts(int32_t raw_sample) {
    const float VREF = 4.5f;
    const float SCALE =
        (VREF * 1e6f) / (static_cast<float>(_config.pga_gain) * 8388608.0f);

    return static_cast<float>(raw_sample) * SCALE;
}

//=============================================================================
// Advanced Channel Configuration
//=============================================================================

bool MindStreamer::setChannelInputMux(uint8_t channel, uint8_t mux) {
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

    chnset &= ~ADS1299_CHNSET_MUX_MASK;
    chnset |= (mux & 0x07);

    return _spi.writeRegister(ADS1299_REG_CH1SET + channel - 1, chnset);
}

bool MindStreamer::enableTestSignal(uint8_t channel,
                                    PGAGain gain,
                                    uint8_t amplitude,
                                    uint8_t frequency) {
    if (channel < 1 || channel > _config.num_channels) {
        _last_error = ERROR_INVALID_CHANNEL;
        return false;
    }

    if (_is_streaming) {
        _last_error = ERROR_STREAMING_ACTIVE;
        return false;
    }

    if (!configureGlobalTestSignal(1, amplitude, frequency)) {
        return false;
    }

    if (!setGain(gain, channel)) {
        return false;
    }

    if (!setChannelInputMux(channel, ADS1299_MUX_TEST_SIG)) {
        return false;
    }

    return true;
}

bool MindStreamer::disableTestSignal(uint8_t channel) {
    if (channel < 1 || channel > _config.num_channels) {
        _last_error = ERROR_INVALID_CHANNEL;
        return false;
    }

    return setChannelInputMux(channel, ADS1299_MUX_NORMAL);
}

bool MindStreamer::readTemperature(uint8_t channel, float& temperature_celsius) {
    if (!_initialized) {
        _last_error = ERROR_INVALID_CONFIG;
        return false;
    }

    if (channel < 1 || channel > _config.num_channels) {
        _last_error = ERROR_INVALID_CHANNEL;
        return false;
    }

    const bool streaming_was_active = _is_streaming;

    uint8_t original_chnset = 0;
    uint8_t temp_chnset = 0;
    bool register_modified = false;
    bool measurement_started = false;

    uint8_t status[3] = {0, 0, 0};
    int32_t frame_samples[16] = {0};

    int64_t sample_sum = 0;
    float averaged_sample = 0.0f;
    float v_temp = 0.0f;

    static constexpr uint8_t kDiscardCount = 4;
    static constexpr uint8_t kAverageCount = 8;

    const float VREF = 4.5f;
    const float TEMP_25C_VOLTS = 0.1453f;
    const float TEMP_COEFF_V_PER_C = 490e-6f;

    if (streaming_was_active) {
        if (!stopStreaming()) {
            return false;
        }
    } else {
        if (!_spi.sendCommand(ADS1299_CMD_SDATAC)) {
            _last_error = _spi.getLastError();
            return false;
        }
    }

    if (!_spi.readRegister(ADS1299_REG_CH1SET + channel - 1, &original_chnset)) {
        _last_error = ERROR_REGISTER_READ;
        if (streaming_was_active) {
            startStreaming();
        }
        return false;
    }

    temp_chnset = original_chnset;
    temp_chnset &= ~(ADS1299_CHNSET_PD | ADS1299_CHNSET_GAIN_MASK | ADS1299_CHNSET_MUX_MASK);
    temp_chnset |= ADS1299_GAIN_1;
    temp_chnset |= ADS1299_MUX_TEMP_SENS;

    if (!_spi.writeRegister(ADS1299_REG_CH1SET + channel - 1, temp_chnset)) {
        _last_error = ERROR_REGISTER_WRITE;
        if (streaming_was_active) {
            startStreaming();
        }
        return false;
    }
    register_modified = true;

    if (!_spi.sendCommand(ADS1299_CMD_START)) {
        _last_error = _spi.getLastError();
        goto cleanup;
    }

    if (!_spi.sendCommand(ADS1299_CMD_RDATAC)) {
        _last_error = _spi.getLastError();
        goto cleanup;
    }

    measurement_started = true;

    delay(5);

    for (uint8_t i = 0; i < kDiscardCount; ++i) {
        if (!_spi.readSample(status, frame_samples, _config.num_channels)) {
            _last_error = _spi.getLastError();
            goto cleanup;
        }
    }

    sample_sum = 0;
    for (uint8_t i = 0; i < kAverageCount; ++i) {
        if (!_spi.readSample(status, frame_samples, _config.num_channels)) {
            _last_error = _spi.getLastError();
            goto cleanup;
        }
        sample_sum += frame_samples[channel - 1];
    }

    averaged_sample = static_cast<float>(sample_sum) / static_cast<float>(kAverageCount);
    v_temp = (averaged_sample / 8388608.0f) * VREF;
    temperature_celsius = 25.0f + ((v_temp - TEMP_25C_VOLTS) / TEMP_COEFF_V_PER_C);

    _last_error = ERROR_NONE;

cleanup:
    if (measurement_started) {
        _spi.sendCommand(ADS1299_CMD_STOP);
        _spi.sendCommand(ADS1299_CMD_SDATAC);
    }

    if (register_modified) {
        if (!_spi.writeRegister(ADS1299_REG_CH1SET + channel - 1, original_chnset)) {
            _last_error = ERROR_REGISTER_WRITE;
        }
    }

    if (streaming_was_active) {
        if (!startStreaming() && _last_error == ERROR_NONE) {
            _last_error = _spi.getLastError();
        }
    }

    return (_last_error == ERROR_NONE);
}

namespace {

static float leadOffCurrentToAmps(uint8_t current_source) {
    switch (current_source) {
        case LEAD_OFF_6nA:  return 6.0e-9f;
        case LEAD_OFF_24nA: return 24.0e-9f;
        case LEAD_OFF_6uA:  return 6.0e-6f;
        case LEAD_OFF_24uA: return 24.0e-6f;
        default:            return 0.0f;
    }
}

static float currentDataRateToHz(DataRate rate) {
    return static_cast<float>(static_cast<uint32_t>(rate));
}

}  // namespace

bool MindStreamer::enableLeadOff(uint8_t channel, uint8_t current_source) {
    if (channel < 1 || channel > _config.num_channels) {
        _last_error = ERROR_INVALID_CHANNEL;
        return false;
    }

    if (_is_streaming) {
        _last_error = ERROR_STREAMING_ACTIVE;
        return false;
    }

    uint8_t loff = 0;
    if (!_spi.readRegister(ADS1299_REG_LOFF, &loff)) {
        _last_error = ERROR_REGISTER_READ;
        return false;
    }

    loff &= ~(ADS1299_LOFF_ILEAD_OFF_MASK | ADS1299_LOFF_FLEAD_OFF_MASK);

    uint8_t ilead_bits = 0;
    switch (current_source) {
        case 0:
            return disableLeadOff(channel);
        case 1:
            ilead_bits = ADS1299_LOFF_ILEAD_6NA;
            break;
        case 2:
            ilead_bits = ADS1299_LOFF_ILEAD_24NA;
            break;
        case 3:
            ilead_bits = ADS1299_LOFF_ILEAD_6UA;
            break;
        case 4:
            ilead_bits = ADS1299_LOFF_ILEAD_24UA;
            break;
        default:
            _last_error = ERROR_INVALID_CONFIG;
            return false;
    }

    loff |= ilead_bits;
    loff |= ADS1299_LOFF_FREQ_DC;

    if (!_spi.writeRegister(ADS1299_REG_LOFF, loff)) {
        _last_error = ERROR_REGISTER_WRITE;
        return false;
    }

    uint8_t config4 = 0;
    if (!_spi.readRegister(ADS1299_REG_CONFIG4, &config4)) {
        _last_error = ERROR_REGISTER_READ;
        return false;
    }
    config4 |= ADS1299_CONFIG4_PD_LOFF_COMP;
    if (!_spi.writeRegister(ADS1299_REG_CONFIG4, config4)) {
        _last_error = ERROR_REGISTER_WRITE;
        return false;
    }

    uint8_t loff_sensp = 0;
    uint8_t loff_sensn = 0;

    if (!_spi.readRegister(ADS1299_REG_LOFF_SENSP, &loff_sensp)) {
        _last_error = ERROR_REGISTER_READ;
        return false;
    }
    if (!_spi.readRegister(ADS1299_REG_LOFF_SENSN, &loff_sensn)) {
        _last_error = ERROR_REGISTER_READ;
        return false;
    }

    loff_sensp |= (1u << (channel - 1));
    loff_sensn |= (1u << (channel - 1));

    if (!_spi.writeRegister(ADS1299_REG_LOFF_SENSP, loff_sensp)) {
        _last_error = ERROR_REGISTER_WRITE;
        return false;
    }
    if (!_spi.writeRegister(ADS1299_REG_LOFF_SENSN, loff_sensn)) {
        _last_error = ERROR_REGISTER_WRITE;
        return false;
    }

    return true;
}

bool MindStreamer::disableLeadOff(uint8_t channel) {
    if (channel < 1 || channel > _config.num_channels) {
        _last_error = ERROR_INVALID_CHANNEL;
        return false;
    }

    if (_is_streaming) {
        _last_error = ERROR_STREAMING_ACTIVE;
        return false;
    }

    uint8_t loff_sensp = 0;
    uint8_t loff_sensn = 0;

    if (!_spi.readRegister(ADS1299_REG_LOFF_SENSP, &loff_sensp)) {
        _last_error = ERROR_REGISTER_READ;
        return false;
    }
    if (!_spi.readRegister(ADS1299_REG_LOFF_SENSN, &loff_sensn)) {
        _last_error = ERROR_REGISTER_READ;
        return false;
    }

    loff_sensp &= ~(1u << (channel - 1));
    loff_sensn &= ~(1u << (channel - 1));

    if (!_spi.writeRegister(ADS1299_REG_LOFF_SENSP, loff_sensp)) {
        _last_error = ERROR_REGISTER_WRITE;
        return false;
    }
    if (!_spi.writeRegister(ADS1299_REG_LOFF_SENSN, loff_sensn)) {
        _last_error = ERROR_REGISTER_WRITE;
        return false;
    }

    if (loff_sensp == 0 && loff_sensn == 0) {
        uint8_t config4 = 0;
        if (_spi.readRegister(ADS1299_REG_CONFIG4, &config4)) {
            config4 &= ~ADS1299_CONFIG4_PD_LOFF_COMP;
            _spi.writeRegister(ADS1299_REG_CONFIG4, config4);
        }
    }

    return true;
}

bool MindStreamer::readLeadOffStatus(uint8_t channel, bool& positive_off, bool& negative_off) {
    if (channel < 1 || channel > _config.num_channels) {
        _last_error = ERROR_INVALID_CHANNEL;
        return false;
    }

    uint8_t loff_statp, loff_statn;
    if (!_spi.readRegister(ADS1299_REG_LOFF_STATP, &loff_statp)) return false;
    if (!_spi.readRegister(ADS1299_REG_LOFF_STATN, &loff_statn)) return false;

    positive_off = (loff_statp >> (channel - 1)) & 0x01;
    negative_off = (loff_statn >> (channel - 1)) & 0x01;

    return true;
}

bool MindStreamer::measureImpedance(uint8_t channel, float& impedance_kohm) {
    if (channel < 1 || channel > _config.num_channels) {
        _last_error = ERROR_INVALID_CHANNEL;
        return false;
    }

    if (!_initialized) {
        _last_error = ERROR_INVALID_CONFIG;
        return false;
    }

    static constexpr uint8_t kLeadOffCurrent = LEAD_OFF_24nA;
    static constexpr uint8_t kLeadOffFreqBits = ADS1299_LOFF_FREQ_31P2HZ;
    static constexpr float kLeadOffFreqHz = 31.25f;  // fCLK / 2^16 at 2.048 MHz
    static constexpr uint16_t kDiscardCount = 16;
    static constexpr uint16_t kMeasureCount = 128;

    bool streaming_was_active = _is_streaming;
    bool measurement_started = false;

    uint8_t original_loff = 0;
    uint8_t original_loff_sensp = 0;
    uint8_t original_loff_sensn = 0;
    uint8_t original_loff_flip = 0;
    uint8_t original_config4 = 0;

    uint8_t status[3] = {0, 0, 0};
    int32_t frame_samples[16] = {0};

    impedance_kohm = 0.0f;

    if (streaming_was_active) {
        if (!stopStreaming()) {
            return false;
        }
    }

    if (!_spi.readRegister(ADS1299_REG_LOFF, &original_loff)) {
        _last_error = ERROR_REGISTER_READ;
        goto cleanup;
    }
    if (!_spi.readRegister(ADS1299_REG_LOFF_SENSP, &original_loff_sensp)) {
        _last_error = ERROR_REGISTER_READ;
        goto cleanup;
    }
    if (!_spi.readRegister(ADS1299_REG_LOFF_SENSN, &original_loff_sensn)) {
        _last_error = ERROR_REGISTER_READ;
        goto cleanup;
    }
    if (!_spi.readRegister(ADS1299_REG_LOFF_FLIP, &original_loff_flip)) {
        _last_error = ERROR_REGISTER_READ;
        goto cleanup;
    }
    if (!_spi.readRegister(ADS1299_REG_CONFIG4, &original_config4)) {
        _last_error = ERROR_REGISTER_READ;
        goto cleanup;
    }

    {
        uint8_t loff = original_loff;
        loff &= ~(ADS1299_LOFF_ILEAD_OFF_MASK | ADS1299_LOFF_FLEAD_OFF_MASK);
        loff |= ADS1299_LOFF_ILEAD_24NA;
        loff |= kLeadOffFreqBits;

        if (!_spi.writeRegister(ADS1299_REG_LOFF, loff)) {
            _last_error = ERROR_REGISTER_WRITE;
            goto cleanup;
        }
    }

    {
        uint8_t config4 = original_config4;
        config4 &= ~ADS1299_CONFIG4_SINGLE_SHOT;
        config4 &= ~ADS1299_CONFIG4_PD_LOFF_COMP;
        if (!_spi.writeRegister(ADS1299_REG_CONFIG4, config4)) {
            _last_error = ERROR_REGISTER_WRITE;
            goto cleanup;
        }
    }

    if (!_spi.writeRegister(ADS1299_REG_LOFF_FLIP, 0x00)) {
        _last_error = ERROR_REGISTER_WRITE;
        goto cleanup;
    }

    {
        uint8_t loff_sensp = original_loff_sensp | (1u << (channel - 1));
        uint8_t loff_sensn = original_loff_sensn | (1u << (channel - 1));

        if (!_spi.writeRegister(ADS1299_REG_LOFF_SENSP, loff_sensp)) {
            _last_error = ERROR_REGISTER_WRITE;
            goto cleanup;
        }
        if (!_spi.writeRegister(ADS1299_REG_LOFF_SENSN, loff_sensn)) {
            _last_error = ERROR_REGISTER_WRITE;
            goto cleanup;
        }
    }

    if (!_spi.sendCommand(ADS1299_CMD_START)) {
        _last_error = _spi.getLastError();
        goto cleanup;
    }
    if (!_spi.sendCommand(ADS1299_CMD_RDATAC)) {
        _last_error = _spi.getLastError();
        goto cleanup;
    }

    measurement_started = true;
    delay(20);

    for (uint16_t i = 0; i < kDiscardCount; ++i) {
        if (!_spi.readSample(status, frame_samples, _config.num_channels)) {
            _last_error = _spi.getLastError();
            goto cleanup;
        }
    }

    {
        const float sample_rate_hz = currentDataRateToHz(_config.data_rate);
        const float volts_per_code = 4.5f / (static_cast<float>(_config.pga_gain) * 8388608.0f);
        const float phase_step = 2.0f * static_cast<float>(M_PI) * (kLeadOffFreqHz / sample_rate_hz);

        float acc_i = 0.0f;
        float acc_q = 0.0f;

        for (uint16_t n = 0; n < kMeasureCount; ++n) {
            if (!_spi.readSample(status, frame_samples, _config.num_channels)) {
                _last_error = _spi.getLastError();
                goto cleanup;
            }

            const float x = static_cast<float>(frame_samples[channel - 1]);
            const float phase = phase_step * static_cast<float>(n);
            acc_i += x * cosf(phase);
            acc_q += x * sinf(phase);
        }

        const float measured_peak_codes =
            (2.0f / static_cast<float>(kMeasureCount)) * sqrtf((acc_i * acc_i) + (acc_q * acc_q));
        const float measured_peak_volts = measured_peak_codes * volts_per_code;

        const float dc_current_amplitude = leadOffCurrentToAmps(kLeadOffCurrent);
        if (dc_current_amplitude <= 0.0f) {
            _last_error = ERROR_INVALID_CONFIG;
            goto cleanup;
        }

        // AC lead-off alternates source and sink, so the excitation current is a square wave.
        // Use the fundamental amplitude (4 / pi) * I to estimate impedance at the excitation frequency.
        const float fundamental_current_peak = (4.0f / static_cast<float>(M_PI)) * dc_current_amplitude;
        const float impedance_ohms = measured_peak_volts / fundamental_current_peak;

        if (!isfinite(impedance_ohms) || impedance_ohms < 0.0f) {
            _last_error = ERROR_INVALID_CONFIG;
            goto cleanup;
        }

        impedance_kohm = impedance_ohms / 1000.0f;
        _last_error = ERROR_NONE;
    }

  cleanup:
    if (measurement_started) {
        _spi.sendCommand(ADS1299_CMD_STOP);
        _spi.sendCommand(ADS1299_CMD_SDATAC);
    }

    _spi.writeRegister(ADS1299_REG_LOFF, original_loff);
    _spi.writeRegister(ADS1299_REG_LOFF_SENSP, original_loff_sensp);
    _spi.writeRegister(ADS1299_REG_LOFF_SENSN, original_loff_sensn);
    _spi.writeRegister(ADS1299_REG_LOFF_FLIP, original_loff_flip);
    _spi.writeRegister(ADS1299_REG_CONFIG4, original_config4);

    if (streaming_was_active) {
        if (!startStreaming() && _last_error == ERROR_NONE) {
            _last_error = _spi.getLastError();
        }
    }

    return (_last_error == ERROR_NONE);
}

bool MindStreamer::enableBiasMeasurement(uint8_t channel, bool enable) {
    if (channel < 1 || channel > _config.num_channels) {
        _last_error = ERROR_INVALID_CHANNEL;
        return false;
    }

    if (_is_streaming) {
        _last_error = ERROR_STREAMING_ACTIVE;
        return false;
    }

    uint8_t config3;
    _spi.readRegister(ADS1299_REG_CONFIG3, &config3);

    if (enable) {
        setChannelInputMux(channel, ADS1299_MUX_BIAS_MEAS);
        config3 |= ADS1299_CONFIG3_BIAS_MEAS;
    } else {
        setChannelInputMux(channel, ADS1299_MUX_NORMAL);
        config3 &= ~ADS1299_CONFIG3_BIAS_MEAS;
    }

    return _spi.writeRegister(ADS1299_REG_CONFIG3, config3);
}

bool MindStreamer::readBiasMeasurement(uint8_t channel, float& bias_volts) {
    if (channel < 1 || channel > _config.num_channels) {
        _last_error = ERROR_INVALID_CHANNEL;
        return false;
    }

    bool was_streaming = _is_streaming;
    if (was_streaming) {
        stopStreaming();
    }

    uint8_t chnset = 0;
    if (!_spi.readRegister(ADS1299_REG_CH1SET + channel - 1, &chnset)) {
        _last_error = ERROR_REGISTER_READ;
        if (was_streaming) startStreaming();
        return false;
    }

    if ((chnset & ADS1299_CHNSET_MUX_MASK) != ADS1299_MUX_BIAS_MEAS) {
        _last_error = ERROR_INVALID_CONFIG;
        if (was_streaming) startStreaming();
        return false;
    }

    if (!_spi.sendCommand(ADS1299_CMD_RDATAC)) {
        if (was_streaming) startStreaming();
        return false;
    }

    if (!_spi.sendCommand(ADS1299_CMD_START)) {
        _spi.sendCommand(ADS1299_CMD_SDATAC);
        if (was_streaming) startStreaming();
        return false;
    }

    delay(10);

    int32_t sample = 0;
    uint8_t status[3] = {0, 0, 0};

    bool ok = _spi.readSample(status, &sample, 1);

    _spi.sendCommand(ADS1299_CMD_STOP);
    _spi.sendCommand(ADS1299_CMD_SDATAC);

    if (was_streaming) {
        startStreaming();
    }

    if (!ok) {
        _last_error = _spi.getLastError();
        return false;
    }

    const float VREF = 4.5f;
    bias_volts = (static_cast<float>(sample) / 8388608.0f) * VREF;

    return true;
}

bool MindStreamer::configureGlobalTestSignal(uint8_t source, uint8_t amplitude, uint8_t frequency) {
    if (_is_streaming) {
        _last_error = ERROR_STREAMING_ACTIVE;
        return false;
    }

    uint8_t config2 = 0;
    if (!_spi.readRegister(ADS1299_REG_CONFIG2, &config2)) {
        _last_error = ERROR_REGISTER_READ;
        return false;
    }

    config2 |= ADS1299_CONFIG2_RESERVED;

    config2 &= ~ADS1299_CONFIG2_TEST_SIG_SRC;
    config2 &= ~ADS1299_CONFIG2_TEST_SIG_AMP;
    config2 &= ~ADS1299_CONFIG2_TEST_SIG_FREQ;

    if (source) {
        config2 |= ADS1299_CONFIG2_TEST_SIG_SRC;
    }

    if (amplitude >= 2) {
        config2 |= ADS1299_CONFIG2_TEST_SIG_AMP;
    }

    switch (frequency) {
        case 20:
            config2 |= ADS1299_TEST_FREQ_fCLK_20;
            break;
        case 21:
            config2 |= ADS1299_TEST_FREQ_fCLK_21;
            break;
        case 0:
            config2 |= ADS1299_TEST_FREQ_DC;
            break;
        default:
            _last_error = ERROR_INVALID_CONFIG;
            return false;
    }

    if (!_spi.writeRegister(ADS1299_REG_CONFIG2, config2)) {
        _last_error = ERROR_REGISTER_WRITE;
        return false;
    }

    return true;
}