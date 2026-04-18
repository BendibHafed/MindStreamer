/**
 * @file BinaryStreamer.h
 * @brief Binary packet output formatter (0xBB...0xEE)
 * @author Dr. Hafed-Eddine Bendib
 * @version 1.0.0
 */

#ifndef MINDSTREAMER_BINARY_STREAMER_H
#define MINDSTREAMER_BINARY_STREAMER_H

#include "DataStreamer.h"

/**
 * @brief Stream a single-channel sample in compact binary form
 * @note The channel argument is accepted for API consistency but is not
 * encoded in the binary packet format.
 */
class BinaryStreamer : public DataStreamer {
public:
    BinaryStreamer(Stream& output = Serial);
    virtual ~BinaryStreamer() = default;
    
    void begin() override;
    void streamSample(uint32_t sample_counter, 
                      const uint8_t* status,
                      const int32_t* samples, 
                      uint8_t num_channels) override;
    void streamSingleChannel(uint32_t sample_counter,
                             uint8_t channel,
                             int32_t sample) override;
    void streamRegisterDump(const uint8_t* registers, uint8_t num_registers) override;
    OutputMode getMode() const override { return OUTPUT_BINARY; }
    
private:
    Stream& _output;
    uint8_t _buffer[256];
};

#endif // MINDSTREAMER_BINARY_STREAMER_H