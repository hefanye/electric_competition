/**
 * @file algorithm_test.h
 * @brief 算法实机测试选择器。main.c 只需修改 ALGORITHM_TEST_SELECTED。
 */
#ifndef ALGORITHM_TEST_H
#define ALGORITHM_TEST_H

#include "algorithm_test_common.h"

typedef enum {
    ALGO_TEST_ADC_BASELINE = 0,
    ALGO_TEST_MOVING_AVERAGE,
    ALGO_TEST_MEDIAN,
    ALGO_TEST_ONE_POLE_LPF,
    ALGO_TEST_DC_BLOCKER,
    ALGO_TEST_BIQUAD_LOWPASS,
    ALGO_TEST_STATISTICS,
    ALGO_TEST_AMPLITUDE,
    ALGO_TEST_FREQUENCY,
    ALGO_TEST_PHASE_SYNTHETIC,
    ALGO_TEST_SIGNAL_QUALITY,
    ALGO_TEST_STABILITY,
    ALGO_TEST_CALIBRATION_SYNTHETIC,
    ALGO_TEST_WINDOW_FUNCTIONS,
    ALGO_TEST_FFT_SPECTRUM,
    ALGO_TEST_GOERTZEL,
    ALGO_TEST_NCO_SYNTHETIC,
    ALGO_TEST_SWEEP_PLANNER,
    ALGO_TEST_AM_ENVELOPE_SYNTHETIC,
    ALGO_TEST_FSK,
    ALGO_TEST_COUNT
} algorithm_test_id_t;

void Algorithm_Test_Init(UART_HandleTypeDef *huart,
                         algorithm_test_id_t selected,
                         float sample_rate_hz);
void Algorithm_Test_OnAdcFrame(const uint16_t *codes,
                               size_t length,
                               float vdda_volts);
uint8_t Algorithm_Test_IsBaselineSelected(void);
const char *Algorithm_Test_Name(algorithm_test_id_t id);

/* 各独立测试文件的入口。 */
void Test_MovingAverage_Run(const algorithm_test_frame_t *frame);
void Test_Median_Run(const algorithm_test_frame_t *frame);
void Test_OnePole_Run(const algorithm_test_frame_t *frame);
void Test_DcBlocker_Run(const algorithm_test_frame_t *frame);
void Test_Biquad_Run(const algorithm_test_frame_t *frame);
void Test_Statistics_Run(const algorithm_test_frame_t *frame);
void Test_Amplitude_Run(const algorithm_test_frame_t *frame);
void Test_Frequency_Run(const algorithm_test_frame_t *frame);
void Test_Phase_Run(const algorithm_test_frame_t *frame);
void Test_Quality_Run(const algorithm_test_frame_t *frame);
void Test_Stability_Run(const algorithm_test_frame_t *frame);
void Test_Calibration_Run(const algorithm_test_frame_t *frame);
void Test_WindowFunctions_Run(const algorithm_test_frame_t *frame);
void Test_FftSpectrum_Run(const algorithm_test_frame_t *frame);
void Test_Goertzel_Run(const algorithm_test_frame_t *frame);
void Test_Nco_Run(const algorithm_test_frame_t *frame);
void Test_Sweep_Run(const algorithm_test_frame_t *frame);
void Test_AmEnvelope_Run(const algorithm_test_frame_t *frame);
void Test_Fsk_Run(const algorithm_test_frame_t *frame);

#endif /* ALGORITHM_TEST_H */
