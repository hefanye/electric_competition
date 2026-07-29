/** @file test_biquad.c @brief 验证二阶 Butterworth 低通的频率抑制。 */
#include "algorithm_test.h"
#include "biquad_filter.h"
#include "amplitude_measurement.h"

void Test_Biquad_Run(const algorithm_test_frame_t *frame)
{
    static uint8_t announced;
    biquad_coefficients_t coefficients;
    biquad_filter_t filter;
    float *input = AlgoTest_WorkBuffer(0U);
    float *centered = AlgoTest_WorkBuffer(1U);
    float *output = AlgoTest_WorkBuffer(2U);
    float raw_rms, out_rms;
    size_t length = frame->length;
    if (length > ALGO_TEST_ADC_FRAME_SIZE) length = ALGO_TEST_ADC_FRAME_SIZE;
    if (announced == 0U) {
        announced = 1U;
        AlgoTest_Log("DDS setting: POINT sine, 1000 Hz, safe amplitude, then feed signal+bias to PA0.\r\n");
        AlgoTest_Log("Filter: Butterworth LPF cutoff=500 Hz; 1 kHz output must be attenuated.\r\n");
    }
    AlgoTest_FrameToVolts(frame, input);
    AlgoTest_RemoveMean(input, centered, length);
    (void)biquad_design_butterworth_lowpass(&coefficients, frame->sample_rate_hz, 500.0f);
    (void)biquad_filter_init(&filter, &coefficients);
    (void)biquad_filter_process_block(&filter, centered, output, length);
    (void)amplitude_rms_f32(centered, length, &raw_rms);
    (void)amplitude_rms_f32(output, length, &out_rms);
    AlgoTest_Log("BIQUAD LP500: in_rms=%lu mV, out_rms=%lu mV, %s\r\n",
                 (unsigned long)AlgoTest_ToMilli(raw_rms),
                 (unsigned long)AlgoTest_ToMilli(out_rms),
                 (out_rms < raw_rms) ? "PASS" : "CHECK INPUT");
}
