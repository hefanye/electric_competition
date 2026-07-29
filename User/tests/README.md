# STM32F407 算法库实机测试

## 使用方式

ADC 统一由 `TIM2(20 kHz) -> ADC1(PA0) -> DMA(1000 点循环缓冲)` 采集；日志统一由 USART1(115200) 输出。

不要让每一个测试文件各自启动 ADC、DMA 或实现 `HAL_ADC_ConvCpltCallback()`：这些动作只在 `adc_baseline_test.c` 中进行。测试选择器每秒把一帧 ADC 数据交给当前测试，因此不同算法不会争抢 DMA 回调。

在 `Core/Src/main.c` 中修改这一行：

```c
#define ALGORITHM_TEST_SELECTED ALGO_TEST_ADC_BASELINE
```

例如下一项要验证移动平均，改成：

```c
#define ALGORITHM_TEST_SELECTED ALGO_TEST_MOVING_AVERAGE
```

重新编译、下载即可。当前选择 `ALGO_TEST_ADC_BASELINE`，正对应阶段 0 的 1.65 V 基线测试。

## 测试顺序与输入要求

| 顺序 | 选择宏 | 文件 | 是否要 DDS/屏幕 | 上位机设置/操作 | 通过关注点 |
|---:|---|---|---|---|---|
| 0 | `ALGO_TEST_ADC_BASELINE` | `adc_baseline_test.c` | 否 | PA0 仅接 1.65 V 偏置 | 均值接近 1.65 V、无削顶、记录噪声 |
| 1 | `ALGO_TEST_MOVING_AVERAGE` | `test_moving_average.c` | 否 | 保持偏置 | `out_std < raw_std` |
| 2 | `ALGO_TEST_MEDIAN` | `test_median.c` | 否 | 保持偏置；可人为引入一次脉冲干扰 | 固定脉冲样例正确；实际峰峰值通常下降 |
| 3 | `ALGO_TEST_ONE_POLE_LPF` | `test_one_pole_lpf.c` | 否 | 保持偏置 | 100 Hz 一阶低通后标准差下降 |
| 4 | `ALGO_TEST_DC_BLOCKER` | `test_dc_blocker.c` | 否 | 保持 1.65 V 偏置，等待 3 次输出 | 去直流后均值趋近 0 |
| 5 | `ALGO_TEST_BIQUAD_LOWPASS` | `test_biquad.c` | 是 | POINT 正弦 1000 Hz，经 1.65 V 偏置送 PA0 | 500 Hz 二阶低通后 RMS 显著下降 |
| 6 | `ALGO_TEST_STATISTICS` | `test_statistics.c` | 否 | 保持偏置 | 均值、极值、标准差和基线一致 |
| 7 | `ALGO_TEST_AMPLITUDE` | `test_amplitude.c` | 是 | POINT 正弦 1000 Hz，经偏置送 PA0 | 串口 RMS/PP 与示波器相符；正弦 crest 约 1.414 |
| 8 | `ALGO_TEST_FREQUENCY` | `test_frequency.c` | 是 | POINT 正弦 1000 Hz，经偏置送 PA0 | 频率接近 1000 Hz，quality 高 |
| 9 | `ALGO_TEST_PHASE_SYNTHETIC` | `test_phase.c` | 否 | 无 | 固定双通道样本输出约 500 mrad；实测相位需以后接第二同步 ADC 通道 |
| 10 | `ALGO_TEST_SIGNAL_QUALITY` | `test_signal_quality.c` | 可选 | 先偏置、后输入安全幅度正弦 | `usable=1`，不削顶 |
| 11 | `ALGO_TEST_STABILITY` | `test_stability.c` | 可选 | 保持输入不变，至少等 10 次输出 | `stable=1` |
| 12 | `ALGO_TEST_CALIBRATION_SYNTHETIC` | `test_calibration.c` | 否 | 无；后续用 DAC8830 记录 5 个实测点 | 合成拟合 PASS；实测校准表另行录入 |
| 13 | `ALGO_TEST_WINDOW_FUNCTIONS` | `test_window_functions.c` | 否 | 无 | Hann 五点为 0,0.5,1,0.5,0 |
| 14 | `ALGO_TEST_FFT_SPECTRUM` | `test_fft_spectrum.c` | 是 | POINT 正弦 1000 Hz，经偏置送 PA0 | 频谱峰值接近 1000 Hz |
| 15 | `ALGO_TEST_GOERTZEL` | `test_goertzel.c` | 是 | POINT 正弦 1000 Hz，经偏置送 PA0 | 1 kHz 幅值明显非零 |
| 16 | `ALGO_TEST_NCO_SYNTHETIC` | `test_nco.c` | 否 | 无 | RMS 约 0.707107 |
| 17 | `ALGO_TEST_SWEEP_PLANNER` | `test_sweep_planner.c` | 否 | 无 | 线性 100,200...1000；对数 10,100,1000 |
| 18 | `ALGO_TEST_AM_ENVELOPE_SYNTHETIC` | `test_am_envelope.c` | 否 | 无 | 合成调制度约 0.500 |
| 19 | `ALGO_TEST_FSK` | `test_fsk.c` | 可选 | 先看合成 TONE0；实测时屏幕设 FSK 1000/2000 Hz | 实测分别判为 tone0/tone1 |

## 接线与安全

- 所有外设 GND 必须共地。
- PA0 绝不能超过 `0~3.3 V`；DDS 交流信号必须先叠加 1.65 V 偏置并确认不削顶。
- DDS 幅度先调小，用示波器确认 ADC 输入范围后再测试。
- USART1 只用于电脑日志：`PA9 -> USB-TTL RXD`，GND 共地；USB-TTL 的 5 V 不接开发板。

## 当前限制

- 目前 ADC 只有 PA0 单通道，因此相位差实测尚不具备硬件条件；相位模块先用固定双通道样本验证数学实现。
- AM 包络和扫频规划器当前先以可预测合成数据验证算法；等需要真实 AM/扫频测量时，再把 DDS 控制和采集时序接入相应测试文件。
