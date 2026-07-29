/**
 ******************************************************************************
 * @file    adc_baseline_test.c
 * @brief   STM32F407 ADC 直流基线测试实现。
 *
 * 注意：本文件只处理 ADC1 的 DMA 完成回调。不要同时在其他文件中重复定义
 * HAL_ADC_ConvCpltCallback()。
 ******************************************************************************
 */

#include "adc_baseline_test.h"
#include "algorithm_test.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define ADC_FULL_SCALE_CODE          (4095.0f)
#define ADC_BASELINE_EXPECTED_VOLTAGE (1.650f)
#define ADC_BASELINE_TOLERANCE_VOLTAGE (0.100f)
#define ADC_REPORT_INTERVAL_MS       (1000U)

static uint16_t s_adc_samples[ADC_BASELINE_SAMPLE_COUNT];
static ADC_HandleTypeDef *s_hadc;
static TIM_HandleTypeDef *s_htim;
static UART_HandleTypeDef *s_huart;
static volatile uint8_t s_dma_frame_ready;
static uint32_t s_last_report_tick;
static float s_vdda_volts = 3.300f;
static adc_baseline_result_t s_result;
static uint8_t s_report_enabled = 1U;

static uint32_t ADC_Baseline_ToMilliVolts(float voltage)
{
    if (voltage <= 0.0f) {
        return 0U;
    }
    return (uint32_t)(voltage * 1000.0f + 0.5f);
}

static uint32_t ADC_Baseline_ToCodeX100(float code)
{
    if (code <= 0.0f) {
        return 0U;
    }
    return (uint32_t)(code * 100.0f + 0.5f);
}

static uint32_t ADC_Baseline_ToCodeX1000(float code)
{
    if (code <= 0.0f) {
        return 0U;
    }
    return (uint32_t)(code * 1000.0f + 0.5f);
}

static void ADC_Baseline_UartPrint(const char *text)
{
    if ((s_huart != NULL) && (text != NULL)) {
        (void)HAL_UART_Transmit(s_huart, (uint8_t *)text,
                                (uint16_t)strlen(text), 100U);
    }
}

static void ADC_Baseline_Calculate(void)
{
    uint32_t i;
    float mean = 0.0f;
    float m2 = 0.0f;
    uint16_t minimum = 0xFFFFU;
    uint16_t maximum = 0U;

    /* Welford 算法：避免用“平方和 - 均值平方”计算标准差时损失精度。 */
    for (i = 0U; i < ADC_BASELINE_SAMPLE_COUNT; i++) {
        float sample = (float)s_adc_samples[i];
        float delta = sample - mean;
        float delta2;

        mean += delta / (float)(i + 1U);
        delta2 = sample - mean;
        m2 += delta * delta2;

        if (s_adc_samples[i] < minimum) {
            minimum = s_adc_samples[i];
        }
        if (s_adc_samples[i] > maximum) {
            maximum = s_adc_samples[i];
        }
    }

    s_result.mean_code = mean;
    s_result.stddev_code = sqrtf(m2 / (float)ADC_BASELINE_SAMPLE_COUNT);
    s_result.min_code = minimum;
    s_result.max_code = maximum;
    s_result.peak_to_peak_code = (uint16_t)(maximum - minimum);
    s_result.mean_voltage = mean * s_vdda_volts / ADC_FULL_SCALE_CODE;
    s_result.stddev_voltage = s_result.stddev_code * s_vdda_volts / ADC_FULL_SCALE_CODE;

    /* 仅检查是否接近满量程；不是对噪声大小作硬性判定。 */
    s_result.range_ok = ((minimum > 16U) && (maximum < 4079U)) ? 1U : 0U;
    s_result.baseline_ok = ((fabsf(s_result.mean_voltage - ADC_BASELINE_EXPECTED_VOLTAGE)
                             <= ADC_BASELINE_TOLERANCE_VOLTAGE) &&
                            (s_result.range_ok != 0U)) ? 1U : 0U;
}

static void ADC_Baseline_PrintResult(void)
{
    char line[320];
    uint32_t mean_code_x100 = ADC_Baseline_ToCodeX100(s_result.mean_code);
    uint32_t std_code_x1000 = ADC_Baseline_ToCodeX1000(s_result.stddev_code);
    uint32_t mean_mv = ADC_Baseline_ToMilliVolts(s_result.mean_voltage);
    uint32_t std_uv = (uint32_t)(s_result.stddev_voltage * 1000000.0f + 0.5f);

    (void)snprintf(line, sizeof(line),
                   "\r\n=== ADC BASELINE (1000 samples) ===\r\n"
                   "mean = %lu.%02lu code, %lu.%03lu V\r\n"
                   "min  = %u code, max = %u code, pp = %u code\r\n"
                   "std  = %lu.%03lu code, %lu.%03lu mV\r\n"
                   "range = %s, baseline(1.65V +/-0.10V) = %s\r\n",
                   (unsigned long)(mean_code_x100 / 100U),
                   (unsigned long)(mean_code_x100 % 100U),
                   (unsigned long)(mean_mv / 1000U),
                   (unsigned long)(mean_mv % 1000U),
                   s_result.min_code, s_result.max_code, s_result.peak_to_peak_code,
                   (unsigned long)(std_code_x1000 / 1000U),
                   (unsigned long)(std_code_x1000 % 1000U),
                   (unsigned long)(std_uv / 1000U),
                   (unsigned long)(std_uv % 1000U),
                   (s_result.range_ok != 0U) ? "OK" : "CLIPPED",
                   (s_result.baseline_ok != 0U) ? "PASS" : "CHECK");
    ADC_Baseline_UartPrint(line);
}

void ADC_Baseline_Test_Init(ADC_HandleTypeDef *hadc,
                            TIM_HandleTypeDef *htim,
                            UART_HandleTypeDef *huart,
                            float vdda_volts)
{
    s_hadc = hadc;
    s_htim = htim;
    s_huart = huart;
    s_vdda_volts = (vdda_volts > 0.1f) ? vdda_volts : 3.300f;
    s_dma_frame_ready = 0U;
    s_last_report_tick = HAL_GetTick();

    ADC_Baseline_UartPrint("\r\nADC baseline test started. TIM2 -> ADC1 -> DMA.\r\n");

    if ((s_hadc == NULL) || (s_htim == NULL)) {
        ADC_Baseline_UartPrint("ERROR: ADC or TIM handle is NULL.\r\n");
        return;
    }

    if (HAL_ADC_Start_DMA(s_hadc, (uint32_t *)s_adc_samples,
                          ADC_BASELINE_SAMPLE_COUNT) != HAL_OK) {
        ADC_Baseline_UartPrint("ERROR: HAL_ADC_Start_DMA failed.\r\n");
        return;
    }

    if (HAL_TIM_Base_Start(s_htim) != HAL_OK) {
        (void)HAL_ADC_Stop_DMA(s_hadc);
        ADC_Baseline_UartPrint("ERROR: HAL_TIM_Base_Start failed.\r\n");
    }
}

void ADC_Baseline_Test_Process(void)
{
    if (s_dma_frame_ready == 0U) {
        return;
    }

    if ((HAL_GetTick() - s_last_report_tick) < ADC_REPORT_INTERVAL_MS) {
        return;
    }

    s_dma_frame_ready = 0U;
    s_last_report_tick = HAL_GetTick();
    /* 所有算法测试共用此唯一 ADC+DMA 数据入口，避免重复定义 HAL 回调。 */
    Algorithm_Test_OnAdcFrame(s_adc_samples, ADC_BASELINE_SAMPLE_COUNT, s_vdda_volts);
    ADC_Baseline_Calculate();
    if (s_report_enabled != 0U) {
        ADC_Baseline_PrintResult();
    }
}

void ADC_Baseline_Test_SetReportEnabled(uint8_t enabled)
{
    s_report_enabled = (enabled != 0U) ? 1U : 0U;
}

const adc_baseline_result_t *ADC_Baseline_Test_GetResult(void)
{
    return &s_result;
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc == s_hadc) {
        s_dma_frame_ready = 1U;
    }
}
