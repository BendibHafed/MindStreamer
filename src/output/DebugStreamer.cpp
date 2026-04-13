/**
 * @file DebugStreamer.cpp
 * @brief Implementation of debug output formatter
 * @author Dr. Hafed-Eddine Bendib
 * @version 1.0.0
 */

#include "DebugStreamer.h"

DebugStreamer::DebugStreamer(Stream& output) : _output(output) {}

void DebugStreamer::begin() {
    _output.println("=== MindStreamer Debug Mode ===");
}

void DebugStreamer::streamSample(uint32_t sample_counter, 
                                  const uint8_t* status,
                                  const int32_t* samples, 
                                  uint8_t num_channels) {
    _output.print("Sample: ");
    _output.println(sample_counter);
    
    _output.print("Status: ");
    for (int i = 0; i < 3; i++) {
        _printBinary(status[i]);
        _output.print(" ");
    }
    _output.println();
    
    for (uint8_t ch = 0; ch < num_channels; ch++) {
        _output.print("CH");
        _output.print(ch + 1);
        _output.print(": ");
        _output.println(samples[ch]);
    }
    _output.println("---");
}

void DebugStreamer::streamSingleChannel(uint32_t sample_counter,
                                        uint8_t channel,
                                        int32_t sample) {
    _output.print("Sample: ");
    _output.print(sample_counter);
    _output.print(" CH");
    _output.print(channel);
    _output.print(": ");
    _output.println(sample);
}

void DebugStreamer::streamRegisterDump(const uint8_t* registers, uint8_t num_registers) {
    _output.println("=== Register Dump ===");
    for (uint8_t i = 0; i < num_registers; i++) {
        _output.print("REG 0x");
        if (i < 0x10) _output.print("0");
        _output.print(i, HEX);
        _output.print(": 0x");
        if (registers[i] < 0x10) _output.print("0");
        _output.print(registers[i], HEX);
        _output.print(" (");
        _printBinary(registers[i]);
        _output.println(")");
    }
    _output.println("====================");
}

void DebugStreamer::_printBinary(uint8_t value) {
    for (int i = 7; i >= 0; i--) {
        _output.print((value >> i) & 1);
    }
}