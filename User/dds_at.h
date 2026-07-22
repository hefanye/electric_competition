/**
 ******************************************************************************
 * @file    dds_at.h
 * @brief   康威 AD9959 AT 指令模块（UART5）。
 *
 * 本文件只负责 STM32 与 DDS 模块的串口 AT 通信，完全不使用 UART4，
 * 因此不会影响已经调通的串口屏通信代码。
 ******************************************************************************
 */
#ifndef DDS_AT_H
#define DDS_AT_H

#include "main.h"
#include <stdint.h>

typedef enum
{
    DDS_MODE_POINT = 0,
    DDS_MODE_SWEEP,
    DDS_MODE_FSK2,
    DDS_MODE_FSK4,
    DDS_MODE_AM
} DDS_Mode_t;

/** 绑定用于 DDS 的 UART 句柄。本工程传入 &huart5。 */
void DDS_Init(UART_HandleTypeDef *huart);

/**
 * 发送一条 AT 指令。
 * 调用时不需要自行添加 \r\n；函数会自动补齐命令结束符。
 * 每发送成功一条命令，模块会等待 DDS 返回对应的一条 OK 或 ERROR。
 */
HAL_StatusTypeDef DDS_SendCommand(const char *command);

/** 开始一组新的 DDS 配置事务，清空上一组 OK/ERROR 统计。 */
void DDS_BeginTransaction(void);

/** 由 UART5 接收完成回调调用；不要在应用层直接调用。 */
void DDS_UartRxCpltCallback(void);

/** 当前事务中已确认的 OK 数量。 */
uint16_t DDS_GetOkCount(void);

/** 当前已发送、但尚未收到 OK 或 ERROR 回包的指令数量。 */
uint16_t DDS_GetPendingCount(void);

/** 当前事务中收到 ERROR 回包的数量。 */
uint16_t DDS_GetErrorCount(void);

/** 最近一次 ERROR 对应的 AT 命令；无错误时返回空字符串。 */
const char *DDS_GetLastErrorCommand(void);

/** 最近一次 DDS 原始错误行，例如 ERROR_DATA_OVER_RANGEM。 */
const char *DDS_GetLastErrorResponse(void);

/** 所有已发送指令均返回 OK 时为 1；仍在等待或收到 ERROR 时为 0。 */
uint8_t DDS_IsTransactionSuccessful(void);

/** 选择当前操作通道，channel 范围为 1~4。 */
HAL_StatusTypeDef DDS_SelectChannel(uint8_t channel);

/** 切换 DDS 的工作模式。 */
HAL_StatusTypeDef DDS_SetMode(DDS_Mode_t mode);

/** 设置当前通道的点频频率，范围 1~200000000 Hz。 */
HAL_StatusTypeDef DDS_SetFrequency(uint32_t frequency_hz);

/** 设置当前通道幅度，范围 0~1023。幅度为 0 可用于关闭该通道的点频输出。 */
HAL_StatusTypeDef DDS_SetAmplitude(uint16_t amplitude);

/** 设置当前通道相位，范围 0~16383，对应 0~360 度。 */
HAL_StatusTypeDef DDS_SetPhase(uint16_t phase);

/** 配置点频：POINT 模式 -> 通道 -> 幅度 -> 相位 -> 频率。 */
HAL_StatusTypeDef DDS_ConfigPoint(uint8_t channel, uint32_t frequency_hz,
                                  uint16_t amplitude, uint16_t phase);

/**
 * 配置并开启一个通道的 2FSK。
 * 注意：f1/f2 的实际切换由 DDS 模块对应的 P1~P4 引脚电平控制。
 */
HAL_StatusTypeDef DDS_ConfigFsk2(uint8_t channel, uint32_t f1_hz,
                                 uint32_t f2_hz, uint16_t amplitude);

/** 配置并开启一个通道的重复扫频。time_ms 范围 1~9999 ms。 */
HAL_StatusTypeDef DDS_StartSweep(uint8_t channel, uint32_t start_hz,
                                 uint32_t end_hz, uint16_t start_amplitude,
                                 uint16_t end_amplitude, uint16_t time_ms,
                                 uint32_t step_hz);

/** 停止当前扫频模式的扫频输出。 */
HAL_StatusTypeDef DDS_StopSweep(void);

/** 关闭当前通道的点频输出（通过设置当前通道幅度为 0）。 */
HAL_StatusTypeDef DDS_OutputOff(void);

/** 上电链路测试：CH1 输出 1 kHz、幅度 512、相位 0；预期收到 6 个 OK。 */
HAL_StatusTypeDef DDS_DebugPointTest(void);

#endif /* DDS_AT_H */
