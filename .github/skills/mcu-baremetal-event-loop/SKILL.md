---
name: mcu-baremetal-event-loop
description: '编写或改造裸机 MCU（Cortex-M / STM32 / PY32 / 嵌入式 C++）代码时使用本仓库（queue.hpp、stream_buffer.hpp、timers.hpp、irq_lock.hpp）已有的工程模式：中断只投递事件、由 Application 的事件循环统一派发、BSP 层 Board 统一管理板级资源、静态内存、irq_lock 临界区、1ms 软件定时器。Use when adding peripherals, events, timers, UART protocols, drivers, BSP/board ports, or porting this library into a new bare-metal project, and generate code that follows these patterns instead of inventing new ones.'
argument-hint: '要新增的外设 / 事件 / 定时任务，例如"加一个按键长按事件"'
---

# 裸机事件驱动工程模式（non-os-mcu-cpp-utility 仓库）

本仓库是 header-only 的 C++17 工具库，配套一套固定的工程骨架。**只要工程引用了本仓库的头文件，新增代码就必须沿用下面的模式**，不要引入 RTOS、动态内存或另一套事件机制。

仓库自带一个可直接编译烧录的完整范例 [`examples/py32f002b_start_kit/`](../../../examples/py32f002b_start_kit/README.md)，包含 1ms tick 接入、UART 中断接收、`Board` BSP 单例和 `Application` 事件循环的完整写法，可作为实现参考。注意其中的 `Board` / `Serial` 是**某块板子的 BSP 产物**，换板时仍要按本文契约重写，不要直接复制。

## 何时使用

- 工程中包含 `queue.hpp` / `stream_buffer.hpp` / `timers.hpp` / `irq_lock.hpp`
- 要新增外设、事件、定时任务、串口协议处理
- 要把厂商 HAL（CubeMX、PY32 HAL 等）工程改造成本模式
- 要把本仓库移植到新的 MCU

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
bsp/      板级支持包：Board —— 板级资源与 HAL 回调的唯一入口，按板子实现
app/      Application —— 事件循环唯一实现位置
```

依赖方向只能是 `app → bsp → utility → port → CMSIS`：应用层只认识 `Board` 暴露的接口，绝不直接 `#include` 厂商 HAL 头；`utility` 不认识 MCU。

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

1. **需要新资源**：在 `Board` 增加获取接口，并在该板 BSP 中完成 HAL 初始化与回调转发；应用层不接触 HAL 句柄。
2. **定事件**：在 `Application` 里加 `static constexpr uint32_t EVT_XXX`。带载荷的走 `queue`，纯电平/标志类事件用 `volatile` 位图（见模板）。
3. **定载荷**：在 `QueueItem` 的 `union` 里加 POD 成员，优先 ≤ 8 字节，避免深拷贝。
4. **写投递函数**：`PostXxx()` 只构造 `QueueItem` 并 `push`，供 BSP 回调在中断里调用。
5. **写 handler**：`Start()` 的 `switch` 加 case 与 `HandleXxx()`，立即返回；等待过程拆状态机。
6. **要定时就挂 timer**：用 `timers.hpp` 的 `timer`，回调里只 `Post` 事件，不要写 `HAL_Delay`，也不要自己数 tick。
7. **验证**：用 `-std=c++17 -fno-exceptions -fno-rtti -Wall -Wextra` 编译（C++17 inline 静态成员要求 GCC ≥ 7）。

## 代码风格

- `utility` 层：类名 snake_case（`timer`、`queue`、`stream_buffer`、`irq_lock`），方法 snake_case，成员后置下划线。
- `app` / `bsp` 层：类名 PascalCase（`Application`、`Board`），方法 PascalCase（贴合 ST HAL 风格），成员后置下划线。
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
| ISR 里触发单例首次构造 | 在开中断前完成所有单例初始化 |

## 完整模板

`Board` 契约、`Application` 事件循环、电平事件位图、`main()` 集成与检查清单在 [references/adapter-templates.md](./references/adapter-templates.md)。模板中的 `Serial` / `Timer` 只作为接口命名示意，**不要复制其实现**——那是某块板子的 BSP 产物。

需要完整可运行的例子时，直接看仓库的 [`examples/py32f002b_start_kit/`](../../../examples/py32f002b_start_kit/README.md)：它把定时器、串口、BSP 和事件循环串成了一条可编译的链路。
