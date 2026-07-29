/**
 * @file g_signal_pc_debug.c
 * @brief G题测量结果 PC 串口旁路输出实现（仅输出，不控制采集）。
 *
 * 采集由串口屏令牌触发（比赛模式），本模块在 analyse_frame() 完成后
 * 将测量结果通过 USART1 输出到 PC，不影响串口屏交互。
 */

#include "g_signal_pc_debug.h"

#include <stdio.h>
#include <string.h>

#define PC_UART_TIMEOUT_MS   500U
#define WAVE_POINTS          200U

static UART_HandleTypeDef *s_uart;

static void pc_send(const char *text)
{
    (void)HAL_UART_Transmit(s_uart, (uint8_t *)text,
                            (uint16_t)strlen(text), PC_UART_TIMEOUT_MS);
}

void GSignal_PCDebug_Init(UART_HandleTypeDef *huart)
{
    s_uart = huart;
}

void GSignal_PCDebug_Print(const ui_wave_measurement_t *wave,
                           const ui_spectrum_measurement_t *spectrum,
                           const uint8_t *wave_samples,
                           const uint8_t *spectrum_samples)
{
    char line[128];
    uint32_t i;

    if (s_uart == NULL || wave == NULL || spectrum == NULL)
    {
        return;
    }

    /* 帧头 */
    pc_send("=== G-Signal Frame ===\r\n");

    /* 时域结果 */
    {
        uint32_t freq_hz = wave->fundamental_mHz / 1000U;
        uint32_t freq_mhz = wave->fundamental_mHz % 1000U;
        (void)snprintf(line, sizeof(line),
                       "Upp=%lu mV  Urms=%lu mV  f1=%lu.%03lu Hz\r\n",
                       (unsigned long)wave->upp_mV,
                       (unsigned long)wave->urms_mV,
                       (unsigned long)freq_hz,
                       (unsigned long)freq_mhz);
        pc_send(line);
    }

    /* 频域 Top3 */
    for (i = 0U; i < 3U; ++i)
    {
        uint32_t freq_khz = spectrum->frequency_mHz[i] / 1000000U;
        uint32_t freq_hz_part = (spectrum->frequency_mHz[i] / 1000U) % 1000U;
        (void)snprintf(line, sizeof(line),
                       "f%u=%lu.%03lu kHz A%u=%lu mV\r\n",
                       (unsigned int)(i + 1U),
                       (unsigned long)freq_khz,
                       (unsigned long)freq_hz_part,
                       (unsigned int)(i + 1U),
                       (unsigned long)spectrum->amplitude_mV[i]);
        pc_send(line);
    }

    /* 波形曲线 200 点 */
    if (wave_samples != NULL)
    {
        pc_send("WAVE:");
        for (i = 0U; i < WAVE_POINTS; ++i)
        {
            if (i == WAVE_POINTS - 1U)
            {
                (void)snprintf(line, sizeof(line), "%u\r\n",
                               (unsigned int)wave_samples[i]);
            }
            else
            {
                (void)snprintf(line, sizeof(line), "%u,",
                               (unsigned int)wave_samples[i]);
            }
            pc_send(line);
        }
    }

    /* 频谱曲线 200 点 */
    if (spectrum_samples != NULL)
    {
        pc_send("SPECTRUM:");
        for (i = 0U; i < WAVE_POINTS; ++i)
        {
            if (i == WAVE_POINTS - 1U)
            {
                (void)snprintf(line, sizeof(line), "%u\r\n",
                               (unsigned int)spectrum_samples[i]);
            }
            else
            {
                (void)snprintf(line, sizeof(line), "%u,",
                               (unsigned int)spectrum_samples[i]);
            }
            pc_send(line);
        }
    }

    pc_send("=== End ===\r\n\r\n");
}
