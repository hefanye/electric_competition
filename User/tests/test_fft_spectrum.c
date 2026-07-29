/** @file test_fft_spectrum.c @brief 用 ADC 实测正弦验证 Hann 窗、FFT 和频谱峰值插值。 */
#include "algorithm_test.h"
#include "spectrum_analysis.h"

void Test_FftSpectrum_Run(const algorithm_test_frame_t *frame)
{
    static uint8_t announced;
    static sp_complex_f32_t fft_buffer[ALGO_TEST_FFT_SIZE];
    static float magnitude[ALGO_TEST_FFT_SIZE / 2U + 1U];
    spectrum_peak_t peak;
    float *input = AlgoTest_WorkBuffer(0U);
    float *centered = AlgoTest_WorkBuffer(1U);
    if (frame->length < ALGO_TEST_FFT_SIZE) return;
    if (announced == 0U) {
        announced = 1U;
        AlgoTest_Log("DDS setting: POINT sine 1000 Hz, safe amplitude, signal+bias to PA0. Fs=20 kHz, N=512.\r\n");
    }
    AlgoTest_FrameToVolts(frame, input);
    AlgoTest_RemoveMean(input, centered, ALGO_TEST_FFT_SIZE);
    if (spectrum_compute_real_f32(centered, ALGO_TEST_FFT_SIZE, SP_WINDOW_HANN,
                                  fft_buffer, magnitude, ALGO_TEST_FFT_SIZE / 2U + 1U) == SIGNAL_PROCESS_STATUS_OK &&
        spectrum_find_peak_f32(magnitude, ALGO_TEST_FFT_SIZE / 2U + 1U,
                               frame->sample_rate_hz, ALGO_TEST_FFT_SIZE, 1U,
                               ALGO_TEST_FFT_SIZE / 2U - 1U, &peak) == SIGNAL_PROCESS_STATUS_OK) {
        AlgoTest_Log("FFT: peak=%lu.%03lu Hz amp=%lu mV bin=%lu\r\n",
                     (unsigned long)peak.frequency_hz,
                     (unsigned long)((peak.frequency_hz - (float)(uint32_t)peak.frequency_hz) * 1000.0f + 0.5f),
                     (unsigned long)AlgoTest_ToMilli(peak.amplitude_peak),
                     (unsigned long)peak.peak_bin);
    } else {
        AlgoTest_Log("FFT: processing failed.\r\n");
    }
}
