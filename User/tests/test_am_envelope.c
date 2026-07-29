/** @file test_am_envelope.c @brief 用可预测包络样本验证 AM 包络跟随和调制度计算。 */
#include "algorithm_test.h"
#include "am_envelope.h"
#include <math.h>

#define ALGO_TEST_PI_F (3.14159265358979323846f)

void Test_AmEnvelope_Run(const algorithm_test_frame_t *frame)
{
    am_envelope_follower_t follower;
    float input[400];
    float envelope[400];
    float modulation, min, max;
    uint32_t i;
    (void)frame;
    /* 直接用非负的理想包络，规避载波和外部 AM 源尚未接入的问题。 */
    for (i = 0U; i < 400U; ++i) {
        input[i] = 1.0f + 0.5f * sinf(2.0f * ALGO_TEST_PI_F * (float)i / 100.0f);
    }
    (void)am_envelope_init_alpha_f32(&follower, 1.0f, 1.0f);
    (void)am_envelope_process_block_f32(&follower, input, envelope, 400U);
    (void)am_modulation_index_f32(envelope, 400U, 0.01f, &modulation, &min, &max);
    AlgoTest_Log("AM envelope synthetic: min=%lu max=%lu m_x1000=%lu, expect 500, %s\r\n",
                 (unsigned long)AlgoTest_ToMilli(min), (unsigned long)AlgoTest_ToMilli(max),
                 (unsigned long)(modulation * 1000.0f + 0.5f),
                 (modulation > 0.49f && modulation < 0.51f) ? "PASS" : "FAIL");
}
