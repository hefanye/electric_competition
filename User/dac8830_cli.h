/**
 * @file dac8830_cli.h
 * @brief USART1 串口助手控制 DAC8830 的文本命令接口。
 */

#ifndef DAC8830_CLI_H
#define DAC8830_CLI_H

#include "main.h"

/** 初始化 USART1 命令接口并启动单字节中断接收。 */
HAL_StatusTypeDef DAC8830_CLI_Init(UART_HandleTypeDef *huart);

/** 在 USART1 的 HAL_UART_RxCpltCallback 中调用。 */
void DAC8830_CLI_RxCpltCallback(void);

/** 在 main 的 while(1) 中循环调用，用于解析已经收完整的一行命令。 */
void DAC8830_CLI_Process(void);

/** 在 USART1 的 HAL_UART_ErrorCallback 中调用，用于恢复接收。 */
void DAC8830_CLI_UartErrorCallback(void);

#endif /* DAC8830_CLI_H */
