/**
 ******************************************************************************
 * @file    dds_ui.c
 * @brief   串口屏 DDS 指令解析、DDS 配置和 tsta 状态显示。
 *
 * 支持的屏幕命令（每一条之后均为 FF FF FF）：
 *   DDS:CH:2
 *   DDS:POINT:100,100                 频率 Hz，幅度 0~1023
 *   DDS:FSK2:1000,10000,512           f0 Hz，f1 Hz，幅度
 *   DDS:SWEEP:100,10000,100,10,512    起始，终止，步进，间隔 ms，幅度
 *   DDS:STOP
 *   DDS:STATUS
 *
 * POINT/FSK2/SWEEP/STOP 均在 DDS 返回本次所有 OK 后，才修改 tsta 的
 * 工作通道列表；例如先成功开启 CH2、再成功开启 CH1，显示为 "2,1"。
 ******************************************************************************
 */
#include "dds_ui.h"

#include "dds_at.h"
#include "screen_protocol.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define DDS_UI_TIMEOUT_MS  2000U
#define DDS_UI_COMMAND_GAP_MS 300U
#define DDS_UI_RESET_SETTLE_MS 500U
#define DDS_UI_MAX_COMMANDS 10U
#define DDS_UI_COMMAND_LEN  32U

typedef enum
{
    DDS_UI_ACTION_NONE = 0,
    DDS_UI_ACTION_START,
    DDS_UI_ACTION_STOP,
    DDS_UI_ACTION_RESET
} DDS_UI_Action_t;

static uint8_t s_selected_channel = 1U;
static uint8_t s_active_order[4];
static uint8_t s_active_count;
static DDS_Mode_t s_channel_mode[4];

static uint8_t s_busy;
static uint8_t s_action_channel;
static DDS_Mode_t s_action_mode;
static DDS_UI_Action_t s_action;
static uint32_t s_action_start_tick;

/* 一次操作的 AT 命令队列。严格等待当前命令 OK 后才发送下一条。 */
static char s_command_queue[DDS_UI_MAX_COMMANDS][DDS_UI_COMMAND_LEN];
static uint8_t s_command_count;
static uint8_t s_command_index;
static uint8_t s_wait_next_command;
static uint32_t s_next_command_tick;

static void DDS_UI_ResetQueue(void)
{
    s_command_count = 0U;
    s_command_index = 0U;
    s_wait_next_command = 0U;
}

static uint8_t DDS_UI_AddCommand(const char *format, ...)
{
    va_list args;
    int length;

    if (s_command_count >= DDS_UI_MAX_COMMANDS) return 0U;

    va_start(args, format);
    length = vsnprintf(s_command_queue[s_command_count], DDS_UI_COMMAND_LEN,
                       format, args);
    va_end(args);

    if ((length < 0) || ((uint32_t)length >= DDS_UI_COMMAND_LEN)) return 0U;
    s_command_count++;
    return 1U;
}

/* 解析严格的十进制 CSV 参数，例如 "100,200,512"。 */
static uint8_t DDS_UI_ParseCsv(const char *text, uint32_t *values, uint8_t count)
{
    uint8_t i;
    const char *p = text;

    for (i = 0U; i < count; i++)
    {
        uint32_t value = 0U;
        uint8_t digits = 0U;

        while ((*p >= '0') && (*p <= '9'))
        {
            if (value > 429496729UL) return 0U;
            value = value * 10U + (uint32_t)(*p - '0');
            p++;
            digits++;
        }
        if (digits == 0U) return 0U;
        values[i] = value;

        if (i + 1U < count)
        {
            if (*p != ',') return 0U;
            p++;
        }
    }
    return (*p == '\0') ? 1U : 0U;
}

/* 将成功开启的通道记录为“开启先后顺序”。 */
static void DDS_UI_AddActiveChannel(uint8_t channel)
{
    uint8_t i;
    for (i = 0U; i < s_active_count; i++)
    {
        if (s_active_order[i] == channel) return;
    }
    if (s_active_count < 4U)
    {
        s_active_order[s_active_count++] = channel;
    }
}

static void DDS_UI_RemoveActiveChannel(uint8_t channel)
{
    uint8_t i;
    for (i = 0U; i < s_active_count; i++)
    {
        if (s_active_order[i] == channel)
        {
            while (i + 1U < s_active_count)
            {
                s_active_order[i] = s_active_order[i + 1U];
                i++;
            }
            s_active_count--;
            return;
        }
    }
}

/* 在 tsta 中显示工作通道，例如 "2,1"；没有工作通道则显示 READY。 */
static void DDS_UI_ShowActiveChannels(void)
{
    char text[12];
    uint8_t i;
    uint8_t pos = 0U;

    if (s_active_count == 0U)
    {
        Screen_SendText("tsta", "READY");
        return;
    }

    for (i = 0U; i < s_active_count; i++)
    {
        if ((i > 0U) && (pos < (sizeof(text) - 1U))) text[pos++] = ',';
        if (pos < (sizeof(text) - 1U)) text[pos++] = (char)('0' + s_active_order[i]);
    }
    text[pos] = '\0';
    Screen_SendText("tsta", text);
}

static void DDS_UI_StartWait(DDS_UI_Action_t action, uint8_t channel,
                             DDS_Mode_t mode)
{
    s_action = action;
    s_action_channel = channel;
    s_action_mode = mode;
    s_action_start_tick = HAL_GetTick();
    s_busy = 1U;
    Screen_SendText("tsta", "WAIT_OK");
}

/* 发送失败时立即给出状态，不能把该通道写入工作列表。 */
static void DDS_UI_SendFailed(void)
{
    DDS_BeginTransaction();
    DDS_UI_ResetQueue();
    s_busy = 0U;
    s_action = DDS_UI_ACTION_NONE;
    Screen_SendText("tsta", "DDS_TX_ERR");
}

/* 只发送队列中的当前一条命令；其 OK/ERROR 由 DDS_UI_Process 处理。 */
static void DDS_UI_SendCurrentCommand(void)
{
    DDS_BeginTransaction();
    s_wait_next_command = 0U;
    s_action_start_tick = HAL_GetTick();

    if ((s_command_index >= s_command_count) ||
        (DDS_SendCommand(s_command_queue[s_command_index]) != HAL_OK))
    {
        DDS_UI_SendFailed();
        return;
    }

    /* AT+RESET 是模块复位命令；不依赖其 OK，留出重启稳定时间即可。 */
    if ((s_action == DDS_UI_ACTION_RESET) && (s_command_index == 0U))
    {
        DDS_BeginTransaction();
        s_command_index++;
        s_wait_next_command = 1U;
        s_next_command_tick = HAL_GetTick() + DDS_UI_RESET_SETTLE_MS;
    }

}

static void DDS_UI_StartQueuedAction(DDS_UI_Action_t action, uint8_t channel,
                                     DDS_Mode_t mode)
{
    if (s_command_count == 0U)
    {
        DDS_UI_SendFailed();
        return;
    }
    DDS_UI_StartWait(action, channel, mode);
    DDS_UI_SendCurrentCommand();
}

/* 将失败 AT 命令缩写为可在 tsta 显示的定位码。 */
static const char *DDS_UI_ErrorLocation(void)
{
    const char *cmd = DDS_GetLastErrorCommand();

    if (strncmp(cmd, "AT+RESET", 8U) == 0) return "E_RST";
    if (strncmp(cmd, "AT+MODE", 7U) == 0) return "E_MODE";
    if (strncmp(cmd, "AT+CHANNEL", 10U) == 0) return "E_CH";
    if (strncmp(cmd, "AT+STARTFRE", 11U) == 0) return "E_START";
    if (strncmp(cmd, "AT+ENDFRE", 9U) == 0) return "E_END";
    if (strncmp(cmd, "AT+STARTAMP", 11U) == 0) return "E_SAMP";
    if (strncmp(cmd, "AT+ENDAMP", 9U) == 0) return "E_EAMP";
    if (strncmp(cmd, "AT+TIME", 7U) == 0) return "E_TIME";
    if (strncmp(cmd, "AT+STEP", 7U) == 0) return "E_STEP";
    if (strncmp(cmd, "AT+SWEEP", 8U) == 0) return "E_ON";
    if (strncmp(cmd, "AT+AMP", 6U) == 0) return "E_AMP";
    if (strncmp(cmd, "AT+FRE", 6U) == 0) return "E_FREQ";
    if (strncmp(cmd, "AT+PHA", 6U) == 0) return "E_PHA";
    return "DDS_ERR";
}

void DDS_UI_Init(void)
{
    uint8_t i;
    s_selected_channel = 1U;
    s_active_count = 0U;
    s_busy = 0U;
    s_action = DDS_UI_ACTION_NONE;
    DDS_UI_ResetQueue();
    for (i = 0U; i < 4U; i++) s_channel_mode[i] = DDS_MODE_POINT;
}

uint8_t DDS_UI_HandleScreenToken(const char *token)
{
    uint32_t value[5];
    HAL_StatusTypeDef status;

    if ((token == NULL) || (strncmp(token, "DDS:", 4U) != 0)) return 0U;

    /* 页面回到 dds_ctrl 时可请求一次当前工作列表，解决键盘返回后 tsta 复位。 */
    if (strcmp(token, "DDS:STATUS") == 0)
    {
        if (s_busy) Screen_SendText("tsta", "WAIT_OK");
        else DDS_UI_ShowActiveChannels();
        return 1U;
    }

    if (s_busy)
    {
        Screen_SendText("tsta", "DDS_BUSY");
        return 1U;
    }

    /* 一键复位：严格对应 DDS 指令集中的 AT+RESET。 */
    if (strcmp(token, "DDS:RESET") == 0)
    {
        DDS_UI_ResetQueue();
        status = DDS_UI_AddCommand("AT+RESET") ? HAL_OK : HAL_ERROR;

        if (status == HAL_OK)
            DDS_UI_StartQueuedAction(DDS_UI_ACTION_RESET, 1U, DDS_MODE_POINT);
        else
            DDS_UI_SendFailed();
        return 1U;
    }

    if (strncmp(token, "DDS:CH:", 7U) == 0)
    {
        if (DDS_UI_ParseCsv(token + 7U, value, 1U) &&
            (value[0] >= 1U) && (value[0] <= 4U))
        {
            s_selected_channel = (uint8_t)value[0];
        }
        else
        {
            Screen_SendText("tsta", "CH_ERR");
        }
        return 1U;
    }

    if (strcmp(token, "DDS:STOP") == 0)
    {
        DDS_UI_ResetQueue();
        if (DDS_UI_AddCommand("AT+CHANNEL+%u", (unsigned int)s_selected_channel))
        {
            if (s_channel_mode[s_selected_channel - 1U] == DDS_MODE_SWEEP)
                status = DDS_UI_AddCommand("AT+SWEEP+OFF") ? HAL_OK : HAL_ERROR;
            else
                status = DDS_UI_AddCommand("AT+AMP+0") ? HAL_OK : HAL_ERROR;
        }
        else status = HAL_ERROR;

        if (status == HAL_OK)
            DDS_UI_StartQueuedAction(DDS_UI_ACTION_STOP, s_selected_channel,
                                     s_channel_mode[s_selected_channel - 1U]);
        else DDS_UI_SendFailed();
        return 1U;
    }

    if (strncmp(token, "DDS:POINT:", 10U) == 0)
    {
        if (!DDS_UI_ParseCsv(token + 10U, value, 2U))
        {
            Screen_SendText("tsta", "POINT_ERR");
            return 1U;
        }
        if ((value[0] < 1U) || (value[0] > 200000000UL) || (value[1] > 1023U))
        {
            Screen_SendText("tsta", "POINT_ERR");
            return 1U;
        }
        DDS_UI_ResetQueue();
        status = (DDS_UI_AddCommand("AT+MODE+POINT") &&
                  DDS_UI_AddCommand("AT+CHANNEL+%u", (unsigned int)s_selected_channel) &&
                  DDS_UI_AddCommand("AT+AMP+%u", (unsigned int)value[1]) &&
                  DDS_UI_AddCommand("AT+PHA+0") &&
                  DDS_UI_AddCommand("AT+FRE+%lu", (unsigned long)value[0])) ? HAL_OK : HAL_ERROR;
        if (status == HAL_OK)
            DDS_UI_StartQueuedAction(DDS_UI_ACTION_START, s_selected_channel, DDS_MODE_POINT);
        else
            DDS_UI_SendFailed();
        return 1U;
    }

    if (strncmp(token, "DDS:FSK2:", 9U) == 0)
    {
        if (!DDS_UI_ParseCsv(token + 9U, value, 3U))
        {
            Screen_SendText("tsta", "FSK_ERR");
            return 1U;
        }
        if ((value[0] < 1U) || (value[0] > 200000000UL) ||
            (value[1] < 1U) || (value[1] > 200000000UL) || (value[2] > 1023U))
        {
            Screen_SendText("tsta", "FSK_ERR");
            return 1U;
        }
        DDS_UI_ResetQueue();
        status = (DDS_UI_AddCommand("AT+MODE+FSK2") &&
                  DDS_UI_AddCommand("AT+CHANNEL+%u", (unsigned int)s_selected_channel) &&
                  DDS_UI_AddCommand("AT+AMP+%u", (unsigned int)value[2]) &&
                  DDS_UI_AddCommand("AT+FRE1+%lu", (unsigned long)value[0]) &&
                  DDS_UI_AddCommand("AT+FRE2+%lu", (unsigned long)value[1])) ? HAL_OK : HAL_ERROR;
        if (status == HAL_OK)
            DDS_UI_StartQueuedAction(DDS_UI_ACTION_START, s_selected_channel, DDS_MODE_FSK2);
        else
            DDS_UI_SendFailed();
        return 1U;
    }

    if (strncmp(token, "DDS:SWEEP:", 10U) == 0)
    {
        if (!DDS_UI_ParseCsv(token + 10U, value, 5U))
        {
            Screen_SendText("tsta", "SWEEP_ERR");
            return 1U;
        }
        if ((value[0] < 1U) || (value[0] > 200000000UL) ||
            (value[1] < 1U) || (value[1] > 200000000UL) ||
            (value[2] < 1U) || (value[2] > 200000000UL) ||
            (value[3] < 1U) || (value[3] > 9999U) || (value[4] > 1023U))
        {
            Screen_SendText("tsta", "SWEEP_ERR");
            return 1U;
        }
        DDS_UI_ResetQueue();
        status = (DDS_UI_AddCommand("AT+MODE+SWEEP") &&
                  DDS_UI_AddCommand("AT+CHANNEL+%u", (unsigned int)s_selected_channel) &&
                  DDS_UI_AddCommand("AT+STARTFRE+%lu", (unsigned long)value[0]) &&
                  DDS_UI_AddCommand("AT+ENDFRE+%lu", (unsigned long)value[1]) &&
                  DDS_UI_AddCommand("AT+STARTAMP+%u", (unsigned int)value[4]) &&
                  DDS_UI_AddCommand("AT+ENDAMP+%u", (unsigned int)value[4]) &&
                  DDS_UI_AddCommand("AT+TIME+%u", (unsigned int)value[3]) &&
                  DDS_UI_AddCommand("AT+STEP+%lu", (unsigned long)value[2]) &&
                  DDS_UI_AddCommand("AT+SWEEP+ON")) ? HAL_OK : HAL_ERROR;
        if (status == HAL_OK)
            DDS_UI_StartQueuedAction(DDS_UI_ACTION_START, s_selected_channel, DDS_MODE_SWEEP);
        else
            DDS_UI_SendFailed();
        return 1U;
    }

    Screen_SendText("tsta", "DDS_CMD_ERR");
    return 1U;
}

void DDS_UI_Process(void)
{
    if (!s_busy) return;

    /* DDS 回 OK 后仍预留处理时间，模拟串口助手逐条手动发送的节奏。 */
    if (s_wait_next_command)
    {
        if ((int32_t)(HAL_GetTick() - s_next_command_tick) < 0) return;
        s_wait_next_command = 0U;

        /* RESET 已发出且已等待模块稳定，直接完成，不再等待可能丢失的 OK。 */
        if ((s_action == DDS_UI_ACTION_RESET) &&
            (s_command_index >= s_command_count))
        {
            uint8_t i;
            s_busy = 0U;
            s_selected_channel = 1U;
            s_active_count = 0U;
            for (i = 0U; i < 4U; i++) s_channel_mode[i] = DDS_MODE_POINT;
            s_action = DDS_UI_ACTION_NONE;
            DDS_UI_ResetQueue();
            Screen_SendText("t6", "");
            Screen_SendText("t4", "");
            DDS_UI_ShowActiveChannels();
            return;
        }

        DDS_UI_SendCurrentCommand();
        return;
    }

    if (DDS_GetErrorCount() > 0U)
    {
        s_busy = 0U;
        s_action = DDS_UI_ACTION_NONE;
        DDS_UI_ResetQueue();
        Screen_SendText("tsta", DDS_UI_ErrorLocation());
        return;
    }

    if (DDS_IsTransactionSuccessful())
    {
        s_command_index++;

        /* 当前命令已确认，严格串行地发送下一条。 */
        if (s_command_index < s_command_count)
        {
            s_wait_next_command = 1U;
            s_next_command_tick = HAL_GetTick() + DDS_UI_COMMAND_GAP_MS;
            return;
        }

        s_busy = 0U;

        if (s_action == DDS_UI_ACTION_START)
        {
            s_channel_mode[s_action_channel - 1U] = s_action_mode;
            DDS_UI_AddActiveChannel(s_action_channel);
        }
        else if (s_action == DDS_UI_ACTION_STOP)
        {
            DDS_UI_RemoveActiveChannel(s_action_channel);
        }
        else if (s_action == DDS_UI_ACTION_RESET)
        {
            uint8_t i;
            s_selected_channel = 1U;
            s_active_count = 0U;
            for (i = 0U; i < 4U; i++) s_channel_mode[i] = DDS_MODE_POINT;

            /* 主页面两个输入框由 MCU 在 DDS 安全关闭后清空。 */
            Screen_SendText("t6", "");
            Screen_SendText("t4", "");
        }

        s_action = DDS_UI_ACTION_NONE;
        DDS_UI_ResetQueue();
        DDS_UI_ShowActiveChannels();
        return;
    }

    if ((HAL_GetTick() - s_action_start_tick) >= DDS_UI_TIMEOUT_MS)
    {
        s_busy = 0U;
        s_action = DDS_UI_ACTION_NONE;
        DDS_BeginTransaction();
        DDS_UI_ResetQueue();
        Screen_SendText("tsta", "DDS_TIMEOUT");
    }
}
