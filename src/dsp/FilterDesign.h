/**
 * @file FilterDesign.h
 * @brief Predefined filter designs for EEG applications
 * @author Dr. Hafed-Eddine Bendib
 * @version 1.0.0
 * 
 * Header-only: all functions are inline.
 */

#ifndef MINDSTREAMER_FILTER_DESIGN_H
#define MINDSTREAMER_FILTER_DESIGN_H

#include "Biquad.h"
#include "FilterCascade.h"

namespace MindStreamer {
namespace DSP {

/**
 * @brief Helper: normalize frequency (Hz to normalized 0-0.5)
 */
inline float normalizeFreq(float freq_hz, float sample_rate_hz) {
    return freq_hz / (sample_rate_hz * 0.5f);
}

/**
 * @brief Predefined EEG filter designs
 */
namespace EEGDesign {

/**
 * @brief Low-pass filter at 40 Hz (standard EEG)
 * @param sample_rate_hz Sample rate (e.g., 250)
 * @return Single biquad (2nd order)
 */
inline Biquad lowpass40Hz(float sample_rate_hz) {
    float fc = normalizeFreq(40.0f, sample_rate_hz);
    return Biquad(FilterType::LOWPASS, fc, 0.707f);
}

/**
 * @brief High-pass filter at 0.5 Hz (remove DC drift)
 */
inline Biquad highpass0_5Hz(float sample_rate_hz) {
    float fc = normalizeFreq(0.5f, sample_rate_hz);
    return Biquad(FilterType::HIGHPASS, fc, 0.707f);
}

/**
 * @brief Band-pass for Alpha waves (8-13 Hz)
 */
inline Biquad bandpassAlpha(float sample_rate_hz) {
    float fc_low = normalizeFreq(8.0f, sample_rate_hz);
    float fc_high = normalizeFreq(13.0f, sample_rate_hz);
    float center = (fc_low + fc_high) * 0.5f;
    float bandwidth = fc_high - fc_low;
    float Q = center / bandwidth;
    return Biquad(FilterType::BANDPASS, center, Q);
}

/**
 * @brief Band-pass for Beta waves (13-30 Hz)
 */
inline Biquad bandpassBeta(float sample_rate_hz) {
    float fc_low = normalizeFreq(13.0f, sample_rate_hz);
    float fc_high = normalizeFreq(30.0f, sample_rate_hz);
    float center = (fc_low + fc_high) * 0.5f;
    float bandwidth = fc_high - fc_low;
    float Q = center / bandwidth;
    return Biquad(FilterType::BANDPASS, center, Q);
}

/**
 * @brief Notch filter for 50 Hz power line
 */
inline Biquad notch50Hz(float sample_rate_hz, float Q = 8.0f) {
    float fc = normalizeFreq(50.0f, sample_rate_hz);
    return Biquad(FilterType::NOTCH, fc, Q);
}

/**
 * @brief Notch filter for 60 Hz power line
 */
inline Biquad notch60Hz(float sample_rate_hz, float Q = 8.0f) {
    float fc = normalizeFreq(60.0f, sample_rate_hz);
    return Biquad(FilterType::NOTCH, fc, Q);
}

/**
 * @brief Complete standard EEG filter (0.5-40 Hz bandpass + optional notch)
 * @return FilterCascade (4th order Butterworth)
 */
inline FilterCascade standardEEGFilter(float sample_rate_hz, bool include_notch = true) {
    FilterCascade cascade;
    cascade.addStage(highpass0_5Hz(sample_rate_hz));
    // Two lowpass stages for 4th order (sharper roll-off)
    float fc_lp = normalizeFreq(40.0f, sample_rate_hz);
    cascade.addStage(Biquad(FilterType::LOWPASS, fc_lp, 0.707f));
    cascade.addStage(Biquad(FilterType::LOWPASS, fc_lp, 0.707f));
    if (include_notch) {
        cascade.addStage(notch50Hz(sample_rate_hz));
    }
    return cascade;
}

} // namespace EEGDesign
} // namespace DSP
} // namespace MindStreamer

#endif // MINDSTREAMER_FILTER_DESIGN_H