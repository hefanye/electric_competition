/** @file test_statistics.c @brief 验证均值、极值和总体标准差统计模块。 */
#include "algorithm_test.h"
#include "statistics.h"

void Test_Statistics_Run(const algorithm_test_frame_t *frame)
{
    static uint8_t announced;
    float *input = AlgoTest_WorkBuffer(0U);
    float mean, min, max, std;
    size_t length = frame->length;
    if (length > ALGO_TEST_ADC_FRAME_SIZE) length = ALGO_TEST_ADC_FRAME_SIZE;
    if (announced == 0U) {
        announced = 1U;
        AlgoTest_Log("Input: 1.65 V bias only; DDS not needed.\r\n");
    }
    AlgoTest_FrameToVolts(frame, input);
    (void)statistics_mean_f32(input, length, &mean);
    (void)statistics_min_max_f32(input, length, &min, &max);
    (void)statistics_stddev_population_f32(input, length, &std);
    AlgoTest_Log("STAT: mean=%lu mV min=%lu max=%lu std=%lu uV\r\n",
                 (unsigned long)AlgoTest_ToMilli(mean),
                 (unsigned long)AlgoTest_ToMilli(min),
                 (unsigned long)AlgoTest_ToMilli(max),
                 (unsigned long)AlgoTest_ToMicro(std));
}
