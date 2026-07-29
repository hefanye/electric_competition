# G题串口屏显示模块

本目录是正式工程的显示层：MCU 接收页面令牌后，使用串口屏的原生
`cle` / `add` 指令写入 Waveform 波形控件，同时更新测量数值和频谱峰值文本。

## 当前模拟数据

模拟数据由 `ui_demo_data.c` 在 MCU 内生成，便于先确认界面和曲线：

| 需求 | 模拟成分 | 显示含义 |
|---|---|---|
| Ua | 100 kHz 基波 + 200 / 300 kHz 谐波 | 复合周期信号 |
| Ub | 150 kHz 基波 + 300 / 450 kHz 谐波 | 最高频率仍不超过 500 kHz |
| U | 120 kHz 基波 + 240 / 360 kHz 谐波 | 表示经 1 MHz 干扰抑制后的恢复结果 |

这些点仅用于显示。后续真实算法得到数据后，用
`UI_Controller_SetWaveMeasurement()`、`UI_Controller_SetSpectrumMeasurement()`、
`UI_Controller_SetWaveSamples()` 覆盖即可，页面和通信协议不需要更改。

## 必须在串口屏工程建立的页面和控件

页面名称可自行定义；以下文本控件名称和 Waveform 组件 ID 必须与
`ui_hmi_map.h` 一致，否则只修改该映射文件即可。

| 页面用途 | 文本控件 | Waveform 组件 |
|---|---|---|
| 主页面 | `tsta`（设为全局） | 无 |
| 一个周期显示页 | `t_w1_upp`、`t_w1_urms`、`t_w1_f` | ID = 1，通道 0 |
| 三个周期显示页 | `t_w3_upp`、`t_w3_urms`、`t_w3_f` | ID = 1，通道 0 |
| 频谱显示页 | `t_sp1`、`t_sp2`、`t_sp3` | ID = 1，通道 0 |

注意：每个页面的 Waveform ID 在编辑器中可能不同。把真实 ID 填进
`ui_hmi_map.h` 的三个 `*_COMPONENT_ID` 宏后重新编译 MCU。

## 串口屏事件代码

按钮先发令牌、再执行 `page`。用于回到主页的按钮只需要 `page p_home`。

主页面 Ua / Ub / U 三个按钮的“弹起事件”分别写：

```text
prints "UI:REQ:UA",0
printh FF FF FF
page p_measure_menu
```

```text
prints "UI:REQ:UB",0
printh FF FF FF
page p_measure_menu
```

```text
prints "UI:REQ:U",0
printh FF FF FF
page p_measure_menu
```

“波形与数值”按钮进入 `p_wave_choice`；“电压频谱图与幅值”按钮进入
`p_spectrum`；一个周期和三个周期按钮分别进入 `p_wave1` 和 `p_wave3`。

三个显示页面的“后初始化事件”必须写以下代码，使页面已切换完成后 MCU 才开始
发送文本和曲线：

```text
// p_wave1 后初始化
prints "UI:PAGE:WAVE1",0
printh FF FF FF
```

```text
// p_wave3 后初始化
prints "UI:PAGE:WAVE3",0
printh FF FF FF
```

```text
// p_spectrum 后初始化
prints "UI:PAGE:SPECTRUM",0
printh FF FF FF
```

主页面 `p_home` 的后初始化事件：

```text
prints "UI:PAGE:HOME",0
printh FF FF FF
```

上电后 MCU 也会主动把 `tsta` 写成 `READY`；因此主页面 `tsta` 显示 READY
说明 UART4 从 MCU 到串口屏的发送链路正常。
