/**
 ******************************************************************************
 * @file    dds_at.c
 * @brief   康威 AD9959 AT 指令模块实现。
 *
 * UART5 的串口参数由 CubeMX/usart.c 配置为 9600, 8N1。
 * 发送使用阻塞式 HAL_UART_Transmit；接收使用 UART5 单字节中断，
 * 用于解析 DDS 对每条 AT 指令返回的 OK 或 ERROR。
 ******************************************************************************
 */
#include "dds_at.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define DDS_UART_TIMEOUT_MS     200U
#define DDS_COMMAND_GAP_MS      30U
#define DDS_MAX_FREQUENCY_HZ    200000000UL
#define DDS_TRANSACTION_MAX_CMDS 12U
#define DDS_COMMAND_LABEL_LEN    24U

static UART_HandleTypeDef *s_dds_uart = NULL;
static uint8_t s_rx_byte;
static char s_rx_line[32];
static uint8_t s_rx_line_length;

static volatile uint16_t s_ok_count;
static volatile uint16_t s_pending_count;
static volatile uint16_t s_error_count;

/* DDS 回包按命令发送顺序返回，用小队列记录每一条命令以定位 ERROR。 */
static char s_command_queue[DDS_TRANSACTION_MAX_CMDS][DDS_COMMAND_LABEL_LEN];
static uint8_t s_command_count;
static uint8_t s_response_index;
static char s_last_error_command[DDS_COMMAND_LABEL_LEN];
static char s_last_error_response[32];

static void DDS_CopyText(char *dst, uint8_t dst_size, const char *src)
{
    uint8_t i = 0U;
    if ((dst == NULL) || (src == NULL) || (dst_size == 0U)) return;
    while ((i + 1U < dst_size) && (src[i] != '\0'))
    {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

/* 启动一次 UART5 单字节中断接收。 */
static void DDS_StartReceiveIT(void)
{
    if (s_dds_uart != NULL)
    {
        (void)HAL_UART_Receive_IT(s_dds_uart, &s_rx_byte, 1U);
    }
}

/* 将完整的一行 DDS 回包计入当前事务。 */
static void DDS_HandleResponseLine(void)
{
    if (strcmp(s_rx_line, "OK") == 0)
    {
        s_ok_count++;
        if (s_pending_count > 0U) s_pending_count--;
    }
    else if (strncmp(s_rx_line, "ERROR", 5U) == 0)
    {
        s_error_count++;
        if (s_response_index < s_command_count)
        {
            DDS_CopyText(s_last_error_command, sizeof(s_last_error_command),
                         s_command_queue[s_response_index]);
        }
        DDS_CopyText(s_last_error_response, sizeof(s_last_error_response), s_rx_line);
        if (s_pending_count > 0U) s_pending_count--;
    }

    /* 一条 OK 或 ERROR 都对应当前事务的一条已发送命令。 */
    if ((strcmp(s_rx_line, "OK") == 0) ||
        (strncmp(s_rx_line, "ERROR", 5U) == 0))
    {
        if (s_response_index < s_command_count) s_response_index++;
    }
}

/* 发送格式化 AT 命令的内部函数。 */
static HAL_StatusTypeDef DDS_SendFormat(const char *format, ...)
{
    char command[64];
    va_list args;
    int length;

    va_start(args, format);
    length = vsnprintf(command, sizeof(command), format, args);
    va_end(args);

    if ((length < 0) || ((size_t)length >= sizeof(command)))
    {
        return HAL_ERROR;
    }

    return DDS_SendCommand(command);
}

/* 连续命令之间留出模块处理时间，且发送失败时立即退出。 */
static HAL_StatusTypeDef DDS_CheckStep(HAL_StatusTypeDef status)
{
    if (status == HAL_OK)
    {
        HAL_Delay(DDS_COMMAND_GAP_MS);
    }
    return status;
}

void DDS_Init(UART_HandleTypeDef *huart)
{
    s_dds_uart = huart;
    DDS_BeginTransaction();
    DDS_StartReceiveIT();
}

void DDS_BeginTransaction(void)
{
    s_ok_count = 0U;
    s_pending_count = 0U;
    s_error_count = 0U;
    s_command_count = 0U;
    s_response_index = 0U;
    s_last_error_command[0] = '\0';
    s_last_error_response[0] = '\0';
}

HAL_StatusTypeDef DDS_SendCommand(const char *command)
{
    uint8_t buffer[68];
    size_t length;

    if ((s_dds_uart == NULL) || (command == NULL))
    {
        return HAL_ERROR;
    }

    length = strlen(command);
    if ((length == 0U) || (length > (sizeof(buffer) - 3U)))
    {
        return HAL_ERROR;
    }

    memcpy(buffer, command, length);

    /* 指令集要求每条 AT 指令以 \r\n 结束。 */
    if ((length < 2U) || (buffer[length - 2U] != '\r') ||
        (buffer[length - 1U] != '\n'))
    {
        buffer[length++] = '\r';
        buffer[length++] = '\n';
    }

    if (s_command_count >= DDS_TRANSACTION_MAX_CMDS)
    {
        return HAL_ERROR;
    }

    /* 先记录命令并计入等待，避免模块极快回 OK 时漏计数；发送失败则回退。 */
    DDS_CopyText(s_command_queue[s_command_count], DDS_COMMAND_LABEL_LEN, command);
    s_command_count++;
    s_pending_count++;
    if (HAL_UART_Transmit(s_dds_uart, buffer, (uint16_t)length,
                          DDS_UART_TIMEOUT_MS) != HAL_OK)
    {
        s_pending_count--;
        s_command_count--;
        return HAL_ERROR;
    }
    return HAL_OK;
}

void DDS_UartRxCpltCallback(void)
{
    uint8_t byte = s_rx_byte;

    if (byte == '\n')
    {
        /* DDS 回包为 OK\r\n 或 ERROR...\r\n。 */
        if ((s_rx_line_length > 0U) &&
            (s_rx_line[s_rx_line_length - 1U] == '\r'))
        {
            s_rx_line_length--;
        }
        s_rx_line[s_rx_line_length] = '\0';
        if (s_rx_line_length > 0U) DDS_HandleResponseLine();
        s_rx_line_length = 0U;
    }
    else if (s_rx_line_length < (sizeof(s_rx_line) - 1U))
    {
        s_rx_line[s_rx_line_length++] = (char)byte;
    }
    else
    {
        /* 异常长回包直接丢弃本行，等待下一行重新同步。 */
        s_rx_line_length = 0U;
    }

    DDS_StartReceiveIT();
}

uint16_t DDS_GetOkCount(void)
{
    return s_ok_count;
}

uint16_t DDS_GetPendingCount(void)
{
    return s_pending_count;
}

uint16_t DDS_GetErrorCount(void)
{
    return s_error_count;
}

const char *DDS_GetLastErrorCommand(void)
{
    return s_last_error_command;
}

const char *DDS_GetLastErrorResponse(void)
{
    return s_last_error_response;
}

uint8_t DDS_IsTransactionSuccessful(void)
{
    return ((s_ok_count > 0U) && (s_pending_count == 0U) &&
            (s_error_count == 0U)) ? 1U : 0U;
}

HAL_StatusTypeDef DDS_SelectChannel(uint8_t channel)
{
    if ((channel < 1U) || (channel > 4U)) return HAL_ERROR;
    return DDS_SendFormat("AT+CHANNEL+%u", channel);
}

HAL_StatusTypeDef DDS_SetMode(DDS_Mode_t mode)
{
    const char *mode_name;

    switch (mode)
    {
        case DDS_MODE_POINT: mode_name = "POINT"; break;
        case DDS_MODE_SWEEP: mode_name = "SWEEP"; break;
        case DDS_MODE_FSK2:  mode_name = "FSK2";  break;
        case DDS_MODE_FSK4:  mode_name = "FSK4";  break;
        case DDS_MODE_AM:    mode_name = "AM";    break;
        default: return HAL_ERROR;
    }
    return DDS_SendFormat("AT+MODE+%s", mode_name);
}

HAL_StatusTypeDef DDS_SetFrequency(uint32_t frequency_hz)
{
    if ((frequency_hz < 1U) || (frequency_hz > DDS_MAX_FREQUENCY_HZ)) return HAL_ERROR;
    return DDS_SendFormat("AT+FRE+%lu", (unsigned long)frequency_hz);
}

HAL_StatusTypeDef DDS_SetAmplitude(uint16_t amplitude)
{
    if (amplitude > 1023U) return HAL_ERROR;
    return DDS_SendFormat("AT+AMP+%u", amplitude);
}

HAL_StatusTypeDef DDS_SetPhase(uint16_t phase)
{
    if (phase > 16383U) return HAL_ERROR;
    return DDS_SendFormat("AT+PHA+%u", phase);
}

HAL_StatusTypeDef DDS_ConfigPoint(uint8_t channel, uint32_t frequency_hz,
                                  uint16_t amplitude, uint16_t phase)
{
    HAL_StatusTypeDef status;

    status = DDS_CheckStep(DDS_SetMode(DDS_MODE_POINT));
    if (status != HAL_OK) return status;
    status = DDS_CheckStep(DDS_SelectChannel(channel));
    if (status != HAL_OK) return status;
    status = DDS_CheckStep(DDS_SetAmplitude(amplitude));
    if (status != HAL_OK) return status;
    status = DDS_CheckStep(DDS_SetPhase(phase));
    if (status != HAL_OK) return status;
    return DDS_SetFrequency(frequency_hz);
}

HAL_StatusTypeDef DDS_ConfigFsk2(uint8_t channel, uint32_t f1_hz,
                                 uint32_t f2_hz, uint16_t amplitude)
{
    HAL_StatusTypeDef status;

    if ((f1_hz < 1U) || (f1_hz > DDS_MAX_FREQUENCY_HZ) ||
        (f2_hz < 1U) || (f2_hz > DDS_MAX_FREQUENCY_HZ) ||
        (amplitude > 1023U)) return HAL_ERROR;

    status = DDS_CheckStep(DDS_SetMode(DDS_MODE_FSK2));
    if (status != HAL_OK) return status;
    status = DDS_CheckStep(DDS_SelectChannel(channel));
    if (status != HAL_OK) return status;
    status = DDS_CheckStep(DDS_SetAmplitude(amplitude));
    if (status != HAL_OK) return status;
    status = DDS_CheckStep(DDS_SendFormat("AT+FRE1+%lu", (unsigned long)f1_hz));
    if (status != HAL_OK) return status;
    return DDS_SendFormat("AT+FRE2+%lu", (unsigned long)f2_hz);
}

HAL_StatusTypeDef DDS_StartSweep(uint8_t channel, uint32_t start_hz,
                                 uint32_t end_hz, uint16_t start_amplitude,
                                 uint16_t end_amplitude, uint16_t time_ms,
                                 uint32_t step_hz)
{
    HAL_StatusTypeDef status;

    if ((start_hz < 1U) || (start_hz > DDS_MAX_FREQUENCY_HZ) ||
        (end_hz < 1U) || (end_hz > DDS_MAX_FREQUENCY_HZ) ||
        (start_amplitude > 1023U) || (end_amplitude > 1023U) ||
        (time_ms < 1U) || (time_ms > 9999U) ||
        (step_hz < 1U) || (step_hz > DDS_MAX_FREQUENCY_HZ)) return HAL_ERROR;

    status = DDS_CheckStep(DDS_SetMode(DDS_MODE_SWEEP));
    if (status != HAL_OK) return status;
    status = DDS_CheckStep(DDS_SelectChannel(channel));
    if (status != HAL_OK) return status;
    status = DDS_CheckStep(DDS_SendFormat("AT+STARTFRE+%lu", (unsigned long)start_hz));
    if (status != HAL_OK) return status;
    status = DDS_CheckStep(DDS_SendFormat("AT+ENDFRE+%lu", (unsigned long)end_hz));
    if (status != HAL_OK) return status;
    status = DDS_CheckStep(DDS_SendFormat("AT+STARTAMP+%u", start_amplitude));
    if (status != HAL_OK) return status;
    status = DDS_CheckStep(DDS_SendFormat("AT+ENDAMP+%u", end_amplitude));
    if (status != HAL_OK) return status;
    status = DDS_CheckStep(DDS_SendFormat("AT+TIME+%u", time_ms));
    if (status != HAL_OK) return status;
    status = DDS_CheckStep(DDS_SendFormat("AT+STEP+%lu", (unsigned long)step_hz));
    if (status != HAL_OK) return status;
    return DDS_SendCommand("AT+SWEEP+ON");
}

HAL_StatusTypeDef DDS_StopSweep(void)
{
    return DDS_SendCommand("AT+SWEEP+OFF");
}

HAL_StatusTypeDef DDS_OutputOff(void)
{
    return DDS_SetAmplitude(0U);
}

HAL_StatusTypeDef DDS_DebugPointTest(void)
{
    HAL_StatusTypeDef status;

    DDS_BeginTransaction();

    /* 通信测试。预期收到 1 个 OK。 */
    status = DDS_CheckStep(DDS_SendCommand("AT"));
    if (status != HAL_OK) return status;

    return DDS_ConfigPoint(1U, 1000U, 512U, 0U);
}
