/**
 * @file Biquad.h
 * @brief Single biquad filter section (2nd order IIR)
 * @author Dr. Hafed-Eddine Bendib
 * @version 1.0.0
 * 
 * This implementation is based on the classic biquad filter code by Nigel Redmon
 * (EarLevel Engineering). The coefficient calculation formulas follow the
 * standard bilinear transform method.
 * 
 * Original reference: http://www.earlevel.com/main/2012/11/26/biquad-c-source-code/
 * License of original code: permissive, similar to MIT.
 * 
 * Modifications for MindStreamer:
 * - Use `float` instead of `double` for ESP32 performance
 * - Add `noexcept` specifiers
 * - Integrate into MindStreamer namespace
 */

#ifndef MINDSTREAMER_BIQUAD_H
#define MINDSTREAMER_BIQUAD_H

#include <cmath>
#include <cstdint>

namespace MindStreamer {
namespace DSP {

/**
 * @brief Filter types for biquad design
 */
enum class FilterType : uint8_t {
    LOWPASS,    ///< Low-pass filter
    HIGHPASS,   ///< High-pass filter
    BANDPASS,   ///< Band-pass filter
    NOTCH,      ///< Notch filter (band-stop)
    PEAK,       ///< Peaking EQ filter
    LOWSHELF,   ///< Low-shelf filter
    HIGHSHELF,  ///< High-shelf filter
    ALLPASS     ///< All-pass filter (phase only)
};

/**
 * @brief Single biquad filter section (floating-point)
 * 
 * Transfer function: H(z) = (b0 + b1*z^-1 + b2*z^-2) / (1 + a1*z^-1 + a2*z^-2)
 */
class Biquad {
public:
    /**
     * @brief Default constructor (pass-through filter)
     */
    Biquad() noexcept = default;
    
    /**
     * @brief Constructor with coefficients
     */
    Biquad(float b0, float b1, float b2, float a1, float a2) noexcept
        : _b0(b0), _b1(b1), _b2(b2), _a1(a1), _a2(a2), _z1(0.0f), _z2(0.0f) {}
    
    /**
     * @brief Design a biquad filter from specifications
     * @param type Filter type
     * @param fc_norm Normalized cutoff frequency (0 to 0.5, where 0.5 = Nyquist)
     * @param Q Quality factor
     * @param peak_gain_db Peak gain in dB (for peaking/shelf filters)
     */
    Biquad(FilterType type, float fc_norm, float Q, float peak_gain_db = 0.0f) noexcept {
        design(type, fc_norm, Q, peak_gain_db);
    }
    
    /**
     * @brief Process a single sample
     * @param input Input sample
     * @return Filtered output
     */
    inline float process(float input) noexcept {
        // Direct Form I Transposed
        float output = _b0 * input + _z1;
        _z1 = _b1 * input + _z2 - _a1 * output;
        _z2 = _b2 * input - _a2 * output;
        return output;
    }
    
    /**
     * @brief Process a block of samples
     * @param input Input buffer
     * @param output Output buffer (can be same as input)
     * @param count Number of samples
     */
    void processBlock(const float* input, float* output, size_t count) noexcept {
        for (size_t i = 0; i < count; ++i) {
            output[i] = process(input[i]);
        }
    }
    
    /**
     * @brief Reset filter state (zero delay elements)
     */
    void reset() noexcept {
        _z1 = 0.0f;
        _z2 = 0.0f;
    }
    
    /**
     * @brief Update coefficients (for runtime filter changes)
     */
    void setCoefficients(float b0, float b1, float b2, float a1, float a2) noexcept {
        _b0 = b0; _b1 = b1; _b2 = b2;
        _a1 = a1; _a2 = a2;
    }
    
    /**
     * @brief Get current coefficients
     */
    void getCoefficients(float& b0, float& b1, float& b2, float& a1, float& a2) const noexcept {
        b0 = _b0; b1 = _b1; b2 = _b2;
        a1 = _a1; a2 = _a2;
    }
    
    /**
     * @brief Check if filter is stable (|a1| < 1 + a2 and |a2| < 1)
     */
    bool isStable() const noexcept {
        return (std::abs(_a2) < 1.0f) && (std::abs(_a1) < 1.0f + _a2);
    }
    
    /**
     * @brief Design filter from specifications
     */
    void design(FilterType type, float fc_norm, float Q, float peak_gain_db = 0.0f) noexcept;
    
private:
    float _b0 = 1.0f, _b1 = 0.0f, _b2 = 0.0f;
    float _a1 = 0.0f, _a2 = 0.0f;
    float _z1 = 0.0f, _z2 = 0.0f;
};

} // namespace DSP
} // namespace MindStreamer

#endif // MINDSTREAMER_BIQUAD_H