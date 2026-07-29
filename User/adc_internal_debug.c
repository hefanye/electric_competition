/**
 * @file adc_internal_debug.c
 * @brief STM32F407 片内 ADC1 通道 0 (PA0) 采样调试实现。
 *
 * TIM2_TRGO 触发 ADC1 转换，DMA 循环搬运到 RAM。
 * 主循环通过 DMA NDTR 轮询判断哪半区就绪，复制后计算统计。
 * 不使用 HAL_ADC_ConvCpltCallback / HAL_ADC_ConvHalfCpltCallback，
 * 避免与 adc_baseline_test.c 的回调定义冲突。
 *
 * 打印格式参考 ad9226_debug.c。
 */

#include "adc_internal_debug.h"
#include <stdio.h>
#include <string.h>

#define ADC_INTERNAL_FRAME_SAMPLES   (256U)
#define ADC_INTERNAL_DMA_BUFFER_LEN  (ADC_INTERNAL_FRAME_SAMPLES * 2U)
#define ADC_INTERNAL_UART_TIMEOUT_MS (100U)
#define ADC_INTERNAL_VREF_MV         (3300U)
#define ADC_INTERNAL_FULL_SCALE      (4095U)
#define ADC_INTERNAL_MID_CODE        (2048U)

static ADC_HandleTypeDef *s_adc;
static TIM_HandleTypeDef *s_tim;
static UART_HandleTypeDef *s_uart;

/* DMA 双缓冲：512 点循环 buffer，前/后半区各 256 点 */
static volatile uint16_t s_dma_buffer[ADC_INTERNAL_DMA_BUFFER_LEN];
static uint16_t s_frame_samples[ADC_INTERNAL_FRAME_SAMPLES];
static uint8_t  s_last_phase = 0U;
static uint32_t s_frame_number = 0U;

/* 一帧统计结果 */
static uint16_t s_last_min;
static uint16_t s_last_max;
static uint16_t s_last_pp;
static uint32_t s_last_sum;
static volatile uint8_t s_frame_ready;

static void ComputeFrameStats(const uint16_t *samples, uint32_t count)
{
    uint32_t i;
    uint16_t mn = 0xFFFFU;
    uint16_t mx = 0U;
    uint32_t sum = 0U;

    for (i = 0U; i < count; ++i)
    {
        uint16_t v = samples[i];
        if (v < mn) mn = v;
        if (v > mx) mx = v;
        sum += v;
    }

    s_last_min = mn;
    s_last_max = mx;
    s_last_pp  = (uint16_t)(mx - mn);
    s_last_sum = sum;
    s_frame_ready = 1U;
}

HAL_StatusTypeDef ADC_Internal_Debug_Init(ADC_HandleTypeDef *hadc,
                                           TIM_HandleTypeDef *htim,
                                           UART_HandleTypeDef *huart)
{
    s_adc = hadc;
    s_tim = htim;
    s_uart = huart;
    s_frame_number = 0U;
    s_frame_ready = 0U;
    s_last_phase = 0U;

    /* 启动 DMA 循环采集 */
    if (HAL_ADC_Start_DMA(hadc, (uint32_t *)s_dma_buffer,
                          ADC_INTERNAL_DMA_BUFFER_LEN) != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* 启动 TIM2 触发 ADC */
    if (HAL_TIM_Base_Start(htim) != HAL_OK)
    {
        (void)HAL_ADC_Stop_DMA(hadc);
        return HAL_ERROR;
    }

    /* 打印头部信息 */
    {
        const char *header =
            "\r\n=== STM32 internal ADC1 IN0 debug ===\r\n"
            "PA0=ADC1_IN0, 20kHz sample rate, 256 samples/frame\r\n"
            "Signal must be biased to 0~3.3V (mid=1.65V)\r\n\r\n";
        HAL_UART_Transmit(huart, (uint8_t *)header,
                          (uint16_t)strlen(header), ADC_INTERNAL_UART_TIMEOUT_MS);
    }

    return HAL_OK;
}

void ADC_Internal_Debug_Process(void)
{
    if ((s_adc == NULL) || (s_adc->DMA_Handle == NULL))
    {
        return;
    }

    /* 通过 DMA NDTR 判断当前 DMA 写入位置 */
    {
        uint16_t ndtr = (uint16_t)__HAL_DMA_GET_COUNTER(s_adc->DMA_Handle);
        /* NDTR 从 512 递减到 0 再重载
         * phase 0: NDTR > 256, DMA 在写前半区, 后半区可读
         * phase 1: NDTR <= 256, DMA 在写后半区, 前半区可读 */
        uint8_t phase = (ndtr > ADC_INTERNAL_FRAME_SAMPLES) ? 0U : 1U;

        if (phase != s_last_phase)
        {
            const uint16_t *frame;
            if (phase == 1U)
            {
                /* 刚进入后半区，前半区写完 */
                frame = (const uint16_t *)s_dma_buffer;
            }
            else
            {
                /* 刚重载回前半区，后半区写完 */
                frame = (const uint16_t *)&s_dma_buffer[ADC_INTERNAL_FRAME_SAMPLES];
            }

            memcpy(s_frame_samples, frame,
                   ADC_INTERNAL_FRAME_SAMPLES * sizeof(uint16_t));
            ComputeFrameStats(s_frame_samples, ADC_INTERNAL_FRAME_SAMPLES);

            s_last_phase = phase;
        }
    }

    /* 有新帧就打印 */
    if (s_frame_ready)
    {
        char line[128];
        int len;
        uint16_t mn = s_last_min;
        uint16_t mx = s_last_max;
        uint16_t pp = s_last_pp;
        uint32_t mean = s_last_sum / ADC_INTERNAL_FRAME_SAMPLES;
        int32_t dc_mv = (int32_t)(((int32_t)mean - (int32_t)ADC_INTERNAL_MID_CODE)
                                   * (int32_t)ADC_INTERNAL_VREF_MV
                                   / (int32_t)ADC_INTERNAL_FULL_SCALE);
        uint32_t pp_mv = (uint32_t)pp * ADC_INTERNAL_VREF_MV / ADC_INTERNAL_FULL_SCALE;

        s_frame_ready = 0U;
        ++s_frame_number;

        len = snprintf(line, sizeof(line),
            "ADC1 IN0: frame=%lu mean=%lu min=%u max=%u pp=%u dc_est=%ld mV pp_adc=%lu mV\r\n",
            (unsigned long)s_frame_number, (unsigned long)mean,
            (unsigned)mn, (unsigned)mx, (unsigned)pp,
            (long)dc_mv, (unsigned long)pp_mv);

        if (len > 0)
        {
            HAL_UART_Transmit(s_uart, (uint8_t *)line,
                              (uint16_t)len, ADC_INTERNAL_UART_TIMEOUT_MS);
        }
    }
}
