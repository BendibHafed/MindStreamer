/**
 * @file TextStreamer.cpp
 * @brief Implementation of text/CSV output formatter
 * @author Dr. Hafed-Eddine Bendib
 * @version 1.0.0
 */

#include "TextStreamer.h"

TextStreamer::TextStreamer(Stream& output)
    : _output(output), _print_header(true), _header_printed(false) {}

void TextStreamer::begin() {
    if (_print_header && !_header_printed) {
        _output.print("sample");
        // Header will be completed in streamSample when we know num_channels
        _header_printed = true;
    }
}

void TextStreamer::streamSample(uint32_t sample_counter, 
                                  const uint8_t* status,
                                  const int32_t* samples, 
                                  uint8_t num_channels) {
    // Print header on first sample
    if (_print_header && !_header_printed) {
        for (uint8_t ch = 0; ch < num_channels; ch++) {
            _output.print(",ch");
            _output.print(ch + 1);
        }
        _output.println();
        _header_printed = true;
    }
    
    // Print sample counter
    _output.print(sample_counter);
    
    // Print each channel
    for (uint8_t ch = 0; ch < num_channels; ch++) {
        _output.print(",");
        _output.print(samples[ch]);
    }
    _output.println();
}

void TextStreamer::streamSingleChannel(uint32_t sample_counter,
                                        uint8_t channel,
                                        int32_t sample) {
    _output.print(sample_counter);
    _output.print(",");
    _output.print(channel);
    _output.print(",");
    _output.println(sample);
}

void TextStreamer::streamRegisterDump(const uint8_t* registers, uint8_t num_registers) {
    // Not supported in text mode
}