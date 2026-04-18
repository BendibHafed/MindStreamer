/**
 * @file ADS1299_SPI.h
 * @brief Low-level SPI communication manager for ADS1299
 * @author Dr. Hafed-Eddine Bendib
 * @version 1.0.0
 */

#ifndef MINDSTREAMER_ADS1299_SPI_H
#define MINDSTREAMER_ADS1299_SPI_H

#include <Arduino.h>
#include <SPI.h>
#include "../include/MindStreamer_Types.h"
#include "ADS1299_Registers.h"

/**
 * Fixed SPI mapping for DOIT ESP32 DEVKIT V1:
 * SCLK=18, MISO=19, MOSI=23
 */
#define MINDSTREAMER_SPI_SCLK  18
#define MINDSTREAMER_SPI_MISO  19
#define MINDSTREAMER_SPI_MOSI  23

class ADS1299_SPI {
public:
    /**
     * @brief Construct SPI manager with specified pins
     * @param drdy_pin Data ready pin (interrupt capable)
     * @param cs_pin Chip select pin
     * @param rst_pin Reset pin
     */
    ADS1299_SPI(int drdy_pin, int cs_pin, int rst_pin);
    ~ADS1299_SPI();

    bool debugReadFrameRaw(uint8_t* raw_bytes, uint8_t num_channels);
    
    /**
     * @brief Initialize SPI interface and hardware pins
     * @param spi_clock_hz SPI clock frequency in Hz (max 2 MHz for ADS1299)
     * @return true if initialization successful
     */
    bool begin(uint32_t spi_clock_hz = 500000);
    
    /**
     * @brief Close SPI interface
     */
    void end();
    
    /**
     * @brief Reset ADS1299 using hardware reset pin
     */
    void hardwareReset();
    
    /**
     * @brief Send system command to ADS1299
     * @param cmd Command byte (see ADS1299_CMD_* definitions)
     * @return true if command sent successfully
     */
    bool sendCommand(uint8_t cmd);
    
    /**
     * @brief Read a single register
     * @param reg_addr Register address
     * @param value Output parameter for register value
     * @return true if read successful
     */
    bool readRegister(uint8_t reg_addr, uint8_t* value);
    
    /**
     * @brief Read multiple consecutive registers
     * @param start_addr Starting register address
     * @param count Number of registers to read
     * @param values Output buffer (must be at least count bytes)
     * @return true if read successful
     */
    bool readRegisters(uint8_t start_addr, uint8_t count, uint8_t* values);

    /**
     * @brief Write a single register
     * @param reg_addr Register address
     * @param value Value to write
     * @return true if write successful
     */
    bool writeRegister(uint8_t reg_addr, uint8_t value);
    
    /**
     * @brief Write multiple consecutive registers
     * @param start_addr Starting register address
     * @param count Number of registers to write
     * @param values Buffer containing values to write
     * @return true if write successful
     */
    bool writeRegisters(uint8_t start_addr, uint8_t count, const uint8_t* values);
    
    /**
     * @brief Check if data is ready from ADS1299
     * @return true if DRDY pin is low (data ready)
     */
    bool isDataReady();
    
    /**
     * @brief Wait for DRDY with timeout
     * @param timeout_ms Timeout in milliseconds
     * @return true if data ready within timeout
     */
    bool waitForDataReady(uint32_t timeout_ms = 100);
    
    /**
     * @brief Read one complete sample (status + all channels)
     * @param status_buffer Buffer for 3-byte status (or NULL if not needed)
     * @param channel_data Buffer for channel data (24 bits per channel)
     * @param num_channels Number of channels to read
     * @return true if read successful
     */
    bool readSample(uint8_t* status_buffer, int32_t* channel_data, uint8_t num_channels);
    
    /**
     * @brief Get the last error code
     * @return Last error code
     */
    ErrorCode getLastError() const { return _last_error; }
    
    /**
     * @brief Clear the last error code
     */
    void clearError() { _last_error = ERROR_NONE; }
    
    /**
     * @brief Verify device communication by reading ID register
     * @return true if device responds with correct ID (0x3E)
     */
    bool verifyDevice();
    
    /**
     * @brief Enable/disable daisy chain mode
     * @param enable true for daisy chain, false for multiple readback
     */
    void setDaisyChainMode(bool enable);

    /**
     * @brief Query internal mode tracking
     */
    bool isContinuousReadMode() const { return _rdatac_mode; }
    
private:
    int _drdy_pin;
    int _cs_pin;
    int _rst_pin;
    SPIClass* _spi;
    uint32_t _spi_clock_hz;
    bool _initialized;
    bool _rdatac_mode;
    ErrorCode _last_error;

    void _selectChip();
    void _deselectChip();
    uint8_t _transfer(uint8_t data);
    bool _waitForDRDY(uint32_t timeout_ms);

    void _delayCommandDecode() const;
    void _delayResetRecovery() const;
    bool _enterRegisterAccessMode(bool& restore_rdatac);
    void _restoreReadMode(bool restore_rdatac);
};

#endif // MINDSTREAMER_ADS1299_SPI_H