/** @file test_amplitude.c @brief 验证 AC RMS、峰峰值和波峰因子测量。 */
#include "algorithm_test.h"
#include "amplitude_measurement.h"

void Test_Amplitude_Run(const algorithm_test_frame_t *frame)
{
    static uint8_t announced;
    float *input = AlgoTest_WorkBuffer(0U);
    float *centered = AlgoTest_WorkBuffer(1U);
    float ac_rms, dc, pp, crest;
    size_t length = frame->length;
    if (length > ALGO_TEST_ADC_FRAME_SIZE) length = ALGO_TEST_ADC_FRAME_SIZE;
    if (announced == 0U) {
        announced = 1U;
        AlgoTest_Log("DDS setting: POINT sine, 1000 Hz, safe amplitude; add 1.65 V bias before PA0.\r\n");
        AlgoTest_Log("Compare PP with oscilloscope. For a clean sine crest should approach 1.414.\r\n");
    }
    AlgoTest_FrameToVolts(frame, input);
    (void)amplitude_ac_rms_f32(input, length, &ac_rms, &dc);
    (void)amplitude_peak_to_peak_f32(input, length, &pp);
    /* 波峰因子描述交流波形形状，必须先去除 ADC 的直流偏置。 */
    AlgoTest_RemoveMean(input, centered, length);
    (void)amplitude_crest_factor_f32(centered, length, &crest);
    AlgoTest_Log("AMPLITUDE: dc=%lu mV ac_rms=%lu mV pp=%lu mV crest_x1000=%lu\r\n",
                 (unsigned long)AlgoTest_ToMilli(dc),
                 (unsigned long)AlgoTest_ToMilli(ac_rms),
                 (unsigned long)AlgoTest_ToMilli(pp),
                 (unsigned long)(crest * 1000.0f + 0.5f));
}
