/** @file test_signal_quality.c @brief 验证削顶、幅度不足和输入范围检查。 */
#include "algorithm_test.h"
#include "signal_quality.h"

void Test_Quality_Run(const algorithm_test_frame_t *frame)
{
    static uint8_t announced;
    signal_quality_config_t config = {0.0f, 3.3f, 0.05f, 0.010f, 0.001f};
    signal_quality_result_t result;
    float *input = AlgoTest_WorkBuffer(0U);
    size_t length = frame->length;
    if (length > ALGO_TEST_ADC_FRAME_SIZE) length = ALGO_TEST_ADC_FRAME_SIZE;
    if (announced == 0U) {
        announced = 1U;
        AlgoTest_Log("Input: first bias only, then a safe sine. Do not exceed ADC 0..3.3 V.\r\n");
    }
    AlgoTest_FrameToVolts(frame, input);
    (void)signal_quality_analyze_f32(input, length, &config, &result);
    AlgoTest_Log("QUALITY: mean=%lu mV ac_rms=%lu mV clip=%lu/1000000 usable=%u flags=0x%08lX\r\n",
                 (unsigned long)AlgoTest_ToMilli(result.mean),
                 (unsigned long)AlgoTest_ToMilli(result.ac_rms),
                 (unsigned long)(result.clipped_fraction * 1000000.0f),
                 (unsigned int)result.usable, (unsigned long)result.flags);
}
