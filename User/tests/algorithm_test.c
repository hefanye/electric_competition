/** @file algorithm_test.c @brief 算法测试调度器；不包含 ADC/DMA 回调。 */
#include "algorithm_test.h"

static algorithm_test_id_t s_selected;
static float s_sample_rate_hz;

static const char *const s_names[ALGO_TEST_COUNT] = {
    "ADC_BASELINE", "MOVING_AVERAGE", "MEDIAN", "ONE_POLE_LPF", "DC_BLOCKER",
    "BIQUAD_LOWPASS", "STATISTICS", "AMPLITUDE", "FREQUENCY", "PHASE_SYNTHETIC",
    "SIGNAL_QUALITY", "STABILITY", "CALIBRATION_SYNTHETIC", "WINDOW_FUNCTIONS", "FFT_SPECTRUM",
    "GOERTZEL", "NCO_SYNTHETIC", "SWEEP_PLANNER", "AM_ENVELOPE_SYNTHETIC", "FSK"
};

const char *Algorithm_Test_Name(algorithm_test_id_t id)
{
    return (id < ALGO_TEST_COUNT) ? s_names[id] : "INVALID";
}

uint8_t Algorithm_Test_IsBaselineSelected(void)
{
    return (s_selected == ALGO_TEST_ADC_BASELINE) ? 1U : 0U;
}

void Algorithm_Test_Init(UART_HandleTypeDef *huart,
                         algorithm_test_id_t selected,
                         float sample_rate_hz)
{
    s_selected = (selected < ALGO_TEST_COUNT) ? selected : ALGO_TEST_ADC_BASELINE;
    s_sample_rate_hz = sample_rate_hz;
    AlgoTest_Common_Init(huart);
    AlgoTest_Log("\r\n=== ALGORITHM TEST: %s ===\r\n", Algorithm_Test_Name(s_selected));
}

void Algorithm_Test_OnAdcFrame(const uint16_t *codes, size_t length, float vdda_volts)
{
    algorithm_test_frame_t frame;
    if ((codes == NULL) || (length == 0U)) {
        return;
    }
    frame.codes = codes;
    frame.length = length;
    frame.vdda_volts = vdda_volts;
    frame.sample_rate_hz = s_sample_rate_hz;

    switch (s_selected) {
    case ALGO_TEST_MOVING_AVERAGE: Test_MovingAverage_Run(&frame); break;
    case ALGO_TEST_MEDIAN: Test_Median_Run(&frame); break;
    case ALGO_TEST_ONE_POLE_LPF: Test_OnePole_Run(&frame); break;
    case ALGO_TEST_DC_BLOCKER: Test_DcBlocker_Run(&frame); break;
    case ALGO_TEST_BIQUAD_LOWPASS: Test_Biquad_Run(&frame); break;
    case ALGO_TEST_STATISTICS: Test_Statistics_Run(&frame); break;
    case ALGO_TEST_AMPLITUDE: Test_Amplitude_Run(&frame); break;
    case ALGO_TEST_FREQUENCY: Test_Frequency_Run(&frame); break;
    case ALGO_TEST_PHASE_SYNTHETIC: Test_Phase_Run(&frame); break;
    case ALGO_TEST_SIGNAL_QUALITY: Test_Quality_Run(&frame); break;
    case ALGO_TEST_STABILITY: Test_Stability_Run(&frame); break;
    case ALGO_TEST_CALIBRATION_SYNTHETIC: Test_Calibration_Run(&frame); break;
    case ALGO_TEST_WINDOW_FUNCTIONS: Test_WindowFunctions_Run(&frame); break;
    case ALGO_TEST_FFT_SPECTRUM: Test_FftSpectrum_Run(&frame); break;
    case ALGO_TEST_GOERTZEL: Test_Goertzel_Run(&frame); break;
    case ALGO_TEST_NCO_SYNTHETIC: Test_Nco_Run(&frame); break;
    case ALGO_TEST_SWEEP_PLANNER: Test_Sweep_Run(&frame); break;
    case ALGO_TEST_AM_ENVELOPE_SYNTHETIC: Test_AmEnvelope_Run(&frame); break;
    case ALGO_TEST_FSK: Test_Fsk_Run(&frame); break;
    case ALGO_TEST_ADC_BASELINE:
    default: break;
    }
}
