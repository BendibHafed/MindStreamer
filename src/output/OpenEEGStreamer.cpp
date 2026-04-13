/**
 * @file OpenEEGStreamer.cpp
 * @brief Implementation of OpenEEG/BrainBay P2 protocol
 * @author Dr. Hafed-Eddine Bendib
 * @version 1.0.0
 */

#include "OpenEEGStreamer.h"

OpenEEGStreamer::OpenEEGStreamer(Stream& output) : _output(output), _counter(0) {}

void OpenEEGStreamer::begin() {
    _counter = 0;
}

void OpenEEGStreamer::streamSample(uint32_t sample_counter, 
                                    const uint8_t* status,
                                    const int32_t* samples, 
                                    uint8_t num_channels) {
    // P2 protocol: 6 channels max
    uint8_t num_to_send = (num_channels > 6) ? 6 : num_channels;
    
    // Sync bytes
    _packet[0] = 0xA5;
    _packet[1] = 0x5A;
    
    // Version
    _packet[2] = 0x02;
    
    // Sample counter (low byte only)
    _packet[3] = (uint8_t)(sample_counter & 0xFF);
    
    // 6 channels, 2 bytes each (10 bits)
    for (uint8_t ch = 0; ch < 6; ch++) {
        if (ch < num_to_send) {
            uint8_t scaled = _scaleSample(samples[ch]);
            _packet[4 + ch * 2] = (scaled >> 8) & 0x03;  // High 2 bits
            _packet[5 + ch * 2] = scaled & 0xFF;         // Low 8 bits
        } else {
            _packet[4 + ch * 2] = 0;
            _packet[5 + ch * 2] = 0;
        }
    }
    
    // Switches (battery status, etc.)
    _counter++;
    if (_counter >= 18) _counter = 0;
    _packet[16] = (_counter >= 9) ? 0x0F : 0x07;
    
    // Send packet
    _output.write(_packet, 17);
}

void OpenEEGStreamer::streamSingleChannel(uint32_t sample_counter,
                                           uint8_t channel,
                                           int32_t sample) {
    // Not typically used, but implement for completeness
    if (channel <= 6) {
        int32_t samples[6] = {0};
        samples[channel - 1] = sample;
        streamSample(sample_counter, nullptr, samples, 6);
    }
}

void OpenEEGStreamer::streamRegisterDump(const uint8_t* registers, uint8_t num_registers) {
    // Not supported
}

uint8_t OpenEEGStreamer::_scaleSample(int32_t sample) {
    // Scale 24-bit EEG sample to 10-bit for OpenEEG (0-1023)
    // Sample range: -2^23 to 2^23-1 (approx ±8 million)
    // Target: 0-1023
    
    // Shift right by 13 bits (divide by 8192) to bring into range
    int32_t scaled = sample >> 13;
    
    // Clamp to 10-bit range and add offset to make unsigned
    scaled += 512;
    if (scaled < 0) scaled = 0;
    if (scaled > 1023) scaled = 1023;
    
    return (uint8_t)scaled;
}