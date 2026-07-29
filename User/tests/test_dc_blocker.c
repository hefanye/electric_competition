/** @file test_dc_blocker.c @brief 验证连续 DC 阻断器能移除 1.65 V ADC 偏置。 */
#include "algorithm_test.h"
#include "dc_blocker.h"
#include "statistics.h"

void Test_DcBlocker_Run(const algorithm_test_frame_t *frame)
{
    static uint8_t announced;
    static dc_blocker_t filter;
    static uint8_t initialized;
    static uint32_t frame_count;
    float *input = AlgoTest_WorkBuffer(0U);
    float *output = AlgoTest_WorkBuffer(1U);
    float mean;
    size_t length = frame->length;
    if (length > ALGO_TEST_ADC_FRAME_SIZE) length = ALGO_TEST_ADC_FRAME_SIZE;
    if (announced == 0U) {
        announced = 1U;
        AlgoTest_Log("Input: 1.65 V bias only; DDS not needed. Wait several reports for settling.\r\n");
    }
    if (initialized == 0U) {
        (void)dc_blocker_init_cutoff(&filter, frame->sample_rate_hz, 5.0f);
        initialized = 1U;
    }
    AlgoTest_FrameToVolts(frame, input);
    (void)dc_blocker_process_block(&filter, input, output, length);
    (void)statistics_mean_f32(output, length, &mean);
    frame_count++;
    AlgoTest_Log("DC blocker: output_mean=%ld uV, frame=%lu, %s\r\n",
                 (long)(mean * 1000000.0f), (unsigned long)frame_count,
                 (frame_count >= 3U && mean < 0.020f && mean > -0.020f) ? "PASS" : "SETTLING");
}
