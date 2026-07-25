# AD9226 双路并行 ADC 驱动（STM32F407）

`ad9226_parallel.c/.h` 是 AD9226 的低、中速并口采样封装。它负责输出采样时钟、在定时器更新中断中读取一次 GPIO 数据、完成模块特殊位序转换，并以帧形式交给上层算法处理。

本模块是 STM32F407 上的调试与仪器原型方案，不追求 AD9226 的 65 MSPS 满速；满速连续采样需要 FPGA 或专用并口采集逻辑。

## 当前已验证的接线（A 通道）

| AD9226 模块 | STM32F407VGT6 | 说明 |
| --- | --- | --- |
| `ACLK` | `PA8 / TIM1_CH1` | 采样时钟输出 |
| `AD0` ~ `AD11` | `PE0` ~ `PE11` | 12 位并行数据；模块的 AD0 是 MSB |
| `GND` | `GND` | 必须共地 |
| `+5V` | 稳定 5 V 电源 | 给 ADC 模块供电，不接 MCU 3.3 V |

先用示波器确认数据高电平约为 3.3 V，才可直接接 F407 GPIO；若是 5 V，必须经过高速 5 V→3.3 V 电平转换。

模块模拟输入固定为 50 Ω。信号源负载应设为 50 Ω；若信号源没有该选项，以 ADC 输入 SMA 处的示波器实测幅度为准。

## CubeMX 配置

- `TIM1 / CH1 PWM`：PA8 输出 ACLK。
- 100 kHz ACLK：`PSC=0`、`ARR=1679`、`Pulse=840`。
- 开启 `TIM1_UP_TIM10_IRQn`。
- PE0~PE11 配为 GPIO Input、No pull。
- 用户代码中必须把 `HAL_TIM_PeriodElapsedCallback()` 转交给驱动。

## 最小使用方式

```c
#include "ad9226_parallel.h"

#define FRAME_N  2048U
static uint16_t samples[FRAME_N];
static AD9226_Handle ad9226_a;

AD9226_Init(&ad9226_a, &htim1, TIM_CHANNEL_1,
            GPIOE, 0U, 1U, samples, FRAME_N);
AD9226_Start(&ad9226_a);

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    AD9226_OnTimerUpdate(&ad9226_a, htim);
}

if (AD9226_IsFrameReady(&ad9226_a))
{
    AD9226_FrameStats stats;
    AD9226_ComputeFrameStats(samples, FRAME_N, &stats);
    AD9226_ReleaseFrame(&ad9226_a);
}
```

禁止在采样中断内 `printf` 或运行 FFT；帧完成后在主循环中处理。

## 帧长选择

采样帧至少应覆盖一个完整的待测最低频率周期：

`frame_length >= sample_rate / lowest_frequency`

例如 100 kSPS 下测 100 Hz，至少需要 1000 点，建议 2048 点；256 点只覆盖 2.56 ms，不能用来稳定计算 100 Hz 信号的峰峰值。

## 已完成实测

在 100 kHz ACLK、A 通道、信号源设置为 50 Ω 负载的条件下，已验证：

- 0.5 Vpp / 1 kHz：约 0.48~0.50 Vpp；
- 1.0 Vpp / 1 kHz：约 0.95~0.99 Vpp；
- 2.0 Vpp / 1 kHz：约 1.88~1.95 Vpp；
- 100 Hz~2 kHz 输入均可正常采样；
- 没有持续满码/零码，数据位序与幅度换算正确。

`ad9226_debug.c/.h` 是串口打印验证示例；正式工程可直接使用 `ad9226_parallel.c/.h`。
