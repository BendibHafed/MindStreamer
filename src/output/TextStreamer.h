/**
 * @file TextStreamer.h
 * @brief Text/CSV output formatter
 * @author Dr. Hafed-Eddine Bendib
 * @version 1.0.0
 */

#ifndef MINDSTREAMER_TEXT_STREAMER_H
#define MINDSTREAMER_TEXT_STREAMER_H

#include "DataStreamer.h"

class TextStreamer : public DataStreamer {
public:
    TextStreamer(Stream& output = Serial);
    virtual ~TextStreamer() = default;
    
    void begin() override;
    void streamSample(uint32_t sample_counter, 
                      const uint8_t* status,
                      const int32_t* samples, 
                      uint8_t num_channels) override;
    void streamSingleChannel(uint32_t sample_counter,
                             uint8_t channel,
                             int32_t sample) override;
    void streamRegisterDump(const uint8_t* registers, uint8_t num_registers) override;
    OutputMode getMode() const override { return OUTPUT_TEXT; }
    
    void setPrintHeader(bool print) { _print_header = print; }
    
private:
    Stream& _output;
    bool _print_header;
    bool _header_printed;
};

#endif // MINDSTREAMER_TEXT_STREAMER_H