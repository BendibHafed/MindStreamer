/**
 * @file BinaryStreamer.cpp
 * @brief Implementation of binary packet output formatter
 * @author Dr. Hafed-Eddine Bendib
 * @version 1.0.0
 */

#include "BinaryStreamer.h"

BinaryStreamer::BinaryStreamer(Stream& output) : _output(output) {}

void BinaryStreamer::begin() {
    // No initialization needed for binary mode
}

void BinaryStreamer::streamSample(uint32_t sample_counter, 
                                   const uint8_t* status,
                                   const int32_t* samples, 
                                   uint8_t num_channels) {
    uint8_t idx = 0;
    
    // Header
    _buffer[idx++] = MINDSTREAMER_PACKET_HEADER;
    
    // Sample counter (4 bytes, little-endian)
    _buffer[idx++] = (sample_counter >> 0) & 0xFF;
    _buffer[idx++] = (sample_counter >> 8) & 0xFF;
    _buffer[idx++] = (sample_counter >> 16) & 0xFF;
    _buffer[idx++] = (sample_counter >> 24) & 0xFF;
    
    // Status register (3 bytes)
    if (status) {
        _buffer[idx++] = status[0];
        _buffer[idx++] = status[1];
        _buffer[idx++] = status[2];
    } else {
        _buffer[idx++] = 0;
        _buffer[idx++] = 0;
        _buffer[idx++] = 0;
    }
    
    // Channel data (3 bytes per channel, 24-bit, big-endian)
    for (uint8_t ch = 0; ch < num_channels; ch++) {
        int32_t sample = samples[ch];
        // Extract 24-bit value (discard upper 8 bits of sign-extended 32-bit)
        uint32_t val = sample & 0x00FFFFFF;
        _buffer[idx++] = (val >> 16) & 0xFF;
        _buffer[idx++] = (val >> 8) & 0xFF;
        _buffer[idx++] = val & 0xFF;
    }
    
    // Footer
    _buffer[idx++] = MINDSTREAMER_PACKET_FOOTER;
    
    // Send packet
    _output.write(_buffer, idx);
}

void BinaryStreamer::streamSingleChannel(uint32_t sample_counter,
                                         uint8_t channel,
                                         int32_t sample) {
    uint8_t idx = 0;
    
    _buffer[idx++] = MINDSTREAMER_PACKET_HEADER;
    
    _buffer[idx++] = (sample_counter >> 0) & 0xFF;
    _buffer[idx++] = (sample_counter >> 8) & 0xFF;
    _buffer[idx++] = (sample_counter >> 16) & 0xFF;
    _buffer[idx++] = (sample_counter >> 24) & 0xFF;
    
    // Status (dummy)
    _buffer[idx++] = 0;
    _buffer[idx++] = 0;
    _buffer[idx++] = 0;
    
    // Single channel
    uint32_t val = sample & 0x00FFFFFF;
    _buffer[idx++] = (val >> 16) & 0xFF;
    _buffer[idx++] = (val >> 8) & 0xFF;
    _buffer[idx++] = val & 0xFF;
    
    _buffer[idx++] = MINDSTREAMER_PACKET_FOOTER;
    
    _output.write(_buffer, idx);
}

void BinaryStreamer::streamRegisterDump(const uint8_t* registers, uint8_t num_registers) {
    // Not supported in binary mode – ignore
}