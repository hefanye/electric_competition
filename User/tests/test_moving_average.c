/** @file test_moving_average.c @brief 实机验证 16 点移动平均是否降低 ADC 随机噪声。 */
#include "algorithm_test.h"
#include "moving_average.h"
#include "statistics.h"

void Test_MovingAverage_Run(const algorithm_test_frame_t *frame)
{
    static uint8_t announced;
    float history[16] = {0};
    moving_average_t filter;
    float *input = AlgoTest_WorkBuffer(0U);
    float *output = AlgoTest_WorkBuffer(1U);
    float raw_std, filtered_std;
    size_t length = frame->length;

    if (length > ALGO_TEST_ADC_FRAME_SIZE) length = ALGO_TEST_ADC_FRAME_SIZE;
    if (announced == 0U) {
        announced = 1U;
        AlgoTest_Log("Input: bias only; DDS not needed. Expect filtered std < raw std.\r\n");
    }
    AlgoTest_FrameToVolts(frame, input);
    (void)moving_average_init(&filter, history, 16U);
    (void)moving_average_process_block(&filter, input, output, length);
    (void)statistics_stddev_population_f32(input, length, &raw_std);
    (void)statistics_stddev_population_f32(output, length, &filtered_std);
    AlgoTest_Log("MA16: raw_std=%lu uV, out_std=%lu uV, %s\r\n",
                 (unsigned long)AlgoTest_ToMicro(raw_std),
                 (unsigned long)AlgoTest_ToMicro(filtered_std),
                 (filtered_std < raw_std) ? "PASS" : "CHECK");
}
