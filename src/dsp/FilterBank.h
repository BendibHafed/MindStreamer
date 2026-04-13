/**
 * @file FilterBank.h
 * @brief Multi-channel filter bank for EEG processing
 * @author Dr. Hafed-Eddine Bendib
 * @version 1.0.0
 */

#ifndef MINDSTREAMER_FILTER_BANK_H
#define MINDSTREAMER_FILTER_BANK_H

#include "FilterCascade.h"
#include <vector>

namespace MindStreamer {
namespace DSP {

/**
 * @brief Filter bank for multi-channel EEG data
 * 
 * Each channel gets its own copy of the filter state,
 * but all channels share the same coefficients.
 */
class FilterBank {
public:
    explicit FilterBank(uint8_t num_channels) 
        : _num_channels(num_channels), _enabled(num_channels, true), _filters(num_channels) {}
    
    void setFilter(const FilterCascade& cascade) {
        _prototype = cascade;
        for (auto& filter : _filters) filter = _prototype;
    }
    
    void setFilter(const Biquad& stage) {
        FilterCascade cascade;
        cascade.addStage(stage);
        setFilter(cascade);
    }
    
    /**
     * @brief Process one sample from all channels (interleaved input)
     * @param inputs Array of input samples (size = num_channels)
     * @param outputs Array for output samples (size = num_channels)
     */
    void process(const float* inputs, float* outputs) noexcept {
        for (uint8_t ch = 0; ch < _num_channels; ++ch) {
            if (_enabled[ch]) {
                outputs[ch] = _filters[ch].process(inputs[ch]);
            } else {
                outputs[ch] = inputs[ch];
            }
        }
    }
    
    /**
     * @brief Process a block of interleaved samples
     * @param inputs Input buffer (channels interleaved: ch0_s0, ch1_s0, ..., ch0_s1, ...)
     * @param outputs Output buffer (same interleaving)
     * @param num_samples Number of samples per channel
     */
    void processBlock(const float* inputs, float* outputs, size_t num_samples) noexcept {
        for (size_t i = 0; i < num_samples; ++i) {
            for (uint8_t ch = 0; ch < _num_channels; ++ch) {
                size_t idx = i * _num_channels + ch;
                if (_enabled[ch]) {
                    outputs[idx] = _filters[ch].process(inputs[idx]);
                } else {
                    outputs[idx] = inputs[idx];
                }
            }
        }
    }
    
    void reset() noexcept {
        for (auto& filter : _filters) filter.reset();
    }
    
    void enableChannel(uint8_t channel, bool enabled) {
        if (channel < _num_channels) _enabled[channel] = enabled;
    }
    
    bool isChannelEnabled(uint8_t channel) const {
        return (channel < _num_channels) ? _enabled[channel] : false;
    }
    
    uint8_t getNumChannels() const { return _num_channels; }
    
private:
    uint8_t _num_channels;
    std::vector<bool> _enabled;
    std::vector<FilterCascade> _filters;
    FilterCascade _prototype;
};

} // namespace DSP
} // namespace MindStreamer

#endif // MINDSTREAMER_FILTER_BANK_H