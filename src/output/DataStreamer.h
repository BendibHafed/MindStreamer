/**
 * @file DataStreamer.h
 * @brief Abstract base class for data output formatters
 * @author Dr. Hafed-Eddine Bendib
 * @version 1.0.0
 */

#ifndef MINDSTREAMER_DATA_STREAMER_H
#define MINDSTREAMER_DATA_STREAMER_H

#include <Arduino.h>
#include "../include/MindStreamer_Types.h"

class DataStreamer {
public:
    virtual ~DataStreamer() = default;
    
    virtual void begin() = 0;
    virtual void streamSample(uint32_t sample_counter, 
                              const uint8_t* status,
                              const int32_t* samples, 
                              uint8_t num_channels) = 0;
    virtual void streamSingleChannel(uint32_t sample_counter,
                                     uint8_t channel,
                                     int32_t sample) = 0;
    virtual void streamRegisterDump(const uint8_t* registers, uint8_t num_registers) = 0;
    virtual OutputMode getMode() const = 0;
};

#endif // MINDSTREAMER_DATA_STREAMER_H