/**
 * @file OpenEEGStreamer.h
 * @brief OpenEEG/BrainBay P2 protocol output formatter
 * @author Dr. Hafed-Eddine Bendib
 * @version 1.0.0
 */

#ifndef MINDSTREAMER_OPENEEG_STREAMER_H
#define MINDSTREAMER_OPENEEG_STREAMER_H

#include "DataStreamer.h"

class OpenEEGStreamer : public DataStreamer {
public:
    OpenEEGStreamer(Stream& output = Serial);
    virtual ~OpenEEGStreamer() = default;
    
    void begin() override;
    void streamSample(uint32_t sample_counter, 
                      const uint8_t* status,
                      const int32_t* samples, 
                      uint8_t num_channels) override;
    void streamSingleChannel(uint32_t sample_counter,
                             uint8_t channel,
                             int32_t sample) override;
    void streamRegisterDump(const uint8_t* registers, uint8_t num_registers) override;
    OutputMode getMode() const override { return OUTPUT_OPENEEG; }
    
private:
    Stream& _output;
    uint8_t _packet[17];  // P2 packet is 17 bytes
    int _counter;
    
    uint8_t _scaleSample(int32_t sample);
};

#endif // MINDSTREAMER_OPENEEG_STREAMER_H