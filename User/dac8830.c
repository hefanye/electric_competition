/**
 * @file dac8830.c
 * @brief 双路 DAC8830 模块 SPI 驱动实现。
 */

#include "dac8830.h"

/* 与 CubeMX 当前 GPIO 配置对应。 */
#define DAC8830_CS_A_PORT    GPIOB
#define DAC8830_CS_A_PIN     GPIO_PIN_0
#define DAC8830_CS_B_PORT    GPIOB
#define DAC8830_CS_B_PIN     GPIO_PIN_1

#define DAC8830_SPI_TIMEOUT_MS    (10U)

static SPI_HandleTypeDef *s_hspi;

static void DAC8830_SetCs(dac8830_channel_t channel, GPIO_PinState state)
{
    if (channel == DAC8830_CHANNEL_A)
    {
        HAL_GPIO_WritePin(DAC8830_CS_A_PORT, DAC8830_CS_A_PIN, state);
    }
    else
    {
        HAL_GPIO_WritePin(DAC8830_CS_B_PORT, DAC8830_CS_B_PIN, state);
    }
}

HAL_StatusTypeDef DAC8830_Init(SPI_HandleTypeDef *hspi)
{
    if (hspi == NULL)
    {
        return HAL_ERROR;
    }

    s_hspi = hspi;

    /* CS 为低有效，初始化后必须保持高电平，避免误写 DAC。 */
    HAL_GPIO_WritePin(DAC8830_CS_A_PORT, DAC8830_CS_A_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(DAC8830_CS_B_PORT, DAC8830_CS_B_PIN, GPIO_PIN_SET);

    return HAL_OK;
}

HAL_StatusTypeDef DAC8830_SetCode(dac8830_channel_t channel, uint16_t code)
{
    uint8_t tx_data[2];
    HAL_StatusTypeDef status;

    if ((s_hspi == NULL) || (channel > DAC8830_CHANNEL_B))
    {
        return HAL_ERROR;
    }

    /* DAC8830 要求 MSB 先传输，在 CS 拉低的完整 16 个时钟内完成。 */
    tx_data[0] = (uint8_t)(code >> 8);
    tx_data[1] = (uint8_t)(code & 0xFFU);

    DAC8830_SetCs(channel, GPIO_PIN_RESET);
    status = HAL_SPI_Transmit(s_hspi, tx_data, 2U, DAC8830_SPI_TIMEOUT_MS);
    DAC8830_SetCs(channel, GPIO_PIN_SET);

    return status;
}

uint16_t DAC8830_VoltageToCode(float voltage)
{
    uint32_t code;

    if (voltage <= 0.0f)
    {
        code = 0U;
    }
    else if (voltage >= DAC8830_OUTPUT_FULL_SCALE_VOLTS)
    {
        code = 0xFFFFU;
    }
    else
    {
        code = (uint32_t)((voltage * 65535.0f / DAC8830_OUTPUT_FULL_SCALE_VOLTS) + 0.5f);
    }

    return (uint16_t)code;
}

HAL_StatusTypeDef DAC8830_SetVoltage(dac8830_channel_t channel, float voltage)
{
    return DAC8830_SetCode(channel, DAC8830_VoltageToCode(voltage));
}

HAL_StatusTypeDef DAC8830_ResetAll(void)
{
    HAL_StatusTypeDef status;

    status = DAC8830_SetCode(DAC8830_CHANNEL_A, 0x0000U);
    if (status != HAL_OK)
    {
        return status;
    }

    return DAC8830_SetCode(DAC8830_CHANNEL_B, 0x0000U);
}
