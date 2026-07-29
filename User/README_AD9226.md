# AD9226 双路并行 ADC 驱动（STM32F407）

本目录提供 AD9226 模块的低、中速并行采集封装，面向 STM32F407 仪器仪表原型验证。它不追求 AD9226 的 65 MSPS 满速；满速连续采集应使用 FPGA 或专用并行采集逻辑。

## 文件说明

| 文件 | 作用 |
| --- | --- |
| `ad9226_parallel.c/.h` | 可复用采集驱动：输出 ACLK、读取 12 位 GPIO 并行数据、位序转换、帧缓存与统计量。 |
| `ad9226_debug.c/.h` | A 通道串口调试示例：打印原始码统计、外部输入直流估计和峰峰值。 |

正式项目优先调用 `ad9226_parallel.c/.h`；`ad9226_debug.c/.h` 只用于接线、位序和量程验证。

## 已验证的 A 通道接线

| AD9226 模块 | STM32F407VGT6 | 说明 |
| --- | --- | --- |
| `ACLK` | `PA8 / TIM1_CH1` | 采样时钟输出。 |
| `AD0` ~ `AD11` | `PE0` ~ `PE11` | 12 位并行数据；本模块中 `AD0` 是 MSB。 |
| `GND` | `GND` | 必须共地。 |
| `+5V` | 稳定 +5 V 电源 | ADC 模块供电，不接 MCU 的 3.3 V。 |

先用示波器确认 AD0~AD11 的高电平约为 3.3 V；若为 5 V，必须先做高速 5 V→3.3 V 电平转换。

## CubeMX / 定时器配置

- `TIM1_CH1`：PWM 输出到 `PA8`，作为 ACLK。
- 推荐初始配置：`PSC=0`、`ARR=1679`、`Pulse=840`。TIM1 时钟为 168 MHz 时，ACLK 为 100 kHz、约 50% 占空比。
- 开启 `TIM1_UP_TIM10_IRQn`。
- `PE0~PE11`：GPIO Input、No Pull。
- USART1：仅调试示例需要，用于打印结果。

100 kHz 下 256 点仅覆盖 2.56 ms：测 1 kHz 信号足够；测 100 Hz 的峰峰值建议帧长至少 1000 点，推荐 2048 点。

## 最小使用方式

```c
#include "ad9226_parallel.h"

#define FRAME_N 2048U
static uint16_t samples[FRAME_N];
static AD9226_Handle ad9226_a;

void App_AD9226_Start(void)
{
    (void)AD9226_Init(&ad9226_a, &htim1, TIM_CHANNEL_1,
                      GPIOE, 0U, 1U, samples, FRAME_N);
    (void)AD9226_Start(&ad9226_a);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    AD9226_OnTimerUpdate(&ad9226_a, htim);
}

void App_AD9226_Process(void)
{
    AD9226_FrameStats stats;

    if (AD9226_IsFrameReady(&ad9226_a) != 0U)
    {
        (void)AD9226_ComputeFrameStats(samples, FRAME_N, &stats);
        AD9226_ReleaseFrame(&ad9226_a);
    }
}
```

不要在采样中断内执行 `printf`、FFT 或滤波；中断中只采样写入 RAM，帧完成后在主循环中处理。

## 原始码与外部输入电压

模块手册给出的 A/B 通道外部输入范围为 `-5 V ~ +5 V`，12 位原始码 `D` 的换算关系为：

```text
D = 2048 - Vin × 2048 / 5
Vin = (2048 - D) × 5 / 2048
Vpp = (Dmax - Dmin) × 5 / 2048
```

因此：0 V 对应约 2048；+5 V 接近 0；-5 V 接近 4095。`ad9226_debug.c` 中的 `mean/min/max/pp` 都是每一帧的原始数字码统计；`pp_adc` 是按上式计算出的模块外部模拟输入峰峰值。

## DDS 或其他信号源的衰减校准

`pp_adc` 是唯一直接可信的 ADC 输入端测量值。DDS 输出到模块输入之间可能有 50 Ω 负载、串联电阻、分压或接触损耗，不属于 AD9226 驱动本身。

若已经用示波器实测出：

```text
V_adc = V_source × A / B
```

可在 `ad9226_debug.c` 编译前覆盖：

```c
#define AD9226_SOURCE_TO_ADC_NUM A
#define AD9226_SOURCE_TO_ADC_DEN B
```

调试输出中的 `pp_dds_eq` 仅是按该校准系数反推的源端估计值；未校准时默认 `1/1`，与 `pp_adc` 相同。不要把某一台 DDS 的系数写进通用驱动。

## 已完成验证

- 高阻信号发生器输入时，0.5 Vpp、1.0 Vpp、2.0 Vpp 正弦信号的 `pp_adc` 能正确跟随实际输入。
- A 通道 12 位数据位序、原始码范围及 `-5 V~+5 V` 换算已验证。
- 当前 DDS 输出通路的实际衰减仍应以模块模拟输入端的示波器实测值单独校准。
