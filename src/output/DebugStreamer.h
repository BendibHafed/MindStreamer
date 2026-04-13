/**
 * @file DebugStreamer.h
 * @brief Debug output formatter (register dumps)
 * @author Dr. Hafed-Eddine Bendib
 * @version 1.0.0
 */

#ifndef MINDSTREAMER_DEBUG_STREAMER_H
#define MINDSTREAMER_DEBUG_STREAMER_H

#include "DataStreamer.h"

class DebugStreamer : public DataStreamer {
public:
    DebugStreamer(Stream& output = Serial);
    virtual ~DebugStreamer() = default;
    
    void begin() override;
    void streamSample(uint32_t sample_counter, 
                      const uint8_t* status,
                      const int32_t* samples, 
                      uint8_t num_channels) override;
    void streamSingleChannel(uint32_t sample_counter,
                             uint8_t channel,
                             int32_t sample) override;
    void streamRegisterDump(const uint8_t* registers, uint8_t num_registers) override;
    OutputMode getMode() const override { return OUTPUT_DEBUG; }
    
private:
    Stream& _output;
    void _printBinary(uint8_t value);
};

#endif // MINDSTREAMER_DEBUG_STREAMER_H