/** @file test_frequency.c @brief 验证去偏置后过零插值测频。 */
#include "algorithm_test.h"
#include "frequency_measurement.h"

void Test_Frequency_Run(const algorithm_test_frame_t *frame)
{
    static uint8_t announced;
    frequency_zero_crossing_config_t config;
    measurement_result_t result;
    float *input = AlgoTest_WorkBuffer(0U);
    float *centered = AlgoTest_WorkBuffer(1U);
    size_t length = frame->length;
    measurement_status_t status;
    if (length > ALGO_TEST_ADC_FRAME_SIZE) length = ALGO_TEST_ADC_FRAME_SIZE;
    if (announced == 0U) {
        announced = 1U;
        AlgoTest_Log("DDS setting: POINT sine 1000 Hz, safe amplitude, signal+bias to PA0.\r\n");
    }
    AlgoTest_FrameToVolts(frame, input);
    AlgoTest_RemoveMean(input, centered, length);
    config.sample_rate = frame->sample_rate_hz;
    config.negative_threshold = -0.030f;
    config.positive_threshold = 0.030f;
    config.min_frequency = 10.0f;
    config.max_frequency = 5000.0f;
    status = frequency_measure_zero_crossing_f32(centered, length, &config, &result);
    if (status == MEASUREMENT_STATUS_OK) {
        AlgoTest_Log("FREQUENCY: %lu.%03lu Hz quality_x1000=%lu periods=%lu\r\n",
                     (unsigned long)result.value,
                     (unsigned long)((result.value - (float)(uint32_t)result.value) * 1000.0f + 0.5f),
                     (unsigned long)(result.quality * 1000.0f + 0.5f),
                     (unsigned long)result.samples_used);
    } else {
        AlgoTest_Log("FREQUENCY: status=%d (increase DDS amplitude or check wiring)\r\n", (int)status);
    }
}
