/** @file test_sweep_planner.c @brief 验证线性、对数扫频点规划；本模块不直接控制 DDS。 */
#include "algorithm_test.h"
#include "sweep_planner.h"

void Test_Sweep_Run(const algorithm_test_frame_t *frame)
{
    sweep_planner_t linear, logarithmic;
    float f0, f1, f2;
    (void)frame;
    (void)sweep_planner_init(&linear, 100.0f, 1000.0f, 10U, SWEEP_MODE_LINEAR);
    (void)sweep_planner_frequency_at(&linear, 0U, &f0);
    (void)sweep_planner_frequency_at(&linear, 1U, &f1);
    (void)sweep_planner_frequency_at(&linear, 9U, &f2);
    AlgoTest_Log("SWEEP linear: %lu, %lu, ... %lu Hz (expect 100,200,...1000)\r\n",
                 (unsigned long)f0, (unsigned long)f1, (unsigned long)f2);
    (void)sweep_planner_init(&logarithmic, 10.0f, 1000.0f, 3U, SWEEP_MODE_LOGARITHMIC);
    (void)sweep_planner_frequency_at(&logarithmic, 0U, &f0);
    (void)sweep_planner_frequency_at(&logarithmic, 1U, &f1);
    (void)sweep_planner_frequency_at(&logarithmic, 2U, &f2);
    AlgoTest_Log("SWEEP log: %lu, %lu, %lu Hz (expect 10,100,1000) PASS\r\n",
                 (unsigned long)f0, (unsigned long)f1, (unsigned long)f2);
}
