# non-os-mcu-cpp-utility

面向裸机 MCU 的 C++17 轻量工具库。header-only、无动态内存、中断安全。

## 提供什么

| 文件 | 内容 |
|---|---|
| `queue.hpp` | `Queue<T, N>`：单生产者/单消费者事件队列，适用于"中断投递、主循环处理" |
| `stream_buffer.hpp` | `StreamBuffer<N>`：字节流环形缓冲，适用于串口等外设收发的字节流 |
| `timers.hpp` | `Timer`：静态链表软件定时器，支持周期/单次触发、启停、改周期 |
| `irq_lock.hpp` | `IrqLock`：RAII 临界区（Cortex-M 用 `PRIMASK`） |
| `.github/skills/` | Agent Skill：`main/` 单一结构、事件循环、`Board` 契约、`references/`（平台契约、移植、工具链） |

所有容器与定时器都受 `IrqLock` 保护，可在主循环和中断之间安全共用；对象是静态的，不做任何堆分配。

## 快速开始

### 1. 加入 include 路径

把仓库根目录加入工程的 include 路径（EIDE 的 `incList`）即可，不需要编译任何 `.cpp`。
工程自己的 `main/`、`main/board/`、`utility/` 也要在 include 路径里：
`irq_lock.hpp` 靠 `main/` 找到 `mcu_cmsis.h`。

### 2. 提供 MCU 桥接头（唯一的移植点）

`irq_lock.hpp` 通过 `mcu_cmsis.h` 间接依赖当前 MCU 的 CMSIS，工程里需要提供这个文件：

```cpp
// main/mcu_cmsis.h
#pragma once
#include "py32f0xx.h"   // 换成你的器件头，例如 "stm32f1xx.h"
```

如果器件头里没有 `__get_PRIMASK` / `__disable_irq` / `__set_PRIMASK`，**不要改 `irq_lock.hpp`**，
直接在这个头里补齐同名转接（参见 [porting.md](./.github/skills/mcu-baremetal-event-loop/references/porting.md)）。

### 3. 接入 1ms tick

在任意 1ms 中断里调用一次 `Timer::OnMilliSecondTick()`：

```cpp
#include "timers.hpp"
#include "py32f0xx_hal.h"

extern "C" void SysTick_Handler(void)
{
	HAL_IncTick();
	Timer::OnMilliSecondTick();
}
```

### 最小示例

```cpp
#include "queue.hpp"
#include "timers.hpp"

struct Event {
	uint32_t id;
};

static Queue<Event, 8> events;
static Timer tick;

int main()
{
	tick.Create(100, true, +[](void *) {
		events.Push(Event{1});   // 中断里只投递事件
	}, nullptr);
	tick.Start();

	Event e;
	while (true) {
		if (events.Pop(e)) {
			// 在主循环里做业务
		}
	}
}
```

`Timer` 的回调运行在 1ms 中断上下文，必须保持简短，只做"投递事件"这类操作。

## 工程模式

配套的骨架是：**中断只投递事件 → `Application` 的事件循环统一派发 → 静态内存 → `IrqLock` 保护共享状态**。

- `main/`：自有代码的唯一根目录，放 `application.hpp/.cpp`、`mcu_cmsis.h`、必有的 `board/`、按外设可选的 `serial/` 等；
- `Application`：全工程唯一的事件循环，也是唯一持有事件队列的地方。中断通过 `PostXxx()` 投递，`Start()` 里 `Pop` 后按事件号派发；
- `Board`：具体类，是应用获取板载设备的唯一入口，负责持有厂商句柄、构造驱动对象、独占并转发厂商弱回调；
- `main/<periph>/`：具体外设驱动（`Serial`、`Key`、`Led`…），构造函数只存句柄，`Init()` 里才 arm 中断。

完整规则见 [SKILL.md](./.github/skills/mcu-baremetal-event-loop/SKILL.md)。

## 目录

```text
utility/
├── queue.hpp / stream_buffer.hpp / timers.hpp / irq_lock.hpp
├── README.md / changes.md
└── .github/skills/mcu-baremetal-event-loop/
    ├── SKILL.md
    └── references/{platform-profile,porting,toolchains}.md
```

## 许可证

[MIT](./LICENSE) © 2026 wwx-03

## 变更

见 [changes.md](./changes.md)。