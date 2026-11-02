---
name: mcu-baremetal-event-loop
description: '编写或改造裸机 MCU（Cortex-M / RISC-V 嵌入式 C++）代码时使用本仓库（queue.hpp、stream_buffer.hpp、timers.hpp、irq_lock.hpp）已有的工程模式：自有代码全部放在 main/（application.*、mcu_cmsis.h、必有的 board/，外设目录按需创建）；中断只做清标志 / 搬数据 / 投递事件，由 Application 单例的事件循环统一派发；Board 是具体类，也是应用获取板载设备的唯一入口，核心只有 GetInstance() / Init()，GetSerial / GetKey / GetLed / RefreshWatchdog 只在需求包含该外设时才出现；生成顺序固定为"读厂商基线 → 空骨架先编译通过 → 按需逐个加外设"，未请求的外设不生成目录、成员、事件与 include；可选外设模板见 references/optional-peripherals.md。Use when scaffolding a bare-metal project on any MCU and any IDE, porting this library into an existing vendor project, generating a minimal zero-peripheral skeleton, or adding only the peripherals that were requested (events, timers, UART/串口 protocols, interrupts, drivers, board layer) following these patterns instead of inventing new ones.'
argument-hint: '目标 MCU + IDE / 工具链 + 需要的外设，例如"M0+ + 厂商 SDK，只生成空骨架" / "加一个串口收到 \\n 投递事件" / "加一个按键长按事件"'
---

# 裸机事件驱动工程模式（平台无关核心）

本技能约束三件事：**工程结构**、**中断 / 主循环边界**、**自用代码规范**。

不同 MCU、厂商 SDK、IDE 生成的初始工程各不相同（目录名、HAL 头、句柄变量、弱回调名、tick 位置、工程文件格式都不同）。因此本技能：

- 只固定 `main/` 下的自有代码结构，不假设厂商目录叫什么；
- 固定架构只包含 `application.*`、`mcu_cmsis.h`、`board/`；**外设目录与 `GetSerial` / `GetKey` / `GetLed` / `RefreshWatchdog` 都属于可选件**，需求里没有就不生成；
- 首先生成可编译的空骨架，再按需求逐个加外设；可选外设的完整模板见 [references/optional-peripherals.md](./references/optional-peripherals.md)。

## 核心与可选

| 分类 | 内容 |
| --- | --- |
| 核心（每个工程都生成） | `main/application.hpp` / `application.cpp`、`main/mcu_cmsis.h`、`main/board/board.hpp` / `board.cpp`；`Board::GetInstance()` / `Board::Init()`；`Application::Start()` / 事件队列 / `START_EVENT`；`utility/` 的 `Queue` / `IrqLock` |
| 可选（只在需求明确要求时生成） | `main/serial/`、`main/key/`、`main/led/` 等目录；`Board::GetSerial` / `GetKey` / `GetLed` / `RefreshWatchdog`；`Timer` 周期任务；`StreamBuffer` |

空骨架的判定标准：`Board` 只有 `GetInstance()` 与空的 `Init()`，`Application` 只有 `START_EVENT` 一条事件，`main/` 下没有外设目录。

## 生成顺序（按阶段推进，每阶段结束都要编译）

| 阶段 | 产出 | 结束条件 |
| --- | --- | --- |
| 0 读基线 | 填写平台契约（厂商头、句柄、回调名、tick 位置） | 基线表每项都有实际值或 TODO |
| 1 空骨架 | `main/mcu_cmsis.h`、`main/board/`、`main/application.*`、1 处入口调用 | 实际工具链编译通过，`START_EVENT` 命中一次 |
| 2 按需加外设（可选） | 仅限需求里提到的外设：`main/<periph>/<periph>.hpp` + `Board` 成员 + `Start()` 里的回调注册 + 1 组事件 | 编译通过，外设事件能被 handler 处理 |
| 3 业务 | handler 状态机、定时任务 | [完成标准](#完成标准dod) 全部勾选 |

**先跑通空骨架。** 一次生成全部外设会让编译错误和初始化顺序问题混在一起，定位成本远高于分两次生成。需求里没有的外设，第 2 步整步跳过。

## 第 0 步：读厂商基线（不可跳过）

从目标工程已有代码里读出下表的值，填写 [references/platform-profile.md](./references/platform-profile.md)：

| 需要确认 | 从哪里读 |
| --- | --- |
| CMSIS 器件头 | 厂商 `Inc/`、`Core/Include/` 或 SDK 里器件目录的顶层头文件 |
| HAL / SDK 主头 | 厂商外设初始化代码里 include 的那个入口头 |
| UART 句柄变量与类型 | 厂商外设初始化文件、`main.c` 的句柄声明 |
| UART 接收完成回调名 | HAL 头里的弱回调声明（`*_RxCpltCallback` 之类） |
| UART 接收 / 发送 / 中止 API | HAL 头里的函数声明与返回值类型 |
| 1ms tick 位置 | SysTick 中断、HAL tick 函数或用户 tick 钩子 |
| 程序入口 | 厂商 `main()` 及其 `while (1)` 的位置 |
| 启动文件与链接脚本 | 工程目录（`.s` / `.ld` / `.sct`） |
| 工程文件格式 | IDE 工程文件（`.eide/eide.yml`、`.uvprojx`、`Makefile`、`CMakeLists.txt`…） |
| C / C++ 混编方式 | 工程里 `.c` 与 `.cpp` 的编译选项、链接方式 |
| 中断优先级分组 | NVIC 配置代码 |

平台语义查不到时标 TODO 并向用户确认，不凭型号猜。

## 第 1 步：生成最小骨架（零外设）

只写下面这些文件，`Board` 里没有任何设备：

```text
<project>/
├── main/
│   ├── mcu_cmsis.h
│   ├── board/
│   │   ├── board.hpp
│   │   └── board.cpp
│   ├── application.hpp
│   └── application.cpp
└── utility/            # 本仓库的 queue.hpp / timers.hpp / irq_lock.hpp（或工程内副本）
                        # timers.hpp 只在本工程有 Timer 任务时才需要
```

除 `main/application.*`、`main/mcu_cmsis.h`、`main/board/` 与 `utility/` 之外，**不要创建任何其他目录**。

```cpp
// main/mcu_cmsis.h
#pragma once

#include "py32f0xx.h"   // 换成目标工程的 CMSIS 器件头
```

```cpp
// main/board/board.hpp
#pragma once

class Board {
public:
	static Board &GetInstance() {
		static Board instance;
		return instance;
	}

	Board(const Board &) = delete;
	Board &operator=(const Board &) = delete;

	// 可选外设的 Init() 在这里调用；空骨架保持为空
	void Init() {}

private:
	Board() = default;
};
```

```cpp
// main/application.hpp
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "queue.hpp"

class Application {
public:
	static Application &GetInstance() {
		static Application instance;
		return instance;
	}

	Application(const Application &) = delete;
	Application &operator=(const Application &) = delete;

	void Start();

private:
	static constexpr size_t QUEUE_LENGTH = 16;
	static constexpr uint32_t START_EVENT = 0x00000001u;

	struct QueueItem {
		uint32_t event;
		union {
			uint32_t value;
		} data;
	};

	Application();
	~Application() = default;

	void PostEvent(uint32_t e);
	void HandleStart();

	Queue<QueueItem, QUEUE_LENGTH> queue_;
	volatile uint32_t dropped_ = 0;
};
```

```cpp
// main/application.cpp
#include "application.hpp"

#include "board.hpp"

#include "irq_lock.hpp"

Application::Application() {
	PostEvent(START_EVENT);
}

void Application::Start() {
	// 加外设时在这里、Board::Init() 之前注册设备回调
	Board::GetInstance().Init();

	QueueItem item = {};
	while (true) {
		if (!queue_.Pop(item)) {
			continue;
		}
		switch (item.event) {
		case START_EVENT:
			HandleStart();
			break;
		default:
			break;
		}
	}
}

void Application::PostEvent(uint32_t e) {
	QueueItem item = {};
	item.event = e;
	if (!queue_.Push(item)) {
		IrqLock lock;
		++dropped_;
	}
}

void Application::HandleStart() {
}

extern "C" void app_main(void) {
	Application::GetInstance().Start();
}
```

`board.cpp` 在骨架阶段可以不建；`Board` 一旦有成员或回调转发就建。

### 有外设时的回调注册位置（固定顺序）

所有设备回调都在 `Application::Start()` 里注册，且必须在 `Board::Init()` 之前。下面同时列出串口与按键两段，**只用于说明位置与顺序**：本工程没有的外设，整段删除；只生成空骨架时，`Start()` 里只有 `Board::Init()` 与事件循环，没有任何注册代码。

```cpp
void Application::Start() {
	auto &board = Board::GetInstance();

	// 仅当本工程有串口时保留
	auto *serial = board.GetSerial();
	serial->SetSpecificCharactersCallback('\n', +[](size_t size, void *ctx) {
		static_cast<Application *>(ctx)->PostSerialReceivedEvent(size);
	}, this);
	serial->SetRxBufferFullCallback(+[](size_t size, void *ctx) {
		static_cast<Application *>(ctx)->PostSerialReceivedEvent(size);
	}, this);

	// 仅当本工程有按键时保留
	board.GetKey(0)->SetCallback(+[](Key::Event e, void *ctx) {
		static_cast<Application *>(ctx)->PostKeyEvent(e);
	}, this);

	board.Init();   // 只在这里 arm 中断

	QueueItem item = {};
	while (true) {
		// Pop → switch → HandleXxx
	}
}
```

理由：

1. `Board::Init()` 一返回，中断就可能到达，注册必须已经完成；
2. 注册与事件循环启动集中在一处，顺序只有一种写法；
3. `Key::Event` 这类设备类型经 `board.hpp` 传递到 `application.cpp`，不需要额外 include 驱动头。

`Application` 构造函数里只做两件事：如果有 Timer 任务就 `Create` 各 `Timer`，然后 `PostEvent(START_EVENT)`。没有 Timer 时只 `PostEvent(START_EVENT)`。

### 入口衔接

`main/` 对外只提供一个 C 入口：

```cpp
extern "C" void app_main(void);
```

按厂商 `main()` 的形态接入，只改一行：

| 厂商 main 的形态 | 接入方式 |
| --- | --- |
| 已有 `while (1)` 空循环 | 在 `while (1)` 之前调用 `app_main();` |
| 初始化函数后回到调度器 / RTOS | 在启动调度器之前调用 `app_main();`，事件循环里改用阻塞等待 |
| `main.c` 是纯 C 文件 | 在调用处上方加 `extern void app_main(void);` |
| 厂商 `main()` 不允许修改 | 在 `main/` 里不改入口，改为在板级初始化函数的末尾追加调用，并在平台契约里记录 |

`app_main()` 之前必须完成的厂商初始化：时钟、HAL 初始化、所用外设的 `*_Init`。缺一项就补，不要把它搬进 `main/`。

**第 1 步验收**：实际工具链编译通过；`HandleStart()` 命中一次；在 `HandleStart()` 里翻转一个空闲 GPIO 或置一个变量，确认没有卡在 `while` 之前。

空骨架必须干净，逐条核对：

```text
[ ] Board 只有 GetInstance() 与 Init()
[ ] Board 没有 GetSerial / GetKey / GetLed / RefreshWatchdog
[ ] Board 没有 Serial / Key / Led 成员
[ ] main/ 下没有 serial/ key/ led/ 等外设目录
[ ] board.cpp 里没有厂商弱回调
[ ] Application 里没有外设事件号与 PostXxx()
```

用户只要求"空骨架 / 先搭框架"时，到这里就结束，不要预埋任何外设。

## 第 2 步：按需加第一个外设（可选）

**本步只在用户明确要求某个外设时执行。** 需求里没有的外设，不建目录、不加成员、不加 `GetXxx()`、不加事件、不加 include 路径；只生成空骨架的请求在第 1 步就结束。

固定四件事，顺序不能换：

1. 建驱动：`main/<periph>/<periph>.hpp`（需要时再配 `.cpp`）。构造函数只存句柄，`Init()` 里才 arm 中断。
2. `Board`：加成员 + `GetXxx()` + 构造里注入句柄（厂商没配的板级 GPIO 也在构造里配）+ `Init()` 里调用设备 `Init()` + 转发厂商弱回调。
3. `Application::Start()`：在 `Board::Init()` 之前注册该设备的回调；再加 `static constexpr uint32_t XXX_EVENT`、`PostXxx()`、`case` 与 `HandleXxx()`。
4. 编译并确认事件能被处理，再开始下一个外设。

`board.hpp` 用短名引用驱动头（`#include "serial.hpp"`），所以每加一个外设目录，就要同步把它加进 include 路径。

驱动通用规则（与具体外设无关）：

1. 构造函数只保存句柄与静态配置，不碰寄存器、不 arm 中断；中断使能放在 `Init()`。
2. 厂商类型 / 函数只在 `main/board/` 与驱动头文件里出现，`application.*` 内为零。
3. `HandleXxxEvent()` 先取局部副本，再写缓冲、重挂接收、比较字节。
4. `Write` 先检查 `size == 0`、容量与发送空闲，再 `memcpy`，避免覆盖仍在发送的缓冲。
5. 回调注册用函数指针 + `void *ctx`，禁止 `std::function`。
6. 析构里不写厂商 `*_DeInit`：静态对象在裸机上永不析构，只会拖入 `__cxa_atexit` 白占 flash。
7. 驱动不定义事件号、不解析协议、不写业务逻辑。

具体外设的完整骨架与契约按需复制：

| 需求 | 参考 |
| --- | --- |
| 串口收发（`Serial` 驱动 + `Board` 增量 + 回调注册） | [references/optional-peripherals.md](./references/optional-peripherals.md#1-serialuart-中断收发) |
| 按键（消抖 + 长按，`Key` 契约） | [references/optional-peripherals.md](./references/optional-peripherals.md#2-keygpio-按键消抖--长按) |
| LED 指示（`Led` 契约） | [references/optional-peripherals.md](./references/optional-peripherals.md#3-ledgpio-输出无中断) |
| 周期任务 / 设备内部时序（`Timer`） | [references/optional-peripherals.md](./references/optional-peripherals.md#4-timer周期任务与设备内部时序) |

加完一个外设，按同一文件末尾的「每加一个外设的落地清单」与「负向检查」核对是否多加了东西。

## 工程结构（固定）

```text
<project>/
├── main/                       # 自有代码的唯一根目录
│   ├── application.hpp / .cpp
│   ├── mcu_cmsis.h             # 唯一的 MCU 依赖点
│   ├── board/                  # 每个工程必须有
│   │   ├── board.hpp
│   │   └── board.cpp
│   └── <periph>/               # 可选，可不存在：只有需求里有的外设才建（serial/ key/ led/…）
├── utility/                    # queue.hpp / stream_buffer.hpp / timers.hpp / irq_lock.hpp
├── <厂商生成目录>/              # 名称随厂商与 IDE：Core/、Drivers/、Src/、Inc/、startup、.ld/.sct
└── <IDE 工程目录>/              # 名称随 IDE：.eide/、MDK-ARM/、Makefile、CMakeLists.txt
```

| 路径 | 职责 |
| --- | --- |
| `main/application.*` | 事件号、`QueueItem`、`PostXxx()`、`HandleXxx()`、事件循环 |
| `main/board/` | 构造驱动对象并持有句柄（不重复厂商初始化）、配置板级 GPIO、暴露 `GetXxx()`、转发厂商回调、`Init()` 里让设备就绪 |
| `main/<periph>/` | 可选：具体外设驱动（缓冲、回调注册、`Read` / `Write` / `Init`），只在需要时创建 |
| `main/mcu_cmsis.h` | include 目标器件 CMSIS 头，并按需补齐中断原语转接 |
| `utility/` | header-only，不认识 MCU |

规则：

1. `main/` 是所有自有代码的唯一根目录；`app/`、`bsp/`、`devices/`、`port/` 一律不建。
2. `main/board/` 必须有。`Board` 是应用获取板载设备的唯一入口。
3. 一外设一目录：`main/serial/serial.hpp`、`main/key/key.hpp`；目录名与类名对应。没有该外设就没有该目录。
4. `main/mcu_cmsis.h` 是移植点，不单独建 `port/` 目录。
5. `application.hpp` / `application.cpp` 只 include `application.hpp`、`board.hpp`、`queue.hpp`、`timers.hpp`、`irq_lock.hpp`、`mcu_cmsis.h` 与标准库；设备类型（如 `Key::Event`）经 `board.hpp` 传递。
6. `board.hpp` / `board.cpp` 负责构造并持有**本工程实际存在的**设备对象。
7. `board.hpp` 用短名引用驱动头（`#include "serial.hpp"`），因此 include 路径要包含 `main/`、`main/board/` 与**每个外设目录**（`main/serial/`、`main/key/`…）；驱动头可以 include 厂商 HAL，但只被 `board.*` include。
8. 每块板一份 `Board`；换板时改 `main/board/` 与 `main/mcu_cmsis.h`，`application.*` 不动。
9. 厂商生成目录、启动文件、链接脚本、IDE 工程文件只做最小追加（1 行入口调用、include 路径、源文件列表），不改既有源码。
10. 所有工程用这一种结构，生成前不做架构选型。

## 三条铁律

1. **中断只做三件事**：清标志、搬数据、投递事件（`push`）。禁止在 ISR 里延时、阻塞发送、`printf`、跑业务循环。
2. **静态内存**：禁止 `new` / `malloc` / `std::vector` / `std::string` / `std::function` / 智能指针。
   设备对象是 `Board` 的成员；事件队列是 `Application` 的成员。
   函数内 `static` 只用于 `Application::GetInstance()` 与 `Board::GetInstance()` 的单例实例，且必须在开中断前首次调用。
3. **共享可变状态一律用 `IrqLock`**：临界区只包住几条语句，不要在锁内调用 HAL 阻塞接口。

## 初始化顺序与职责边界（固定）

```text
厂商 main()          时钟、HAL_Init、MX_*_Init —— 句柄与外设引脚已就绪
Board 构造           构造本工程实际存在的设备对象（只存句柄）；配置厂商没做的板级 GPIO
Application 构造     （有 Timer 任务时 Create 各 Timer）→ PostEvent(START_EVENT)；不接触设备句柄
Application::Start   ① 注册各设备回调 → ② Board::Init() → ③ 进入事件循环
Board::Init()        只调用本工程实际存在的设备 Init()
```

| 事项 | 归谁做 | 说明 |
| --- | --- | --- |
| 系统时钟、`HAL_Init` | 厂商 `main()` | 不搬进 `main/` |
| 外设初始化 `MX_*_Init`（UART/TIM/ADC…） | 厂商 `main()` | 句柄已存在，`Board` 不重复调用 |
| 板级 GPIO（LED、独立按键、片选…） | `Board` 构造 | 厂商没配的才配；引脚常量留在 `board.cpp` |
| 设备对象构造 | `Board` 构造 | 只保存句柄与静态配置 |
| 中断使能（`HAL_*_Receive_IT`、采样 Timer `Start`） | `Board::Init()` → 设备 `Init()` | 全工程唯一的 arm 点 |
| 设备回调注册 | `Application::Start()`，`Board::Init()` 之前 | 注册早于中断使能 |
| 1ms tick | 厂商已有的 tick 位置 | 追加一次 `Timer::OnMilliSecondTick()` |

`Board` 不重复做厂商已经做完的初始化：`HAL_UART_Init` / `MX_USART1_UART_Init` 已经在厂商初始化里执行过，`Board` 只保存句柄并在 `Init()` 里 arm 接收。

## Application：事件循环与投递

`Application` 是整个工程里**唯一**的事件循环，也是唯一持有事件队列的地方，同时是单例。

事件链路只有一条：

```text
任一 ISR（UART / GPIO / TIM / SysTick）
        │  只做三件事：清标志、搬数据、调用 PostXxx()
        ▼
Application::PostXxx()   ──►  Queue<QueueItem, N>（IrqLock 保护）
                                      │
                                      ▼
Application::Start()  while (true)：pop → switch 事件号 → HandleXxx(item)
                                      │
                                      ▼
                   响应：写 Board 暴露的设备、启动 Timer、推进状态机
```

规则：

1. **投递入口只有 `PostXxx()`**：每个都等价于构造一个 `QueueItem` 再 `queue_.Push(item)`。它们会被中断调用，只允许 POD 构造 + `Push`，不得调用 HAL、不得延时、不得解析数据。
2. **`Push` 失败必须计数**：返回 `false` 时在 `IrqLock` 内 `++dropped_`，调试期可加断言，禁止静默丢弃。
3. **`Application::Start()` 永不返回**：`Pop` 失败可以 `continue`，也可以睡眠。睡眠必须"关中断检查 + WFI"，否则检查与睡眠之间到达的中断会丢失唤醒：

   ```cpp
   {
       IrqLock lock;
       if (queue_.Empty()) {
           __WFI();     // 有 pending 中断即唤醒，handler 在解锁后执行
       }
   }
   ```
   `__WFI()` 换成目标内核的等待指令；没有等效指令时直接 `continue` 忙轮询。
4. **事件号集中定义**在 `Application` 里（`static constexpr uint32_t XXXX_EVENT`），其他文件不得自定义事件号。
5. **载荷只放 POD**，优先 ≤ 8 字节，按值放进 `QueueItem` 的 `union`；不放指针（生命周期无法保证）。
6. **事件是 FIFO，没有优先级**：高频事件会挤压低频事件。忙时用位图合并同类事件，或用"读取可用字节数"代替"每字节一个事件"。
7. **handler 必须快速返回且幂等**：handler 的执行时间就是其它事件的等待时间；同一事件可能连续到达，长流程拆成状态机，由后续事件或定时器推进。
8. **handler 内再次 `PostXxx()` 是允许的**（状态机推进），但不要形成自激循环；在中断之外主动触发时同样从 `PostXxx()` 进入，不直接调用 `HandleXxx()`。

## Board：具体类，板级唯一入口

`Board` 是一个具体类，每块板一份实现。同一 MCU 但引脚、外设、器件不同，就改 `Board`，`application.*` 不动。

职责（且只有这些）：

1. 构造并持有**本工程实际存在的**驱动对象，通过 `GetXxx()` 暴露；构造函数只注入句柄与静态配置。
2. 配置厂商初始化没有覆盖的板级 GPIO（LED、独立按键、片选等），引脚常量留在 `board.cpp`。
3. **独占并转发厂商弱回调**，按句柄 / 引脚路由到对应驱动。
4. 提供板级常量与能力查询（引脚定义、时钟、是否支持 DMA）；板上有看门狗时提供 `RefreshWatchdog()`。
5. 提供 `Init()`，只做一件事：按固定顺序调用本工程实际存在设备的 `Init()`。

核心 API（每个工程都有）：

| 成员 | 用途 |
| --- | --- |
| `static Board &GetInstance()` | 全工程唯一实例 |
| `void Init()` | 让本工程实际存在的设备就绪，由 `Application::Start()` 在回调注册之后调用 |

可选 API（**只在本工程使用该外设时出现**，不用的整块不要写）：

| 成员 | 出现条件 |
| --- | --- |
| `Serial<kRx, kTx> *GetSerial()` | 本工程有串口 |
| `Key *GetKey(size_t index)` | 本工程有按键 |
| `Led *GetLed(size_t index)` | 本工程有 LED |
| `void RefreshWatchdog()` | 板上有已启动的看门狗 |

`Serial` 的模板容量按本工程的 RAM 与最大帧长填写，示例的 `128` 只是取值示例。

空骨架阶段的 `Board` 只有 `GetInstance()` 与空的 `Init()`：没有 `GetSerial` / `GetKey` / `GetLed` / `RefreshWatchdog`，没有设备成员，`board.cpp` 里也没有弱回调。

不做：

- 不做时钟 / `HAL_Init` / `MX_*_Init`：这些由厂商 `main()` 负责，重复调用会重置已配置的外设。
- 不放业务逻辑、不做协议解析、不定义事件编号。
- 不缓存事件、不持有事件队列。
- 不给未请求的外设预埋 `GetXxx()`、成员、`Init()` 调用或事件。
- 不在构造函数里 arm 中断或调用驱动 `Init()`。

**中断回调契约**：驱动在中断里发现"有新数据 / 状态变化"时，只调用上层注册的通知回调（带上字节数或标志），不解析内容；上层回调再把结果 `Post` 成事件。

## 可选外设：只在需要时出现

`Board` 的核心只有 `GetInstance()` 与 `Init()`。`GetSerial()` / `GetKey()` / `GetLed()` / `RefreshWatchdog()` 属于可选 API，只在用户要求该外设时出现。

| 可选外设 | 目录 | `Board` 增量 | 详细模板 |
| --- | --- | --- | --- |
| 串口 | `main/serial/serial.hpp` | `GetSerial()` + `Serial` 成员 + `Init()` 里 `serial_.Init()` | [optional-peripherals.md §1](./references/optional-peripherals.md#1-serialuart-中断收发) |
| 按键 | `main/key/key.hpp` | `GetKey(index)` + `Key` 成员 + `Init()` 里 `key_.Init()` | [optional-peripherals.md §2](./references/optional-peripherals.md#2-keygpio-按键消抖--长按) |
| LED | `main/led/led.hpp` | `GetLed(index)` + `Led` 成员 + `Init()` 里 `led_.Init()` | [optional-peripherals.md §3](./references/optional-peripherals.md#3-ledgpio-输出无中断) |
| 周期任务 | 不建目录 | 无（`Timer` 挂在 `Application` / `Board` / 设备上） | [optional-peripherals.md §4](./references/optional-peripherals.md#4-timer周期任务与设备内部时序) |

规则：

1. 需求里没有的外设：不建目录、不加 `GetXxx()`、不加成员、不加事件、不加 include 路径。
2. 一次只加一个外设，编译并验证事件能被 handler 处理后再加下一个。
3. `Board::Init()` 里只调用本工程实际存在的设备。
4. 加完外设按 [optional-peripherals.md §5](./references/optional-peripherals.md#5-每加一个外设的落地清单) 的清单与 §6 的负向检查核对。

## 代码规范（只约束 `main/**` 与 `utility/**`）

厂商生成目录、IDE 工程文件与第三方代码不动。

| 项目 | 规则 |
| --- | --- |
| 目录 | `main/application.*`、`main/mcu_cmsis.h`、`main/board/` 必有；`main/<periph>/` 只在需要该外设时创建 |
| 头文件 | 自写头文件一律 `#pragma once` |
| 文件命名 | `snake_case.hpp` / `snake_case.cpp`（`application.cpp`、`board.cpp`、`serial.hpp`） |
| 类名 | `main/` 与 `utility/` 都用 PascalCase：`Application`、`Board`、`Serial` |
| 方法 | `main/` 与 `utility/` 都用 PascalCase：`Start`、`Init`、`GetSerial`、`Push`、`Pop`、`Create` |
| 成员变量 | 后置 `_` |
| 常量 | 事件号与容量用 `UPPER_SNAKE_CASE`（`START_EVENT`、`QUEUE_LENGTH`）；模板参数用 `kRxSize` / `kTxSize` |
| include | `application.cpp` 只 include `application.hpp`、`board.hpp`、`queue.hpp`、`timers.hpp`、`irq_lock.hpp`、`mcu_cmsis.h` 与标准库 |
| 板级访问 | `application.*` 只能通过 `board.hpp` 取设备；驱动头只被 `board.*` include |
| 厂商符号 | 类型 / 函数 / 枚举 / 宏只出现在 `main/board/` 与驱动头文件；`application.*` 内为零 |
| 初始化 | `Board` 构造只配置；驱动构造只存句柄；`Application::Start()` 注册回调后调用 `Board::Init()` |
| 内存 | 禁止 `new` / `malloc` / `std::vector` / `std::string` / `std::function` / 智能指针；`std::array` / `std::tuple` 只作固定大小值类型 |
| 共享状态 | 一律 `IrqLock`；`dropped_++` 也在锁内 |
| 返回值 | `queue_.Push`、`sb_.Write`、`Read` / `Write` 的返回值必须处理，失败计入 `dropped_` 或断言 |
| ISR | 只清标志、搬 1 字节、`push`；禁止延时、`printf`、阻塞发送、长循环 |
| 错误 | 应用层用 `bool` / `size_t`；厂商状态类型只在驱动与 `Board` 内部出现 |
| 编译 | C++17；`-fno-exceptions -fno-rtti -Wall -Wextra`（第三方单独关闭告警） |
| 单例 | `Application`、`Board` 私有构造 + 删除拷贝 + `GetInstance()` |
| 注释 | 中文，解释"为什么"；不写无信息注释 |

## 检查命令（在工程根目录执行，只扫自有代码）

```powershell
# 应用层不得 include 驱动头与厂商 HAL
rg -n -g 'main/application.*' '#include\s+"(<|serial|usart|hal|gpio|device)' main/

# 应用层不得出现厂商 API / 句柄
rg -n -g 'main/application.*' '\b(HAL_|LL_|huart|htim|hadc)' main/

# 自有代码不得动态分配（只扫 main/ 与本工程的 utility 头文件，跳过厂商 SDK）
rg -n '\b(new|malloc|std::vector|std::string|std::function|std::shared_ptr)\b' main/ utility/queue.hpp utility/stream_buffer.hpp utility/timers.hpp utility/irq_lock.hpp

# 丢弃计数必须在锁内
rg -n '\+\+dropped_' main/ utility/queue.hpp utility/stream_buffer.hpp utility/timers.hpp utility/irq_lock.hpp

# 队列 / 缓冲 / 写接口的返回值必须出现在条件或赋值里
rg -n '(queue_\.Push|sb_\.Write)' main/

# 设备句柄只允许出现在 Start() 里（不得出现在 Application 构造函数）
rg -n -A 3 'Application::Application' main/application.cpp

# Board 不得重复厂商初始化
rg -n '\b(HAL_Init|SystemClock_Config|MX_.*_Init)\b' main/

# 空骨架阶段：Board 不应出现可选外设
rg -n 'GetSerial|GetKey|GetLed|RefreshWatchdog' main/board/

# 空骨架阶段：main/ 下不应有多余外设目录
Get-ChildItem main -Directory | Select-Object -ExpandProperty Name
```

## 完成标准（DoD）

骨架阶段（每个工程都要过）：

- [ ] `main/board/` 存在，`Board` 是应用获取板载设备的唯一入口
- [ ] `Board` 只有 `GetInstance()` 与 `Init()`；没有 `GetSerial` / `GetKey` / `GetLed` / `RefreshWatchdog`，没有设备成员
- [ ] `main/` 下没有需求里未提及的外设目录
- [ ] 入口调用已接入厂商 `main()`，只新增一行
- [ ] include 路径包含 `main/`、`main/board/`、`utility/`（有外设时再加对应目录）
- [ ] 实际工具链编译通过，`START_EVENT` 命中一次
- [ ] `Board` 构造不 arm 中断，`Application::Start()` 里先注册回调（若有）再 `Board::Init()`
- [ ] 时钟 / `HAL_Init` / `MX_*_Init` 仍在厂商 `main()` 里，`Board` 没有重复调用
- [ ] 自写头文件都有 `#pragma once`，`GetInstance()` 拼写正确
- [ ] `Board` 私有构造 + 删除拷贝

外设与业务阶段（只对本工程实际存在的外设适用）：

- [ ] 每个外设都在需求里被明确要求；`Board` 里只有这些外设的 getter / 成员 / `Init()` 调用
- [ ] `application.*` 内没有驱动头、HAL 头与厂商句柄
- [ ] 所有设备回调都在 `Application::Start()` 里、`Board::Init()` 之前注册
- [ ] 驱动构造只存句柄，`Init()` 里才 arm 中断
- [ ] 若使用 `Serial`：`HandleXxxEvent()` 先取局部字节，再重挂接收与比较
- [ ] 若使用 `Key` / `Led`：接口与语义符合 [optional-peripherals.md](./references/optional-peripherals.md) 的契约
- [ ] 若使用 `Timer`：`Create` 在构造函数、`Start` 在 `Start()` / `Init()` 里
- [ ] `dropped_++` 在 `IrqLock` 内
- [ ] 所有 `queue_.Push` / `sb_.Write` / `Write` 返回值有处理
- [ ] 用实际工具链编译通过，无新增告警
- [ ] 上电 smoke test：tick 周期正确、无 HardFault；有串口时看串口输出，没有串口时用 GPIO 翻转或调试变量验证

## utility 头文件用法

| 头文件 | 谁 include | 用途 |
| --- | --- | --- |
| `queue.hpp` | `application.hpp` | 事件队列 |
| `timers.hpp` | 持有时钟任务的一方：`application.hpp`、`key.hpp` 或 `board.hpp` | 1ms 软件定时器 |
| `irq_lock.hpp` | `application.cpp`、驱动内部 | 临界区 |
| `stream_buffer.hpp` | 驱动头文件（如 `serial.hpp`） | 字节流环形缓冲 |

| 组件 | 签名 | 说明 |
|---|---|---|
| `Queue<T, N>` | `Push(const T&)` / `Pop(T&)` / `Empty()` / `Full()` / `Size()` / `Clear()` | 多中断生产者 + 单主循环消费者（`Push` 内部已加锁），满时返回 `false` |
| `StreamBuffer<N>` | `Write(const void*, size_t)` / `Read(void*, size_t)` / `Peek` / `Skip` / `Size` / `Space` / `Empty` / `Clear` | 字节流环形缓冲，保留一个空位区分满 / 空 |
| `Timer` | `Create(period_ms, reload, cb, ctx)` / `Start` / `Stop` / `Reset` / `ChangePeriod` / `Destroy` / `IsCreated` / `IsActive` / `PeriodMs` | 静态链表软件定时器，`period_ms >= 1` |
| `Timer::OnMilliSecondTick()` | `static void` | 由 1ms 中断调用，回调在 ISR 上下文执行 |
| `IrqLock` | RAII，构造关中断、析构恢复 | 不可拷贝；不要跨越长逻辑持有 |

新代码统一使用 `timers.hpp` 的 `Timer`，不另写毫秒计数。

## 生成新功能的标准流程

1. **先确认需求清单**：把用户请求逐条翻译成"要不要某个外设"。没有提到的外设，后面所有步骤都不生成。
2. **需要新资源**：在 `Board` 增加 `GetXxx()`，构造里注入句柄并配置厂商没做的板级 GPIO，`Board::Init()` 里调用设备 `Init()`，同时实现并转发厂商弱回调。应用层不接触句柄。
3. **需要新外设驱动**：在 `main/<periph>/` 建具体类，由 `Board` 持有并暴露；构造函数只存句柄 + `Timer::Create`，`Init()` 里 arm（`HAL_*_Receive_IT`、`Timer::Start`）。
4. **定事件**：在 `Application` 里加 `static constexpr uint32_t XXX_EVENT`。带载荷的走 `queue`，纯电平 / 标志类事件用 `volatile` 位图。
5. **定载荷**：在 `QueueItem` 的 `union` 里加 POD 成员，优先 ≤ 8 字节。
6. **写投递函数**：`PostXxx()` 只构造 `QueueItem` 并 `Push`，失败时在 `IrqLock` 内计数。
7. **写 handler**：`Application::Start()` 的 `switch` 加 case 与 `HandleXxx()`，立即返回；等待过程拆状态机。
8. **要定时就挂 Timer**：回调里只 `PostXxx()`，不写阻塞延时，不自己数 tick。
9. **验证**：用工程实际工具链编译（C++17、`-fno-exceptions -fno-rtti`；GCC 额外 `-Wall -Wextra`），以构建日志为准。

## 平台适配与工具链

- 可选外设模板（Serial / Key / Led / Timer）→ [references/optional-peripherals.md](./references/optional-peripherals.md)
- 移植点、中断原语、1ms tick、回调上下文 → [references/porting.md](./references/porting.md)
- EIDE / Keil MDK（AC6）/ GCC 配置与验证 → [references/toolchains.md](./references/toolchains.md)
- 平台契约模板 → [references/platform-profile.md](./references/platform-profile.md)

## 移植三步

1. 提供 `main/mcu_cmsis.h`，include 目标器件的 CMSIS 头。
2. 目标平台没有 `__get_PRIMASK` / `__disable_irq` / `__set_PRIMASK` 时，在 `mcu_cmsis.h` 里补齐同名转接，**不要改 `irq_lock.hpp`**。
3. 在 1ms 中断里调用一次 `Timer::OnMilliSecondTick()`：找到工程已有的 1ms 位置（SysTick 中断或 HAL 的 tick 函数）追加这一次调用，不要新建重复的 tick 源。

> 本仓库是 header-only，加入工程的 **include 路径**即可：`main/`、`main/board/`、每个外设目录（`main/serial/`、`main/key/`…）、`utility/`。
> 若必须加入 `srcDirs`，把 `main/` 与 `utility/` 加进去即可，`utility/` 里没有 `.cpp`。

## 反模式

| 反模式 | 做法 |
|---|---|
| 建 `app/`、`bsp/`、`devices/`、`port/` 多级目录 | 自有代码全部放 `main/`，外设按 `main/<periph>/` 分目录 |
| 把示例里的具体句柄、头文件名、回调名直接抄到新 MCU | 先读基线取值，再换成目标工程的实际名字 |
| 一次生成全部外设再编译 | 先空骨架编译通过，再逐个外设 |
| 需求里没提串口/按键/LED，却在 `Board` 里预埋 `GetSerial` / `GetKey` / `GetLed` | 只生成核心 API，外设按需再加 |
| 复制示例时把示例用到的所有外设一起带上 | 只保留需求里有的外设，其余整段删除 |
| 为了过 smoke test 而补一个没被要求的外设 | smoke test 用 GPIO 翻转或调试变量验证 |
| `Board` 写成抽象基类 + 工厂 / 注册宏 | `Board` 直接持有具体驱动成员并返回具体类型指针 |
| 在 `board.cpp` 之外创建驱动对象 | 设备对象作为 `Board` 成员，经 `GetXxx()` 取出 |
| `Board` 构造里启动接收 / 调用驱动 `Init()` | 构造只建对象，中断在 `Board::Init()` 里 arm |
| 驱动构造里 arm 接收 | 构造只存句柄，`Init()` 里 arm |
| 把 `HAL_Init` / `SystemClock_Config` / `MX_*_Init` 搬进 `Board` | 留在厂商 `main()`，`Board` 只保存句柄 |
| 在 `Application` 构造函数里注册设备回调 | 放在 `Application::Start()` 里、`Board::Init()` 之前 |
| `board.hpp` 用短名 include 驱动头，却没把 `main/serial/` 等目录加进 include 路径 | 每个外设目录都要加进 include 路径 |
| 按键消抖 / 长按计时写在 `Application` | 放在 `Key` 驱动里，用 `Timer` 采样，回调只 `PostXxx()` |
| `dropped_++` 不加锁 | `IrqLock` 内自增 |
| `HandleXxxEvent()` 先重挂接收再比较暂存字节 | 先取局部 `byte`，再重挂、比较 |
| 忽略 `queue_.Push` / `sb_.Write` / `Write` 返回值 | 失败计入 `dropped_` 或断言 |
| `Write` 里先 `memcpy` 再调发送 API | 先检查 `size`、容量与发送空闲，再拷贝 |
| ISR 里 `printf` / 延时 / 阻塞发送 | 投递事件，主循环处理 |
| 回调里解析协议、写 Flash | 回调只 `push`，业务放 handler |
| 应用层 include 厂商 HAL、操作句柄 | 只通过 `Board::GetXxx()` 取驱动 |
| `new` / STL 容器 / `std::function` | 静态数组、函数指针 + `void *ctx` |
| 函数内 `static` 惰性构造外设 | 设备对象作为 `Board` 成员，开中断前完成构造 |
| 整个函数用 `IrqLock` 包住 | 只保护几条共享变量的读写 |
| 自己写毫秒计数 | 用 `Timer` |
| 多个模块各自实现同一个厂商弱回调 | 由 `Board` 统一实现并按句柄 / 引脚路由 |
| 用中断接收 API 却实现成别的通道的回调 | 回调名与所用 API 对齐 |
| 厂商工程已启动看门狗却不喂 | 用 `Timer` 周期投递事件，在 handler 里调用 `Board::RefreshWatchdog()` |
| 驱动构造里初始化 GPIO、析构里反初始化 | 初始化归 `Board`，驱动只读写电平 |
| 构造函数里读取器件初值 | 提供 `Init()`，由 `Board::Init()` 调用 |
| 自写头文件缺 `#pragma once`，或 `GetInstance` 拼错 | 每个头文件首行 `#pragma once`，单例命名统一 |
| 单例构造函数 public、拷贝未删除 | 私有构造 + `Board(const Board &) = delete` |
| 修改厂商生成的源码来放业务 | 只在厂商 `main()` 追加一行入口调用 |

**记住**：核心规则平台无关；平台语义必须通过基线确认，不要凭型号记忆生成代码。
