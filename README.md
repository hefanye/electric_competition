# STM32F407 + 串口屏控制 AD9959 DDS

本工程用于电赛信号源/仪器仪表原型：淘金驰串口屏通过 STM32F407VGT6 选择通道、输入参数并启动 DDS；STM32 再通过 UART AT 指令控制康威 AD9959 模块输出信号。

## 已实现功能

- 串口屏选择 CH1~CH4。
- POINT 点频：设置频率（Hz）和幅度（0~1023）。
- 2FSK：设置 f0、f1 和幅度。
- SWEEP：设置起始频率、终止频率、步进、扫描时间和幅度。
- 关闭当前选中通道。
- 一键复位：发送 `AT+RESET`，软件清空通道状态并显示 `READY`。
- 每一条 DDS AT 指令均等待模块返回 `OK` 或 `ERROR`；命令之间保留 300 ms 间隔，避免模块连续收命令时配置异常。
- DDS 配置成功后才刷新屏幕 `tsta` 状态；失败时显示对应错误位置。

## 硬件连接

| 设备 | STM32F407VGT6 | 串口参数 |
| --- | --- | --- |
| 淘金驰串口屏 | UART4：PC10(TX)、PA1(RX) | 115200, 8N1 |
| 康威 AD9959 AT 模块 | UART5：PC12(TX)、PD2(RX) | 9600, 8N1 |

三者必须共地。DDS 模块 RX 接 PC12；若要接收 `OK/ERROR`，DDS 模块 TX 必须接 PD2。

## 工程结构

| 路径 | 作用 |
| --- | --- |
| `User/screen_protocol.c` | UART4 串口屏收发、以 `FF FF FF` 作为帧结束符。 |
| `User/dds_at.c` | UART5 的 AD9959 AT 指令发送、`OK/ERROR` 接收解析。 |
| `User/dds_ui.c` | 屏幕命令解析、DDS 命令队列、状态与错误显示。 |
| `Core/Src/main.c` | 初始化屏幕和 DDS，并在主循环调用 `Screen_Process()` 与 `DDS_UI_Process()`。 |
| `prepare.ioc` | STM32CubeMX 配置。 |
| `MDK-ARM/prepare.uvprojx` | Keil MDK 工程文件。 |

## 串口屏到 STM32 的命令协议

每一帧末尾都必须发送三个字节 `FF FF FF`。

| 屏幕命令 | 含义 |
| --- | --- |
| `DDS:CH:1` ~ `DDS:CH:4` | 选择 DDS 通道。 |
| `DDS:POINT:<freq>,<amp>` | 配置当前通道点频。 |
| `DDS:FSK2:<f0>,<f1>,<amp>` | 配置当前通道 2FSK。 |
| `DDS:SWEEP:<start>,<end>,<step>,<time_ms>,<amp>` | 配置当前通道扫频。 |
| `DDS:STOP` | 关闭当前通道输出。 |
| `DDS:STATUS` | 请求刷新状态文本。 |
| `DDS:RESET` | 对 DDS 发送 `AT+RESET` 并清空软件状态。 |

### 屏幕控件约定

- 主页面：频率输入框 `t6`，幅度输入框 `t4`，状态框 `tsta`。
- 模式变量：全局数值变量 `mode_sel`，`0=POINT`、`1=FSK2`、`2=SWEEP`。
- 模式页面：`fsk0`、`fsk1`、`sw_start`、`sw_end`、`sw_step` 为文本输入框。
- 主页面后初始化事件建议发送 `DDS:STATUS`，让 `tsta` 显示 `READY` 或已启用的通道列表。

## 使用顺序

1. 用 CubeMX/Keil 打开工程，确认 UART4=115200、UART5=9600，下载程序。
2. 将串口屏工程下载到屏幕，屏幕波特率设为 115200。
3. 上电后主页面 `tsta` 应显示 `READY`。
4. 选择通道，输入参数，选择模式，再按“开”。
5. 等待 `tsta` 更新：显示通道号表示该通道已成功配置；`E_...` 表示 DDS 返回错误；`DDS_TIMEOU` 表示未收到回复。
6. 多通道依次重复设置。关闭某一路时，先选择该路，再按“关”。
7. 状态混乱或重新开始测试时，按“一键复位”。

## 重要说明

- AD9959 模块使用 AT 指令，指令行结尾是 `\r\n`，由 `dds_at.c` 自动补齐。
- 不能把多条 AT 命令一次性无间隔发送；本工程采取“发一条 → 等 OK/ERROR → 间隔 300 ms → 下一条”。
- `AT+RESET` 可能使 DDS 立即重启而来不及回 `OK`，软件对此命令不等待回复，固定等待 500 ms 后恢复为 `READY`。
- FSK 实际切换还需要按模块手册连接并驱动对应模式选择引脚；本工程负责设置 f0/f1 参数。
- 本仓库不提交 `axf/hex/o/d` 等 Keil 构建产物，也不提交个人的 `uvoptx/uvguix` 设置。

## Branches / 分支说明

| Branch | Purpose |
| --- | --- |
| `master` | Repository overview and the stable STM32F407 DDS + serial-screen project baseline. |
| `stm32f407-dds-screen` | STM32F407VGT6 project: Taobao/TJC serial screen controls the AD9959 AT-command DDS module. |
| `stm32f407-ad9226-parallel` | STM32F407 AD9226 parallel-ADC reusable capture driver and the verified A-channel debug example. |

Use the branch that matches the hardware task; do not mix generated build files between branches.
