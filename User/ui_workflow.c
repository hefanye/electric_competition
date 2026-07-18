#include "ui_workflow.h"
#include "screen_protocol.h"
#include "usart.h"

/*===========================================================================
 * 令牌解析：按 ":" 分割，最多 4 段
 *===========================================================================*/

static int ParseToken(const char *cmd, char parts[][UI_TOKEN_PART_LEN], uint8_t *count)
{
    uint8_t part = 0, ch = 0;
    memset(parts, 0, UI_TOKEN_MAX_PARTS * UI_TOKEN_PART_LEN);
    while (*cmd && part < UI_TOKEN_MAX_PARTS) {
        if (*cmd == ':') {
            parts[part][ch] = '\0';
            part++; ch = 0;
        } else if (ch < UI_TOKEN_PART_LEN - 1) {
            parts[part][ch++] = *cmd;
        }
        cmd++;
    }
    parts[part][ch] = '\0';
    *count = part + 1;
    return (*count > 0);
}

/*===========================================================================
 * 事件识别
 *===========================================================================*/

static UI_EventId IdentifyEvent(char p[][UI_TOKEN_PART_LEN], uint8_t cnt,
                                uint16_t *p1, const char **p2)
{
    *p1 = 0; *p2 = NULL;
    if (cnt < 2) return UI_EVENT_NONE;

    if (strcmp(p[0], "CAL") == 0) {
        if (strcmp(p[1], "OPEN") == 0)  return UI_EVENT_OPEN_CAL;
        if (strcmp(p[1], "START") == 0 && cnt >= 4) {
            *p1 = (uint16_t)atoi(p[2]);
            *p2 = p[3];
            return UI_EVENT_CAL_START;
        }
        return UI_EVENT_NONE;
    }
    if (strcmp(p[0], "TEST") == 0) {
        if (strcmp(p[1], "OPEN") == 0)    return UI_EVENT_OPEN_TEST;
        if (strcmp(p[1], "DUAL") == 0)    return UI_EVENT_OPEN_DUAL;
        if (strcmp(p[1], "SINGLE") == 0)  return UI_EVENT_OPEN_SINGLE;
        return UI_EVENT_NONE;
    }
    if (strcmp(p[0], "HOME") == 0 && strcmp(p[1], "BACK") == 0)
        return UI_EVENT_BACK_HOME;

    if (strcmp(p[0], "DUAL") == 0) {
        if (strcmp(p[1], "FULL") == 0)     return UI_EVENT_DUAL_FULL;
        if (strcmp(p[1], "WIREMAP") == 0)  return UI_EVENT_DUAL_WIREMAP;
        if (strcmp(p[1], "SHIELD") == 0)   return UI_EVENT_DUAL_SHIELD;
        if (strcmp(p[1], "RES") == 0)      return UI_EVENT_DUAL_RESISTANCE;
        if (strcmp(p[1], "LOSS") == 0)     return UI_EVENT_DUAL_LOSS;
        return UI_EVENT_NONE;
    }
    if (strcmp(p[0], "SINGLE") == 0) {
        if (strcmp(p[1], "FULL") == 0)   return UI_EVENT_SINGLE_FULL;
        if (strcmp(p[1], "LEN") == 0)    return UI_EVENT_SINGLE_LENGTH;
        if (strcmp(p[1], "SHORT") == 0)  return UI_EVENT_SINGLE_SHORT_DETECT;
        if (strcmp(p[1], "LOC") == 0)    return UI_EVENT_SINGLE_SHORT_LOCATE;
        return UI_EVENT_NONE;
    }
    return UI_EVENT_NONE;
}

/*===========================================================================
 * MCU 主动切页 (仅用于 run / result 页)
 *===========================================================================*/

void UI_ShowRunning(const char *desc)
{
    Screen_SwitchPage(PAGE_NAME_RUN);
    HAL_Delay(500);
    Screen_SendText(CTRL_TSTA, STATUS_RUNNING);
    if (desc) Screen_SendText(CTRL_TITLE, desc);
}

void UI_ShowResult(const char *title, const char *lines[], uint8_t n)
{
    const char *row[] = {CTRL_T0, CTRL_T1, CTRL_T2, CTRL_T3, CTRL_T4, CTRL_T5};
    uint8_t i;

    Screen_SwitchPage(PAGE_NAME_RESULT);
    HAL_Delay(500);
    if (title) Screen_SendText(CTRL_TITLE, title);
    for (i = 0; i < n && i < 6; i++)
        if (lines[i]) Screen_SendText(row[i], lines[i]);
}

/*===========================================================================
 * 返回主页 (屏幕按钮已切页，MCU 只需刷新文本)
 *===========================================================================*/

void UI_BackHome(void)
{
    HAL_Delay(500);
    Screen_SendText(CTRL_TSTA, STATUS_READY);
}

/*===========================================================================
 * 测试模拟
 *===========================================================================*/

void Simulate_CalStart(uint16_t len, const char *type)
{
    char l1[32], l2[32];
    snprintf(l1, sizeof(l1), "LEN=%um", len);
    snprintf(l2, sizeof(l2), "TYPE=%s", type ? type : "UTP");
    UI_ShowRunning("CAL...");
    HAL_Delay(100);
    const char *r[] = {l1, l2};
    UI_ShowResult("CAL_DONE", r, 2);
}

void Simulate_DualFull(void)
{
    const char *r[] = {"WIREMAP:OK","SHIELD:SFTP","R1=12.3R R2=12.5R",
                       "R3=12.4R R4=12.6R","LOSS=-3.2dB"};
    UI_ShowRunning("DUAL FULL...");
    HAL_Delay(200);
    UI_ShowResult("DUAL_FULL", r, 5);
}

void Simulate_DualWiremap(void)
{
    UI_ShowRunning("WIREMAP...");
    HAL_Delay(100);
    UI_ShowResult("WIREMAP", NULL, 0);
}

void Simulate_DualShield(void)
{
    UI_ShowRunning("SHIELD...");
    HAL_Delay(100);
    UI_ShowResult("SHIELD:SFTP", NULL, 0);
}

void Simulate_DualResistance(void)
{
    const char *r[] = {"PAIR1=12.3R","PAIR2=12.5R","PAIR3=12.4R","PAIR4=12.6R"};
    UI_ShowRunning("RES...");
    HAL_Delay(100);
    UI_ShowResult("RESISTANCE", r, 4);
}

void Simulate_DualLoss(void)
{
    const char *r[] = {"LOSS=-3.2dB"};
    UI_ShowRunning("LOSS...");
    HAL_Delay(100);
    UI_ShowResult("ATTEN", r, 1);
}

void Simulate_SingleFull(void)
{
    const char *r[] = {"LENGTH=105m","SHORT:NO_SHORT"};
    UI_ShowRunning("SINGLE FULL...");
    HAL_Delay(100);
    UI_ShowResult("SINGLE_FULL", r, 2);
}

void Simulate_SingleLength(void)
{
    const char *r[] = {"LENGTH=105m"};
    UI_ShowRunning("LEN...");
    HAL_Delay(100);
    UI_ShowResult("LENGTH", r, 1);
}

void Simulate_SingleShortDetect(void)
{
    UI_ShowRunning("SHORT DETECT...");
    HAL_Delay(100);
    UI_ShowResult("NO_SHORT", NULL, 0);
}

void Simulate_SingleShortLocate(void)
{
    const char *r[] = {"SHORT@PAIR2","DIST=32.5m"};
    UI_ShowRunning("SHORT LOC...");
    HAL_Delay(100);
    UI_ShowResult("SHORT_LOC", r, 2);
}

/*===========================================================================
 * 事件派发
 *===========================================================================*/

static void ProcessEvent(UI_EventId evt, uint16_t p1, const char *p2)
{
    switch (evt) {
    case UI_EVENT_OPEN_CAL:     Screen_SendText(CTRL_TSTA, STATUS_CAL_READY);     break;
    case UI_EVENT_OPEN_TEST:    Screen_SendText(CTRL_TSTA, STATUS_TEST_READY);    break;
    case UI_EVENT_OPEN_DUAL:    Screen_SendText(CTRL_TSTA, STATUS_DUAL_READY);    break;
    case UI_EVENT_OPEN_SINGLE:  Screen_SendText(CTRL_TSTA, STATUS_SINGLE_READY);  break;
    case UI_EVENT_BACK_HOME:    UI_BackHome();                                    break;
    case UI_EVENT_CAL_START:           Simulate_CalStart(p1, p2);                 break;
    case UI_EVENT_DUAL_FULL:           Simulate_DualFull();                       break;
    case UI_EVENT_DUAL_WIREMAP:        Simulate_DualWiremap();                    break;
    case UI_EVENT_DUAL_SHIELD:         Simulate_DualShield();                     break;
    case UI_EVENT_DUAL_RESISTANCE:     Simulate_DualResistance();                 break;
    case UI_EVENT_DUAL_LOSS:           Simulate_DualLoss();                       break;
    case UI_EVENT_SINGLE_FULL:         Simulate_SingleFull();                     break;
    case UI_EVENT_SINGLE_LENGTH:       Simulate_SingleLength();                   break;
    case UI_EVENT_SINGLE_SHORT_DETECT: Simulate_SingleShortDetect();              break;
    case UI_EVENT_SINGLE_SHORT_LOCATE: Simulate_SingleShortLocate();              break;
    default: break;
    }
}

/*===========================================================================
 * 初始化 — 和最开始测试成功时一模一样
 *===========================================================================*/

void UI_Init(void)
{
    UART_StartRx();
    HAL_Delay(2000);
    Screen_SendText(CTRL_TSTA, STATUS_READY);
}

/*===========================================================================
 * 主循环 — 收到一帧就解析派发
 *===========================================================================*/

void UI_Process(void)
{
    char parts[UI_TOKEN_MAX_PARTS][UI_TOKEN_PART_LEN];
    uint8_t cnt;
    uint16_t p1;
    const char *p2;

    if (!g_cmd_received) return;
    g_cmd_received = 0;

    if (!ParseToken(g_cmd_buffer, parts, &cnt)) return;

    UI_EventId evt = IdentifyEvent(parts, cnt, &p1, &p2);
    if (evt != UI_EVENT_NONE) ProcessEvent(evt, p1, p2);
}
