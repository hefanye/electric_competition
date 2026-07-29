#include "nrf24l01.h"
#include "spi.h"
#include <string.h>

#define NRF_CHANNEL        60U
#define NRF_PAYLOAD_SIZE   32U
#define NRF_ADDR_LEN       5U

static const uint8_t s_addr[NRF_ADDR_LEN] = {0x34, 0x43, 0x10, 0x10, 0x01};

static uint8_t nrf_spi_rw(uint8_t tx)
{
    uint8_t rx;
    HAL_SPI_TransmitReceive(&hspi1, &tx, &rx, 1U, 100U);
    return rx;
}

uint8_t NRF24L01_ReadReg(uint8_t reg)
{
    uint8_t val;
    nrf_csn_low();
    nrf_spi_rw(NRF_CMD_R_REGISTER | reg);
    val = nrf_spi_rw(0xFF);
    nrf_csn_high();
    return val;
}

void NRF24L01_WriteReg(uint8_t reg, uint8_t value)
{
    nrf_csn_low();
    nrf_spi_rw(NRF_CMD_W_REGISTER | reg);
    nrf_spi_rw(value);
    nrf_csn_high();
}

void NRF24L01_ReadBuf(uint8_t reg, uint8_t *buf, uint8_t len)
{
    nrf_csn_low();
    nrf_spi_rw(NRF_CMD_R_REGISTER | reg);
    while (len-- != 0U) { *buf++ = nrf_spi_rw(0xFF); }
    nrf_csn_high();
}

void NRF24L01_WriteBuf(uint8_t reg, uint8_t *buf, uint8_t len)
{
    nrf_csn_low();
    nrf_spi_rw(NRF_CMD_W_REGISTER | reg);
    while (len-- != 0U) { nrf_spi_rw(*buf++); }
    nrf_csn_high();
}

uint8_t NRF24L01_Check(void)
{
    uint8_t tmp[NRF_ADDR_LEN];
    uint8_t i;

    NRF24L01_WriteBuf(NRF_REG_TX_ADDR, (uint8_t *)s_addr, NRF_ADDR_LEN);
    NRF24L01_ReadBuf(NRF_REG_TX_ADDR, tmp, NRF_ADDR_LEN);
    for (i = 0U; i < NRF_ADDR_LEN; i++)
        if (tmp[i] != s_addr[i]) return 0U;
    return 1U;
}

void NRF24L01_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_SPI1_CLK_ENABLE();

    /*--- NRF 控制脚 PB5(CSN) PB6(CE) PB7(IRQ) ---*/
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Pin = NRF_CE_PIN;
    HAL_GPIO_Init(NRF_CE_PORT, &gpio);
    gpio.Pin = NRF_CSN_PIN;
    HAL_GPIO_Init(NRF_CSN_PORT, &gpio);

    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    gpio.Pin  = NRF_IRQ_PIN;
    HAL_GPIO_Init(NRF_IRQ_PORT, &gpio);

    /*--- SPI 脚：PA5=SCK, PA6=MISO, PA7=MOSI ---*/
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF5_SPI1;
    gpio.Pin = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* PB3/PB4 没接线，强制输入避免干扰 */
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLDOWN;
    gpio.Pin = GPIO_PIN_3 | GPIO_PIN_4;
    HAL_GPIO_Init(GPIOB, &gpio);

    nrf_ce_low();
    nrf_csn_high();

    /* 用 HAL 重新初始化 SPI1（之前 CubeMX 初始化的脚位不对） */
    HAL_SPI_DeInit(&hspi1);
    MX_SPI1_Init();
    HAL_Delay(2U);
}

void NRF24L01_Configure(void)
{
    nrf_ce_low();

    NRF24L01_WriteBuf(NRF_REG_TX_ADDR,    (uint8_t *)s_addr, NRF_ADDR_LEN);
    NRF24L01_WriteBuf(NRF_REG_RX_ADDR_P0, (uint8_t *)s_addr, NRF_ADDR_LEN);

    NRF24L01_WriteReg(NRF_REG_CONFIG, NRF_CFG_EN_CRC | NRF_CFG_CRCO);

    NRF24L01_WriteReg(NRF_REG_EN_AA,      0x01);
    NRF24L01_WriteReg(NRF_REG_EN_RXADDR,  0x01);
    NRF24L01_WriteReg(NRF_REG_SETUP_AW,   0x03U);
    NRF24L01_WriteReg(NRF_REG_SETUP_RETR,  0x1a);
    NRF24L01_WriteReg(NRF_REG_RF_CH,      NRF_CHANNEL);
    NRF24L01_WriteReg(NRF_REG_RF_SETUP,   0x0F);
    NRF24L01_WriteReg(NRF_REG_RX_PW_P0,   NRF_PAYLOAD_SIZE);

    NRF24L01_WriteReg(NRF_REG_STATUS,
                      NRF_ST_RX_DR | NRF_ST_TX_DS | NRF_ST_MAX_RT);
    nrf_csn_low(); nrf_spi_rw(NRF_CMD_FLUSH_TX); nrf_csn_high();
    nrf_csn_low(); nrf_spi_rw(NRF_CMD_FLUSH_RX); nrf_csn_high();

    NRF24L01_WriteReg(NRF_REG_CONFIG,
                      NRF_CFG_EN_CRC | NRF_CFG_CRCO | NRF_CFG_PWR_UP);
    HAL_Delay(5U);
}

void NRF24L01_TX_Mode(void)
{
    uint8_t cfg = NRF24L01_ReadReg(NRF_REG_CONFIG);
    nrf_ce_low();
    cfg &= (uint8_t)~NRF_CFG_PRIM_RX;
    NRF24L01_WriteReg(NRF_REG_CONFIG, cfg);
    nrf_ce_high();
}

void NRF24L01_RX_Mode(void)
{
    uint8_t cfg = NRF24L01_ReadReg(NRF_REG_CONFIG);
    cfg |= NRF_CFG_PRIM_RX;
    NRF24L01_WriteReg(NRF_REG_CONFIG, cfg);
    nrf_ce_high();
}

uint8_t NRF24L01_TxPacket(uint8_t *data, uint8_t len)
{
    uint8_t status;
    uint16_t timeout = 0U;

    nrf_ce_low();
    nrf_csn_low(); nrf_spi_rw(NRF_CMD_FLUSH_TX); nrf_csn_high();

    nrf_csn_low();
    nrf_spi_rw(NRF_CMD_W_TX_PAYLOAD);
    while (len-- != 0U) { nrf_spi_rw(*data++); }
    nrf_csn_high();

    nrf_ce_high();

    while ((HAL_GPIO_ReadPin(NRF_IRQ_PORT, NRF_IRQ_PIN) == GPIO_PIN_SET)
           && (timeout < 500U))
    {
        HAL_Delay(1U);
        timeout++;
    }

    nrf_ce_low();

    status = NRF24L01_ReadReg(NRF_REG_STATUS);
    NRF24L01_WriteReg(NRF_REG_STATUS, status);

    if ((status & NRF_ST_MAX_RT) != 0U)
    {
        nrf_csn_low(); nrf_spi_rw(NRF_CMD_FLUSH_TX); nrf_csn_high();
        return 0U;
    }

    return (status & NRF_ST_TX_DS) != 0U ? 1U : 0U;
}

void NRF24L01_TxPacketNoWait(uint8_t *data, uint8_t len)
{
    nrf_ce_low();
    nrf_csn_low(); nrf_spi_rw(NRF_CMD_FLUSH_TX); nrf_csn_high();

    nrf_csn_low();
    nrf_spi_rw(NRF_CMD_W_TX_PAYLOAD);
    while (len-- != 0U) { nrf_spi_rw(*data++); }
    nrf_csn_high();

    nrf_ce_high();
    HAL_Delay(10);
    nrf_ce_low();
}

uint8_t NRF24L01_RxPacket(uint8_t *buf)
{
    uint8_t status = NRF24L01_ReadReg(NRF_REG_STATUS);

    if ((status & NRF_ST_RX_DR) != 0U)
    {
        nrf_csn_low();
        nrf_spi_rw(NRF_CMD_R_RX_PAYLOAD);
        for (uint8_t i = 0U; i < NRF_PAYLOAD_SIZE; i++)
            buf[i] = nrf_spi_rw(0xFF);
        nrf_csn_high();

        NRF24L01_WriteReg(NRF_REG_STATUS, NRF_ST_RX_DR);
        nrf_csn_low(); nrf_spi_rw(NRF_CMD_FLUSH_RX); nrf_csn_high();
        return NRF_PAYLOAD_SIZE;
    }
    return 0U;
}
