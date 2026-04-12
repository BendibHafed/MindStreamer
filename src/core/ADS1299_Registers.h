/**
 * @file ADS1299_Registers.h
 * @brief ADS1299 register addresses, command definitions, and bit masks
 * @author Dr. Hafed-Eddine Bendib
 * @version 1.0.0
 * 
 * Based on Texas Instruments ADS1299 datasheet (rev. D, 2017)
 */

#ifndef MINDSTREAMER_ADS1299_REGISTERS_H
#define MINDSTREAMER_ADS1299_REGISTERS_H

#include <stdint.h>

//=============================================================================
// Register Addresses (Register Map - Datasheet p.44)
//=============================================================================

// Read-only ID register
#define ADS1299_REG_ID           0x00

// Global settings registers
#define ADS1299_REG_CONFIG1      0x01
#define ADS1299_REG_CONFIG2      0x02
#define ADS1299_REG_CONFIG3      0x03
#define ADS1299_REG_LOFF         0x04

// Channel-specific settings registers (8 channels)
#define ADS1299_REG_CH1SET       0x05
#define ADS1299_REG_CH2SET       0x06
#define ADS1299_REG_CH3SET       0x07
#define ADS1299_REG_CH4SET       0x08
#define ADS1299_REG_CH5SET       0x09
#define ADS1299_REG_CH6SET       0x0A
#define ADS1299_REG_CH7SET       0x0B
#define ADS1299_REG_CH8SET       0x0C

// BIAS and lead-off control registers
#define ADS1299_REG_BIAS_SENSP   0x0D
#define ADS1299_REG_BIAS_SENSN   0x0E
#define ADS1299_REG_LOFF_SENSP   0x0F
#define ADS1299_REG_LOFF_SENSN   0x10
#define ADS1299_REG_LOFF_FLIP    0x11

// Read-only lead-off status registers
#define ADS1299_REG_LOFF_STATP   0x12
#define ADS1299_REG_LOFF_STATN   0x13

// GPIO and miscellaneous registers
#define ADS1299_REG_GPIO         0x14
#define ADS1299_REG_MISC1        0x15
#define ADS1299_REG_MISC2        0x16
#define ADS1299_REG_CONFIG4      0x17

// Total number of registers
#define ADS1299_NUM_REGISTERS    0x18

//=============================================================================
// System Commands (Datasheet p.40)
//=============================================================================

#define ADS1299_CMD_WAKEUP       0x02   // Wake up from standby mode
#define ADS1299_CMD_STANDBY      0x04   // Enter standby mode
#define ADS1299_CMD_RESET        0x06   // Reset device to default settings
#define ADS1299_CMD_START        0x08   // Start/restart conversions
#define ADS1299_CMD_STOP         0x0A   // Stop conversions

//=============================================================================
// Data Read Commands (Datasheet p.40-41)
//=============================================================================

#define ADS1299_CMD_RDATAC       0x10   // Read data continuous mode
#define ADS1299_CMD_SDATAC       0x11   // Stop read data continuous mode
#define ADS1299_CMD_RDATA        0x12   // Read data by command

//=============================================================================
// Register Read/Write Commands (Datasheet p.42-43)
//=============================================================================

#define ADS1299_CMD_RREG         0x20   // Read register (OR with address)
#define ADS1299_CMD_WREG         0x40   // Write register (OR with address)

//=============================================================================
// CONFIG1 Register Bits (Address 0x01)
//=============================================================================

#define ADS1299_CONFIG1_DR_MASK  0x07   // Data rate mask (bits 2:0)
#define ADS1299_CONFIG1_DR_250   0x06   // 250 SPS (default)
#define ADS1299_CONFIG1_DR_500   0x05   // 500 SPS
#define ADS1299_CONFIG1_DR_1000  0x04   // 1000 SPS
#define ADS1299_CONFIG1_DR_2000  0x03   // 2000 SPS
#define ADS1299_CONFIG1_DR_4000  0x02   // 4000 SPS
#define ADS1299_CONFIG1_DR_8000  0x01   // 8000 SPS
#define ADS1299_CONFIG1_DR_16000 0x00   // 16000 SPS

#define ADS1299_CONFIG1_DAISY_EN 0x40   // Daisy-chain mode enable (bit 6)

//=============================================================================
// CONFIG2 Register Bits (Address 0x02)
//=============================================================================

#define ADS1299_CONFIG2_TEST_SIG_SRC  0x10   // Test signal source (1=internal, 0=external)
#define ADS1299_CONFIG2_TEST_SIG_AMP  0x04   // Test signal amplitude (1=2x, 0=1x)
#define ADS1299_CONFIG2_TEST_SIG_FREQ 0x03   // Test signal frequency mask

// Test signal frequency values
#define ADS1299_TEST_FREQ_fCLK_21     0x00   // fCLK / 2^21
#define ADS1299_TEST_FREQ_fCLK_20     0x01   // fCLK / 2^20
#define ADS1299_TEST_FREQ_DC          0x03   // DC

//=============================================================================
// CONFIG3 Register Bits (Address 0x03)
//=============================================================================

#define ADS1299_CONFIG3_PDB_REFBUF    0x80   // Power-down reference buffer (1=power down)
#define ADS1299_CONFIG3_BIAS_REF_INT  0x08   // Internal BIASREF enable (1=internal)
#define ADS1299_CONFIG3_BIAS_BUFFER   0x04   // BIAS buffer power (1=power on)
#define ADS1299_CONFIG3_BIAS_MEAS     0x10   // BIAS measurement enable
#define ADS1299_CONFIG3_BIAS_SENS     0x02   // BIAS sense enable
#define ADS1299_CONFIG3_BIAS_LOFF_SENS 0x01  // BIAS lead-off sense

//=============================================================================
// CHnSET Register Bits (Addresses 0x05-0x0C)
//=============================================================================

#define ADS1299_CHNSET_PD            0x80   // Power down channel (1=power down)
#define ADS1299_CHNSET_GAIN_MASK     0x70   // Gain bits (bits 6:4)
#define ADS1299_CHNSET_SRB2          0x08   // SRB2 switch (1=closed)
#define ADS1299_CHNSET_MUX_MASK      0x07   // Input mux selection (bits 2:0)

// Gain settings (value << 4)
#define ADS1299_GAIN_1               0x00   // Gain = 1
#define ADS1299_GAIN_2               0x10   // Gain = 2
#define ADS1299_GAIN_4               0x20   // Gain = 4
#define ADS1299_GAIN_6               0x30   // Gain = 6
#define ADS1299_GAIN_8               0x40   // Gain = 8
#define ADS1299_GAIN_12              0x50   // Gain = 12
#define ADS1299_GAIN_24              0x60   // Gain = 24

// Input mux selections
#define ADS1299_MUX_NORMAL           0x00   // Normal electrode input
#define ADS1299_MUX_SHORTED          0x01   // Inputs shorted (for offset measurement)
#define ADS1299_MUX_BIAS_MEAS        0x02   // BIAS measurement
#define ADS1299_MUX_MVDD_MEAS        0x03   // MVDD measurement
#define ADS1299_MUX_TEMP_SENS        0x04   // Temperature sensor
#define ADS1299_MUX_TEST_SIG         0x05   // Test signal
#define ADS1299_MUX_DRV_POS          0x06   // Positive driver
#define ADS1299_MUX_DRV_NEG          0x07   // Negative driver

//=============================================================================
// MISC1 Register Bits (Address 0x15)
//=============================================================================

#define ADS1299_MISC1_SRB1           0x20   // SRB1 switch (1=closed)

//=============================================================================
// CONFIG4 Register Bits (Address 0x17)
//=============================================================================

#define ADS1299_CONFIG4_SINGLE_SHOT  0x08   // Single-shot mode (1=enabled)

//=============================================================================
// Default Register Values (Datasheet p.45-48)
//=============================================================================

#define ADS1299_DEFAULT_ID          0x3E   // Expected device ID
#define ADS1299_DEFAULT_CONFIG1     0x96   // 250 SPS, daisy-chain disabled
#define ADS1299_DEFAULT_CONFIG2     0xC0   // Test signals off
#define ADS1299_DEFAULT_CONFIG3     0xEC   // Internal ref, BIAS on
#define ADS1299_DEFAULT_CONFIG4     0x00   // Continuous mode
#define ADS1299_DEFAULT_CHnSET      0x60   // Gain=6, normal input, channel on
#define ADS1299_DEFAULT_MISC1       0x00   // SRB1 off

//=============================================================================
// Packet Format Constants
//=============================================================================

#define MINDSTREAMER_PACKET_HEADER  0xBB   // Binary packet header
#define MINDSTREAMER_PACKET_FOOTER  0xEE   // Binary packet footer
#define ADS1299_STATUS_BYTES        3      // Status register bytes
#define ADS1299_CHANNEL_BYTES       3      // Bytes per channel (24-bit)

//=============================================================================
// Timing Constants (in microseconds)
//=============================================================================

#define ADS1299_T_POR_US            1000   // Power-on reset time (1 ms)
#define ADS1299_T_RST_US            2      // Reset pulse width (2 μs)
#define ADS1299_T_START_US          18     // Start command delay (18 μs)
#define ADS1299_T_SDECODE_US        4      // Command decode time (4 μs)

#endif // MINDSTREAMER_ADS1299_REGISTERS_H