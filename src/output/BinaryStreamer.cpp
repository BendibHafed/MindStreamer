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
    // Packet format:
    // [0]   = header (0xBB)
    // [1:4] = sample counter, little-endian
    // [5:7] = 3-byte ADS1299 status
    // [8..] = 3 bytes per channel, big-endian 24-bit sample
    // [last]= footer (0xEE)

    // Maximum payload supported by this fixed buffer
    const uint16_t required_size = static_cast<uint16_t>(1 + 4 + 3 + (3 * num_channels) + 1);
    if (required_size > sizeof(_buffer) || samples == nullptr) {
        return;
    }

    uint16_t idx = 0;

    // Header
    _buffer[idx++] = MINDSTREAMER_PACKET_HEADER;

    // Sample counter (4 bytes, little-endian)
    _buffer[idx++] = static_cast<uint8_t>((sample_counter >> 0) & 0xFF);
    _buffer[idx++] = static_cast<uint8_t>((sample_counter >> 8) & 0xFF);
    _buffer[idx++] = static_cast<uint8_t>((sample_counter >> 16) & 0xFF);
    _buffer[idx++] = static_cast<uint8_t>((sample_counter >> 24) & 0xFF);

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

    // Channel data: serialize the lower 24 bits in ADS1299 byte order (MSB first)
    for (uint8_t ch = 0; ch < num_channels; ch++) {
        const uint32_t val = static_cast<uint32_t>(samples[ch]) & 0x00FFFFFFUL;
        _buffer[idx++] = static_cast<uint8_t>((val >> 16) & 0xFF);
        _buffer[idx++] = static_cast<uint8_t>((val >> 8) & 0xFF);
        _buffer[idx++] = static_cast<uint8_t>(val & 0xFF);
    }

    // Footer
    _buffer[idx++] = MINDSTREAMER_PACKET_FOOTER;

    _output.write(_buffer, idx);
}

void BinaryStreamer::streamSingleChannel(uint32_t sample_counter,
                                         uint8_t channel,
                                         int32_t sample) {
    (void)channel;  // Channel index is not encoded in this compact packet format

    const uint16_t required_size = 1 + 4 + 3 + 3 + 1;
    if (required_size > sizeof(_buffer)) {
        return;
    }

    uint16_t idx = 0;

    _buffer[idx++] = MINDSTREAMER_PACKET_HEADER;

    _buffer[idx++] = static_cast<uint8_t>((sample_counter >> 0) & 0xFF);
    _buffer[idx++] = static_cast<uint8_t>((sample_counter >> 8) & 0xFF);
    _buffer[idx++] = static_cast<uint8_t>((sample_counter >> 16) & 0xFF);
    _buffer[idx++] = static_cast<uint8_t>((sample_counter >> 24) & 0xFF);

    // Dummy 3-byte status
    _buffer[idx++] = 0;
    _buffer[idx++] = 0;
    _buffer[idx++] = 0;

    const uint32_t val = static_cast<uint32_t>(sample) & 0x00FFFFFFUL;
    _buffer[idx++] = static_cast<uint8_t>((val >> 16) & 0xFF);
    _buffer[idx++] = static_cast<uint8_t>((val >> 8) & 0xFF);
    _buffer[idx++] = static_cast<uint8_t>(val & 0xFF);

    _buffer[idx++] = MINDSTREAMER_PACKET_FOOTER;

    _output.write(_buffer, idx);
}

void BinaryStreamer::streamRegisterDump(const uint8_t* registers, uint8_t num_registers) {
    (void)registers;
    (void)num_registers;
    // Not supported in binary mode
}