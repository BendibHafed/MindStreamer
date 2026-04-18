/**
 * @file Biquad.cpp
 * @brief Implementation of biquad filter design
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

#include "Biquad.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

namespace DSP {

void Biquad::design(FilterType type, float fc_norm, float Q, float peak_gain_db) noexcept {
    // Clamp frequency to a safe practical range
    if (fc_norm > 0.49f) fc_norm = 0.49f;
    if (fc_norm < 0.001f) fc_norm = 0.001f;

    // Prevent divide-by-zero / numerically unstable Q
    if (Q < 0.001f) Q = 0.001f;

    // Pre-warped frequency for bilinear transform
    const float K = tanf(M_PI * fc_norm);
    const float V = powf(10.0f, fabsf(peak_gain_db) / 20.0f);
    float norm = 1.0f;

    switch (type) {
        case FilterType::LOWPASS:
            norm = 1.0f / (1.0f + K / Q + K * K);
            _b0 = K * K * norm;
            _b1 = 2.0f * _b0;
            _b2 = _b0;
            _a1 = 2.0f * (K * K - 1.0f) * norm;
            _a2 = (1.0f - K / Q + K * K) * norm;
            break;

        case FilterType::HIGHPASS:
            norm = 1.0f / (1.0f + K / Q + K * K);
            _b0 = 1.0f * norm;
            _b1 = -2.0f * _b0;
            _b2 = _b0;
            _a1 = 2.0f * (K * K - 1.0f) * norm;
            _a2 = (1.0f - K / Q + K * K) * norm;
            break;

        case FilterType::BANDPASS:
            norm = 1.0f / (1.0f + K / Q + K * K);
            _b0 = (K / Q) * norm;
            _b1 = 0.0f;
            _b2 = -_b0;
            _a1 = 2.0f * (K * K - 1.0f) * norm;
            _a2 = (1.0f - K / Q + K * K) * norm;
            break;

        case FilterType::NOTCH:
            norm = 1.0f / (1.0f + K / Q + K * K);
            _b0 = (1.0f + K * K) * norm;
            _b1 = 2.0f * (K * K - 1.0f) * norm;
            _b2 = _b0;
            _a1 = _b1;
            _a2 = (1.0f - K / Q + K * K) * norm;
            break;

        case FilterType::PEAK:
            if (peak_gain_db >= 0.0f) {  // Boost
                norm = 1.0f / (1.0f + (1.0f / Q) * K + K * K);
                _b0 = (1.0f + (V / Q) * K + K * K) * norm;
                _b1 = 2.0f * (K * K - 1.0f) * norm;
                _b2 = (1.0f - (V / Q) * K + K * K) * norm;
                _a1 = _b1;
                _a2 = (1.0f - (1.0f / Q) * K + K * K) * norm;
            } else {  // Cut
                norm = 1.0f / (1.0f + (V / Q) * K + K * K);
                _b0 = (1.0f + (1.0f / Q) * K + K * K) * norm;
                _b1 = 2.0f * (K * K - 1.0f) * norm;
                _b2 = (1.0f - (1.0f / Q) * K + K * K) * norm;
                _a1 = _b1;
                _a2 = (1.0f - (V / Q) * K + K * K) * norm;
            }
            break;

        case FilterType::LOWSHELF:
            if (peak_gain_db >= 0.0f) {  // Boost
                norm = 1.0f / (1.0f + 1.41421356f * K + K * K);
                _b0 = (1.0f + sqrtf(2.0f * V) * K + V * K * K) * norm;
                _b1 = 2.0f * (V * K * K - 1.0f) * norm;
                _b2 = (1.0f - sqrtf(2.0f * V) * K + V * K * K) * norm;
                _a1 = 2.0f * (K * K - 1.0f) * norm;
                _a2 = (1.0f - 1.41421356f * K + K * K) * norm;
            } else {  // Cut
                norm = 1.0f / (1.0f + sqrtf(2.0f * V) * K + V * K * K);
                _b0 = (1.0f + 1.41421356f * K + K * K) * norm;
                _b1 = 2.0f * (K * K - 1.0f) * norm;
                _b2 = (1.0f - 1.41421356f * K + K * K) * norm;
                _a1 = 2.0f * (V * K * K - 1.0f) * norm;
                _a2 = (1.0f - sqrtf(2.0f * V) * K + V * K * K) * norm;
            }
            break;

        case FilterType::HIGHSHELF:
            if (peak_gain_db >= 0.0f) {  // Boost
                norm = 1.0f / (1.0f + 1.41421356f * K + K * K);
                _b0 = (V + sqrtf(2.0f * V) * K + K * K) * norm;
                _b1 = 2.0f * (K * K - V) * norm;
                _b2 = (V - sqrtf(2.0f * V) * K + K * K) * norm;
                _a1 = 2.0f * (K * K - 1.0f) * norm;
                _a2 = (1.0f - 1.41421356f * K + K * K) * norm;
            } else {  // Cut
                norm = 1.0f / (V + sqrtf(2.0f * V) * K + K * K);
                _b0 = (1.0f + 1.41421356f * K + K * K) * norm;
                _b1 = 2.0f * (K * K - 1.0f) * norm;
                _b2 = (1.0f - 1.41421356f * K + K * K) * norm;
                _a1 = 2.0f * (K * K - V) * norm;
                _a2 = (V - sqrtf(2.0f * V) * K + K * K) * norm;
            }
            break;

        case FilterType::ALLPASS:
            norm = 1.0f / (1.0f + K / Q + K * K);
            _b0 = (1.0f - K / Q + K * K) * norm;
            _b1 = 2.0f * (K * K - 1.0f) * norm;
            _b2 = norm;
            _a1 = _b1;
            _a2 = _b0;
            break;
    }
}

} // namespace DSP