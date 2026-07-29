/** @file test_fsk.c @brief 固定双音验证 FSK 判决，并保留 DDS 实测接入接口。 */
#include "algorithm_test.h"
#include "fsk_detector.h"
#include <math.h>

#define ALGO_TEST_PI_F (3.14159265358979323846f)

void Test_Fsk_Run(const algorithm_test_frame_t *frame)
{
    static uint8_t synthetic_done;
    float synthetic[ALGO_TEST_FFT_SIZE];
    fsk_detector_config_t config;
    fsk_detector_result_t result;
    uint32_t i;
    config.sample_rate_hz = 20000.0f;
    config.tone_0_hz = 1000.0f;
    config.tone_1_hz = 2000.0f;
    config.window = SP_WINDOW_HANN;
    config.remove_mean = 1;
    config.minimum_amplitude = 0.01f;
    config.minimum_power_ratio = 2.0f;
    if (synthetic_done == 0U) {
        synthetic_done = 1U;
        for (i = 0U; i < ALGO_TEST_FFT_SIZE; ++i) {
            synthetic[i] = sinf(2.0f * ALGO_TEST_PI_F * 1000.0f * (float)i / 20000.0f);
        }
        (void)fsk_detector_analyze_f32(synthetic, ALGO_TEST_FFT_SIZE, &config, &result);
        AlgoTest_Log("FSK synthetic 1k: decision=%d (expect 1=TONE0) ratio_x1000=%lu\r\n",
                     (int)result.decision, (unsigned long)(result.power_ratio * 1000.0f));
        AlgoTest_Log("DDS real test: select FSK 1000/2000 Hz, feed PA0; each stable tone should give tone0/tone1.\r\n");
    }
    if (frame->length >= ALGO_TEST_FFT_SIZE) {
        float *input = AlgoTest_WorkBuffer(0U);
        float *centered = AlgoTest_WorkBuffer(1U);
        AlgoTest_FrameToVolts(frame, input);
        AlgoTest_RemoveMean(input, centered, ALGO_TEST_FFT_SIZE);
        config.sample_rate_hz = frame->sample_rate_hz;
        if (fsk_detector_analyze_f32(centered, ALGO_TEST_FFT_SIZE, &config, &result) == SIGNAL_PROCESS_STATUS_OK) {
            AlgoTest_Log("FSK real: decision=%d A0=%lu mV A1=%lu mV confidence_x1000=%lu\r\n",
                         (int)result.decision, (unsigned long)AlgoTest_ToMilli(result.tone_0_amplitude),
                         (unsigned long)AlgoTest_ToMilli(result.tone_1_amplitude),
                         (unsigned long)(result.confidence * 1000.0f));
        }
    }
}
