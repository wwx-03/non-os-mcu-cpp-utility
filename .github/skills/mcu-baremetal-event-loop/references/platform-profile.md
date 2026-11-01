# 裸机平台契约（生成代码前必须填写）

本文件是生成或改造工程前的**硬门槛**。MCU、厂商库、启动文件、链接脚本和工具链都会
随平台变化，不能凭记忆或旧工程经验直接套用。

规则：

- 每一项都要有依据（器件手册、SDK 头文件、工程文件、实际编译日志）；
- **不确定就不许猜**：查不到就标 `TODO` 并向用户确认；
- 契约要落到工程里（例如 `docs/platform.md` 或 README），不能只停留在对话里。

## 1. 契约模板

复制下表，按目标工程填写。左列是 `SKILL.md`「第 0 步：读厂商基线」要读的项。

| 契约项 | 取值 | 依据 / 备注 |
| --- | --- | --- |
| MCU / 内核 | 例：PY32F002Bx5 / Cortex-M0+ | 数据手册 |
| 位宽 / FPU / ABI | 32-bit / 无 FPU / softfp | 编译选项 |
| CMSIS 器件头 | 例：`py32f0xx.h` | 写进 `main/mcu_cmsis.h` |
| 中断控制原语 | `__get_PRIMASK` / `__disable_irq` / `__set_PRIMASK` 是否可用 | 器件头 / `cmsis_gcc.h` |
| HAL / SDK 与版本 | 厂商与版本 | 工程里的 `Drivers/` |
| HAL 主头 | 例：`py32f0xx_hal.h` | 驱动头文件 include 它 |
| 板载设备清单 | 串口、按键、LED、看门狗… | 决定 `Board` 的成员与 `GetXxx()` |
| 外设句柄 | 变量名与类型，例：`husart1` / `UART_HandleTypeDef` | 厂商外设初始化文件 |
| 外设弱回调名 | 例：`HAL_UART_RxCpltCallback` | 与该外设使用的 API 对齐 |
| tick 来源 | SysTick 或 TIMx | 厂商初始化代码 |
| tick 频率 | 1 ms（`Timer` 的前提） | `HAL_Init` / 时钟树 |
| tick 接入点 | 已有的 1 ms 位置（SysTick 中断或 HAL tick 函数） | 全工程只能接入一次 |
| 程序入口 | 厂商 `main()` 与 `while (1)` 的位置 | `app_main()` 插在它们之间 |
| 中断优先级规则 | NVIC 分组、可抢占优先级、HAL 回调所在上下文 | 厂商文档 |
| 内存策略 | 仅静态；栈大小；是否启用堆 | 链接脚本 / `startup_*.s` |
| Flash / RAM 布局 | 起始地址与大小 | 链接脚本 / `.sct` |
| 启动文件 | GCC 还是 ARM 语法 | 与工具链一致 |
| 链接脚本 | `.ld` 或 scatter `.sct` | 厂商生成 |
| 工具链 | AC6 / GCC / 其它 | 工程来源决定，不擅自切换 |
| 工程文件 | `eide.yml` / `.uvprojx` / Makefile | 必须与工具链一致 |
| 验证命令 | build / size / flash | 见 `references/toolchains.md` |

## 2. 逐项查证方法

### 2.1 MCU 与 CMSIS 头

- 打开 `main/mcu_cmsis.h`，确认它 include 的器件头与目标芯片一致；
- 在器件头里搜索 `__get_PRIMASK`、`__NVIC_PRIO_BITS`，确认中断原语与优先级位数；
- 换 MCU 时只改 `mcu_cmsis.h`，**不要改 `irq_lock.hpp`**。

### 2.2 板载设备

- 逐个列出 `Board` 需要暴露的设备与它们的 `GetXxx()`；
- 记录每个设备使用的厂商句柄变量与初始化函数，`Board` 构造只保存句柄；
- 记录每个设备的弱回调名，由 `Board` 独占实现并按句柄 / 引脚路由；
- 板上有看门狗时，记下喂狗函数，`Board::RefreshWatchdog()` 用它。

### 2.3 tick

- 确认 1 ms tick 只有一个来源：`SysTick_Handler` 或 TIM 中断；
- 确认 `Timer::OnMilliSecondTick()` **只被调用一次**；
- 如果 `Core/Src/*_it.c` 已经定义了 `SysTick_Handler`，不要重复定义，
  改为追加到已有的 tick 函数（见 `references/porting.md`）。

### 2.4 中断与回调上下文

- 厂商 HAL 的 `HAL_*_Callback` 默认运行在 ISR 上下文；
- 确认厂商生成的中断入口（UART、EXTI、TIM）实际调用了哪个回调；
- 回调里只允许"投递事件"：不能解析协议、不能阻塞、不能写 Flash；
- 中断优先级必须满足 HAL / 自身代码的假设，不要跨平台套用数值。

### 2.5 内存与链接

- 打开链接脚本，确认 Flash / RAM 起始地址与容量；
- 确认启动文件与工具链匹配（GCC 用 GCC 语法启动文件，AC6 用 ARM 语法）；
- 确认工程确实不依赖堆：`malloc` / `new` / STL 容器不应出现在应用代码里；
- 栈大小要按实际调用深度与 `printf` / 浮点使用情况留余量。

### 2.6 工具链与工程文件

- 从 Keil / MDK-ARM 导入的工程继续用 AC6 + `startup_*.s`（ARM 语法）+ EIDE 生成的 `.sct`；
- GCC 工程用 GCC + CMSIS GCC 启动文件 + 厂商 `.ld`；
- 两套工具链的编译参数互不识别，不能混用；
- 详细配置见 `references/toolchains.md`。

## 3. 常见"未确认就生成代码"的后果

| 未确认项 | 后果 |
| --- | --- |
| tick 单位 / 频率不是 1 ms | `Timer` 周期整体偏移 |
| 重复定义 `SysTick_Handler` | 链接冲突或实际中断走错实现 |
| 弱回调名与实际 API 不匹配 | 中断进了回调但驱动收不到数据 |
| HAL 回调上下文判断错 | 在中断里解析协议 / 阻塞，系统卡死 |
| 中断优先级超出 HAL 假设 | 临界区失效、偶发死锁 |
| CMSIS 原语缺失却改 `irq_lock.hpp` | `utility` 层平台分叉 |
| 工具链与启动文件 / 链接脚本不匹配 | 链接失败或运行即 HardFault |

**没有填完契约就开始写代码，视为未完成平台适配。**