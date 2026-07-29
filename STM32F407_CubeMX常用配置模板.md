# STM32F407VGT6 电赛常用 CubeMX 配置模板

适用范围：信号题、仪器仪表题、频率/幅值/相位测量、FFT、DDS 扫频、低频任意波输出。

本文只列出需要在 STM32CubeMX 主动配置的项目；没有写到的项目通常保持默认即可。不同模板按题目组合使用，不要为了“配置齐全”而把所有外设都开启。

---

## 1. 基础系统配置

### 1.1 SYS

在 `System Core > SYS` 中：

| 项目 | 配置 |
| --- | --- |
| Debug | `Serial Wire` |

保留 PA13（SWDIO）和 PA14（SWCLK），便于下载、断点和调试；不要把这两个引脚当普通 GPIO 使用。

---

## 2. 时钟配置

推荐统一将 STM32F407 运行在 168 MHz。这样 ADC、定时器、DMA 和算法运算都有足够余量。

### 2.1 使用内部 HSI 时钟（当前工程可直接采用）

在 `Clock Configuration` 中配置：

| 项目 | 值 |
| --- | --- |
| HSI | ON，16 MHz |
| PLL Source | HSI |
| PLLM | 8 |
| PLLN | 168 |
| PLLP | 2 |
| PLLQ | 4；若使用 USB，则改为 7 |
| System Clock Mux | PLLCLK |
| AHB Prescaler | `/1` |
| APB1 Prescaler | `/4` |
| APB2 Prescaler | `/2` |
| SYSCLK / HCLK | 168 MHz |
| PCLK1 | 42 MHz |
| PCLK2 | 84 MHz |
| Flash Latency | 5 |

计算关系：

```text
16 MHz / 8 × 168 / 2 = 168 MHz
```

当前配置下：

| 总线/定时器 | 时钟 |
| --- | ---: |
| HCLK | 168 MHz |
| APB1 外设 | 42 MHz |
| APB1 定时器（TIM2~TIM7） | 84 MHz |
| APB2 外设 | 84 MHz |
| APB2 定时器（TIM1、TIM8 等） | 168 MHz |

说明：当 APB 分频不等于 1 时，挂在该 APB 总线上的定时器时钟会自动乘 2。

如果需要 USB 48 MHz 时钟，PLLQ 必须设为 `/7`：

```text
16 MHz / 8 × 168 / 7 = 48 MHz
```

### 2.2 使用 8 MHz 外部 HSE 晶振时

仅在开发板确实有并且已正确启用 8 MHz 外部晶振时采用：

| 项目 | 值 |
| --- | --- |
| HSE | `Crystal/Ceramic Resonator` |
| PLL Source | HSE |
| PLLM | 8 |
| PLLN | 336 |
| PLLP | 2 |
| PLLQ | 7 |
| AHB / APB1 / APB2 | `/1`、`/4`、`/2` |

```text
8 MHz / 8 × 336 / 2 = 168 MHz
```

---

## 3. ADC + DMA 固定采样模板

这是 RMS、均值、过零测频、FFT、相位测量、滤波算法的共同底座。

硬件数据链路：

```text
TIM2 定时触发 → ADC1 转换 → DMA 写入循环缓冲区 → 主循环/任务处理数据
```

### 3.1 模板 A：单通道固定采样

适用：一路电压或电流测量、RMS、频率、FFT。

#### ADC1 参数

在 `Analog > ADC1 > Parameter Settings`：

| 项目 | 配置 |
| --- | --- |
| Resolution | 12 Bits |
| Data Alignment | Right Alignment |
| Scan Conversion Mode | Disable |
| Continuous Conversion Mode | Disable |
| Discontinuous Conversion Mode | Disable |
| Number Of Conversion | 1 |
| External Trigger Conversion Source | `TIM2 TRGO` |
| External Trigger Conversion Edge | Rising |
| DMA Continuous Requests | Enable |
| EOC Selection | End of single conversion |

在 `ADC Common`：

| 项目 | 配置 |
| --- | --- |
| Mode | Independent mode |
| Prescaler | `PCLK2 divided by 4` |
| DMA Access Mode | Disabled |

此时 ADC 时钟为：

```text
PCLK2 = 84 MHz
ADC Clock = 84 / 4 = 21 MHz
```

不要选 `PCLK2 divided by 2`，因为 42 MHz 超过 ADC 推荐工作上限。

#### ADC 通道和引脚

| 项目 | 配置 |
| --- | --- |
| GPIO Mode | Analog |
| Pull-up/Pull-down | No pull |
| Rank | Rank 1 |
| Sampling Time | 根据前端阻抗选择 |

采样时间选择：

| 模拟前端 | 建议 Sampling Time |
| --- | --- |
| 运放缓冲、低阻输出 | 3 / 15 Cycles |
| 普通分压、电阻网络 | 28 / 56 Cycles |
| 高阻分压、传感器 | 84 / 144 Cycles |
| 极高阻或精度异常 | 480 Cycles |

前端阻抗较高时不能盲目选择 3 Cycles，否则 ADC 采样电容充电不充分，测量值会偏低或不稳定。

#### ADC DMA 设置

在 `ADC1 > DMA Settings` 添加 DMA：

| 项目 | 配置 |
| --- | --- |
| Direction | Peripheral To Memory |
| Mode | Circular |
| Peripheral Increment | Disable |
| Memory Increment | Enable |
| Peripheral Data Width | Half Word |
| Memory Data Width | Half Word |
| Priority | Very High |
| FIFO Mode | Disable |

ADC 是 12 位数据，使用 Half Word。DMA 设为 Circular 后，采样不会在填满一次数组后停止。后续可利用半传输与全传输事件分别处理缓冲区前半段和后半段。

### 3.2 TIM2 采样定时器

在 `Timers > TIM2` 中：

| 项目 | 配置 |
| --- | --- |
| Clock Source | Internal Clock |
| Counter Mode | Up |
| Clock Division | No Division |
| Auto-reload Preload | Enable |
| Trigger Output TRGO | Update Event |
| NVIC | 不开启 TIM2 中断 |

TIM2 位于 APB1，在本时钟方案下的实际时钟为 84 MHz。

```text
Fs = 84,000,000 / ((PSC + 1) × (ARR + 1))
```

常用采样率：

| 采样率 Fs | PSC | ARR |
| ---: | ---: | ---: |
| 10 kHz | 83 | 99 |
| 20 kHz | 83 | 49 |
| 50 kHz | 83 | 19 |
| 100 kHz | 0 | 839 |
| 200 kHz | 0 | 419 |
| 1 MHz | 0 | 83 |

此模板由硬件完成触发和搬运；不需要开启 TIM2 中断。

### 3.3 模板 B：多通道扫描采样

适用：电压和电流同时采样、双通道相位差、多个传感器。

ADC1 修改项：

| 项目 | 配置 |
| --- | --- |
| Scan Conversion Mode | Enable |
| Number Of Conversion | 实际通道数 |
| External Trigger | TIM2 TRGO |
| DMA Continuous Requests | Enable |

各通道依次设置为 Rank 1、Rank 2、Rank 3…… DMA 缓冲区中的数据顺序为：

```text
CH1, CH2, CH3, CH1, CH2, CH3 ...
```

双通道例子：

```text
buf[0] = 电压第 1 点
buf[1] = 电流第 1 点
buf[2] = 电压第 2 点
buf[3] = 电流第 2 点
```

### 3.4 模板 C：低速直流测量

适用：电源电压、直流电流、温度、慢变化传感器。

| 项目 | 建议 |
| --- | --- |
| TIM2 触发频率 | 100 Hz 或 1 kHz |
| ADC Sampling Time | 56 / 84 Cycles |
| DMA 模式 | Circular |
| 每通道缓冲长度 | 16~64 点 |
| 数据处理 | 均值、中值或一阶低通 |

不要将单次 ADC 原始读数直接显示到串口屏。

### 3.5 模板 D：FFT 高速采集

适用：频谱分析、谐波分析、调制识别。

| 项目 | 建议 |
| --- | --- |
| ADC 时钟 | 21 MHz |
| Sampling Time | 前端允许时 3 / 15 Cycles |
| 采样率 | 100 kHz ~ 1 MHz |
| FFT 点数 | 256 / 512 / 1024 |
| DMA | Circular 或 Normal |
| 触发源 | TIM2 TRGO |

内置 ADC 不是示波器 ADC。高频测量前应有模拟抗混叠滤波；待分析的最高频率应明显低于采样率的一半。

---

## 4. TIM6 + DAC + DMA 任意波形模板

STM32F407 的 DAC 引脚：

| DAC 通道 | 输出引脚 |
| --- | --- |
| DAC_OUT1 | PA4 |
| DAC_OUT2 | PA5 |

### 4.1 TIM6 的正确配置

TIM6 是基本定时器，没有 GPIO 输出、输入捕获或 PWM 功能，因此 CubeMX 中只有 `Activated` 和 `One Pulse Mode` 等少量选项。它默认使用 APB1 的内部定时器时钟，不需要另外选择“内部时钟”。

在 `Timers > TIM6`：

| 项目 | 配置 |
| --- | --- |
| Activated | Enable |
| One Pulse Mode | Disable |
| Prescaler | 按目标 DAC 更新率计算 |
| Counter Mode | Up |
| Counter Period | 按目标 DAC 更新率计算 |
| auto-reload preload | Enable |
| Trigger Event Selection | `Update Event` |
| NVIC | 通常不启用 |

必须把 `Trigger Event Selection` 从 `Reset (UG bit from TIMx_EGR)` 改为 `Update Event`。只有这样 TIM6 溢出时才会通过 TRGO 触发 DAC。

TIM6 时钟为 84 MHz：

```text
DAC 更新率 = 84,000,000 / ((PSC + 1) × (ARR + 1))
```

例如 DAC 更新率设为 100 kHz：

| 项目 | 值 |
| --- | ---: |
| PSC | 0 |
| ARR | 839 |

```text
84 MHz / 840 = 100 kHz
```

### 4.2 DAC 输出固定电压

适用：可调偏置、阈值、控制参考电压、模拟控制量。

在 `Analog > DAC` 中：

| 项目 | 配置 |
| --- | --- |
| Channel | OUT1 或 OUT2 |
| Trigger | Software Trigger |
| Output Buffer | Enable |
| Wave Generation | Disable |
| DMA | 不开启 |

### 4.3 DAC + TIM6 + DMA 连续波形输出

适用：低频正弦波、三角波、锯齿波、任意波形、辅助控制曲线。

在 `DAC Channel 1`：

| 项目 | 配置 |
| --- | --- |
| Trigger | `TIM6 TRGO` |
| Output Buffer | Enable |
| Wave Generation | Disable |

在 DAC 的 `DMA Settings`：

| 项目 | 配置 |
| --- | --- |
| Direction | Memory To Peripheral |
| Mode | Circular |
| Peripheral Increment | Disable |
| Memory Increment | Enable |
| Peripheral Data Width | Half Word |
| Memory Data Width | Half Word |
| Priority | High 或 Very High |

数据链路：

```text
TIM6 Update Event → TRGO → DAC 触发 → DMA 送入下一个样点 → PA4/PA5 输出
```

波形频率关系：

```text
波形频率 = DAC 更新率 / 每周期样点数
```

例如，TIM6 更新率 100 kHz，正弦表使用 100 点，则输出为 1 kHz 正弦波。

注意：

- DAC 输出范围大约是 0~3.3 V，不能直接输出负电压。
- 要输出交流波形时，样点需要加中点偏置。
- DAC 输出后建议加缓冲运放和低通滤波器。
- 当前 AD9959 已可承担高质量、高频信号源任务；STM32 DAC 更适合作为辅助低频波形或控制电压输出。

---

## 5. 常用定时器模板

### 5.1 PWM 输出

适用：PWM 控制、方波激励、蜂鸣器、低速模拟控制量。

建议使用 TIM3/TIM4；要求更高精度时使用 TIM1。

以 TIM3 输出 20 kHz PWM 为例：

| 项目 | 值 |
| --- | --- |
| Clock Source | Internal Clock |
| Prescaler | 0 |
| Counter Period | 4199 |
| Channel Mode | PWM Generation CHx |
| Polarity | High |
| Fast Mode | Disable |

```text
Fpwm = TimerClock / ((PSC + 1) × (ARR + 1))
Duty = CCRx / (ARR + 1)
```

TIM3 时钟为 84 MHz：

```text
84 MHz / 4200 = 20 kHz
```

### 5.2 输入捕获测频

适用：方波频率、脉冲周期、脉冲宽度、转速测量。

优先用 TIM5，因为它是 32 位定时器，低频测量不容易溢出。

| 项目 | 配置 |
| --- | --- |
| Timer | TIM5 |
| Clock Source | Internal Clock |
| Channel | Input Capture Direct Mode |
| Polarity | Rising Edge |
| Selection | Direct TI |
| Input Filter | 初始为 0，有毛刺再增大 |
| NVIC | 开启 TIM5 中断 |

推荐先设为 1 us 计数分辨率：

```text
TIM5 Clock = 84 MHz
PSC = 83
Counter Clock = 1 MHz
```

相邻上升沿的捕获差值即为周期的微秒数。

### 5.3 PWM 输入模式

适用：同时测量外部 PWM 的频率和占空比。

| 项目 | 配置 |
| --- | --- |
| Combined Channels | PWM Input |
| Slave Mode | Reset Mode |
| Trigger Source | TI1FP1 或 TI2FP2 |
| NVIC | 开启 |

CubeMX 会自动配置两个捕获通道：一个记录周期，另一个记录高电平时间。

### 5.4 编码器模式

适用：旋转编码器、位置与转速测量。

| 项目 | 配置 |
| --- | --- |
| Timer | TIM3 或 TIM4 |
| Combined Channels | Encoder Mode |
| Encoder Mode | TI1 and TI2 |
| Counter Period | 65535 |
| IC1/IC2 Polarity | Rising Edge |
| IC1/IC2 Filter | 有抖动时提高 |

---

## 6. DMA 通用规则

| 外设 | 方向 | 模式 | 数据宽度 | 优先级 |
| --- | --- | --- | --- | --- |
| ADC | Peripheral To Memory | Circular | Half Word | Very High |
| DAC | Memory To Peripheral | Circular | Half Word | High / Very High |
| 串口屏 TX | Memory To Peripheral | Normal | Byte | Medium |
| 高速串口 RX | Peripheral To Memory | Circular | Byte | Medium |

原则：

1. ADC 和 DAC 的 DMA 优先级最高。
2. 串口屏、DDS 等低速控制通信不能影响采样。
3. 同一 DMA 控制器的 Stream 不能冲突；CubeMX 提示冲突时优先保证 ADC 和 DAC。
4. ADC/DAC 使用 DMA 时，通常不需要在定时器中断中处理每一个采样点。

---

## 7. UART 模板

### 7.1 当前工程串口分配

| 设备 | 外设 | 引脚 | 波特率 | 接收方式 |
| --- | --- | --- | ---: | --- |
| 淘金驰串口屏 | UART4 | PC10 TX、PA1 RX | 115200 | 中断 |
| 康威 AD9959 DDS | UART5 | PC12 TX、PD2 RX | 9600 | 中断 |

三者必须共地。若需要接收 DDS 的 `OK/ERROR`，DDS TX 必须接到 STM32 的 PD2。

### 7.2 串口屏 UART4

| 项目 | 配置 |
| --- | --- |
| Mode | Asynchronous |
| Baud Rate | 115200 |
| Word Length | 8 Bits |
| Parity | None |
| Stop Bits | 1 |
| Hardware Flow Control | None |
| NVIC | UART4 global interrupt Enable |

### 7.3 DDS UART5

| 项目 | 配置 |
| --- | --- |
| Mode | Asynchronous |
| Baud Rate | 9600 |
| Word Length | 8 Bits |
| Parity | None |
| Stop Bits | 1 |
| Hardware Flow Control | None |
| NVIC | UART5 global interrupt Enable |

DDS 使用 AT 指令，应严格遵守：

```text
发送一条命令 → 等待 OK/ERROR → 再发送下一条
```

---

## 8. SPI 模板

适用：外接 ADC、外接 DAC、Flash、数字电位器。

| 项目 | 配置 |
| --- | --- |
| Mode | Full-Duplex Master |
| Hardware NSS | Disable |
| NSS | Software |
| Data Size | 8 Bits |
| First Bit | MSB First |
| Baud Rate Prescaler | 初始从 /16 或 /32 开始 |
| CRC Calculation | Disable |
| DMA | 高速连续传输时开启 RX/TX DMA |

CPOL 和 CPHA 必须按外设芯片手册配置：

| SPI 模式 | CPOL | CPHA |
| --- | ---: | ---: |
| Mode 0 | Low | 1 Edge |
| Mode 1 | Low | 2 Edge |
| Mode 2 | High | 1 Edge |
| Mode 3 | High | 2 Edge |

---

## 9. I2C 模板

适用：INA219/INA226、ADS1115、EEPROM、温度和姿态传感器。

| 项目 | 配置 |
| --- | --- |
| Mode | I2C |
| Clock Speed | 100 kHz 起步；稳定后可 400 kHz |
| Addressing Mode | 7-bit |
| Dual Address Mode | Disable |
| General Call Mode | Disable |
| No Stretch Mode | Disable |

I2C 的 SCL 和 SDA 必须有上拉电阻，优先使用外部上拉。

---

## 10. GPIO 与 EXTI 模板

### 10.1 普通输出

适用：LED、继电器、模拟开关、量程切换、DDS 外部模式选择脚。

| 项目 | 配置 |
| --- | --- |
| GPIO Mode | Output Push Pull |
| Pull | No Pull |
| Speed | Low；高速翻转时选 High |
| 默认电平 | 设置为电路安全状态 |

### 10.2 外部中断 EXTI

适用：按键、过零检测、外部同步、比较器输出。

| 项目 | 配置 |
| --- | --- |
| GPIO Mode | External Interrupt Mode |
| Trigger | Rising / Falling / Both Edge，按信号选择 |
| Pull | 按电路选择 |
| NVIC | 开启对应 EXTI Line |

噪声较大的过零信号应先经过比较器、施密特触发器或硬件滤波。

---

## 11. 看门狗模板

比赛现场系统稳定后可启用 IWDG，防止程序异常卡死。

| 项目 | 建议 |
| --- | --- |
| 外设 | IWDG |
| Prescaler | 64 或 128 |
| Reload | 设为约 1~2 秒超时 |
| Window | Disable |

初期调试建议先不开；调试断点时看门狗会导致芯片复位。

---

## 12. NVIC 优先级建议

| 抢占优先级 | 中断 |
| ---: | --- |
| 0 | ADC DMA 半传输/全传输 |
| 1 | DAC DMA |
| 2 | 输入捕获、外部同步信号 |
| 3 | UART5 DDS 接收 |
| 4 | UART4 串口屏接收 |
| 5 | 普通按键、低速外设 |

原则：UART 中断只接收字节、放入缓冲区；FFT、RMS、屏幕刷新等耗时处理放在主循环或任务中完成。

---

## 13. 电赛常用组合

### 13.1 基础电压/频率表

```text
TIM2 → ADC1 → DMA 循环 → RMS / 均值 / 过零测频 → 串口屏
```

开启：时钟、ADC1、DMA、TIM2、UART4。

### 13.2 FFT 频谱分析

```text
TIM2 固定采样 → ADC DMA 双缓冲 → 窗函数 + FFT → 主频/幅值/谐波
```

开启：时钟、ADC1、DMA、TIM2、UART4。推荐从 100 kHz 采样率、1024 点 FFT 开始验证。

### 13.3 DDS 扫频测量

```text
串口屏 → UART4 → STM32 → UART5 → AD9959
TIM2 → ADC DMA → RMS / 相位 / 增益 → 频率特性数据
```

开启：UART4、UART5、TIM2、ADC1、DMA。

### 13.4 DAC 任意波形输出

```text
TIM6 TRGO → DAC → DMA Circular → PA4 / PA5
```

开启：DAC、TIM6、DMA。

### 13.5 双通道阻抗与相位测量

```text
TIM2 → ADC1 双通道扫描 → DMA → 电压/电流同步数据 → RMS/IQ 相位/阻抗
```

开启：ADC1 多通道扫描、DMA、TIM2、UART4；有 DDS 激励时再开启 UART5。

---

## 14. 推荐实际上板验证顺序

1. 配置 168 MHz 时钟，确认串口屏仍能显示 `READY`。
2. 配置 TIM2 + ADC1 + DMA 单通道，确认 DMA 数据连续且采样率正确。
3. 将滤波、均值、RMS、过零测频库接入 ADC 数据流。
4. 再增加双通道 ADC 扫描，验证电压/电流或双路相位数据。
5. 接入 AD9959 的 UART5，完成激励频率与采样测量闭环。
6. 最后再配置 DAC、FFT、扫频测量和频率特性绘制。

