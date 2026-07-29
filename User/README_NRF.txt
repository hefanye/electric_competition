# NRF24L01 无线桥接模块 —— STM32F407VGT6 发送端

## 文件说明
- `User/nrf24l01.h` / `nrf24l01.c`  —— NRF24L01+ SPI 驱动
- `User/nrf_bridge.h` / `nrf_bridge.c` —— 无线桥接逻辑（USART1 → NRF TX）
- 不会修改任何已有文件

## 集成步骤（需手动加 3 行到 main.c）

在 `main.c` 顶部加包含:
```c
#include "nrf_bridge.h"
```

在 `MX_USART1_UART_Init()` 之后、`while(1)` 之前加:
```c
NRF_Bridge_Init();
```

在 `while(1)` 里加（可以替换或并存已有模块调用，影响不大）:
```c
NRF_Bridge_Process();
```

## 注意
- NRF_Bridge 启动时会用 `HAL_UART_AbortReceive_IT` 接管 USART1，
  此时 `DAC8830_CLI` 串口命令行会失效（只 NRF 测试期间）。
- 把 `NRF_Bridge_Init()` 注释掉即可恢复原DAC8830_CLI工作。

## NOR接线

| NRF24L01 模块 | STM32F407VGT6 引脚 | 说明 |
|---------------|---------------------|------|
| VCC           | 3.3V                | 模块供电 |
| GND           | GND                 | 共地 |
| **CE**        | **PB6**             | 模式控制（新加GPIO） |
| **CSN**       | **PB5**             | SPI片选（新加GPIO） |
| **SCK**       | **PB3**             | SPI1 SCK（已有） |
| **MOSI**      | **PA7**             | SPI1 MOSI（已有，DAC8830 共享） |
| **MISO**      | **PB4**             | SPI1 MISO（已有） |
| **IRQ**       | **PB7**             | 中断信号（可选，新加GPIO） |

## 通信参数（需与 TI 端一致）

| 参数 | 值 |
|------|-----|
| 地址 | 0x34, 0x43, 0x10, 0x10, 0x01（5字节） |
| 通道 | 60（2.460GHz） |
| 速率 | 250 kbps |
| 功率 | 0 dBm |
| 包长 | 32字节固定 |
| CRC  | 1字节，自动ACK + 15次重传 |

## 使用方法

1. 打开 PC 串口助手，波特率 115200，连接 USART1（PA9=TX/PA10=RX）
2. STM32 启动后串口会显示: === NRF TX Bridge Ready ===
3. 在串口助手输入文字（如 "Hello"） + 回车
4. STM32 把字符补齐到32字节后通过 NRF24L01 发出
5. 成功: 回显 [NRF TX OK]；失败: 回显 [NRF TX FAIL]

## PSC/GPIO 无需改 .ioc
GPIO PB5/PB6/PB7 在 `nrf24l01.c` 的 `NRF24L01_Init()` 内
用 `HAL_GPIO_Init()` 寄存器级别自配，不需要改动 .ioc 也不需要重生成 CubeMX。

SPI1 复用现有 CubeMX 配置，无需修改。