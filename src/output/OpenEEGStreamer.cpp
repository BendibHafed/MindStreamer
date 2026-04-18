/**
 * @file OpenEEGStreamer.cpp
 * @brief Implementation of OpenEEG/BrainBay P2 protocol
 * @author Dr. Hafed-Eddine Bendib
 * @version 1.0.0
 */

#include "OpenEEGStreamer.h"

OpenEEGStreamer::OpenEEGStreamer(Stream& output)
    : _output(output), _counter(0) {}

void OpenEEGStreamer::begin() {
    _counter = 0;
}

void OpenEEGStreamer::streamSample(uint32_t sample_counter,
                                   const uint8_t* status,
                                   const int32_t* samples,
                                   uint8_t num_channels) {
    (void)status;  // unused in OpenEEG mode
    if (!samples) return;
    // OpenEEG P2 compatibility format carries at most 6 channels.
    // If more are available, only channels 1..6 are transmitted.
    const uint8_t num_to_send = (num_channels > 6) ? 6 : num_channels;

    // Sync bytes
    _packet[0] = 0xA5;
    _packet[1] = 0x5A;

    // Protocol version
    _packet[2] = 0x02;

    // Sample counter: low byte only
    _packet[3] = static_cast<uint8_t>(sample_counter & 0xFF);

    // 6 channels, 10-bit unsigned each packed into 2 bytes
    for (uint8_t ch = 0; ch < 6; ch++) {
        uint16_t scaled = 512;  // mid-scale default

        if (samples && ch < num_to_send) {
            scaled = _scaleSample(samples[ch]);
        }

        _packet[4 + ch * 2] = static_cast<uint8_t>((scaled >> 8) & 0x03); // high 2 bits
        _packet[5 + ch * 2] = static_cast<uint8_t>(scaled & 0xFF);        // low 8 bits
    }

    // Switches / status byte pattern
    _counter++;
    if (_counter >= 18) {
        _counter = 0;
    }
    _packet[16] = (_counter >= 9) ? 0x0F : 0x07;

    _output.write(_packet, 17);
}

void OpenEEGStreamer::streamSingleChannel(uint32_t sample_counter,
                                          uint8_t channel,
                                          int32_t sample) {
    if (channel < 1 || channel > 6) {
        return;
    }

    int32_t samples[6] = {0, 0, 0, 0, 0, 0};
    samples[channel - 1] = sample;
    streamSample(sample_counter, nullptr, samples, 6);
}

void OpenEEGStreamer::streamRegisterDump(const uint8_t* registers, uint8_t num_registers) {
    (void)registers;
    (void)num_registers;
    // Not supported in OpenEEG mode
}

uint16_t OpenEEGStreamer::_scaleSample(int32_t sample) {
    // Map signed 24-bit-ish EEG code to unsigned 10-bit range [0..1023].
    // This is a simple compatibility mapping, not a calibrated voltage scaling.
    int32_t scaled = (sample >> 13) + 512;

    if (scaled < 0) {
        scaled = 0;
    } else if (scaled > 1023) {
        scaled = 1023;
    }

    return static_cast<uint16_t>(scaled);
}