/** @file test_nco.c @brief 用内部固定样本验证 NCO 正弦生成，不需要 DDS。 */
#include "algorithm_test.h"
#include "nco.h"
#include "amplitude_measurement.h"

void Test_Nco_Run(const algorithm_test_frame_t *frame)
{
    nco_f32_t nco;
    float samples[200];
    float rms, mean;
    (void)frame;
    (void)nco_init_f32(&nco, 20000.0f, 1000.0f);
    (void)nco_generate_sine_f32(&nco, samples, 200U);
    (void)amplitude_rms_f32(samples, 200U, &rms);
    mean = 0.0f;
    for (uint32_t i = 0U; i < 200U; ++i) mean += samples[i];
    mean /= 200.0f;
    AlgoTest_Log("NCO synthetic: mean_u=%ld rms_x1000000=%lu, expect mean~0 rms~707107, %s\r\n",
                 (long)(mean * 1000000.0f), (unsigned long)(rms * 1000000.0f + 0.5f),
                 (rms > 0.706f && rms < 0.708f) ? "PASS" : "FAIL");
}
