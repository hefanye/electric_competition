/**
 ******************************************************************************
 * @file    adc_baseline_test.h
 * @brief   STM32F407 ADC 直流基线测试。
 *
 * 用途：使用 TIM2 触发 ADC1 + DMA 连续采集 1000 点，周期性输出平均值、
 * 最小值、最大值、峰峰值和标准差到 USART1。
 ******************************************************************************
 */

#ifndef ADC_BASELINE_TEST_H
#define ADC_BASELINE_TEST_H

#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ADC_BASELINE_SAMPLE_COUNT (1000U)

typedef struct
{
    float mean_code;
    float stddev_code;
    float mean_voltage;
    float stddev_voltage;
    uint16_t min_code;
    uint16_t max_code;
    uint16_t peak_to_peak_code;
    uint8_t range_ok;
    uint8_t baseline_ok;
} adc_baseline_result_t;

/**
 * @brief  启动 ADC1 + DMA + TIM2 的直流基线测试。
 * @param  hadc ADC 句柄（本工程传入 &hadc1）。
 * @param  htim 触发 ADC 的定时器句柄（本工程传入 &htim2）。
 * @param  huart 输出日志的串口句柄（本工程传入 &huart1）。
 * @param  vdda_volts 实测或假定的 VDDA 电压，例如 3.300f。
 */
void ADC_Baseline_Test_Init(ADC_HandleTypeDef *hadc,
                            TIM_HandleTypeDef *htim,
                            UART_HandleTypeDef *huart,
                            float vdda_volts);

/**
 * @brief  放在 main while(1) 中。每秒处理一次最新的 1000 点数据并打印。
 */
void ADC_Baseline_Test_Process(void);

/** @brief 控制是否打印基线报告；非基线算法测试时关闭以避免串口混杂输出。 */
void ADC_Baseline_Test_SetReportEnabled(uint8_t enabled);

/**
 * @brief  获取最近一次计算结果，用于 Keil Watch 或后续显示到串口屏。
 */
const adc_baseline_result_t *ADC_Baseline_Test_GetResult(void);

#ifdef __cplusplus
}
#endif

#endif /* ADC_BASELINE_TEST_H */
