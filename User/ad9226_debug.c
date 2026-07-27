/**
 * @file ad9226_debug.c
 * @brief AD9226 A 通道低速并行采样调试实现。
 *
 * TIM1_CH1 在 PA8 输出约 10 kHz 采样时钟。TIM1 更新中断进入时，
 * AD9226 的并行数据已经满足说明书要求的输出建立时间；读取 GPIOE->IDR
 * 得到 PE0~PE11 后，将位序反转成常用的 12 位二进制码。
 *
 * 注意：中断内只采样、存 RAM；串口打印只在主循环中进行。
 */

#include "ad9226_debug.h"

#include <stdio.h>
#include <string.h>

#define AD9226_FRAME_SAMPLES      (256U)
#define AD9226_UART_TIMEOUT_MS    (100U)

/*
 * Optional source-path calibration.
 *
 * V_adc = V_source * NUM / DEN.  Keep both values at 1 when no calibrated
 * source-path attenuation is known.  This is intentionally not a property of
 * AD9226: it depends on the signal source, output impedance and analogue
 * front-end.  pp_adc_mv is always the actual AD9226 input-side result.
 */
#ifndef AD9226_SOURCE_TO_ADC_NUM
#define AD9226_SOURCE_TO_ADC_NUM  (1U)
#endif

#ifndef AD9226_SOURCE_TO_ADC_DEN
#define AD9226_SOURCE_TO_ADC_DEN  (1U)
#endif

static TIM_HandleTypeDef *s_tim;
static UART_HandleTypeDef *s_uart;

/* 采集到一帧后暂停写入，等待主循环完成统计，避免读写同一缓冲区。 */
static volatile uint16_t s_samples[AD9226_FRAME_SAMPLES];
static volatile uint16_t s_sample_count;
static volatile uint8_t s_frame_ready;
static uint32_t s_frame_number;

/**
 * @brief 把 PE0~PE11 的物理位序转换为常用 12 位 ADC 码值。
 *
 * 模块定义 AD0 为最高位、AD11 为最低位；因此需要将端口的低 12 位反转。
 */
static uint16_t AD9226_PortToCode(uint16_t port_value)
{
    uint16_t code = 0U;
    uint8_t bit;

    for (bit = 0U; bit < 12U; ++bit)
    {
        if ((port_value & (uint16_t)(1U << bit)) != 0U)
        {
            code |= (uint16_t)(1U << (11U - bit));
        }
    }

    return code;
}

static void AD9226_Print(const char *text)
{
    if ((s_uart != NULL) && (text != NULL))
    {
        (void)HAL_UART_Transmit(s_uart, (uint8_t *)text,
                                (uint16_t)strlen(text), AD9226_UART_TIMEOUT_MS);
    }
}

/* TIM1 is on APB2.  If APB2 is prescaled, TIM1 receives PCLK2 x2. */
static uint32_t AD9226_GetTim1ClockHz(void)
{
    uint32_t timer_clock_hz = HAL_RCC_GetPCLK2Freq();

    if ((RCC->CFGR & RCC_CFGR_PPRE2) != 0U)
    {
        timer_clock_hz *= 2U;
    }

    return timer_clock_hz;
}

HAL_StatusTypeDef AD9226_Debug_Init(TIM_HandleTypeDef *htim,
                                    UART_HandleTypeDef *huart)
{
    uint32_t aclk_hz;
    char startup[144];

    if ((htim == NULL) || (huart == NULL))
    {
        return HAL_ERROR;
    }

    s_tim = htim;
    s_uart = huart;
    s_sample_count = 0U;
    s_frame_ready = 0U;
    s_frame_number = 0U;

    /* PA8 上先输出 ACLK，再开启更新中断执行采样。 */
    if (HAL_TIM_PWM_Start(s_tim, TIM_CHANNEL_1) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (HAL_TIM_Base_Start_IT(s_tim) != HAL_OK)
    {
        (void)HAL_TIM_PWM_Stop(s_tim, TIM_CHANNEL_1);
        return HAL_ERROR;
    }

    aclk_hz = AD9226_GetTim1ClockHz() /
              ((s_tim->Init.Prescaler + 1U) * (s_tim->Init.Period + 1U));
    (void)snprintf(startup, sizeof(startup),
                   "\r\n=== AD9226 A-channel debug ===\r\n"
                   "ACLK=%lu Hz (PSC=%lu ARR=%lu CCR1=%lu), data=PE0..PE11, AD0=MSB\r\n"
                   "Tie A input to 0 V first: mean should be near 2048.\r\n",
                   (unsigned long)aclk_hz,
                   (unsigned long)s_tim->Init.Prescaler,
                   (unsigned long)s_tim->Init.Period,
                   (unsigned long)__HAL_TIM_GET_COMPARE(s_tim, TIM_CHANNEL_1));
    AD9226_Print(startup);
    return HAL_OK;
}

void AD9226_Debug_TimPeriodElapsedCallback(void)
{
    uint16_t port_value;

    if ((s_tim == NULL) || (s_frame_ready != 0U))
    {
        return;
    }

    /* 一次读取整个 GPIOE 输入寄存器，PE0~PE11 对应 AD0~AD11。 */
    port_value = (uint16_t)(GPIOE->IDR & 0x0FFFU);
    s_samples[s_sample_count] = AD9226_PortToCode(port_value);
    ++s_sample_count;

    if (s_sample_count >= AD9226_FRAME_SAMPLES)
    {
        s_sample_count = 0U;
        s_frame_ready = 1U;
    }
}

void AD9226_Debug_Process(void)
{
    uint16_t index;
    uint16_t min_code = 0x0FFFU;
    uint16_t max_code = 0U;
    uint16_t code;
    uint32_t sum = 0U;
    uint32_t mean_code;
    uint16_t pp_code;
    int32_t dc_est_mv;
    uint32_t pp_adc_mv;
    uint32_t pp_dds_equiv_mv;
    char report[160];

    if (s_frame_ready == 0U)
    {
        return;
    }

    /* s_frame_ready 期间中断不会写缓冲区，可安全统计整帧。 */
    for (index = 0U; index < AD9226_FRAME_SAMPLES; ++index)
    {
        code = s_samples[index];
        sum += code;

        if (code < min_code)
        {
            min_code = code;
        }
        if (code > max_code)
        {
            max_code = code;
        }
    }

    mean_code = (sum + (AD9226_FRAME_SAMPLES / 2U)) / AD9226_FRAME_SAMPLES;
    pp_code = (uint16_t)(max_code - min_code);

    /* 依据说明书的 -5 V~+5 V 输入量程做近似换算，仅作调试参考。 */
    dc_est_mv = ((int32_t)2048 - (int32_t)mean_code) * 5000 / 2048;
    pp_adc_mv = ((uint32_t)pp_code * 5000U + 1024U) / 2048U;
    pp_dds_equiv_mv = (pp_adc_mv * AD9226_SOURCE_TO_ADC_DEN +
                        (AD9226_SOURCE_TO_ADC_NUM / 2U)) /
                       AD9226_SOURCE_TO_ADC_NUM;

    ++s_frame_number;
    (void)snprintf(report, sizeof(report),
                   "AD9226 A: frame=%lu mean=%lu min=%u max=%u pp=%u "
                   "dc_est=%ld mV pp_adc=%lu mV pp_dds_eq=%lu mV\r\n",
                   (unsigned long)s_frame_number,
                   (unsigned long)mean_code,
                   (unsigned int)min_code,
                   (unsigned int)max_code,
                   (unsigned int)pp_code,
                   (long)dc_est_mv,
                   (unsigned long)pp_adc_mv,
                   (unsigned long)pp_dds_equiv_mv);
    AD9226_Print(report);

    __disable_irq();
    s_frame_ready = 0U;
    __enable_irq();
}
