#ifndef NRF24L01_H
#define NRF24L01_H

#include "main.h"
#include <stdint.h>

/*========== Pin assignments (VGT6, NRF only) ==========*/
/*  SPI1 SCK=PA5, MISO=PA6, MOSI=PA7                      */
/*  NRF  CE=PB6, CSN=PB5, IRQ=PB7                          */
#define NRF_CE_PORT      GPIOB
#define NRF_CE_PIN        GPIO_PIN_6
#define NRF_CSN_PORT      GPIOB
#define NRF_CSN_PIN       GPIO_PIN_5
#define NRF_IRQ_PORT      GPIOB
#define NRF_IRQ_PIN        GPIO_PIN_7

/*========== GPIO 操作宏 ==========*/
#define nrf_ce_high()   (NRF_CE_PORT->BSRR   = NRF_CE_PIN)
#define nrf_ce_low()    (NRF_CE_PORT->BSRR   = (uint32_t)NRF_CE_PIN << 16)
#define nrf_csn_high()  (NRF_CSN_PORT->BSRR  = NRF_CSN_PIN)
#define nrf_csn_low()   (NRF_CSN_PORT->BSRR  = (uint32_t)NRF_CSN_PIN << 16)

/*========== SPI commands ==========*/
#define NRF_CMD_R_REGISTER        0x00
#define NRF_CMD_W_REGISTER        0x20
#define NRF_CMD_R_RX_PAYLOAD      0x61
#define NRF_CMD_W_TX_PAYLOAD      0xA0
#define NRF_CMD_FLUSH_TX          0xE1
#define NRF_CMD_FLUSH_RX          0xE2
#define NRF_CMD_R_RX_PL_WID       0x60
#define NRF_CMD_NOP               0xFF
#define NRF_CMD_W_TX_PAYLOAD_NOACK 0xB0

/*========== Register addresses ==========*/
#define NRF_REG_CONFIG      0x00
#define NRF_REG_EN_AA      0x01
#define NRF_REG_EN_RXADDR   0x02
#define NRF_REG_SETUP_AW    0x03
#define NRF_REG_SETUP_RETR  0x04
#define NRF_REG_RF_CH       0x05
#define NRF_REG_RF_SETUP    0x06
#define NRF_REG_STATUS      0x07
#define NRF_REG_RX_ADDR_P0  0x0A
#define NRF_REG_TX_ADDR     0x10
#define NRF_REG_RX_PW_P0    0x11
#define NRF_REG_FIFO_STATUS 0x17
#define NRF_REG_DYNPD       0x1C
#define NRF_REG_FEATURE     0x1D

/*========== CONFIG bits ==========*/
#define NRF_CFG_MASK_RX_DR  (1<<6)
#define NRF_CFG_MASK_TX_DS  (1<<5)
#define NRF_CFG_MASK_MAX_RT (1<<4)
#define NRF_CFG_EN_CRC      (1<<3)
#define NRF_CFG_CRCO        (1<<2)
#define NRF_CFG_PWR_UP      (1<<1)
#define NRF_CFG_PRIM_RX     (1<<0)

/*========== STATUS bits ==========*/
#define NRF_ST_RX_DR   (1<<6)
#define NRF_ST_TX_DS   (1<<5)
#define NRF_ST_MAX_RT  (1<<4)

/*========== Types ==========*/
typedef enum {
    NRF_MODE_TX = 0,
    NRF_MODE_RX
} nrf_mode_t;

/*========== Public API ==========*/
void     NRF24L01_Init(void);
uint8_t  NRF24L01_Check(void);
void     NRF24L01_Configure(void);
void     NRF24L01_TX_Mode(void);
void     NRF24L01_RX_Mode(void);
uint8_t  NRF24L01_TxPacket(uint8_t *data, uint8_t len);
void     NRF24L01_TxPacketNoWait(uint8_t *data, uint8_t len);
uint8_t  NRF24L01_RxPacket(uint8_t *buf);
uint8_t  NRF24L01_ReadReg(uint8_t reg);
void     NRF24L01_WriteReg(uint8_t reg, uint8_t value);
void     NRF24L01_ReadBuf(uint8_t reg, uint8_t *buf, uint8_t len);
void     NRF24L01_WriteBuf(uint8_t reg, uint8_t *buf, uint8_t len);

#endif /* NRF24L01_H */