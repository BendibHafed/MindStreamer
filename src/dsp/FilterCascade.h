/**
 * @file FilterCascade.h
 * @brief Cascade of multiple biquad sections for higher-order filters
 * @author Dr. Hafed-Eddine Bendib
 * @version 1.0.0
 */

#ifndef MINDSTREAMER_FILTER_CASCADE_H
#define MINDSTREAMER_FILTER_CASCADE_H

#include "Biquad.h"
#include <vector>

namespace DSP {

/**
 * @brief Cascade of biquad filters for high-order IIR filters
 * 
 * Example: 6th order Butterworth = 3 cascaded biquads.
 */
class FilterCascade {
public:
    FilterCascade() = default;
    explicit FilterCascade(std::vector<Biquad> stages) : _stages(std::move(stages)) {}
    
    void addStage(const Biquad& stage) { _stages.push_back(stage); }
    void addStage(Biquad&& stage) { _stages.push_back(std::move(stage)); }
    void clear() { _stages.clear(); }
    size_t getNumStages() const { return _stages.size(); }
    
    /**
     * @brief Process a single sample through all stages
     */
    inline float process(float input) noexcept {
        float output = input;
        for (auto& stage : _stages) {
            output = stage.process(output);
        }
        return output;
    }
    
    /**
     * @brief Process a block of samples
     */
    void processBlock(const float* input, float* output, size_t count) noexcept {
        if (input != output) {
            for (size_t i = 0; i < count; ++i) output[i] = input[i];
        }
        for (auto& stage : _stages) {
            for (size_t i = 0; i < count; ++i) {
                output[i] = stage.process(output[i]);
            }
        }
    }
    
    void reset() noexcept {
        for (auto& stage : _stages) stage.reset();
    }
    
    bool isStable() const noexcept {
        for (const auto& stage : _stages) {
            if (!stage.isStable()) return false;
        }
        return true;
    }
    
private:
    std::vector<Biquad> _stages;
};

} // namespace DSP

#endif // MINDSTREAMER_FILTER_CASCADE_H