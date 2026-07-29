# STM32 算法库移植说明

此目录保存从 `D:\Algo_library` 复制到 STM32F407 工程的纯 C 算法源代码。它们不直接访问 GPIO、ADC、DMA、UART 或 HAL，因此可在不同 MCU 工程中复用。

## 目录

| 目录 | 内容 |
| --- | --- |
| `filter` | 直流阻断、移动平均、中值、一阶低通、通用二阶 Biquad。 |
| `measure` | 统计量、幅度、频率、相位、校准、稳定性和信号质量检测。 |
| `signal_process` | FFT、窗函数、Goertzel、NCO、频谱分析、FSK、AM 包络和扫频规划。 |

## 当前状态

- 所有 `.c` 文件已加入 Keil 工程的三个 `Algorithm/...` 分组。
- 三个算法目录已加入 Keil 头文件搜索路径。
- 当前没有接入 `main.c`，不会占用 ADC、DMA、定时器或改变现有串口屏/DDS 功能。
- 后续每次只在 `main.c` 中接入并验证一个模块，避免多模块同时调试。

## 后续验证顺序

1. ADC DMA 原始采样；
2. DC 阻断；
3. 统计量与 RMS；
4. 过零测频；
5. 移动平均、中值、一阶低通、Biquad；
6. FFT 与频谱；
7. Goertzel、FSK、相位等进阶模块。

