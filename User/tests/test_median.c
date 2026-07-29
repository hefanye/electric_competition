/** @file test_median.c @brief 验证中值滤波的脉冲抑制及实际 ADC 输出。 */
#include "algorithm_test.h"
#include "median_filter.h"
#include "statistics.h"

void Test_Median_Run(const algorithm_test_frame_t *frame)
{
    static uint8_t announced;
    float samples[5] = {0}, scratch[5] = {0};
    median_filter_t filter;
    float *input = AlgoTest_WorkBuffer(0U);
    float *output = AlgoTest_WorkBuffer(1U);
    float raw_min, raw_max, out_min, out_max;
    size_t length = frame->length;
    if (length > ALGO_TEST_ADC_FRAME_SIZE) length = ALGO_TEST_ADC_FRAME_SIZE;
    if (announced == 0U) {
        announced = 1U;
        AlgoTest_Log("Median synthetic: median(1,1,100,1,1)=%lu (expect 1).\r\n",
                     (unsigned long)median_filter_5(1, 1, 100, 1, 1));
        AlgoTest_Log("Input: bias only. For real impulse test, briefly create one interference spike.\r\n");
    }
    AlgoTest_FrameToVolts(frame, input);
    (void)median_filter_init(&filter, samples, scratch, 5U);
    (void)median_filter_process_block(&filter, input, output, length);
    (void)statistics_min_max_f32(input, length, &raw_min, &raw_max);
    (void)statistics_min_max_f32(output, length, &out_min, &out_max);
    AlgoTest_Log("MED5: raw_pp=%lu mV, out_pp=%lu mV\r\n",
                 (unsigned long)AlgoTest_ToMilli(raw_max - raw_min),
                 (unsigned long)AlgoTest_ToMilli(out_max - out_min));
}
