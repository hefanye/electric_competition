/** @file test_one_pole_lpf.c @brief 验证一阶低通的噪声平滑效果。 */
#include "algorithm_test.h"
#include "one_pole_lpf.h"
#include "statistics.h"

void Test_OnePole_Run(const algorithm_test_frame_t *frame)
{
    static uint8_t announced;
    one_pole_lpf_t filter;
    float *input = AlgoTest_WorkBuffer(0U);
    float *output = AlgoTest_WorkBuffer(1U);
    float raw_std, out_std;
    size_t length = frame->length;
    if (length > ALGO_TEST_ADC_FRAME_SIZE) length = ALGO_TEST_ADC_FRAME_SIZE;
    if (announced == 0U) {
        announced = 1U;
        AlgoTest_Log("Input: bias only; DDS not needed. One-pole cutoff=100 Hz at Fs=20 kHz.\r\n");
    }
    AlgoTest_FrameToVolts(frame, input);
    (void)one_pole_lpf_init_cutoff(&filter, frame->sample_rate_hz, 100.0f);
    (void)one_pole_lpf_process_block(&filter, input, output, length);
    (void)statistics_stddev_population_f32(input, length, &raw_std);
    (void)statistics_stddev_population_f32(output, length, &out_std);
    AlgoTest_Log("LPF1: raw_std=%lu uV, out_std=%lu uV, %s\r\n",
                 (unsigned long)AlgoTest_ToMicro(raw_std),
                 (unsigned long)AlgoTest_ToMicro(out_std),
                 (out_std < raw_std) ? "PASS" : "CHECK");
}
