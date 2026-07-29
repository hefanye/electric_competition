/**
 * @file dac8830_cli.c
 * @brief DAC8830 的串口助手命令解析。
 *
 * 一行一条命令。支持 CR/LF 结束；若串口助手不发送结束符，
 * 接收最后一个字节后空闲约 20 ms 也会自动执行：
 *   A=2.50       设置 A 通道为 2.50 V（仅限模块 0~5 V 档）
 *   B=1.65       设置 B 通道为 1.65 V
 *   AC=32768     设置 A 通道原始码值
 *   BC=65535     设置 B 通道原始码值
 *   STATUS       查询两路上次写入的码值
 *   RESET        两路清零
 *   HELP 或 ?    显示帮助
 */

#include "dac8830_cli.h"
#include "dac8830.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define DAC8830_CLI_LINE_MAX       (40U)
#define DAC8830_CLI_TX_TIMEOUT_MS  (100U)
#define DAC8830_CLI_IDLE_END_MS    (20U)

static UART_HandleTypeDef *s_uart;
static uint8_t s_rx_byte;
static char s_rx_line[DAC8830_CLI_LINE_MAX];
static volatile uint8_t s_rx_length;
static volatile uint8_t s_line_ready;
static volatile uint32_t s_last_rx_tick;
static uint16_t s_code_a;
static uint16_t s_code_b;

static void DAC8830_CLI_Send(const char *text)
{
    if ((s_uart != NULL) && (text != NULL))
    {
        (void)HAL_UART_Transmit(s_uart, (uint8_t *)text, (uint16_t)strlen(text),
                                DAC8830_CLI_TX_TIMEOUT_MS);
    }
}

static void DAC8830_CLI_SendHelp(void)
{
    DAC8830_CLI_Send("\r\nDAC8830 CLI\r\n"
                     "A=2.50 / B=1.65  (voltage, 0~5V)\r\n"
                     "AC=32768 / BC=65535 (raw code)\r\n"
                     "STATUS, RESET, HELP\r\n");
}

static uint8_t DAC8830_CLI_ParseFloat(const char *text, float *value)
{
    char *end_ptr;

    if ((text == NULL) || (value == NULL))
    {
        return 0U;
    }

    *value = strtof(text, &end_ptr);
    if (end_ptr == text)
    {
        return 0U;
    }

    while ((*end_ptr == ' ') || (*end_ptr == '\t'))
    {
        ++end_ptr;
    }

    return (*end_ptr == '\0') ? 1U : 0U;
}

static uint8_t DAC8830_CLI_ParseCode(const char *text, uint16_t *code)
{
    char *end_ptr;
    unsigned long value;

    if ((text == NULL) || (code == NULL))
    {
        return 0U;
    }

    value = strtoul(text, &end_ptr, 10);
    if ((end_ptr == text) || (value > 65535UL))
    {
        return 0U;
    }

    while ((*end_ptr == ' ') || (*end_ptr == '\t'))
    {
        ++end_ptr;
    }

    if (*end_ptr != '\0')
    {
        return 0U;
    }

    *code = (uint16_t)value;
    return 1U;
}

static void DAC8830_CLI_ReplyChannel(char name, uint16_t code)
{
    char reply[48];
    uint32_t full_scale_mv = (uint32_t)(DAC8830_OUTPUT_FULL_SCALE_VOLTS * 1000.0f + 0.5f);
    uint32_t millivolts = ((uint32_t)code * full_scale_mv + 32767U) / 65535U;

    (void)snprintf(reply, sizeof(reply), "OK %c code=%u out=%lu mV\r\n",
                   name, (unsigned int)code, (unsigned long)millivolts);
    DAC8830_CLI_Send(reply);
}

static void DAC8830_CLI_SetVoltage(char name, float voltage)
{
    dac8830_channel_t channel = (name == 'A') ? DAC8830_CHANNEL_A : DAC8830_CHANNEL_B;
    HAL_StatusTypeDef status;
    uint16_t code;

    if ((voltage < 0.0f) || (voltage > DAC8830_OUTPUT_FULL_SCALE_VOLTS))
    {
        DAC8830_CLI_Send("ERR voltage must be 0~5.00 V\r\n");
        return;
    }

    code = DAC8830_VoltageToCode(voltage);
    status = DAC8830_SetCode(channel, code);
    if (status != HAL_OK)
    {
        DAC8830_CLI_Send("ERR SPI\r\n");
        return;
    }

    if (name == 'A')
    {
        s_code_a = code;
    }
    else
    {
        s_code_b = code;
    }

    DAC8830_CLI_ReplyChannel(name, code);
}

static void DAC8830_CLI_SetCode(char name, uint16_t code)
{
    dac8830_channel_t channel = (name == 'A') ? DAC8830_CHANNEL_A : DAC8830_CHANNEL_B;

    if (DAC8830_SetCode(channel, code) != HAL_OK)
    {
        DAC8830_CLI_Send("ERR SPI\r\n");
        return;
    }

    if (name == 'A')
    {
        s_code_a = code;
    }
    else
    {
        s_code_b = code;
    }

    DAC8830_CLI_ReplyChannel(name, code);
}

HAL_StatusTypeDef DAC8830_CLI_Init(UART_HandleTypeDef *huart)
{
    if (huart == NULL)
    {
        return HAL_ERROR;
    }

    s_uart = huart;
    s_rx_length = 0U;
    s_line_ready = 0U;
    s_last_rx_tick = HAL_GetTick();
    s_code_a = 0U;
    s_code_b = 0U;

    DAC8830_CLI_Send("\r\nDAC8830 CLI READY; send HELP\r\n");
    return HAL_UART_Receive_IT(s_uart, &s_rx_byte, 1U);
}

void DAC8830_CLI_RxCpltCallback(void)
{
    s_last_rx_tick = HAL_GetTick();

    if ((s_rx_byte == '\r') || (s_rx_byte == '\n'))
    {
        if ((s_rx_length > 0U) && (s_line_ready == 0U))
        {
            s_rx_line[s_rx_length] = '\0';
            s_line_ready = 1U;
        }
    }
    else if (s_line_ready == 0U)
    {
        if (s_rx_length < (DAC8830_CLI_LINE_MAX - 1U))
        {
            s_rx_line[s_rx_length++] = (char)s_rx_byte;
        }
        else
        {
            s_rx_length = 0U;
            DAC8830_CLI_Send("ERR command too long\r\n");
        }
    }

    (void)HAL_UART_Receive_IT(s_uart, &s_rx_byte, 1U);
}

void DAC8830_CLI_UartErrorCallback(void)
{
    if (s_uart != NULL)
    {
        (void)HAL_UART_Receive_IT(s_uart, &s_rx_byte, 1U);
    }
}

void DAC8830_CLI_Process(void)
{
    char command[DAC8830_CLI_LINE_MAX];
    float voltage;
    uint16_t code;

    /* 有些串口助手不会自动附加 CRLF；空闲 20 ms 即视为一条命令结束。 */
    if ((s_line_ready == 0U) && (s_rx_length > 0U) &&
        ((uint32_t)(HAL_GetTick() - s_last_rx_tick) >= DAC8830_CLI_IDLE_END_MS))
    {
        __disable_irq();
        if ((s_rx_length > 0U) && (s_line_ready == 0U))
        {
            s_rx_line[s_rx_length] = '\0';
            s_line_ready = 1U;
        }
        __enable_irq();
    }

    if (s_line_ready == 0U)
    {
        return;
    }

    __disable_irq();
    (void)strncpy(command, s_rx_line, sizeof(command));
    command[sizeof(command) - 1U] = '\0';
    s_rx_length = 0U;
    s_line_ready = 0U;
    __enable_irq();

    if ((strcmp(command, "HELP") == 0) || (strcmp(command, "?") == 0))
    {
        DAC8830_CLI_SendHelp();
    }
    else if (strcmp(command, "RESET") == 0)
    {
        if (DAC8830_ResetAll() == HAL_OK)
        {
            s_code_a = 0U;
            s_code_b = 0U;
            DAC8830_CLI_Send("OK A=0 B=0\r\n");
        }
        else
        {
            DAC8830_CLI_Send("ERR SPI\r\n");
        }
    }
    else if (strcmp(command, "STATUS") == 0)
    {
        DAC8830_CLI_ReplyChannel('A', s_code_a);
        DAC8830_CLI_ReplyChannel('B', s_code_b);
    }
    else if (((command[0] == 'A') || (command[0] == 'B')) && (command[1] == '='))
    {
        if (DAC8830_CLI_ParseFloat(&command[2], &voltage) != 0U)
        {
            DAC8830_CLI_SetVoltage(command[0], voltage);
        }
        else
        {
            DAC8830_CLI_Send("ERR voltage format\r\n");
        }
    }
    else if (((command[0] == 'A') || (command[0] == 'B')) &&
             (command[1] == 'C') && (command[2] == '='))
    {
        if (DAC8830_CLI_ParseCode(&command[3], &code) != 0U)
        {
            DAC8830_CLI_SetCode(command[0], code);
        }
        else
        {
            DAC8830_CLI_Send("ERR code must be 0~65535\r\n");
        }
    }
    else
    {
        DAC8830_CLI_Send("ERR unknown; send HELP\r\n");
    }
}
