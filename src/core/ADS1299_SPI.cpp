/**
 * @file ADS1299_SPI.cpp
 * @brief Implementation of low-level SPI communication for ADS1299
 * @author Dr. Hafed-Eddine Bendib
 * @version 1.0.0
 */

#include "ADS1299_SPI.h"

ADS1299_SPI::ADS1299_SPI(int drdy_pin, int cs_pin, int rst_pin)
    : _drdy_pin(drdy_pin)
    , _cs_pin(cs_pin)
    , _rst_pin(rst_pin)
    , _spi(nullptr)
    , _spi_clock_hz(1000000)
    , _initialized(false)
    , _last_error(ERROR_NONE) {
}

ADS1299_SPI::~ADS1299_SPI() {
    end();
}

bool ADS1299_SPI::begin(uint32_t spi_clock_hz) {
    _spi_clock_hz = spi_clock_hz;
    
    // Configure pins
    pinMode(_cs_pin, OUTPUT);
    pinMode(_rst_pin, OUTPUT);
    pinMode(_drdy_pin, INPUT_PULLUP);
    
    // Deselect chip initially
    digitalWrite(_cs_pin, HIGH);
    
    // Hold reset low
    digitalWrite(_rst_pin, LOW);
    delay(10);
    
    // Initialize SPI
    _spi = new SPIClass(VSPI);
    _spi->begin();
    _spi->beginTransaction(SPISettings(_spi_clock_hz, MSBFIRST, SPI_MODE1));
    
    // Release reset
    digitalWrite(_rst_pin, HIGH);
    delay(ADS1299_T_POR_US / 1000);
    
    _initialized = true;
    _last_error = ERROR_NONE;
    
    // Verify device communication
    if (!verifyDevice()) {
        _last_error = ERROR_DEVICE_NOT_FOUND;
        return false;
    }
    
    return true;
}

void ADS1299_SPI::end() {
    if (_spi) {
        _spi->endTransaction();
        _spi->end();
        delete _spi;
        _spi = nullptr;
    }
    _initialized = false;
}

void ADS1299_SPI::hardwareReset() {
    digitalWrite(_rst_pin, LOW);
    delayMicroseconds(ADS1299_T_RST_US);
    digitalWrite(_rst_pin, HIGH);
    delay(ADS1299_T_POR_US / 1000);
}

bool ADS1299_SPI::sendCommand(uint8_t cmd) {
    if (!_initialized) return false;
    
    _selectChip();
    _transfer(cmd);
    _deselectChip();
    
    // Wait for command decode time (except for WAKEUP which needs longer)
    if (cmd != ADS1299_CMD_WAKEUP) {
        delayMicroseconds(ADS1299_T_SDECODE_US);
    }
    
    return true;
}

bool ADS1299_SPI::readRegister(uint8_t reg_addr, uint8_t* value) {
    if (!_initialized || !value) return false;
    
    // Must exit RDATAC mode first
    sendCommand(ADS1299_CMD_SDATAC);
    delayMicroseconds(ADS1299_T_SDECODE_US);
    
    _selectChip();
    
    // Send read command with address
    _transfer(ADS1299_CMD_RREG | reg_addr);
    _transfer(0x00);  // Read 1 register (0 = 1 register)
    
    // Read register value
    *value = _transfer(0x00);
    
    _deselectChip();
    
    return true;
}

bool ADS1299_SPI::readRegisters(uint8_t start_addr, uint8_t count, uint8_t* values) {
    if (!_initialized || !values || count == 0) return false;
    
    // Must exit RDATAC mode first
    sendCommand(ADS1299_CMD_SDATAC);
    delayMicroseconds(ADS1299_T_SDECODE_US);
    
    _selectChip();
    
    // Send read command with start address
    _transfer(ADS1299_CMD_RREG | start_addr);
    _transfer(count - 1);  // Number of registers to read minus 1
    
    // Read register values
    for (uint8_t i = 0; i < count; i++) {
        values[i] = _transfer(0x00);
    }
    
    _deselectChip();
    
    return true;
}

bool ADS1299_SPI::writeRegister(uint8_t reg_addr, uint8_t value) {
    if (!_initialized) return false;
    
    // Must exit RDATAC mode first
    sendCommand(ADS1299_CMD_SDATAC);
    delayMicroseconds(ADS1299_T_SDECODE_US);
    
    _selectChip();
    
    // Send write command with address
    _transfer(ADS1299_CMD_WREG | reg_addr);
    _transfer(0x00);  // Write 1 register (0 = 1 register)
    _transfer(value);
    
    _deselectChip();
    
    return true;
}

bool ADS1299_SPI::writeRegisters(uint8_t start_addr, uint8_t count, const uint8_t* values) {
    if (!_initialized || !values || count == 0) return false;
    
    // Must exit RDATAC mode first
    sendCommand(ADS1299_CMD_SDATAC);
    delayMicroseconds(ADS1299_T_SDECODE_US);
    
    _selectChip();
    
    // Send write command with start address
    _transfer(ADS1299_CMD_WREG | start_addr);
    _transfer(count - 1);  // Number of registers to write minus 1
    
    // Write register values
    for (uint8_t i = 0; i < count; i++) {
        _transfer(values[i]);
    }
    
    _deselectChip();
    
    return true;
}

bool ADS1299_SPI::isDataReady() {
    return (digitalRead(_drdy_pin) == LOW);
}

bool ADS1299_SPI::waitForDataReady(uint32_t timeout_ms) {
    return _waitForDRDY(timeout_ms);
}

bool ADS1299_SPI::readSample(uint8_t* status_buffer, int32_t* channel_data, uint8_t num_channels) {
    if (!_initialized) return false;
    if (!waitForDataReady()) return false;
    
    _selectChip();
    
    // Read status register (3 bytes)
    for (uint8_t i = 0; i < 3; i++) {
        uint8_t byte = _transfer(0x00);
        if (status_buffer) {
            status_buffer[i] = byte;
        }
    }
    
    // Read channel data (3 bytes per channel)
    for (uint8_t ch = 0; ch < num_channels; ch++) {
        uint32_t sample = 0;
        for (uint8_t i = 0; i < 3; i++) {
            uint8_t byte = _transfer(0x00);
            sample = (sample << 8) | byte;
        }
        
        // Convert 24-bit two's complement to 32-bit signed
        if (sample & 0x800000) {
            sample |= 0xFF000000;  // Sign extend
        }
        
        if (channel_data) {
            channel_data[ch] = (int32_t)sample;
        }
    }
    
    _deselectChip();
    return true;
}

bool ADS1299_SPI::verifyDevice() {
    uint8_t id = 0;
    if (!readRegister(ADS1299_REG_ID, &id)) {
        return false;
    }
    return (id == ADS1299_DEFAULT_ID);
}

void ADS1299_SPI::setDaisyChainMode(bool enable) {
    uint8_t config1;
    if (readRegister(ADS1299_REG_CONFIG1, &config1)) {
        if (enable) {
            config1 |= ADS1299_CONFIG1_DAISY_EN;
        } else {
            config1 &= ~ADS1299_CONFIG1_DAISY_EN;
        }
        writeRegister(ADS1299_REG_CONFIG1, config1);
    }
}

void ADS1299_SPI::_selectChip() {
    digitalWrite(_cs_pin, LOW);
}

void ADS1299_SPI::_deselectChip() {
    digitalWrite(_cs_pin, HIGH);
}

uint8_t ADS1299_SPI::_transfer(uint8_t data) {
    return _spi->transfer(data);
}

bool ADS1299_SPI::_waitForDRDY(uint32_t timeout_ms) {
    uint32_t start = millis();
    while (digitalRead(_drdy_pin) == HIGH) {
        if ((millis() - start) >= timeout_ms) {
            _last_error = ERROR_DRDY_TIMEOUT;
            return false;
        }
        delayMicroseconds(10);
    }
    return true;
}