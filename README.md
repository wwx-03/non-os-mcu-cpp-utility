# non-os-mcu-cpp-utility

面向裸机 MCU 的 C++17 轻量工具库与工程模式。header-only、无动态内存、中断安全，已在 PY32F002B / STM32 风格 HAL 工程上验证。

## 提供什么

| 文件 | 内容 |
|---|---|
| `queue.hpp` | `queue<T, N>`：单生产者/单消费者事件队列，适用于"中断投递、主循环处理" |
| `stream_buffer.hpp` | `stream_buffer<N>`：字节流环形缓冲，适用于串口等外设收发的字节流 |
| `timers.hpp` | `timer`：静态链表软件定时器，支持周期/单次触发、启停、改周期 |
| `irq_lock.hpp` | `irq_lock`：RAII 临界区（Cortex-M 用 `PRIMASK`） |
| `.github/skills/` | Agent Skill：让 AI 按本仓库的模式生成代码 |
| `examples/py32f002b_start_kit/` | 可直接编译烧录的完整范例工程（PY32F002B Start Kit） |

所有容器与定时器都受 `irq_lock` 保护，可在主循环和中断之间安全共用；对象是静态的，不做任何堆分配。

## 快速开始

### 1. 加入 include 路径

把本目录加入工程的 include 路径（EIDE 的 `incList`）即可，不需要编译任何 `.cpp`。

也可以把本目录加进 EIDE 的 `srcDirs`，方便在工程树里浏览头文件；但**必须同时用 `excludeList` 排除 `examples/**`**，否则范例的 `.cpp` 会被重复编译。各字段的取值见 [SKILL.md](./.github/skills/mcu-baremetal-event-loop/SKILL.md) 的「EIDE 工程配置」。

### 2. 提供 MCU 桥接头（唯一的移植点）

`irq_lock.hpp` 通过 `mcu_cmsis.h` 间接依赖当前 MCU 的 CMSIS，工程里需要提供这个文件：

```cpp
// mcu_cmsis.h
#pragma once
#include "py32f0xx.h"   // 换成你的器件头，例如 "stm32f1xx.h"
```

如果器件头里没有 `__get_PRIMASK` / `__disable_irq` / `__set_PRIMASK`，**不要改 `irq_lock.hpp`**，直接在这个头里补齐同名转接（参见 [SKILL.md](./.github/skills/mcu-baremetal-event-loop/SKILL.md) 的"移植三步"）。

### 3. 接入 1ms tick

在任意 1ms 中断里调用一次 `timer::OnMilliSecondTick()`：

```cpp
#include "timers.hpp"
#include "py32f0xx_hal.h"

extern "C" void SysTick_Handler(void)
{
	HAL_IncTick();
	timer::OnMilliSecondTick();
}
```

### 最小示例

```cpp
#include "queue.hpp"
#include "timers.hpp"

struct Event {
	uint32_t id;
};

static queue<Event, 8> events;
static timer tick;

int main()
{
	tick.create(100, true, +[](void *) {
		events.push(Event{1});   // 中断里只投递事件
	}, nullptr);
	tick.start();

	Event e;
	while (true) {
		if (events.pop(e)) {
			// 在主循环里做业务
		}
	}
}
```

`timer` 的回调运行在 1ms 中断上下文，必须保持简短，只做"投递事件"这类操作。

## 范例工程

仓库自带两个完整、可编译烧录的工程，除 MCU 外分别代表两种工具链：

| 范例 | MCU | 工具链 / 工程 |
|---|---|---|
| [`examples/stm32f407_start_kit/`](./examples/stm32f407_start_kit/README.md) | STM32F407ZGTx | Keil **AC6（armclang）**：`MDK-ARM/*.uvprojx` + EIDE（`toolchain: AC6`） |
| [`examples/py32f002b_start_kit/`](./examples/py32f002b_start_kit/README.md) | PY32F002Bx5 | **GCC**（arm-none-eabi）：EIDE + `.ld` 链接脚本 |

两者演示同一套模式：

- 1ms tick：`HAL_IncTick` → `timer::OnMilliSecondTick()`；
- UART 中断接收 → `stream_buffer` → BSP 回调 → `queue` → `Application` 事件循环；
- `Board` 单例作为 BSP，独占 HAL 句柄与 HAL 弱回调。

范例直接引用本仓库根目录的头文件（EIDE `incList` 中的 `../../..`），不复制一份库代码，因此始终与仓库版本一致。目录按 `app/`（应用）、`bsp/`（板级）、`port/`（MCU 桥接）分层，可直接作为新工程的模板：Keil / MDK-ARM 导入的工程用 `stm32f407_start_kit`，GCC 工程用 `py32f002b_start_kit`。

## 工程模式

本仓库配套的骨架是：**中断只投递事件 → `Application` 的事件循环统一派发 → 静态内存 → `irq_lock` 保护共享状态**。

- **Application**：全工程唯一的事件循环，也是唯一持有事件队列的地方。所有中断通过 `PostXxx()` 投递，`Start()` 里 `pop` 后按事件号派发到 handler。**必须用 `GetInstance()` 单例**。
- **BSP（Board）**：全工程唯一接触 HAL 的地方，一块板子一份实现。负责持有 HAL 句柄、构造板级资源、独占并转发 HAL 回调；驱动只上报"有数据/有事件"，不解析协议。**同样是 `GetInstance()` 单例**，不允许改全局对象、工厂或依赖注入。
- **驱动（Serial、Timer 等）**：属于具体板子的 BSP，本仓库**不提供**。换板时按新硬件与 HAL 能力重写，保持接口语义即可。
- **器件抽象（devices）**：与 MCU 无关的行为模型（`Led`、`Button`…），只描述"是什么"，不碰引脚与 HAL；具体实现由 `bsp/` 继承并提供，`Board` 只暴露基类指针。

新增外设、事件、定时任务时请沿用该模式，不要在一个工程里混用 RTOS、动态内存或另一套事件机制。完整的规则、反模式和代码模板见：

- [SKILL.md](./.github/skills/mcu-baremetal-event-loop/SKILL.md)
- [adapter-templates.md](./.github/skills/mcu-baremetal-event-loop/references/adapter-templates.md)

## 让 AI 按本仓库的模式生成代码

本仓库自带一个 Agent Skill：`.github/skills/mcu-baremetal-event-loop/`。引入本仓库后，按下面任一方式让 AI 使用它。

> VS Code 只在**工作区根目录**的 `.github/skills/` 下自动发现 Skill。

### 方式 A：复制到宿主工程（最简单，推荐）

```powershell
$repo = "path\to\non-os-mcu-cpp-utility"   # 换成实际仓库目录（本地目录名可与仓库名不同）
Copy-Item -Recurse "$repo\.github\skills\mcu-baremetal-event-loop" .github\skills\
```

之后在宿主工程里新增需求时，AI 会自动加载该 Skill；也可以直接用 `/mcu-baremetal-event-loop` 手动调用。

### 方式 B：把本目录作为工作区文件夹

VS Code 多根工作区会扫描每个文件夹根目录下的 `.github/skills/`。把本仓库目录加入工作区文件即可（`path` 写仓库所在目录的实际名称，本地目录名可以与仓库名不同）：

```json
{
  "folders": [
    { "path": "." },
    { "path": "non-os-mcu-cpp-utility" }
  ]
}
```

### 方式 C：合并进工程级说明

没有 Skill 机制时，可把 `SKILL.md` 与 `references/adapter-templates.md` 的正文并入工程的 `AGENTS.md` 或 `.github/copilot-instructions.md`。

### 如何确认生效

- 在 Chat 输入 `/`，应能看到 `mcu-baremetal-event-loop`。
- 或直接说"用本仓库模式加一个 500ms 定时打印任务"，生成的代码应使用 `timers.hpp` + `queue`，而不是 `HAL_Delay` 或动态分配。

`SKILL.md` 的 `description` 是自动发现的入口，修改时请保留其中的触发词（裸机、MCU、中断、事件循环、串口、定时器、queue、stream_buffer、timers、irq_lock、bare-metal、interrupt 等）。

## 目录结构

```
non-os-mcu-cpp-utility/
├── queue.hpp
├── stream_buffer.hpp
├── timers.hpp
├── irq_lock.hpp
├── changes.md                          # 变更记录
├── README.md
├── .gitignore
├── .github/skills/mcu-baremetal-event-loop/
│   ├── SKILL.md
│   └── references/adapter-templates.md
└── examples/
    ├── README.md
    ├── py32f002b_start_kit/            # GCC / EIDE 模板
    │   ├── app/
    │   ├── bsp/
    │   ├── port/
    │   ├── Core/
    │   ├── Drivers/
    │   ├── EIDE/
    │   └── README.md
    └── stm32f407_start_kit/            # Keil AC6 模板（同时保留 .uvprojx 与 EIDE 工程）
        ├── app/
        ├── bsp/
        ├── devices/
        ├── port/
        ├── Core/
        ├── Drivers/
        ├── MDK-ARM/
        ├── EIDE/
        ├── stm32f407_start_kit.ioc
        └── README.md
```

## 约束

- C++17 及以上（使用 inline 静态成员，GCC ≥ 7）。
- 编译建议：`-std=c++17 -fno-exceptions -fno-rtti -Wall -Wextra`。
- 不提供动态内存、不加锁的多线程支持；仅面向单核裸机。

## 变更记录

见 [changes.md](./changes.md)。
