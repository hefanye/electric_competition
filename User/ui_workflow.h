#ifndef __UI_WORKFLOW_H
#define __UI_WORKFLOW_H

#include "main.h"
#include <string.h>
#include <stdlib.h>
#include "ui_defines.h"

/*=============================================================================
 * 上层工作流 API
 *===========================================================================*/

void UI_Init(void);
void UI_Process(void);

/*=============================================================================
 * 公开工具函数（供各测试模拟调用）
 *===========================================================================*/

void UI_ShowRunning(const char *description);
void UI_ShowResult(const char *title, const char *lines[], uint8_t line_count);
void UI_BackHome(void);

/*=============================================================================
 * 测试模拟入口
 *===========================================================================*/

void Simulate_CalStart(uint16_t length, const char *type);
void Simulate_DualFull(void);
void Simulate_DualWiremap(void);
void Simulate_DualShield(void);
void Simulate_DualResistance(void);
void Simulate_DualLoss(void);
void Simulate_SingleFull(void);
void Simulate_SingleLength(void);
void Simulate_SingleShortDetect(void);
void Simulate_SingleShortLocate(void);

#endif
