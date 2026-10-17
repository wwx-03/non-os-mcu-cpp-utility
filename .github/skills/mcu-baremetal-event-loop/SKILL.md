---
name: mcu-baremetal-event-loop
description: '编写或改造裸机 MCU（Cortex-M / STM32 / PY32 / 嵌入式 C++）代码时使用本仓库（queue.hpp、stream_buffer.hpp、timers.hpp、irq_lock.hpp）已有的工程模式：中断只投递事件、由 Application 的事件循环统一派发、BSP 层 Board 统一管理板级资源、静态内存、irq_lock 临界区、1ms 软件定时器。新建工程、换板、生成工程骨架时，以仓库自带范例 examples/py32f002b_start_kit 为模板复制 app/bsp/port 三层再替换实现。Use when adding peripherals, events, timers, UART protocols, drivers, BSP/board ports, scaffolding a new bare-metal project, configuring its EIDE project (eide.yml: srcDirs, incList, C++17/no-exceptions/no-rtti, keep the existing toolchain AC6/GCC, linker flags), or porting this library into an existing one, and generate code that follows these patterns instead of inventing new ones.'
argument-hint: '要新增的外设 / 事件 / 定时任务，例如"加一个按键长按事件"'
---

# 裸机事件驱动工程模式（non-os-mcu-cpp-utility 仓库）

本仓库是 header-only 的 C++17 工具库，配套一套固定的工程骨架。**只要工程引用了本仓库的头文件，新增代码就必须沿用下面的模式**，不要引入 RTOS、动态内存或另一套事件机制。

仓库自带一个可直接编译烧录的完整范例 [`examples/py32f002b_start_kit/`](../../../examples/py32f002b_start_kit/README.md)，新建工程或换板时以它为模板（见「用范例生成工程骨架」）。注意其中的 `Board` / `Serial` 是**某块板子的 BSP 产物**，换板时仍要按本文契约重写，不要直接复制。

## 何时使用

- 工程中包含 `queue.hpp` / `stream_buffer.hpp` / `timers.hpp` / `irq_lock.hpp`
- 要新增外设、事件、定时任务、串口协议处理
- 要把厂商 HAL（CubeMX、PY32 HAL 等）工程改造成本模式
- 要把本仓库移植到新的 MCU
- **要新建工程 / 换一块板子 / 从零搭框架** —— 直接以仓库范例为模板生成，见下一节

## 用范例生成工程骨架

仓库自带两个完整范例，**按工程原有工具链挑一个**：

| 范例 | 工具链 / 工程 | 适用场景 |
|---|---|---|
| [`examples/stm32f407_start_kit/`](../../../examples/stm32f407_start_kit/README.md) | Keil **AC6**（armclang）+ `MDK-ARM/*.uvprojx`，EIDE 用 `toolchain: AC6` | 从 Keil MDK / CubeMX MDK-ARM 导入的工程（**最需要保留 Keil 配置的那种**） |
| [`examples/py32f002b_start_kit/`](../../../examples/py32f002b_start_kit/README.md) | **GCC**（arm-none-eabi）+ `.ld` 链接脚本 | CubeMX 生成 Makefile、或工程里已有 `*.ld` |

当用户要求"新建工程 / 换板 / 搭框架"时，**以对应范例为模板生成目录结构**，不要凭空发明：

```
<project>/
├── app/            # 从范例复制：Application 单例、事件号、handler
├── bsp/            # 从范例复制：Board 单例、驱动、HAL 回调转发
├── devices/        # 从范例复制：与 MCU 无关的器件抽象（不 include 任何厂商头）
├── port/           # 从范例复制：mcu_cmsis.h（换成目标器件的 CMSIS 头）
├── Core/           # 厂商工具生成：时钟、外设初始化、中断入口
├── Drivers/        # 厂商工具生成：CMSIS + HAL
├── MDK-ARM/        # Keil 工程（AC6 工程才有）
└── <IDE 工程目录>  # EIDE / Keil / Makefile 等
```

生成步骤：

1. 复制对应范例的 `app/`、`bsp/`、`devices/`、`port/` 四个目录（**不要复制** `Core/`、`Drivers/`、`EIDE/`，它们必须由目标 MCU 的厂商工具生成）。
2. 替换 `port/mcu_cmsis.h` 的器件头；确认平台提供 `__get_PRIMASK` / `__disable_irq` / `__set_PRIMASK`，缺失时在该头里补齐转接。
3. 按目标板重写 `bsp/board.*` 与驱动（串口、按键、LED…），保持 `Board` 单例与"读 / 写 / 注册通知"契约；`devices/` 只保留与 MCU 无关的抽象，板级细节一律留在 BSP。范例里的 `usart.h` / `iwdg.h` 是厂商文件，换板时换成新工程的初始化头；目标工程没有 IWDG 时删掉 `RefreshWatchdog()` 与喂狗定时器。
4. 修改 `app/application.*` 的事件号、`QueueItem` 载荷与 handler。
5. 在 1ms 中断里调用 `timer::OnMilliSecondTick()`。
6. 工程配置：**先确认工程原有工具链并沿用**（Keil / CubeMX 导入的 MDK-ARM 工程用 AC6，GCC 工程用 GCC），再按下面「EIDE 工程配置」设置 `srcDirs` / `incList` / C++17 选项与链接参数，改完必须用该工具链实际编译一次。

生成的代码必须同时满足下面的铁律、分层与命名约定；范例只是骨架，业务逻辑要按需求重新实现。

## EIDE 工程配置

EIDE 工程目录固定为 `<project>/EIDE`，`app/`、`bsp/`、`port/` 与厂商生成的 `Core/`、`Drivers/` 平级。所有配置都在 `<project>/EIDE/.eide/eide.yml` 里，**直接改这个文件**，不要只靠 UI 点击。

### 工具链必须沿用工程现状

EIDE 只是工程壳，**工具链由工程来源决定，不要擅自替换**：

| 工程来源（判断依据） | `toolchain` | 启动文件 | 链接脚本 |
|---|---|---|---|
| Keil MDK / CubeMX 生成的 `MDK-ARM/*.uvprojx` | `AC6`（Keil armclang） | `../MDK-ARM/startup_*.s`（ARM 语法） | `useCustomScatterFile: false`，EIDE 按 `storageLayout` 生成 `.sct` |
| CubeMX 生成 Makefile，或工程里已有 `*.ld` | `GCC`（arm-none-eabi-gcc） | CMSIS `Source/Templates/gcc/startup_*.s` | 厂商 `.ld`，`useCustomScatterFile: true` |

- 从 Keil 导入的工程改成 GCC，等于同时换掉启动文件、链接脚本和 HAL 编译选项；用户还要继续用 Keil 维护，**不要做**。
- 反过来，GCC 模板工程也不要改成 AC6。只有用户明确要求换工具链时才切换，并且启动文件、链接脚本、验证方式要一起换。
- `storageLayout` 按芯片真实内存填写（例：STM32F407ZG 为 `IRAM 0x20000000 + 0x20000`、`IROM 0x08000000 + 0x100000`），不要照抄模板里的近似值。

### 源目录与排除

```yaml
srcDirs:
  - ../app
  - ../bsp
  - ../port
  - ../non-os-mcu-cpp-utility                 # 仓库根目录：header-only，只为了让头文件出现在工程树
targets:
  Debug:
    excludeList:
      - ../non-os-mcu-cpp-utility/examples    # 必须排除，否则范例的 .cpp 会被一起编译
```

- 仓库根目录可以放进 `srcDirs`，但**必须同时用 `excludeList` 排除 `examples/**`**；不想这样做就只保留在 `incList`。
- `Core/`、`Drivers/`、`startup_*.s` 用 `virtualFolder` 引用，不要复制。
- `port/` 里只有 `mcu_cmsis.h`，加进 `srcDirs` 只是方便查看，不会有编译产物。

### include 路径与宏

```yaml
targets:
  Debug:
    cppPreprocessAttrs:
      defineList:
        - PY32F002Bx5
        - USE_HAL_DRIVER
      incList:
        - ../Core/INC
        - ../Drivers/CMSIS/Include
        - ../Drivers/PY32F002B_HAL_Driver/Inc
        - ../Drivers/CMSIS/Device/PY32F0xx/Include
        - ../app
        - ../bsp
        - ../non-os-mcu-cpp-utility
        - ../port
```

`../port` 不能漏：`irq_lock.hpp` 通过它找到 `mcu_cmsis.h`。

### 编译与链接选项（AC6 / Keil armclang）

| 字段 | 值 | 原因 |
|---|---|---|
| `toolchain` | `AC6` | 与 MDK-ARM 工程保持一致 |
| `toolchainConfigMap.AC6.cpuType` / `floatingPointHardware` | `Cortex-M4` / `single` | 按器件填写 |
| `c/cpp-compiler.language-c` | `c99` | 与 CubeMX 生成时保持一致 |
| `c/cpp-compiler.language-cpp` | `c++17` | `timers.hpp` 使用 inline 静态成员 |
| `c/cpp-compiler.CXX_FLAGS` | `-fno-exceptions -fno-rtti` | 裸机不引入异常与 RTTI |
| `c/cpp-compiler.one-elf-section-per-function` | `true` | 配合链接期裁剪 |
| `c/cpp-compiler.warnings` | `ac5-like-warnings` | 容忍厂商 HAL 的告警 |
| `linker.misc-controls` | `--diag_suppress=L6329` | 抑制 armlink 的 RWX 段告警 |
| `useCustomScatterFile` | `false` | 由 EIDE 按 `storageLayout` 生成 `.sct` |

**不要**把 GCC 专用参数（`--specs=nosys.specs`、`-Wl,...`、`-Wno-register`）或 GCC 的配置键（`global.no-exceptions`、`global.misc-control` 等）套到 AC6 上；反过来，AC6 的 `CXX_FLAGS` / `misc-controls` 也不要写进 GCC 工程——两套参数互不识别。

### 编译与链接选项（GCC）

| 字段 | 值 | 原因 |
|---|---|---|
| `toolchain` | `GCC` | 仅用于 GCC 工程 |
| `c/cpp-compiler.language-c` | `c11` | 厂商 HAL 源码 |
| `c/cpp-compiler.language-cpp` | `c++17` | `timers.hpp` 使用 inline 静态成员 |
| `global.no-exceptions` / `global.no-rtti` | `true` | 裸机不引入异常与 RTTI |
| `global.one-elf-section-per-function/-data` | `true` | 配合链接期裁剪 |
| `global.misc-control` | `--specs=nosys.specs --specs=nano.specs` | 无系统调用、精简 libc |
| `global.LD_FLAGS` | `-Wl,--no-warn-rwx-segments` | 消除 binutils ≥ 2.39 的 RWX 段告警 |
| `linker.remove-unused-input-sections` | `true` | `--gc-sections` |
| `useCustomScatterFile` / `scatterFilePath` | `true` / 厂商 `.ld` | 由 MCU Studio 生成，不要手写 |

GCC 的 `no-exceptions` / `no-rtti` / `one-elf-section-*` 属于 `global` 组，放在 `c/cpp-compiler` 下会被 EIDE 忽略。

`uploadConfigMap` 与 `.vscode/tasks.json` 里的 ELF 路径必须与 EIDE 输出名（`build/<target>/<name>.elf`）一致。

### 改完必须验证

**用工程自己的工具链验证**，不要拿另一套工具链的编译结果代替：

- **AC6 工程**：在 EIDE 里执行 build（或直接跑 `unify_builder -p build/<target>/builder.params --rebuild`），日志必须出现 `build successfully !`；同时核对 `build/<target>/<name>.sct` 的 RAM/ROM 与芯片一致。手动单文件验证用：

  ```powershell
  armclang --target=arm-arm-none-eabi -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard `
    -c -std=c++17 -fno-exceptions -fno-rtti -DUSE_HAL_DRIVER -DSTM32F407xx `
    -I../Core/Inc -I../Drivers/CMSIS/Include -I../Drivers/STM32F4xx_HAL_Driver/Inc `
    -I../app -I../bsp -I../non-os-mcu-cpp-utility -I../port `
    ../app/application.cpp -o application.o
  ```

- **GCC 工程**：按 `eide.yml` 的同一组参数手动编译一次：

  ```powershell
  arm-none-eabi-g++ -std=c++17 -fno-exceptions -fno-rtti -Wno-register `
    -mcpu=cortex-m0plus -mthumb -DPY32F002Bx5 -DUSE_HAL_DRIVER `
    -I../Core/INC -I../Drivers/CMSIS/Include -I../Drivers/PY32F002B_HAL_Driver/Inc `
    -I../Drivers/CMSIS/Device/PY32F0xx/Include -I../app -I../bsp `
    -I../non-os-mcu-cpp-utility -I../port `
    -c ../app/application.cpp -o application.o
  ```

  链接阶段再补 `-T<链接脚本> --specs=nosys.specs --specs=nano.specs -Wl,--gc-sections`。

完整 `eide.yml` 片段（GCC 与 AC6 两版）见 [references/adapter-templates.md](./references/adapter-templates.md) §9。

## Keil MDK 工程配置（AC6）

从 Keil 导入的工程通常**同时保留 `MDK-ARM/*.uvprojx`**，改代码后用户还要用 Keil 打开，所以这份工程必须一起改好，不能只改 EIDE。

### 1. 确认真的切到了 AC6

编译器选择写在 `<Target>` 下、`<TargetOption>` **之外**：

```xml
<Target>
  <TargetName>stm32f407_start_kit</TargetName>
  <ToolsetNumber>0x4</ToolsetNumber>
  <ToolsetName>ARM-ADS</ToolsetName>
  <pCCUsed>6140001::V6.14.1::ARMCLANG</pCCUsed>
  <uAC6>1</uAC6>
  <TargetOption>
    <TargetCommonOption>
      ...
```

- `<uAC6>1</uAC6>` 放进 `<TargetCommonOption>` **无效**——µVision 仍会用 AC5，随后因 `-fno-exceptions` 报 `C3900U: Unrecognized option`。
- `<pCCUsed>` 的版本号与本机 AC6 不一致时，µVision 首次打开会自动修正，不必手改。

### 2. 把 C++ 源文件加进工程

`Groups` 里新建一个组，C++ 源文件用 `FileType 8`：

```xml
<Group>
  <GroupName>Application/App</GroupName>
  <Files>
    <File>
      <FileName>application.cpp</FileName>
      <FileType>8</FileType>
      <FilePath>../app/application.cpp</FilePath>
    </File>
    <File>
      <FileName>board.cpp</FileName>
      <FileType>8</FileType>
      <FilePath>../bsp/board.cpp</FilePath>
    </File>
  </Files>
</Group>
```

### 3. include 路径与编译选项

```xml
<Cads>
  <v6Lang>5</v6Lang>            <!-- C++ 标准：沿用 CubeMX 生成值；本机 AC6 6.14 下 C++17 inline 静态成员可编译 -->
  <v6LangP>3</v6LangP>
  <v6Rtti>0</v6Rtti>
  <VariousControls>
    <MiscControls>-fno-exceptions</MiscControls>
    <Define>USE_HAL_DRIVER,STM32F407xx</Define>
    <IncludePath>../Core/Inc;../Drivers/STM32F4xx_HAL_Driver/Inc;../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy;../Drivers/CMSIS/Device/ST/STM32F4xx/Include;../Drivers/CMSIS/Include;../app;../bsp;../port;../non-os-mcu-cpp-utility</IncludePath>
  </VariousControls>
</Cads>
```

- `IncludePath` 必须追加 `../app;../bsp;../port;../non-os-mcu-cpp-utility`，否则 `board.hpp`、`queue.hpp`、`mcu_cmsis.h` 找不到。
- `<v6Rtti>0</v6Rtti>` 关闭 RTTI；异常用 `<MiscControls>-fno-exceptions</MiscControls>` 关闭。
- 工程里若还有 `-Wno-register`、`--specs=*`、`-Wl,*` 等 GCC 选项，一并删掉。

### 4. 验证

```powershell
& "C:\Keil_v5\UV4\UV4.exe" -b <project>\MDK-ARM\<name>.uvprojx -j0 -o build.log
```

构建日志必须满足：

- 首部 `C Compiler: Armclang.exe ...`（不是 `Armcc.exe`）；
- 结尾 `0 Error(s)`，且生成 `MDK-ARM/<name>/<name>.axf`。

`C:\Keil_v5\UV4\UV4.exe -b` 的退出码为 0 不一定代表构建成功（有 warning 时也是 0），**以日志里的 `n Error(s)` 为准**。

## 三条铁律

1. **中断只做三件事**：清标志、搬数据、投递事件（`push`）。禁止在 ISR 里延时、阻塞发送、`printf`、跑业务循环。
2. **静态内存**：禁止 `new` / `malloc` / `std::vector` / `std::string` / `std::function` / 智能指针。对象只能是全局、函数内 `static` 或成员。
3. **共享可变状态一律用 `irq_lock`**：临界区只包住几条语句，不要在里面调用 HAL 阻塞接口。

## 分层与依赖方向

```
core/     startup、链接脚本、时钟初始化（厂商生成，只读）
drivers/  厂商 HAL（只读）
port/     mcu_cmsis.h —— 唯一的 MCU 依赖点
utility/  本仓库：queue / stream_buffer / timers / irq_lock（不含 MCU 类型）
devices/  器件抽象：Led、按键、传感器……只描述行为，**不含任何厂商头**
bsp/      板级支持包：Board —— 板级资源与 HAL 回调的唯一入口，按板子实现
app/      Application —— 事件循环唯一实现位置
```

依赖方向只能是：

```
app → bsp → devices → utility → port → CMSIS
```

- 应用层只认识 `Board` 暴露的接口，绝不直接 `#include` 厂商 HAL 头；
- `devices/` 只依赖 `utility/` 与标准库，**一行 HAL 都不能出现**；
- 板级细节（引脚、极性、外设句柄、DMA 能力）一律留在 `bsp/`；
- `utility/` 不认识 MCU。

**本仓库不提供 `Serial` / `Timer` 这类具体驱动实现。** 每块板子的串口数量、DMA 能力、HAL 版本都不同，BSP 自己实现即可；需要复用的是下面的骨架与约束，不是某份实现代码。

## 移植三步

1. 提供 `mcu_cmsis.h`，内容就是所选器件的 CMSIS 头，例如 `#include "py32f0xx.h"` 或 `#include "stm32f1xx.h"`。
2. 保证 `irq_lock` 可用：Cortex-M 直接可用（仓库现状就是 `PRIMASK`）。目标器件头里没有 `__get_PRIMASK` / `__disable_irq` / `__set_PRIMASK` 时，**不要改 `irq_lock.hpp`**，而是在 `mcu_cmsis.h` 里转接到该平台的中断控制原语：

   ```cpp
   // mcu_cmsis.h
   #pragma once

   #include "xxx_device.h"

   // 厂商头没有 CMSIS 风格内建函数时，在这里补齐同名转接
   static inline uint32_t __get_PRIMASK() { return xxx_get_irq_state(); }
   static inline void __disable_irq() { xxx_disable_irq(); }
   static inline void __set_PRIMASK(uint32_t state) { xxx_restore_irq(state); }
   ```

3. 把 tick 接进来：在 1ms 中断里调用 `timer::OnMilliSecondTick()`（SysTick 或 TIM 均可）。

> **接入方式**：本仓库只有头文件，加入工程的 **include 路径**即可（如 EIDE 的 `incList`）。不要把仓库目录同时加进 `srcDirs`（源目录扫描），否则 `examples/` 下的范例会被重复编译；工程必须扫描该目录时，用 `excludeList` 排除 `examples/**`。

## Application：事件循环与投递

`Application` 是整个工程里**唯一**的事件循环，也是唯一持有事件队列的地方。它同时是**单例**，只能通过 `GetInstance()` 获取，不允许有第二个实例：

```cpp
static Application &GetInstance() {
	static Application instance;
	return instance;
}
```

事件从产生到处理的完整链路只有一条：

```
任一 ISR（UART / GPIO / TIM / SysTick）
        │  只做三件事：清标志、搬数据、调用 PostXxx()
        ▼
Application::PostXxx()   ──►  queue<QueueItem, N>（irq_lock 保护）
                                      │
                                      ▼
Application::Start()  while (true)：pop 一个 → switch 事件号 → HandleXxx(item)
                                      │
                                      ▼
                              业务 / 状态机 / 调用 BSP 接口
```

规则：

1. **投递入口只有 `PostXxx()`**：每个都等价于构造一个 `QueueItem` 再 `queue_.push(item)`。它们会被中断调用，所以只允许 POD 构造 + `push`，不得调用 HAL、不得延时、不得解析数据。
2. **`Start()` 永不返回**：`pop` 成功就派发，失败可以直接 `continue`，或 `__WFI()` 睡眠（SysTick 每 1ms 会唤醒，最坏唤醒延迟 1ms）。
3. **事件号集中定义**在 `Application` 里（`static constexpr uint32_t EVT_XXX`），BSP 和驱动不得自定义事件号。
4. **载荷只放 POD**，优先 ≤ 8 字节，按值放进 `QueueItem` 的 `union`；不要放指针（生命周期无法保证）。
5. **事件是 FIFO，没有优先级**：高频事件会挤压低频事件。忙时用位图合并同类事件，或用"读取可用字节数"代替"每字节一个事件"。
6. **handler 必须快速返回且幂等**：handler 的执行时间就是其它事件的等待时间；同一事件可能连续到达，长流程要拆成状态机，由后续事件或定时器推进。
7. **队列满必须可观测**：`push` 返回 `false` 时计数（`dropped_`），调试期直接断言，禁止静默丢弃。
8. **初始化顺序**：`Application` 构造函数里只做"注册 BSP 回调 + 投递初始事件"，所有单例（含 `Board`）必须在外设中断使能之前构造完成。
9. **单例形态固定**：`Application` 与 `Board` 一律是"私有构造 + 函数内 `static` 实例 + `GetInstance()`"。不要改成全局对象、自由函数、工厂、依赖注入、注册表或宏单例；`app_main()` 里只调用 `Application::GetInstance().Start()`。

handler 内部再次 `PostXxx()` 是允许的（状态机推进），但不要形成自激循环；需要在中断之外主动触发时，也从 `PostXxx()` 进入，不要直接调用 `HandleXxx()`。

## BSP（Board）：板级资源的唯一入口

`Board` 是"这一块板子"的抽象，**一块板子一份实现**。同 MCU 但引脚、外设、器件不同，就换一份 `Board`，应用层代码不变。

`Board` 同样是**单例**：私有构造函数 + `static Board &GetInstance()`。全工程只有这一份实例，禁止改成全局对象、工厂、依赖注入、注册表或宏单例，也不允许创建第二个实例。

```cpp
class Board {
public:
	static Board &GetInstance() {
		static Board instance;
		return instance;
	}

	Board(const Board &) = delete;
	Board &operator=(const Board &) = delete;

private:
	Board() = default;
	// 板级资源、回调指针、常量……
};
```

职责（且只有这些）：

1. 持有并初始化 HAL 句柄（`huart`、`htim`、`hadc`…），完成板级初始化。
2. 构造板级驱动对象（串口、按键、LED、传感器…），并通过接口暴露出去，例如 `GetUart()`、`GetKey()`；返回类型由该板 BSP 决定。
3. **独占并转发 HAL 弱回调**（`HAL_UART_RxCpltCallback`、`HAL_GPIO_EXTI_Callback`…），按句柄/引脚路由到对应驱动。
4. 提供板级常量与能力查询（引脚定义、时钟、是否支持 DMA 等）。

不做什么：

- 不放业务逻辑、不做协议解析、不定义事件编号（事件属于 `Application`）。
- 不缓存事件、不持有事件队列。

**中断回调契约**：驱动在中断里发现"有新数据/状态变化"时，只调用上层注册的通知回调（带上字节数或标志），不解析内容；上层回调再把结果 `Post` 成事件。

`Serial`、`Timer` 这类驱动属于 BSP，**本仓库不提供实现**。每块板子按自己的硬件与 HAL 能力去写，对外只需满足三条最小契约：非阻塞读取（取走缓冲数据）、非阻塞写入（不卡住调用者）、注册通知（有数据或状态变化时回调，由上层转成事件）。这样换板时只替换 BSP，应用层和 `utility` 都不用动。

## devices 层：器件抽象

`devices/` 回答"**这个器件是什么**"（行为与语义），`bsp/` 回答"**它在这块板上怎么接**"（引脚、极性、总线、初始化）。

### 放什么

- 只有行为、没有硬件细节的器件模型：`Led`、`Button`、`Buzzer`、传感器量程换算、按键消抖状态机；
- 多种实现共享的语义：`GpioLed`（直连 GPIO）、`PwmLed`（调光）、`I2cExpanderLed`（端口扩展）。

### 不放什么

- 任何厂商头文件（`stm32f4xx_hal.h`、`py32f0xx_hal.h`…），也不 include `mcu_cmsis.h`；
- 引脚号、GPIO 端口、AF 编号、器件 I2C 地址——这些是板级事实；
- 初始化代码（时钟使能、`HAL_GPIO_Init`、总线初始化）。

### 写法：NVI（非虚接口）

对外方法非虚、由基类维护不变量；派生类只实现最小的硬件钩子：

```cpp
// devices/led.hpp
class Led {
public:
	void SetState(bool state) {
		state_ = state;
		SetIo(state_);
	}

	void Toggle() { SetState(!state_); }
	bool GetState() const { return state_; }

protected:
	~Led() = default;   // 实例都是静态对象，不会通过基类 delete
	bool state_ = false;

	virtual void SetIo(bool on) = 0;   // on == true 表示"点亮"
};
```

```cpp
// bsp/gpio_led.hpp
class GpioLed final : public Led {
public:
	GpioLed(GPIO_TypeDef *port, uint16_t pin, bool active_high = true)
		: port_(port), pin_(pin), active_high_(active_high) {}

private:
	GPIO_TypeDef *port_;
	uint16_t pin_;
	const bool active_high_;

	void SetIo(bool on) override {
		const GPIO_PinState level = (on == active_high_) ? GPIO_PIN_SET : GPIO_PIN_RESET;
		HAL_GPIO_WritePin(port_, pin_, level);
	}
};
```

### 什么时候不该用虚接口

只有一种实现、也不需要 PC 端 mock 时，vptr + vtable 是纯开销。此时把硬件原语注入即可（零开销，见「反模式与正确做法」）：`led` 持有 `void (*write)(bool, void*)`，由 BSP 传入静态函数。

判断标准：**需要运行时替换实现 / 要在 PC 上跑单测** → 虚接口；否则函数指针或模板 policy。

### 实例放哪、何时构造

- 一律静态对象：`Board` 的成员，或 `board.cpp` 里命名空间作用域的静态对象；**禁止 `new`**；
- **不要用函数内 `static`**：C++11 会插入 `__cxa_guard_acquire/release`，每次访问都判断；更严重的是**首次访问若发生在 ISR 里，就等于在中断里做外设初始化**；
- 构造/析构里不要碰硬件：由 `Board` 构造先使能时钟、`HAL_GPIO_Init`，再 `SetState(false)` 确定初始电平；
- 析构里**不要** `HAL_*_DeInit`：静态对象在裸机上永不析构，这段代码只会拖入 `__cxa_atexit` 并白占 flash。

### 命名

- 接口按行为命名：`Led`、`Button`、`Imu`（不加 `I` 前缀，也不加 `Io` 前缀）；
- 实现按差异命名：`GpioLed`、`PwmLed`、`SoftI2cButton`；目录已经表明它是 BSP，不必再加 `Bsp` 前缀；
- 文件名与类名对应：`devices/led.hpp` → `Led`，`bsp/gpio_led.hpp` → `GpioLed`。

`Board` 只暴露基类：`Led *GetLed(size_t index)`，应用层拿不到 `GpioLed`，也就拿不到 HAL 类型。

## API 速查

| 组件 | 签名 | 说明 |
|---|---|---|
| `queue<T, N>` | `push(const T&)` / `pop(T&)` / `empty()` / `full()` / `size()` / `clear()` | 单生产者单消费者事件队列，满时 `push` 返回 `false` 并丢弃 |
| `stream_buffer<N>` | `write(const void*, size_t)` / `read(void*, size_t)` / `peek` / `skip` / `size` / `space` / `empty` / `clear` | 字节流环形缓冲，保留一个空位区分满/空 |
| `timer` | `create(period_ms, reload, cb, ctx)` / `start` / `stop` / `reset` / `change_period` / `destroy` / `is_active` / `period_ms` | 静态链表软件定时器，`period_ms >= 1` |
| `timer::OnMilliSecondTick()` | `static void` | 由 1ms 中断调用，回调在 ISR 上下文执行 |
| `irq_lock` | RAII，构造关中断、析构恢复 | 不可拷贝；不要跨越长逻辑持有 |

约定：**新代码统一使用 `timers.hpp` 的 `timer`**，不要另写毫秒计数；`utility` 之外的 `Timer` / `Serial` 属于 BSP 驱动，接口与实现由各板子决定。

## 生成新功能的标准流程

1. **需要新资源**：在 `Board` 增加获取接口，并在该板 BSP 中完成 HAL 初始化与回调转发；应用层不接触 HAL 句柄。器件行为能在 `devices/` 描述的（LED、按键…）先抽象，再由 BSP 提供实现。
2. **定事件**：在 `Application` 里加 `static constexpr uint32_t EVT_XXX`。带载荷的走 `queue`，纯电平/标志类事件用 `volatile` 位图（见模板）。
3. **定载荷**：在 `QueueItem` 的 `union` 里加 POD 成员，优先 ≤ 8 字节，避免深拷贝。
4. **写投递函数**：`PostXxx()` 只构造 `QueueItem` 并 `push`，供 BSP 回调在中断里调用。
5. **写 handler**：`Start()` 的 `switch` 加 case 与 `HandleXxx()`，立即返回；等待过程拆状态机。
6. **要定时就挂 timer**：用 `timers.hpp` 的 `timer`，回调里只 `Post` 事件，不要写 `HAL_Delay`，也不要自己数 tick。
7. **验证**：用工程实际工具链编译通过（C++17、`-fno-exceptions -fno-rtti`；GCC 额外加 `-Wall -Wextra`），AC6 工程以 EIDE/Keil 构建日志为准。

## 代码风格

- `utility` 层：类名 snake_case（`timer`、`queue`、`stream_buffer`、`irq_lock`），方法 snake_case，成员后置下划线。
- `app` / `bsp` / `devices` 层：类名 PascalCase（`Application`、`Board`、`Led`），方法 PascalCase（贴合 ST HAL 风格），成员后置下划线；文件名 snake_case（`gpio_led.hpp`）。
- 缩进用 Tab，K&R 花括号，`#pragma once`，注释用中文且只写必要的。
- 回调统一用无捕获 lambda：`+[](void *ctx) { ... }`，`ctx` 传 `this` 后 `static_cast` 回来。

## 反模式与正确做法

| 反模式 | 正确做法 |
|---|---|
| ISR 里 `printf` / `HAL_Delay` / 阻塞式串口发送 | 投递事件，主循环处理 |
| 在回调里解析协议、写 Flash | 回调只 `push`，业务放 handler |
| 应用层直接 `#include` 厂商 HAL 头、操作 `huart` 等句柄 | 只通过 `Board` 暴露的接口 |
| 用全局对象 / 自由函数 / 工厂 / 依赖注入替代 `GetInstance()` | `Application`、`Board` 统一用 `GetInstance()` 单例 |
| 创建多个 `Application` / `Board` 实例，或把构造改成 public | 私有构造 + 删除拷贝，只保留一份实例 |
| 在 `Board` 里写业务逻辑、定义事件号 | BSP 只做资源管理与回调转发，事件归 `Application` |
| 把某块板的 `Serial` / `Timer` 实现照抄到新板 | 按新板硬件与 HAL 能力重写驱动，保持接口语义 |
| `new` / STL 容器 / `std::function` | 静态数组、函数指针 + `void *ctx` |
| 整个函数用 `irq_lock` 包住 | 只保护几条共享变量的读写 |
| 自己写毫秒计数 | 用 `timer`（或事件位图 + 1ms tick） |
| 多个模块各自抢 `HAL_*_Callback` | 统一由 `Board` 分发或注册回调 |
| 用 `HAL_UART_Receive_IT` 却实现 `HAL_USART_RxCpltCallback` | 回调名与所用 HAL API 对齐（UART → `HAL_UART_RxCpltCallback`） |
| 厂商工程已启动 IWDG 却不喂狗 | 用 `timer` 周期投递事件，在 handler 里调用 `Board::RefreshWatchdog()` |
| EIDE 把仓库根目录加进 `srcDirs` 却不排除 `examples/**` | 加 `excludeList`，或只把仓库根目录放进 `incList` |
| 从 Keil / CubeMX(MDK-ARM) 导入的工程把 EIDE 工具链改成 GCC（或反向） | 沿用原工具链：AC6 用 `../MDK-ARM/startup_*.s` + EIDE 生成的 `.sct`，GCC 才用 CMSIS gcc 启动文件和 `.ld` |
| `<uAC6>1</uAC6>` 写进 `<TargetCommonOption>` | 必须写在 `<Target>` 下（与 `<pCCUsed>` 同级），否则 µVision 继续用 AC5 |
| 只改 EIDE 不管 `MDK-ARM/*.uvprojx` | Keil 工程也要加 `app/`、`bsp/`、`port/` 源文件与 include 路径，否则用户用 Keil 打开直接编译失败 |
| `devices/` 里 include 厂商 HAL、写死引脚号 / 极性 | `devices/` 只描述器件行为；引脚、极性、初始化全部留在 `bsp/` |
| 在驱动构造里 `HAL_GPIO_Init`、在析构里 `HAL_GPIO_DeInit` | 初始化归 `Board`（先使能时钟再配引脚），驱动只读写电平；静态对象永不析构，DeInit 只会拖入 `__cxa_atexit` |
| `Board::GetXxx()` 内用函数内 `static` 首次构造外设 | 用命名空间作用域 / `Board` 成员静态对象，在开中断前完成构造（避免 `__cxa_guard` 与 ISR 内初始化） |
| 为只有一种实现的器件引入虚基类 | 只有需要运行时替换或 PC 端 mock 才用虚接口，否则用函数指针 / 模板 policy 注入 |
| ISR 里触发单例首次构造 | 在开中断前完成所有单例初始化 |

## 完整模板

`Board` 契约、`Application` 事件循环、电平事件位图、`main()` 集成与检查清单在 [references/adapter-templates.md](./references/adapter-templates.md)。模板中的 `Serial` / `Timer` 只作为接口命名示意，**不要复制其实现**——那是某块板子的 BSP 产物。

需要完整可运行的例子时，直接看仓库的 [`examples/py32f002b_start_kit/`](../../../examples/py32f002b_start_kit/README.md)：它把定时器、串口、BSP 和事件循环串成了一条可编译的链路。
