/**
 ******************************************************************************
 * @file    dds_ui.h
 * @brief   串口屏 DDS 控制协议。
 *
 * 串口屏经 UART4 发来的单帧命令在此解析，再调用 dds_at 模块通过 UART5
 * 控制 AD9959。该文件不直接访问 UART 寄存器。
 ******************************************************************************
 */
#ifndef DDS_UI_H
#define DDS_UI_H

#include <stdint.h>

/** 初始化屏幕 DDS 控制状态。 */
void DDS_UI_Init(void);

/**
 * 处理一条来自串口屏的完整命令。
 * 返回 1 表示该命令属于 DDS；返回 0 表示交回原有屏幕业务处理。
 */
uint8_t DDS_UI_HandleScreenToken(const char *token);

/** 在主循环中周期调用，等待 DDS 的全部 OK/ERROR 回包并更新 tsta。 */
void DDS_UI_Process(void);

#endif /* DDS_UI_H */
