/** @file algorithm_test_common.c @brief 算法实机测试公共实现。 */
#include "algorithm_test_common.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static UART_HandleTypeDef *s_log_uart;
/* 三个共享工作区：测试选择器保证同一时刻只有一个测试使用它们。 */
static float s_work[3][ALGO_TEST_ADC_FRAME_SIZE];

void AlgoTest_Common_Init(UART_HandleTypeDef *huart)
{
    s_log_uart = huart;
}

void AlgoTest_Log(const char *format, ...)
{
    char text[320];
    va_list args;
    int length;

    if ((s_log_uart == NULL) || (format == NULL)) {
        return;
    }
    va_start(args, format);
    length = vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    if (length > 0) {
        if (length >= (int)sizeof(text)) {
            length = (int)sizeof(text) - 1;
        }
        (void)HAL_UART_Transmit(s_log_uart, (uint8_t *)text, (uint16_t)length, 200U);
    }
}

float *AlgoTest_WorkBuffer(uint8_t index)
{
    return s_work[index % 3U];
}

void AlgoTest_FrameToVolts(const algorithm_test_frame_t *frame, float *output)
{
    size_t i;
    if ((frame == NULL) || (frame->codes == NULL) || (output == NULL)) {
        return;
    }
    for (i = 0U; i < frame->length; ++i) {
        output[i] = ((float)frame->codes[i] * frame->vdda_volts) / 4095.0f;
    }
}

void AlgoTest_RemoveMean(const float *input, float *output, size_t length)
{
    size_t i;
    float mean = 0.0f;
    if ((input == NULL) || (output == NULL) || (length == 0U)) {
        return;
    }
    for (i = 0U; i < length; ++i) {
        mean += input[i];
    }
    mean /= (float)length;
    for (i = 0U; i < length; ++i) {
        output[i] = input[i] - mean;
    }
}

uint32_t AlgoTest_ToMilli(float value)
{
    return (value <= 0.0f) ? 0U : (uint32_t)(value * 1000.0f + 0.5f);
}

uint32_t AlgoTest_ToMicro(float value)
{
    return (value <= 0.0f) ? 0U : (uint32_t)(value * 1000000.0f + 0.5f);
}
