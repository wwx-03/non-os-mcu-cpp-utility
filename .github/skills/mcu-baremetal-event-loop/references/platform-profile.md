# 裸机平台契约（生成代码前必须填写）

本文件是生成或改造工程前的**硬门槛**。MCU、厂商库、启动文件、链接脚本和工具链都会
随平台变化，不能凭记忆或旧工程经验直接套用。

规则：

- 每一项都要有依据（器件手册、SDK 头文件、工程文件、实际编译日志）；
- **不确定就不许猜**：查不到就标 `TODO` 并向用户确认；
- 契约要落到工程里（例如 `docs/platform.md` 或 README），不能只停留在对话里。

## 1. 契约模板

复制下表，按目标工程填写。

| 契约项 | 取值 | 依据 / 备注 |
| --- | --- | --- |
| 架构模式 | Layered / DIP / Hybrid | 变体、测试、复杂度、团队、Flash/RAM |
| app 可见的抽象 | 具体 `Board` / 抽象 `Board` / 混合 | 由架构模式决定 |
| 具体板类名（DIP/Hybrid） | 例：`Stm32F407StartKitBoard` | `<MCU><BoardName><Version>Board` |
| 唯一选板点（DIP/Hybrid） | 例：`bsp/board_select.hpp` | 全工程只此一处 |
| app 的 include 是否含 `bsp/` | 否（DIP/Hybrid）/ 是（Layered） | 构建期强制 |
| MCU / 内核 | 例：STM32F407ZGTx / Cortex-M4F | 数据手册 |
| 位宽 / FPU / ABI | 32-bit / 单精度硬件 FPU / hard-float | 编译选项 |
| CMSIS 器件头 | 例：`stm32f4xx.h` | `port/mcu_cmsis.h` 的 include |
| 中断控制原语 | `__get_PRIMASK` / `__disable_irq` / `__set_PRIMASK` 是否可用 | 器件头 / `cmsis_gcc.h` |
| tick 来源 | SysTick 或 TIMx | 厂商初始化代码 |
| tick 频率 | 1 ms（`timer` 的前提） | `HAL_Init` / 时钟树 |
| tick 接入点 | 覆盖 `HAL_IncTick`，或直接在 1 ms 中断里调用 | 全工程只能接入一次 |
| HAL / LL 库 | 厂商与版本 | `Drivers/` 目录 |
| 中断优先级规则 | NVIC 分组、可抢占优先级、HAL 回调所在上下文 | 厂商文档 |
| 内存策略 | 仅静态；栈大小；是否启用堆 | 链接脚本 / `startup_*.s` |
| Flash / RAM 布局 | 起始地址与大小 | 链接脚本 / `.sct` |
| 启动文件 | GCC 还是 ARM 语法 | 与工具链一致 |
| 链接脚本 | `.ld` 或 scatter `.sct` | 厂商生成 |
| 工具链 | AC6 / GCC / 其它 | 工程来源决定，不擅自切换 |
| 工程文件 | `eide.yml` / `.uvprojx` / Makefile | 必须与工具链一致 |
| 验证命令 | build / size / flash | 见 `references/toolchains.md` |

### 1.1 架构模式（先选模式，再填其余契约）

裸机工程同样有 Layered / DIP / Hybrid 三种组织方式，规则与 RTOS 版一致：
抽象 `Board` 只含 `GetXxx()`，具体板命名为 `<MCU><BoardName><Version>Board`，
由唯一的 `bsp/board_select.hpp` 选定，`main()` 注入 `Application::Start(Board &)`。

| 场景 | 推荐 |
| --- | --- |
| 个人玩具、POC、单板、器件固定 | Layered |
| 裸机小工程、固定硬件 | Layered |
| 公司单板、小团队、短周期 | Layered / 轻量 Hybrid |
| 多板/多 MCU、器件可替换、PC 单测、长期维护 | Hybrid / DIP |
| Flash/RAM 极紧且无测试/多板需求 | Layered/Hybrid |

选定后必须记录：

- 模式；
- app 可见的抽象（具体 `Board` / 抽象 `Board` / 混合）；
- 具体板类名（DIP/Hybrid）；
- 唯一选板点；
- app 的 include 路径是否包含 `bsp/`；
- 逐模块依赖路径（Hybrid 必填）。

> ROM/RAM 只作为约束/否决项，不是主判据。用 `size` / `.map` 实测，
> 不要凭 MCU 型号猜。每个多态对象约多 4 B（vtable 指针）。

## 2. 逐项查证方法

### 2.1 MCU 与 CMSIS 头

- 打开 `port/mcu_cmsis.h`，确认它 include 的器件头与目标芯片一致；
- 在器件头里搜索 `__get_PRIMASK`、`__NVIC_PRIO_BITS`，确认中断原语与优先级位数；
- 换 MCU 时只改 `mcu_cmsis.h`，**不要改 `irq_lock.hpp`**。

### 2.2 tick

- 确认 1 ms tick 只有一个来源：`SysTick_Handler` 或 TIM 中断；
- 确认 `timer::OnMilliSecondTick()` **只被调用一次**；
- 如果 `Core/Src/*_it.c` 已经定义了 `SysTick_Handler`，不要重复定义，
  改为覆盖 HAL 的弱符号 `HAL_IncTick`（见 `references/porting.md`）。

### 2.3 中断与回调上下文

- HAL 的 `HAL_*_Callback` 默认运行在 ISR 上下文；
- 确认厂商生成的中断入口（UART、EXTI、TIM）实际调用了哪个回调；
- 回调里只允许"投递事件"：不能解析协议、不能阻塞、不能写 Flash；
- 中断优先级必须满足 HAL / 自身代码的假设，不要跨平台套用数值。

### 2.4 内存与链接

- 打开链接脚本，确认 Flash / RAM 起始地址与容量；
- 确认启动文件与工具链匹配（GCC 用 GCC 语法启动文件，AC6 用 ARM 语法）；
- 确认工程确实不依赖堆：`malloc` / `new` / STL 容器不应出现在应用代码里；
- 栈大小要按实际调用深度与 `printf` / 浮点使用情况留余量。

### 2.5 工具链与工程文件

- 从 Keil / MDK-ARM 导入的工程继续用 AC6 + `startup_*.s`（ARM 语法）+ EIDE 生成的 `.sct`；
- GCC 工程用 GCC + CMSIS GCC 启动文件 + 厂商 `.ld`；
- 两套工具链的编译参数互不识别，不能混用；
- 详细配置见 `references/toolchains.md`。

## 3. 常见"未确认就生成代码"的后果

| 未确认项 | 后果 |
| --- | --- |
| tick 单位 / 频率不是 1 ms | `timer` 周期整体偏移 |
| 重复定义 `SysTick_Handler` | 链接冲突或实际中断走错实现 |
| HAL 回调上下文判断错 | 在中断里解析协议 / 阻塞，系统卡死 |
| 中断优先级超出 HAL 假设 | 临界区失效、偶发死锁 |
| CMSIS 原语缺失却改 `irq_lock.hpp` | `utility` 层平台分叉 |
| 工具链与启动文件 / 链接脚本不匹配 | 链接失败或运行即 HardFault |
| 把仓库根目录加进 `srcDirs` 却没排除 `examples/**` | 范例 `.cpp` 被重复编译 |

**没有填完契约就开始写代码，视为未完成平台适配。**
