/**
 * @file ADS1299_SPI.cpp
 * @brief Implementation of low-level SPI communication for ADS1299
 * @author Dr. Hafed-Eddine Bendib
 * @version 1.0.0
 */

#include "ADS1299_SPI.h"
#include "ADS1299_Registers.h"

ADS1299_SPI::ADS1299_SPI(int drdy_pin, int cs_pin, int rst_pin)
    : _drdy_pin(drdy_pin)
    , _cs_pin(cs_pin)
    , _rst_pin(rst_pin)
    , _spi(nullptr)
    , _spi_clock_hz(500000)
    , _initialized(false)
    , _rdatac_mode(false)
    , _last_error(ERROR_NONE) {
}

ADS1299_SPI::~ADS1299_SPI() {
    end();
}

bool ADS1299_SPI::debugReadFrameRaw(uint8_t* raw_bytes, uint8_t num_channels) {
    if (!_initialized || !_spi) {
        _last_error = ERROR_INVALID_CONFIG;
        return false;
    }

    if (!raw_bytes || num_channels == 0) {
        _last_error = ERROR_INVALID_CONFIG;
        return false;
    }

    if (!_rdatac_mode) {
        _last_error = ERROR_STREAMING_INACTIVE;
        return false;
    }

    if (!waitForDataReady()) {
        if (_last_error == ERROR_NONE) {
            _last_error = ERROR_DRDY_TIMEOUT;
        }
        return false;
    }

    const uint8_t total_bytes = 3 + 3 * num_channels;

    _selectChip();
    for (uint8_t i = 0; i < total_bytes; ++i) {
        raw_bytes[i] = _transfer(0x00);
    }
    _deselectChip();

    _last_error = ERROR_NONE;
    return true;
}

static inline bool isValidADS1299_8ch(uint8_t id) {
    // Fast reject for obvious bus/read failures
    if (id == 0x00 || id == 0xFF) {
        return false;
    }

    const bool bit4_ok  = ((id & ADS1299_ID_BIT4_MASK)  == ADS1299_ID_BIT4_EXPECTED);
    const bool devid_ok = ((id & ADS1299_ID_DEVID_MASK) == ADS1299_ID_DEVID_EXPECTED);
    const bool numch_ok = ((id & ADS1299_ID_NUMCH_MASK) == ADS1299_ID_NUMCH_8CH);

    return bit4_ok && devid_ok && numch_ok;
}

bool ADS1299_SPI::begin(uint32_t spi_clock_hz) {
    _spi_clock_hz = spi_clock_hz;

    pinMode(_cs_pin, OUTPUT);
    pinMode(_rst_pin, OUTPUT);
    pinMode(_drdy_pin, INPUT_PULLUP);

    pinMode(MINDSTREAMER_SPI_SCLK, OUTPUT);
    pinMode(MINDSTREAMER_SPI_MOSI, OUTPUT);
    pinMode(MINDSTREAMER_SPI_MISO, INPUT);

    digitalWrite(_cs_pin, HIGH);
    digitalWrite(_rst_pin, HIGH);

    if (_spi) {
        end();
    }

    _spi = new SPIClass(VSPI);
    _spi->begin(MINDSTREAMER_SPI_SCLK,
            MINDSTREAMER_SPI_MISO,
            MINDSTREAMER_SPI_MOSI,
            _cs_pin);
    _spi->beginTransaction(SPISettings(_spi_clock_hz, MSBFIRST, SPI_MODE1));

    _initialized = true;
    _last_error = ERROR_NONE;
    _rdatac_mode = false;

    hardwareReset();

    // Put device into safe register-access state
    sendCommand(ADS1299_CMD_SDATAC);
    _delayCommandDecode();

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
    _rdatac_mode = false;
}

void ADS1299_SPI::hardwareReset() {
    if (!_initialized) return;

    // Conservative reset pulse
    digitalWrite(_rst_pin, LOW);
    delay(10);
    digitalWrite(_rst_pin, HIGH);

    // Give the digital filter / internal references time to settle
    delay(50);

    _rdatac_mode = true;
}

bool ADS1299_SPI::sendCommand(uint8_t cmd) {
    if (!_initialized || !_spi) return false;

    _selectChip();
    _transfer(cmd);
    _deselectChip();

    switch (cmd) {
        case ADS1299_CMD_RESET:
            _delayResetRecovery();
            _rdatac_mode = true;
            break;

        case ADS1299_CMD_RDATAC:
            _delayCommandDecode();
            _rdatac_mode = true;
            break;

        case ADS1299_CMD_SDATAC:
            _delayCommandDecode();
            _rdatac_mode = false;
            break;

        case ADS1299_CMD_WAKEUP:
        case ADS1299_CMD_START:
        case ADS1299_CMD_STOP:
        case ADS1299_CMD_STANDBY:
        default:
            _delayCommandDecode();
            break;
    }

    return true;
}

bool ADS1299_SPI::readRegister(uint8_t reg_addr, uint8_t* value) {
    if (!_initialized || !value) return false;

    bool restore_rdatac = false;
    if (!_enterRegisterAccessMode(restore_rdatac)) {
        return false;
    }

    _selectChip();

    _transfer(ADS1299_CMD_RREG | reg_addr);
    _delayCommandDecode();

    _transfer(0x00);
    _delayCommandDecode();

    *value = _transfer(0x00);

    _deselectChip();

    _restoreReadMode(restore_rdatac);
    return true;
}

bool ADS1299_SPI::readRegisters(uint8_t start_addr, uint8_t count, uint8_t* values) {
    if (!_initialized || !values || count == 0) return false;

    bool restore_rdatac = false;
    if (!_enterRegisterAccessMode(restore_rdatac)) {
        return false;
    }

    _selectChip();

    _transfer(ADS1299_CMD_RREG | start_addr);
    _delayCommandDecode();

    _transfer(count - 1);
    _delayCommandDecode();

    for (uint8_t i = 0; i < count; i++) {
        values[i] = _transfer(0x00);
    }

    _deselectChip();

    _restoreReadMode(restore_rdatac);
    return true;
}

bool ADS1299_SPI::writeRegister(uint8_t reg_addr, uint8_t value) {
    if (!_initialized) return false;

    bool restore_rdatac = false;
    if (!_enterRegisterAccessMode(restore_rdatac)) {
        return false;
    }

    _selectChip();

    _transfer(ADS1299_CMD_WREG | reg_addr);
    _delayCommandDecode();

    _transfer(0x00);
    _delayCommandDecode();

    _transfer(value);

    _deselectChip();

    _restoreReadMode(restore_rdatac);
    return true;
}

bool ADS1299_SPI::writeRegisters(uint8_t start_addr, uint8_t count, const uint8_t* values) {
    if (!_initialized || !values || count == 0) return false;

    bool restore_rdatac = false;
    if (!_enterRegisterAccessMode(restore_rdatac)) {
        return false;
    }

    _selectChip();

    _transfer(ADS1299_CMD_WREG | start_addr);
    _delayCommandDecode();

    _transfer(count - 1);
    _delayCommandDecode();

    for (uint8_t i = 0; i < count; i++) {
        _transfer(values[i]);
    }

    _deselectChip();

    _restoreReadMode(restore_rdatac);
    return true;
}

bool ADS1299_SPI::isDataReady() {
    return (digitalRead(_drdy_pin) == LOW);
}

bool ADS1299_SPI::waitForDataReady(uint32_t timeout_ms) {
    return _waitForDRDY(timeout_ms);
}

bool ADS1299_SPI::readSample(uint8_t* status_buffer, int32_t* channel_data, uint8_t num_channels) {
    if (!_initialized || !_spi) return false;
    if (num_channels == 0) return false;
    if (!_rdatac_mode) return false;
    if (!waitForDataReady()) return false;

    _selectChip();

    for (uint8_t i = 0; i < 3; i++) {
        uint8_t byte = _transfer(0x00);
        if (status_buffer) {
            status_buffer[i] = byte;
        }
    }

    for (uint8_t ch = 0; ch < num_channels; ch++) {
        uint32_t sample = 0;
        for (uint8_t i = 0; i < 3; i++) {
            sample = (sample << 8) | _transfer(0x00);
        }

        if (sample & 0x00800000UL) {
            sample |= 0xFF000000UL;
        }

        if (channel_data) {
            channel_data[ch] = static_cast<int32_t>(sample);
        }
    }

    _deselectChip();
    return true;
}

bool ADS1299_SPI::verifyDevice() {
    static constexpr uint8_t kMaxAttempts = 5;

    uint8_t id = 0;
    bool saw_successful_read = false;

    for (uint8_t attempt = 0; attempt < kMaxAttempts; ++attempt) {
        if (readRegister(ADS1299_REG_ID, &id)) {
            saw_successful_read = true;

            Serial.print("ADS1299 raw ID read = 0x");
            Serial.println(id, HEX);

            if (isValidADS1299_8ch(id)) {
                _last_error = ERROR_NONE;
                return true;
            }
        }

        delay(2);
    }

    _last_error = saw_successful_read ? ERROR_DEVICE_NOT_FOUND : ERROR_REGISTER_READ;
    return false;
}

void ADS1299_SPI::setDaisyChainMode(bool enable) {
    uint8_t config1 = 0;
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

void ADS1299_SPI::_delayCommandDecode() const {
    delayMicroseconds(ADS1299_T_SDECODE_US);
}

void ADS1299_SPI::_delayResetRecovery() const {
    delayMicroseconds(ADS1299_T_RESET_RECOVERY_US);
}

bool ADS1299_SPI::_enterRegisterAccessMode(bool& restore_rdatac) {
    restore_rdatac = _rdatac_mode;
    if (_rdatac_mode) {
        if (!sendCommand(ADS1299_CMD_SDATAC)) {
            return false;
        }
    }
    return true;
}

void ADS1299_SPI::_restoreReadMode(bool restore_rdatac) {
    if (restore_rdatac) {
        sendCommand(ADS1299_CMD_RDATAC);
    }
}