# AD9226 调试模块：无增益版本（STM32F407）

本目录在 `ad9226_debug.c/.h`（带增益版本）基础上，新增一个**去增益版本** `ad9226_debug_nogain.c/.h`，用于在加入直流偏置/运放跟随器模块后，直接评估 ADC 输入端的真实峰峰值，不做任何源端等效换算。

## 为什么需要这个版本

带增益版本（`ad9226_debug.c`）通过宏 `AD9226_SOURCE_TO_ADC_NUM / DEN` 把 ADC 输入端的 `pp_adc` 反推为源端等效值 `pp_dds_eq`，用于补偿 DDS 到 ADC 之间的已知衰减。当接入直流偏置模块（运放跟随器）后，信号通路衰减可忽略（实测约 2%），源端等效换算不再必要，反而会引入混淆。此时直接观察 `pp_adc` 即可反映 DDS 实际输出。

## 文件对比

| 文件 | 输出字段 | 增益宏 | 适用场景 |
| --- | --- | --- | --- |
| `ad9226_debug.c/.h` | `mean/min/max/pp/dc_est/pp_adc/pp_dds_eq` | `AD9226_SOURCE_TO_ADC_NUM/DEN`（默认 1/1） | 已知 DDS→ADC 衰减系数，需要换算源端幅值时使用 |
| `ad9226_debug_nogain.c/.h` | `mean/min/max/pp/dc_est/pp_adc` | 无 | 直流偏置/跟随器模块接入后，直接看 ADC 端实测值 |

两者接口完全一致：

```c
HAL_StatusTypeDef AD9226_Debug_Init(TIM_HandleTypeDef *htim, UART_HandleTypeDef *huart);
void AD9226_Debug_TimPeriodElapsedCallback(void);
void AD9226_Debug_Process(void);
```

切换时只需在 `main.c` 修改 `#include`，并在 Keil 工程中替换源文件，避免符号重复定义。

## 与带增益版本的差异（仅此而已）

1. 删除 `AD9226_SOURCE_TO_ADC_NUM` / `AD9226_SOURCE_TO_ADC_DEN` 宏定义。
2. 删除 `pp_dds_equiv_mv` 变量及其计算。
3. `snprintf` 输出格式去掉 `pp_dds_eq` 字段，仅保留 `pp_adc`。
4. 横幅标题标注 `(no gain)`，便于在串口输出中区分。

采样逻辑、中断回调、帧统计、UART 打印时序与带增益版本**完全一致**，不改变任何已验证的采样行为。

## 硬件接线

与带增益版本完全相同，详见上级 [README_AD9226.md](./README_AD9226.md)。

## 直流偏置模块接入后的实测结果

测试条件：DDS 输出 1 kHz / 560 mVpp 正弦，经直流偏置模块（运放跟随器，抬 1.65 V）接入 AD9226 A 通道。

```
=== AD9226 A-channel debug (no gain) ===
ACLK=100000 Hz (PSC=0 ARR=1679 CCR1=840), data=PE0..PE11, AD0=MSB
Tie A input to 0 V first: mean should be near 2048.

AD9226 A: frame=169 mean=1619 min=1518 max=1743 pp=225 dc_est=1047 mV pp_adc=549 mV
AD9226 A: frame=170 mean=1619 min=1518 max=1743 pp=225 dc_est=1047 mV pp_adc=549 mV
...
```

- `pp=225` 码，`pp_adc=549 mV`，相对 DDS 标称 560 mVpp 衰减约 2%。
- `mean≈1620`，对应 1.65 V 偏置点叠加少量直流失调，正常。
- 数据稳定，帧间波动小，表明 TIM1 触发采样与帧统计逻辑工作正常。

对比未接直流偏置模块时（DDS 直接接 AD9226 输入），`pp_adc` 仅约 200 mV，衰减达 64%。可见运放跟随器提供的高输入阻抗 / 低输出阻抗缓冲是必要的，AD9226 本身没有问题。

## 使用步骤

1. 在 Keil 工程中移除 `ad9226_debug.c`，添加 `ad9226_debug_nogain.c`。
2. 在 `main.c` 中将 `#include "ad9226_debug.h"` 改为 `#include "ad9226_debug_nogain.h"`。
3. 确认 `AD9226_DEBUG_ENABLE=1`、`INTERNAL_ADC_DEBUG_ENABLE=0`（互斥宏）。
4. Rebuild 并烧录。

仅作评估用途；正式项目仍建议使用 `ad9226_parallel.c/.h` 可复用驱动。
