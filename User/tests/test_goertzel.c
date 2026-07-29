/** @file test_goertzel.c @brief 用 ADC 实测 1 kHz 正弦验证 Goertzel 单频测量。 */
#include "algorithm_test.h"
#include "goertzel.h"

void Test_Goertzel_Run(const algorithm_test_frame_t *frame)
{
    static uint8_t announced;
    goertzel_config_t config;
    goertzel_result_t result;
    float *input = AlgoTest_WorkBuffer(0U);
    float *centered = AlgoTest_WorkBuffer(1U);
    if (frame->length < ALGO_TEST_FFT_SIZE) return;
    if (announced == 0U) {
        announced = 1U;
        AlgoTest_Log("DDS setting: POINT sine 1000 Hz, safe amplitude, signal+bias to PA0.\r\n");
    }
    AlgoTest_FrameToVolts(frame, input);
    AlgoTest_RemoveMean(input, centered, ALGO_TEST_FFT_SIZE);
    config.sample_rate_hz = frame->sample_rate_hz;
    config.target_frequency_hz = 1000.0f;
    config.window = SP_WINDOW_HANN;
    config.remove_mean = 1;
    if (goertzel_analyze_f32(centered, ALGO_TEST_FFT_SIZE, &config, &result) == SIGNAL_PROCESS_STATUS_OK) {
        AlgoTest_Log("GOERTZEL 1k: amp=%lu mV phase=%ld mrad power_u=%lu\r\n",
                     (unsigned long)AlgoTest_ToMilli(result.amplitude_peak),
                     (long)(result.phase_radians * 1000.0f),
                     (unsigned long)(result.power * 1000000.0f));
    }
}
