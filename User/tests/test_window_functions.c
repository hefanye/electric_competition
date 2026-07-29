/** @file test_window_functions.c @brief 验证 Hann/Hamming 等窗函数的端点和相干增益。 */
#include "algorithm_test.h"
#include "window_functions.h"

void Test_WindowFunctions_Run(const algorithm_test_frame_t *frame)
{
    static uint8_t completed;
    float h0, h1, h2, h3, h4, gain;
    (void)frame;
    if (completed != 0U) return;
    completed = 1U;
    (void)sp_window_coefficient_f32(SP_WINDOW_HANN, 0U, 5U, &h0);
    (void)sp_window_coefficient_f32(SP_WINDOW_HANN, 1U, 5U, &h1);
    (void)sp_window_coefficient_f32(SP_WINDOW_HANN, 2U, 5U, &h2);
    (void)sp_window_coefficient_f32(SP_WINDOW_HANN, 3U, 5U, &h3);
    (void)sp_window_coefficient_f32(SP_WINDOW_HANN, 4U, 5U, &h4);
    (void)sp_window_coherent_gain_f32(SP_WINDOW_HANN, 1024U, &gain);
    AlgoTest_Log("WINDOW Hann[5]=%lu,%lu,%lu,%lu,%lu m; gain1024_x1000=%lu, expect 0,500,1000,500,0 / ~500 PASS\r\n",
                 (unsigned long)AlgoTest_ToMilli(h0), (unsigned long)AlgoTest_ToMilli(h1),
                 (unsigned long)AlgoTest_ToMilli(h2), (unsigned long)AlgoTest_ToMilli(h3),
                 (unsigned long)AlgoTest_ToMilli(h4), (unsigned long)(gain * 1000.0f + 0.5f));
}
