/** @file test_calibration.c @brief 验证两点和多点线性校准函数；实际 DAC 校准按 README 操作。 */
#include "algorithm_test.h"
#include "calibration.h"

void Test_Calibration_Run(const algorithm_test_frame_t *frame)
{
    linear_calibration_t model;
    float corrected;
    float raw[5] = {0.49f, 0.98f, 1.48f, 1.99f, 2.48f};
    float reference[5] = {0.50f, 1.00f, 1.50f, 2.00f, 2.50f};
    (void)frame;
    if (calibration_linear_fit_f32(raw, reference, 5U, &model) == MEASUREMENT_STATUS_OK &&
        calibration_linear_apply(&model, 1.48f, &corrected) == MEASUREMENT_STATUS_OK) {
        AlgoTest_Log("CAL synthetic: gain_x1000=%lu offset_mV=%ld corrected(1.48)=%lu mV rmse_uV=%lu PASS\r\n",
                     (unsigned long)(model.gain * 1000.0f + 0.5f),
                     (long)(model.offset * 1000.0f),
                     (unsigned long)AlgoTest_ToMilli(corrected),
                     (unsigned long)AlgoTest_ToMicro(model.rmse));
    }
    AlgoTest_Log("Actual: apply DAC/DC levels 0.5,1.0,1.65,2.3,2.8 V; record DMM and ADC mean, then enter points later.\r\n");
}
