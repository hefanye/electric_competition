/** @file test_stability.c @brief 验证多次 RMS 测量结果的稳定性判定。 */
#include "algorithm_test.h"
#include "stability_detector.h"
#include "amplitude_measurement.h"

void Test_Stability_Run(const algorithm_test_frame_t *frame)
{
    static uint8_t initialized;
    static stability_detector_t detector;
    static float history[10];
    stability_result_t result;
    float *input = AlgoTest_WorkBuffer(0U);
    float rms, dc;
    size_t length = frame->length;
    if (length > ALGO_TEST_ADC_FRAME_SIZE) length = ALGO_TEST_ADC_FRAME_SIZE;
    if (initialized == 0U) {
        initialized = 1U;
        (void)stability_detector_init(&detector, history, 10U, 0.002f, 0.02f);
        AlgoTest_Log("Input: keep 1.65 V bias or fixed DDS sine unchanged for ten reports.\r\n");
    }
    AlgoTest_FrameToVolts(frame, input);
    (void)amplitude_ac_rms_f32(input, length, &rms, &dc);
    (void)stability_detector_process(&detector, rms, &result);
    AlgoTest_Log("STABILITY: rms=%lu uV span=%lu uV allowed=%lu uV stable=%u n=%lu\r\n",
                 (unsigned long)AlgoTest_ToMicro(rms),
                 (unsigned long)AlgoTest_ToMicro(result.actual_span),
                 (unsigned long)AlgoTest_ToMicro(result.allowed_span),
                 (unsigned int)result.stable, (unsigned long)result.sample_count);
}
