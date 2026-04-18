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
    // Do not print the header here because we do not yet know num_channels.
    // The full CSV header is emitted on the first call to streamSample().
    _header_printed = false;
}

void TextStreamer::streamSample(uint32_t sample_counter,
                                const uint8_t* status,
                                const int32_t* samples,
                                uint8_t num_channels) {
    (void)status;  // unused in text mode
    if (!samples) return;
    // Print full CSV header once
    // Wide CSV header: sample,ch1,ch2,...,chN
    // Long CSV header: sample,channel,value
    if (_print_header && !_header_printed) {
        _output.print("sample");
        for (uint8_t ch = 0; ch < num_channels; ch++) {
            _output.print(",ch");
            _output.print(ch + 1);
        }
        _output.println();
        _header_printed = true;
    }

    _output.print(sample_counter);

    for (uint8_t ch = 0; ch < num_channels; ch++) {
        _output.print(",");
        if (samples) {
            _output.print(samples[ch]);
        } else {
            _output.print("0");
        }
    }
    _output.println();
}

void TextStreamer::streamSingleChannel(uint32_t sample_counter,
                                       uint8_t channel,
                                       int32_t sample) {
    if (_print_header && !_header_printed) {
        _output.println("sample,channel,value");
        _header_printed = true;
    }

    _output.print(sample_counter);
    _output.print(",");
    _output.print(channel);
    _output.print(",");
    _output.println(sample);
}

void TextStreamer::streamRegisterDump(const uint8_t* registers, uint8_t num_registers) {
    (void)registers;
    (void)num_registers;
    // Not supported in text mode
}