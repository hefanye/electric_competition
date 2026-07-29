/**
 * @file dac8830.h
 * @brief 双路 DAC8830 模块驱动。
 *
 * 当前硬件连接：SPI1(PB3=SCLK, PA7=MOSI)，PB0=CS1(A)，PB1=CS2(B)。
 * 本驱动按模块处于 0~5 V 单极性输出档使用；切换到其他跳帽档位前，
 * 必须同步修改 DAC8830_OUTPUT_FULL_SCALE_VOLTS。
 */

#ifndef DAC8830_H
#define DAC8830_H

#include "main.h"

typedef enum
{
    DAC8830_CHANNEL_A = 0U,
    DAC8830_CHANNEL_B = 1U
} dac8830_channel_t;

/* 当前调试阶段：模块跳帽处于 0~5 V 输出档。 */
#define DAC8830_OUTPUT_FULL_SCALE_VOLTS    (5.0f)

/** 初始化驱动，并将两个片选脚拉高为非选中状态。 */
HAL_StatusTypeDef DAC8830_Init(SPI_HandleTypeDef *hspi);

/** 向指定通道写入 16 位原始码值：0x0000~0xFFFF。 */
HAL_StatusTypeDef DAC8830_SetCode(dac8830_channel_t channel, uint16_t code);

/** 将 0~5 V 档位下的目标电压换算为 16 位 DAC 码值。 */
uint16_t DAC8830_VoltageToCode(float voltage);

/** 按 0~5 V 档位设置指定通道的目标输出电压，超范围值会被限幅。 */
HAL_StatusTypeDef DAC8830_SetVoltage(dac8830_channel_t channel, float voltage);

/** 将 A、B 两个通道同时设置为零码输出。 */
HAL_StatusTypeDef DAC8830_ResetAll(void);

#endif /* DAC8830_H */
