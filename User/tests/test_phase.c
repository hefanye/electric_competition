/** @file test_phase.c @brief 用固定 0.5 rad 双通道样本验证相位差 I/Q 算法。 */
#include "algorithm_test.h"
#include "phase_measurement.h"
#include <math.h>

void Test_Phase_Run(const algorithm_test_frame_t *frame)
{
    float channel_a[ALGO_TEST_FFT_SIZE];
    float channel_b[ALGO_TEST_FFT_SIZE];
    phase_measurement_config_t config;
    measurement_result_t result;
    size_t i;
    (void)frame;
    for (i = 0U; i < ALGO_TEST_FFT_SIZE; ++i) {
        float angle = 2.0f * MEASUREMENT_PI_F * 1000.0f * (float)i / 20000.0f;
        channel_a[i] = sinf(angle);
        channel_b[i] = sinf(angle + 0.5f);
    }
    config.sample_rate = 20000.0f;
    config.frequency = 1000.0f;
    config.min_amplitude = 0.01f;
    config.window = PHASE_WINDOW_HANN;
    if (phase_measure_difference_iq_f32(channel_a, channel_b, ALGO_TEST_FFT_SIZE, &config, &result) == MEASUREMENT_STATUS_OK) {
        AlgoTest_Log("PHASE synthetic: result=%ld mrad, expect 500 mrad, %s\r\n",
                     (long)(result.value * 1000.0f),
                     (result.value > 0.48f && result.value < 0.52f) ? "PASS" : "FAIL");
    }
    AlgoTest_Log("Real phase requires two synchronous ADC channels; PA0 single-channel setup cannot verify it.\r\n");
}
