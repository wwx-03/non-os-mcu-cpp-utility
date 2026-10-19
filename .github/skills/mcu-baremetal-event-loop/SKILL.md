---
name: mcu-baremetal-event-loop
description: '编写或改造裸机 MCU（Cortex-M / STM32 / PY32 / 嵌入式 C++）代码时使用本仓库（queue.hpp、stream_buffer.hpp、timers.hpp、irq_lock.hpp）已有的工程模式：中断只投递事件、由 Application 的事件循环统一派发、BSP 层 Board 统一管理板级资源、静态内存、irq_lock 临界区、1ms 软件定时器。生成代码前先填写平台契约（MCU/器件头、中断原语、tick 来源、HAL、工具链），再按 templates/ 生成 app/bsp/devices/port，按 references/toolchains.md 配置 EIDE / Keil AC6 / GCC。Use when adding peripherals, events, timers, UART/串口 protocols, interrupts, drivers, BSP/board ports, scaffolding a new bare-metal project, configuring its toolchain (EIDE eide.yml, Keil MDK AC6, GCC), or porting this library into an existing one, and generate code that follows these patterns instead of inventing new ones.'
argument-hint: '目标 MCU/工具链 + 要新增的外设 / 事件 / 定时任务，例如"STM32F407 + Keil AC6，加一个按键长按事件"'
---

# 裸机事件驱动工程模式（平台无关核心）

本技能只约束**工程结构与中断/主循环边界**，不绑定具体 MCU、厂商 HAL 或工具链：

- 中断只做三件事：清标志、搬数据、投递事件；
- `Application` 是全工程唯一的事件循环；
- `Board`（BSP）是板级资源与 HAL 回调的唯一入口；
- `devices/` 只描述器件行为，不碰 HAL；
- 静态内存 + `irq_lock` 保护共享状态。

平台差异放在 `references/`，可复制骨架放在 `templates/`。生成代码前先读
[references/platform-profile.md](./references/platform-profile.md)，完成平台契约，再开始写代码。

## 使用顺序（强制）

1. 从需求/仓库识别：MCU、内核、厂商 HAL/SDK、工程来源、工具链、1 ms tick 来源。
2. 阅读器件头与 HAL 文档，填写 `references/platform-profile.md` 中的契约。
3. 完成唯一移植点 `port/mcu_cmsis.h` 与 tick 接入（见 `references/porting.md`）。
4. 以 `templates/`（或 `examples/` 中的完整范例）生成 `app/bsp/devices/port`；
   `Core/`、`Drivers/`、启动文件、链接脚本交给厂商工具生成，不要复制。
5. 按 `references/toolchains.md` 配置 EIDE / Keil / GCC，**沿用工程原有工具链**。
6. 按 `checklists/bring-up.md` 逐项核对，并用实际工具链编译、上电 smoke test。

**不确定的平台语义不许猜。** 查不到就标 TODO 并向用户确认。

## 平台契约（生成代码前的硬门槛）

| 契约项 | 为什么必须确认 | 典型取值 |
| --- | --- | --- |
| MCU / 内核 | 决定 CMSIS 头、FPU、中断原语 | Cortex-M0+ / M4F |
| CMSIS 器件头 | `mcu_cmsis.h` 的唯一内容 | `py32f0xx.h` / `stm32f4xx.h` |
| 中断原语 | `irq_lock` 能否直接工作 | `__get_PRIMASK` 等是否可用 |
| tick 来源 / 频率 | `timer` 依赖 1 ms | SysTick / TIMx |
| tick 接入点 | 避免重复定义 | `HAL_IncTick` / 中断入口 |
| HAL / LL | 决定 BSP 写法与回调名 | 厂商与版本 |
| 中断优先级 | 决定回调能否安全投递 | NVIC 分组 / 抢占优先级 |
| 内存 / 链接 | 静态分配与栈大小 | `.ld` / `.sct` |
| 工具链 | 决定启动文件与编译参数 | AC6 / GCC |
| 工程文件 | 必须与工具链一致 | `eide.yml` / `.uvprojx` |

完整模板与逐项查证方法见 [references/platform-profile.md](./references/platform-profile.md)。

## 用范例或模板生成工程骨架

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
6. 工程配置：**先确认工程原有工具链并沿用**（Keil / CubeMX 导入的 MDK-ARM 工程用 AC6，GCC 工程用 GCC），再按 [references/toolchains.md](./references/toolchains.md) 设置 `srcDirs` / `incList` / C++17 选项与链接参数，改完必须用该工具链实际编译一次。

生成的代码必须同时满足下面的铁律、分层与命名约定；范例只是骨架，业务逻辑要按需求重新实现。

也可以直接从 [templates/](./templates/README.md) 复制骨架；两者语义一致，模板不含具体板级实现。

## 三条铁律

1. **中断只做三件事**：清标志、搬数据、投递事件（`push`）。禁止在 ISR 里延时、阻塞发送、`printf`、跑业务循环。
2. **静态内存**：禁止 `new` / `malloc` / `std::vector` / `std::string` / `std::function` / 智能指针。对象只能是全局、命名空间作用域静态或成员；不要用函数内 `static`（会引入 `__cxa_guard`，且首次访问若发生在 ISR 里就等于在中断里初始化外设）。
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

> 平台相关的移植、工具链与回调上下文见 [references/porting.md](./references/porting.md) 和
> [references/toolchains.md](./references/toolchains.md)；可复制骨架见
> [templates/](./templates/README.md)。

## 平台适配与工具链

平台相关的移植、EIDE / Keil MDK / GCC 配置已经拆到 references：

- 移植点、中断原语、1 ms tick、回调上下文 → [references/porting.md](./references/porting.md)
- EIDE / Keil MDK（AC6）/ GCC 配置与验证 → [references/toolchains.md](./references/toolchains.md)
- 平台契约模板 → [references/platform-profile.md](./references/platform-profile.md)

本文件只保留平台无关的结构与并发约束。

## 移植三步

移植只发生在 `port/mcu_cmsis.h` 与 tick 接入点，完整说明见
[references/porting.md](./references/porting.md)：

1. 提供 `port/mcu_cmsis.h`，include 目标器件的 CMSIS 头；
2. 目标平台没有 `__get_PRIMASK` / `__disable_irq` / `__set_PRIMASK` 时，
   在 `mcu_cmsis.h` 里补齐同名转接，**不要改 `irq_lock.hpp`**；
3. 在 1 ms 中断里调用一次 `timer::OnMilliSecondTick()`。

> 本仓库是 header-only，加入工程的 **include 路径**即可。若必须加入 `srcDirs`，
> 用 `excludeList` 排除 `examples/**`，否则范例的 `.cpp` 会被重复编译。

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

## devices 层：设备类怎么写

`devices/` 回答"**这个器件是什么**"（行为与语义），`bsp/` 回答"**它在这块板上怎么接**"（引脚、极性、总线、初始化）。设备类**不 include 任何厂商头**，只依赖 `utility/` 与标准库。

写一个设备类按下面的顺序来。

### 1. 先判断值不值得抽象

- 值得：器件本身有行为或状态语义——`Led`（开关 / 翻转）、`Button`（消抖、按下 / 松开）、`Buzzer`、传感器量程换算；
- 不值得：只是"某块板的某个引脚怎么操作"——直接写进 `bsp/`，不要为了结构对称硬造一个设备类；
- 只有当同一语义存在**多种接法**（`GpioLed` / `PwmLed` / `I2cExpanderLed`）或需要 PC 端 mock 时，虚接口才划算。

### 2. 基类用 NVI：对外非虚，只留一个硬件钩子

public 方法维护不变量（状态缓存、边界条件），派生类只实现钩子：

```cpp
// devices/led.hpp
class Led {
public:
	void SetState(bool state) {          // 对外接口：非虚
		state_ = state;
		SetIo(state_);
	}

	void Toggle() { SetState(!state_); }
	bool GetState() const { return state_; }

protected:
	~Led() = default;                    // 静态对象，不会通过基类 delete
	bool state_ = false;

	virtual void SetIo(bool on) = 0;     // 唯一的硬件钩子
};
```

要点：

- 钩子的参数用**语义**（`on == true` 表示点亮），不是电平——电平是板级细节；
- 基类不持有 HAL 类型、不初始化外设、不读寄存器；
- 状态缓存放在基类，派生类不要再存一份，避免两处状态打架。

### 3. 派生类只实现钩子，硬件细节都留在这里

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

派生类可以持有 `GPIO_TypeDef *`、引脚号、极性、总线句柄，但**不初始化引脚**——时钟使能与 `HAL_GPIO_Init` 由 `Board` 统一做。这样构造顺序、初始电平、引脚归属都只有一个地方说了算。

### 4. 需要读初值 / 启动采样的器件：提供 `Start()`，不要在构造函数里读

按键、传感器这类"构造时想先读一次初值"的器件，**不能在基类构造函数里调用纯虚 `Read()`**：基类构造期间派生类还没构造，这是未定义行为（会落到 `__cxa_pure_virtual`）。

正确做法是把"采样起点"做成显式 `Start()`，由 `Board` 在引脚配置完成后调用：

```cpp
class Button {
public:
	void Start() {          // Board 配好引脚后调用
		last_ = Read();     // 此时派生类已构造，虚调用安全
		timer_.start();
	}

	void Stop() { timer_.stop(); }

protected:
	~Button() = default;            // 静态对象
	virtual bool Read() const = 0;  // true == 按下

private:
	timer timer_;
	bool last_ = false;

	void OnTimerTick() {            // 运行在 1ms tick 中断里
		const bool now = Read();
		if (last_ && !now) {        // 按下 -> 松开
			// 只调用上层注册的回调，由上层 PostXxx()，不在这里做业务
		}
		last_ = now;
	}
};
```

派生类只需实现 `Read()`：

```cpp
// bsp/gpio_button.hpp
bool Read() const override {
	const bool level = HAL_GPIO_ReadPin(port_, pin_) == GPIO_PIN_SET;
	return level == active_high_;          // 统一成"是否按下"
}
```

### 5. 实例放在哪、什么时候构造

- 一律静态对象：`Board` 的成员，或 `board.cpp` 里命名空间作用域的静态对象；**禁止 `new`**；
- **不要用函数内 `static`**：C++11 会插入 `__cxa_guard_acquire/release`，每次访问都判断；更严重的是**首次访问若发生在 ISR 里，就等于在中断里做外设初始化**；
- 构造里只保存配置（端口 / 引脚 / 极性），不碰寄存器；初值在 `Board` 初始化完成后通过 `Start()` / `SetState()` 设定；
- 析构里**不要** `HAL_*_DeInit`：静态对象在裸机上永不析构，这段代码只会拖入 `__cxa_atexit` 并白占 flash。

### 6. 什么时候不该用虚接口

只有一种实现、也不需要 PC 端 mock 时，vptr + vtable 是纯开销。此时把硬件原语注入即可：设备类持有 `void (*write)(bool, void *)`，由 BSP 传入静态函数。

判断标准：**需要运行时替换实现 / 要在 PC 上跑单测** → 虚接口；否则用函数指针或模板 policy。

### 7. 命名

- 接口按行为命名：`Led`、`Button`、`Imu`（不加 `I` 前缀，也不加 `Io` 前缀）；
- 实现按差异命名：`GpioLed`、`PwmLed`、`SoftI2cButton`；目录已经表明它是 BSP，不必再加 `Bsp` 前缀；
- 文件名与类名对应：`devices/led.hpp` → `Led`，`bsp/gpio_led.hpp` → `GpioLed`。

`Board` 只暴露基类（`Led *GetLed(size_t)`、`Button *GetButton(size_t)`），应用层拿不到 `GpioLed` / `GpioButton`，也就拿不到 HAL 类型。

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
| `devices/` 里 include 厂商 HAL、写死引脚号 / 极性 | `devices/` 只描述器件行为；引脚、极性、初始化全部留在 `bsp/` |
| 在驱动构造里 `HAL_GPIO_Init`、在析构里 `HAL_GPIO_DeInit` | 初始化归 `Board`（先使能时钟再配引脚），驱动只读写电平；静态对象永不析构，DeInit 只会拖入 `__cxa_atexit` |
| `Board::GetXxx()` 内用函数内 `static` 首次构造外设 | 用命名空间作用域 / `Board` 成员静态对象，在开中断前完成构造（避免 `__cxa_guard` 与 ISR 内初始化） |
| 为只有一种实现的器件引入虚基类 | 只有需要运行时替换或 PC 端 mock 才用虚接口，否则用函数指针 / 模板 policy 注入 |
| 在器件基类构造函数里调用纯虚 `Read()` / `SetIo()` 取初值 | 提供 `Start()`，由 `Board` 在引脚配置完成后调用；基类构造期间调用纯虚函数是 UB |
| ISR 里触发单例首次构造 | 在开中断前完成所有单例初始化 |

## 参考与模板索引

| 文件 | 用途 |
| --- | --- |
| `references/platform-profile.md` | 平台契约模板、逐项查证方法、未确认项的后果 |
| `references/porting.md` | `mcu_cmsis.h`、中断原语转接、1 ms tick、回调上下文 |
| `references/toolchains.md` | EIDE、Keil MDK AC6、GCC 配置与验证 |
| `references/adapter-templates.md` | Board / Application / 事件位图 / 队列容量等契约与设计说明 |
| `templates/` | 可复制的 port / app / bsp / devices / utility 骨架 |
| `checklists/bring-up.md` | 移植、结构、内存、分层、工程配置检查清单 |
| `examples/` | PY32F002B（GCC）与 STM32F407（Keil AC6）完整范例 |

**记住**：核心规则是平台无关的；平台语义必须通过契约确认，不要凭记忆或旧工程经验生成代码。
