/**
 * @file TextStreamer.h
 * @brief Text/CSV output formatter
 * @author Dr. Hafed-Eddine Bendib
 * @version 1.0.0
 */

#ifndef MINDSTREAMER_TEXT_STREAMER_H
#define MINDSTREAMER_TEXT_STREAMER_H

#include "DataStreamer.h"

/**
 * @brief Stream a single-channel sample
 * @param channel 1-based channel index
 */

class TextStreamer : public DataStreamer {
    /**
 * @brief Text/CSV output formatter
 *
 * This streamer supports two CSV layouts:
 *
 * 1. Wide CSV (streamSample):
 *    sample,ch1,ch2,...,chN
 *
 * 2. Long CSV (streamSingleChannel):
 *    sample,channel,value
 */
public:
    TextStreamer(Stream& output = Serial);
    virtual ~TextStreamer() = default;
    
    void begin() override;
    /**
     * @brief Stream one full multi-channel sample row in wide CSV format
     * @details Output schema: sample,ch1,ch2,...,chN
     */
    void streamSample(uint32_t sample_counter, 
                      const uint8_t* status,
                      const int32_t* samples, 
                      uint8_t num_channels) override;
    /**
     * @brief Stream one single-channel sample row in long CSV format
     * @details Output schema: sample,channel,value
     */
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