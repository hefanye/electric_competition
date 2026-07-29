/**
 * @file algorithm_test_common.h
 * @brief STM32F407 算法库实机验证的公共数据结构和串口输出工具。
 */
#ifndef ALGORITHM_TEST_COMMON_H
#define ALGORITHM_TEST_COMMON_H

#include "stm32f4xx_hal.h"
#include <stddef.h>
#include <stdint.h>

#define ALGO_TEST_ADC_FRAME_SIZE  (1000U)
#define ALGO_TEST_FFT_SIZE        (512U)

typedef struct {
    const uint16_t *codes;
    size_t length;
    float vdda_volts;
    float sample_rate_hz;
} algorithm_test_frame_t;

void AlgoTest_Common_Init(UART_HandleTypeDef *huart);
void AlgoTest_Log(const char *format, ...);
float *AlgoTest_WorkBuffer(uint8_t index);
void AlgoTest_FrameToVolts(const algorithm_test_frame_t *frame, float *output);
void AlgoTest_RemoveMean(const float *input, float *output, size_t length);
uint32_t AlgoTest_ToMilli(float value);
uint32_t AlgoTest_ToMicro(float value);

#endif /* ALGORITHM_TEST_COMMON_H */
