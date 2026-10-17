# 变更记录

## 0002 — 2026-09-29

### 新增
- `main/mcu_cmsis.h`：MCU 桥接头，仅转发 `eg. #include "py32f0xx.h"`。让上层头文件通过它间接依赖当前 MCU 的 CMSIS，避免写死具体器件头文件。

### 修改
- `irq_lock.hpp`：
  - 改为 `#include "mcu_cmsis.h"`（此前未包含任何 CMSIS 头）。
  - 修复 `__get_PRIMASK()` / `__disable_irq()` / `__set_PRIMASK()` 未声明的问题——这些内建函数定义在 `cmsis_gcc.h`，需经厂商提供的头文件逐级引入。
  - 清理已不再需要的空实现兜底宏注释。

## 0003 — 2026-10-17

### 新增
- `timers.hpp`：header-only 软件定时器，由 `main/timer` 移植而来。
  - 只需在 1ms 中断中调用 `timer::OnMilliSecondTick()`，即可驱动所有已创建的定时器实例。
  - 支持周期/单次触发、运行中修改周期、启停与重置；无动态内存，实例可以是全局、静态或成员对象。
  - `start()` / `change_period()` 会重新计时；单次触发在回调前失活，因此可在回调内重新 `start()`。
  - 链表操作与状态修改均受 `irq_lock` 保护，可在主循环和中断间安全调用。
  - 回调运行于中断上下文，应保持简短；定时器对象的生命周期需覆盖其绑定期间，释放对象前请先调用 `destroy()`（析构函数会自动处理）。

## 0004 — 2026-10-17

### 新增
- `README.md`：库说明、快速开始、1ms tick 接入方式、工程模式概要，以及 Agent Skill 的三种接入方式。
- `.github/skills/mcu-baremetal-event-loop/`：Agent Skill，约定引入本仓库后必须沿用的工程模式。
  - `SKILL.md`：分层与依赖方向、三条铁律（中断只投递、静态内存、`irq_lock`）、`Application` 事件循环与投递链路、`Board`（BSP）板级资源契约、单例约定（`Application` / `Board` 只允许 `GetInstance()`）、API 速查、新增功能的标准流程、命名风格与反模式对照表。
  - `references/adapter-templates.md`：`mcu_cmsis.h`、tick 接入、`Board` 契约、`Application` 事件循环骨架、电平事件位图、队列容量与溢出策略。`Serial` / `Timer` 只作为接口命名示意，不提供实现。

## 0005 — 2026-10-17

### 修改
- `SKILL.md` / `references/adapter-templates.md`：中断原语的移植方式改为**在 `mcu_cmsis.h` 中补齐 `__get_PRIMASK` / `__disable_irq` / `__set_PRIMASK` 的同名转接**，不再建议修改 `irq_lock.hpp`，保证 `utility` 层跨平台不分叉。
- 删除"本工程已有一份 BSP 实现"这类依赖本工程范例的表述；改为平台无关的「板级驱动最小契约」（读 / 写 / 注册通知），并给出仅含声明的接口示意。

## 0006 — 2026-10-17

### 新增
- `examples/py32f002b_start_kit/`：完整可编译的裸机范例（PY32F002Bx5、USART1 115200、EIDE 工程）。
  - 演示 1ms tick 接入（`HAL_IncTick` → `timer::OnMilliSecondTick()`）、UART 中断接收 + `stream_buffer`、`queue` 事件循环、`Board` BSP 单例与 HAL 回调转发。
  - 只保留本范例需要的 CMSIS / HAL 文件；`queue.hpp` 等直接引用仓库根目录（EIDE `incList` 中的 `../../..`），不复制库代码。
- `.gitignore`：忽略构建产物与编辑器临时文件。

### 修改
- `README.md`：新增「范例工程」一节与目录结构说明；补充"只加 include 路径、不要加 srcDirs"的接入提示。
- `SKILL.md`：补充范例工程入口，说明其 BSP 实现仅作参考、换板仍按契约重写；补充 include 路径与 `srcDirs` 的区别。

## 0007 — 2026-10-17

### 修改
- 范例工程目录对齐 `SKILL.md` 的分层：`main/` 拆分为 `app/`（Application）、`bsp/`（Board + 串口驱动）、`port/`（`mcu_cmsis.h`），EIDE 的 `srcDirs` / `incList` 同步调整。
- 新增 `examples/README.md`：范例索引、目录约定、"作为新工程模板"的步骤。
- `SKILL.md` 新增「用范例生成工程骨架」：新建工程/换板时以 `examples/py32f002b_start_kit` 为模板复制 `app/`、`bsp/`、`port/` 三层，`Core/`、`Drivers/` 交由厂商工具生成；`description` 补充脚手架相关触发词。
- 范例与顶层 README 同步更新目录树、分层说明与代码导读路径。
- 范例关键文件补充角色注释（`app/application.hpp`、`bsp/board.hpp`、`bsp/serial.hpp`、`port/mcu_cmsis.h`），明确各层职责。

## 0008 — 2026-10-17

### 修改
- 范例 `app/`、`bsp/` 补全两处会导致硬件不工作的缺陷：把 `HAL_USART_RxCpltCallback` 改为与 `HAL_UART_*` API 匹配的 `HAL_UART_RxCpltCallback`；新增 `HAL_UART_ErrorCallback`，溢出等错误后重新挂起接收。
- 范例新增 IWDG 周期喂狗：`Application` 每 100 ms 投递 `WATCHDOG_REFRESH_EVENT`，`Board::RefreshWatchdog()` 调用 `HAL_IWDG_Refresh()`，与厂商 `Studio_IWDG_Init()` 对应。
- 范例 EIDE 工程对齐已验证工程的配置：`srcDirs` 加入 `port/` 与仓库根目录，`excludeList` 排除 `examples/**`；`incList` 顺序同步。
- `SKILL.md` 新增「EIDE 工程配置」：`srcDirs` / `excludeList`、`incList`、C/C++ 标准、链接参数与改完后的编译验证命令；反模式表补充 HAL 回调名、IWDG、EIDE 扫描三条。
- `references/adapter-templates.md`：tick 接入改为覆盖 `HAL_IncTick`（避免与厂商 `*_it.c` 里的 `SysTick_Handler` 重复定义）；检查清单补充回调名、IWDG 与 EIDE 排除项；新增 §9「EIDE 工程配置模板」。
- `README.md`、`examples/README.md`、范例 `README.md` 同步 EIDE 配置与喂狗说明。

## 0009 — 2026-10-17

### 新增
- `examples/stm32f407_start_kit/`：STM32F407ZGTx 范例，也是**从 Keil MDK 导入的工程模板**。
  - 与 PY32 范例同一套 `app/` / `bsp/` / `port/` 分层与事件循环模式，工具链改为 Keil **AC6（armclang）**。
  - 同时保留 `MDK-ARM/stm32f407_start_kit.uvprojx`（`uAC6=1`）与 `EIDE/`（`toolchain: AC6`、`useCustomScatterFile: false`），两套工程共用同一份源码。
  - 附 `stm32f407_start_kit.ioc`，可用 CubeMX 重新生成 `Core/`、`Drivers/`；只保留范例用到的 HAL 模块以便直接编译。
  - `Core/Src/retarget.c`：关闭 AC6 armlib 的 semihosting 依赖。
- `SKILL.md` 新增「Keil MDK 工程配置（AC6）」：`<pCCUsed>` + `<uAC6>1</uAC6>` 必须写在 `<Target>` 下、C++ 源文件用 `FileType 8` 加组、`IncludePath` 追加 `app/bsp/port/仓库根目录`、`-fno-exceptions` 与 `v6Rtti`、以及用 `UV4 -b` 读 `n Error(s)` 的验证方法。
- `SKILL.md`「用范例生成工程骨架」改为按工程来源选模板：Keil/MDK-ARM → `stm32f407_start_kit`，GCC/`.ld` → `py32f002b_start_kit`。

### 修改
- `SKILL.md`「EIDE 工程配置」新增「工具链必须沿用工程现状」：明确禁止把 Keil 导入工程改成 GCC（或反向），并给出 AC6 / GCC 两套编译选项表与各自的验证命令。
- `references/adapter-templates.md` §9 增加 AC6 版 `eide.yml` 模板与工具链一致性检查项；检查清单改为「用工程实际工具链验证」。
- `examples/README.md`：范例索引补充工具链列，模板选择按工程来源拆分。

## 0010 — 2026-10-17

### 新增
- `devices/` 层与 `Led` 抽象：`devices/led.hpp` 用 NVI 写法（`SetState`/`Toggle`/`GetState` 非虚，派生类只实现 `SetIo`），不依赖任何厂商头；`bsp/gpio_led.hpp` 提供 `GpioLed`，负责电平与极性。
- `SKILL.md` 新增「devices 层：器件抽象」：放什么/不放什么、NVI 骨架、何时不该用虚接口、静态对象构造时机、命名约定；分层图与依赖方向更新为 `app → bsp → devices → utility → port → CMSIS`。
- `references/adapter-templates.md` 新增 §8b「devices 层骨架」，检查清单补充 devices/HAL 隔离与静态构造项。
- 范例 `stm32f407_start_kit` 同步 `devices/`、`bsp/gpio_led.hpp`、`Board::GetLed()`，EIDE `srcDirs`/`incList` 与 Keil `IncludePath` 增加 `../devices`。

### 修改
- `Board` 成为 LED 的唯一入口：`GetLed(index)` 返回 `Led *`；`Board` 构造函数使能 GPIO 时钟、`HAL_GPIO_Init` 引脚后再确定初始电平；LED 对象改为 `board.cpp` 中命名空间作用域的静态对象（无 `__cxa_guard`）。
- 修正驱动职责：`GpioLed` 构造不再 `HAL_GPIO_Init`，析构删除 `HAL_GPIO_DeInit`（静态对象永不析构，只会拖入 `__cxa_atexit`）。
- `Board` 的常量去掉保留标识符前缀：`_SERIAL1_RX_SIZE` → `SERIAL1_RX_SIZE`。
- 应用层每秒翻转 LED 并输出 `Tick: <秒数>, dropped: <n>`；`Serial` 增加 `vsnprintf` 格式化发送重载，`Core/Src/retarget.c` 关闭 armlib 的 semihosting 依赖。

## 0011 — 2026-10-17

### 新增
- 范例 `stm32f407_start_kit` 增加**按键例子**：PA0 输入下拉、按下为高，每 25 ms 采样消抖，松开时串口输出 `Button0 released`。
  - `devices/button.hpp`：按键设备类（NVI）。`Start()` 记录初始电平并启动采样，检测到"按下 → 松开"时回调上层；回调运行在 1ms tick 上下文，因此只做事件投递。
  - `bsp/gpio_button.hpp`：`GpioButton`，`Read()` 读引脚并按 `active_high` 统一成"是否按下"。
  - `Board` 新增 `BUTTON0_INDEX` / `GetButton()`，构造中把 PA0 配为输入下拉，并在引脚就绪后调用 `button0.Start()`。
  - 应用侧新增 `BUTTON_RELEASED_EVENT`（载荷 `uint8_t index`）、`PostButtonReleasedEvent()`、`HandleButtonReleased()`：中断里只投递事件，串口输出在 handler 里完成。

### 修改
- `SKILL.md` 的「devices 层」改写为**「设备类怎么写」**：按"先判断值不值得抽象 → 基类用 NVI → 派生类只实现钩子 → 需要初值时用 `Start()` → 实例放在哪 / 何时构造 → 什么时候不用虚接口 → 命名"的顺序给出写法，并说明**不要在基类构造函数里调用纯虚函数**。
- `references/adapter-templates.md` 检查清单补充"基类构造函数不调纯虚函数"。
- 修正 `Button` 实现自身的缺陷：构造函数不再调用纯虚 `Read()`（UB），改为显式 `Start()`；`last_` 初始化，析构改为 protected 非虚。
- 范例 `stm32f407_start_kit` 同步按键实现与文档。
