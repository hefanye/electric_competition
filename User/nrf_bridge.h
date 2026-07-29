#ifndef NRF_BRIDGE_H
#define NRF_BRIDGE_H

#include <stdint.h>

/**
 * 无线桥接模块 —— STM32 端 (TX)
 *
 * 数据流: PC 串口助手 -> USART1(115200) -> NRF24L01(TX) -> 无线 -> G3507(RX)
 *
 * 通信规则:
 *   - 在 PC 串口助手输入一行文字（以 \r 或 \n 结尾）
 *   - STM32 缓存到换行后，补齐 32 字节，一次性通过 NRF 发出
 *   - 同时回显到 PC 便于查看
 *
 * 调用方法（在 main.c 里手动加两行，见 README_NRF.txt）:
 *   NRF_Bridge_Init();        // 在所有 MX_xxx_Init() 之后
 *   while(1) { NRF_Bridge_Process(); }
 */
void NRF_Bridge_Init(void);
void NRF_Bridge_Process(void);

#endif /* NRF_BRIDGE_H */