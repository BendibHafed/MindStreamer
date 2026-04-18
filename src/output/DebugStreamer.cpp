/**
 * @file DebugStreamer.cpp
 * @brief Implementation of debug output formatter
 * @author Dr. Hafed-Eddine Bendib
 * @version 1.0.0
 */

#include "DebugStreamer.h"
#include "../core/ADS1299_Registers.h"

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
    if (status) {
        for (uint8_t i = 0; i < 3; i++) {
            _printBinary(status[i]);
            if (i < 2) {
                _output.print(" ");
            }
        }
    } else {
        _output.print("(null)");
    }
    _output.println();

    if (samples) {
        for (uint8_t ch = 0; ch < num_channels; ch++) {
            _output.print("CH");
            _output.print(ch + 1);
            _output.print(": ");
            _output.println(samples[ch]);
        }
    } else {
        _output.println("Samples: (null)");
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

static const char* getRegisterName(uint8_t reg) {
    switch (reg) {
        case ADS1299_REG_ID:         return "ID";
        case ADS1299_REG_CONFIG1:    return "CONFIG1";
        case ADS1299_REG_CONFIG2:    return "CONFIG2";
        case ADS1299_REG_CONFIG3:    return "CONFIG3";
        case ADS1299_REG_LOFF:       return "LOFF";
        case ADS1299_REG_CH1SET:     return "CH1SET";
        case ADS1299_REG_CH2SET:     return "CH2SET";
        case ADS1299_REG_CH3SET:     return "CH3SET";
        case ADS1299_REG_CH4SET:     return "CH4SET";
        case ADS1299_REG_CH5SET:     return "CH5SET";
        case ADS1299_REG_CH6SET:     return "CH6SET";
        case ADS1299_REG_CH7SET:     return "CH7SET";
        case ADS1299_REG_CH8SET:     return "CH8SET";
        case ADS1299_REG_BIAS_SENSP: return "BIAS_SENSP";
        case ADS1299_REG_BIAS_SENSN: return "BIAS_SENSN";
        case ADS1299_REG_LOFF_SENSP: return "LOFF_SENSP";
        case ADS1299_REG_LOFF_SENSN: return "LOFF_SENSN";
        case ADS1299_REG_LOFF_FLIP:  return "LOFF_FLIP";
        case ADS1299_REG_LOFF_STATP: return "LOFF_STATP";
        case ADS1299_REG_LOFF_STATN: return "LOFF_STATN";
        case ADS1299_REG_GPIO:       return "GPIO";
        case ADS1299_REG_MISC1:      return "MISC1";
        case ADS1299_REG_MISC2:      return "MISC2";
        case ADS1299_REG_CONFIG4:    return "CONFIG4";
        default:                     return "UNKNOWN";
    }
}

void DebugStreamer::streamRegisterDump(const uint8_t* registers, uint8_t num_registers) {
    if (!registers) {
        _output.println("=== Register Dump ===");
        _output.println("(null)");
        _output.println("====================");
        return;
    }

    _output.println("=== Register Dump ===");
    for (uint8_t i = 0; i < num_registers; i++) {
        _output.print(getRegisterName(i));
        _output.print(" (0x");
        if (i < 0x10) {
            _output.print("0");
        }
        _output.print(i, HEX);
        _output.print("): 0x");
        if (registers[i] < 0x10) {
            _output.print("0");
        }
        _output.print(registers[i], HEX);

        _output.print(" (");
        _printBinary(registers[i]);
        _output.println(")");
    }
    _output.println("====================");
}

void DebugStreamer::_printBinary(uint8_t value) {
    for (int8_t i = 7; i >= 0; i--) {
        _output.print((value >> i) & 0x01);
    }
}