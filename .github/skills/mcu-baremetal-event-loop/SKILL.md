---
name: mcu-baremetal-event-loop
description: '编写或改造裸机 MCU（Cortex-M / RISC-V 嵌入式 C++）代码时使用本仓库（queue.hpp、stream_buffer.hpp、timers.hpp、irq_lock.hpp）已有的工程模式：自有代码全部放在 main/（application.*、mcu_cmsis.h、必有的 board/、按外设可选的 serial/ 等）；中断只做清标志 / 搬数据 / 投递事件，由 Application 单例的事件循环统一派发；Board 是具体类，也是应用获取板载设备的唯一入口，设备层用 Board::Init() 集中 arm 中断，Application::Start() 进入永不返回的事件循环；生成顺序固定为"读厂商基线 → 空骨架先编译通过 → 逐个外设扩展"，并给出厂商 HAL 串口驱动骨架、静态内存 + irq_lock + 1ms 软件定时器约束、命名 / 返回值 / ripgrep 检查规范。Use when scaffolding a bare-metal project on any MCU and any IDE, porting this library into an existing vendor project, or adding peripherals, events, timers, UART/串口 protocols, interrupts, drivers and a board layer following these patterns instead of inventing new ones.'
argument-hint: '目标 MCU + IDE / 工具链 + 要新增的外设或事件，例如"M0+ 内核 + 厂商 SDK + 加一个按键长按事件"'
---

# 裸机事件驱动工程模式（平台无关核心）

本技能约束三件事：**工程结构**、**中断 / 主循环边界**、**自用代码规范**。

不同 MCU、厂商 SDK、IDE 生成的初始工程各不相同（目录名、HAL 头、句柄变量、弱回调名、tick 位置、工程文件格式都不同）。因此本技能：

- 只固定 `main/` 下的自有代码结构，不假设厂商目录叫什么；
- 第 2 步的驱动与 `Board` 示例用厂商 HAL 的串口写法；换 MCU 时把示例里的头文件、句柄、回调名换成第 0 步读到的实际名字，结构保持不变；
- 生成顺序固定为"**空骨架先跑通，再逐个外设**"。

## 生成顺序（按阶段推进，每阶段结束都要编译）

| 阶段 | 产出 | 结束条件 |
| --- | --- | --- |
| 0 读基线 | 填写平台契约（厂商头、句柄、回调名、tick 位置） | 基线表每项都有实际值或 TODO |
| 1 空骨架 | `main/mcu_cmsis.h`、`main/board/`、`main/application.*`、1 处入口调用 | 实际工具链编译通过，`START_EVENT` 命中一次 |
| 2 单个外设 | `main/<periph>/<periph>.hpp` + `Board` 成员 + 1 组事件 | 编译通过，外设事件能被 handler 处理 |
| 3 业务 | handler 状态机、定时任务 | [完成标准](#完成标准dod) 全部勾选 |

**先跑通空骨架。** 一次生成全部外设会让编译错误和初始化顺序问题混在一起，定位成本远高于分两次生成。

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
```

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

	// 各驱动的中断在这里 arm；骨架阶段为空实现
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

## 第 2 步：加第一个外设

固定四件事，顺序不能换：

1. 建驱动：`main/<periph>/<periph>.hpp`（需要时再配 `.cpp`）。构造函数只存句柄，`Init()` 里才 arm 中断。
2. `Board`：加成员 + `GetXxx()` + 在 `Board::Init()` 里调用驱动 `Init()` + 转发 HAL 弱回调。
3. `Application`：加 `static constexpr uint32_t XXX_EVENT`、`PostXxx()`、`case` 与 `HandleXxx()`。
4. 编译并确认事件能被处理，再开始下一个外设。

以厂商 HAL 库的串口为例（下面取自 PY32 的 USART1，其他厂商按同样结构替换）：

```cpp
// main/serial/serial.hpp
#pragma once

#include "py32f0xx_hal.h"

#include <tuple>
#include <array>
#include <cstring>

#include "stream_buffer.hpp"

template <size_t kRxSize, size_t kTxSize>
class Serial {
public:
	using _CharacterCallback = void (*)(size_t, void *);
	using _FullCallback = void (*)(size_t, void *);

	Serial(UART_HandleTypeDef *huart)
		: huart_(huart) {
	}

	~Serial() = default;

	void Init() {
		HAL_UART_Receive_IT(huart_, &temp_, sizeof(temp_));
	}

	void SetRxBufferFullCallback(_FullCallback cb, void *ctx) {
		full_entry_ = std::make_tuple(cb, ctx);
	}

	void SetSpecificCharactersCallback(char sc, _CharacterCallback cb, void *ctx) {
		character_entry_ = std::make_tuple(sc, cb, ctx);
	}

	size_t Write(const void *src, size_t size) {
		if (size == 0 || size > kTxSize || huart_->gState != HAL_UART_STATE_READY) {
			return 0; // 超长或发送未就绪，直接返回0，避免覆盖仍在发送的缓冲
		}
		memcpy(tx_buffer_.data(), src, size);
		HAL_StatusTypeDef status = HAL_UART_Transmit_IT(huart_, tx_buffer_.data(), static_cast<uint16_t>(size));
		return status == HAL_OK ? size : 0;
	}

	size_t Read(void *const dest, size_t size) {
		return sb_.Read(dest, size);
	}

	void HandleRxCpltEvent() {
		const uint8_t byte = temp_;

		sb_.Write(&byte, sizeof(byte));
		HAL_UART_Receive_IT(huart_, &temp_, sizeof(temp_));

		{ // 执行特定字符接收回调函数
			const auto &[c, cb, ctx] = character_entry_;
			if (c && c == byte && cb) {
				cb(sb_.Size(), ctx);
			}
		}

		{ // 执行缓冲区满回调函数
			const auto &[cb, ctx] = full_entry_;
			if (!sb_.Space() && cb) {
				cb(sb_.Size(), ctx);
			}
		}
	}

private:
	UART_HandleTypeDef *huart_ = nullptr;
	StreamBuffer<kRxSize> sb_;
	std::array<uint8_t, kTxSize> tx_buffer_ = {};
	uint8_t temp_ = 0;

	std::tuple<char, _CharacterCallback, void *> character_entry_ = {};
	std::tuple<_FullCallback, void *> full_entry_ = {};
};
```

写驱动遵循：

1. 构造函数只保存句柄与静态配置，不碰寄存器、不 arm 中断；中断使能放在 `Init()`。
2. 厂商类型 / 函数只在 `main/board/` 与驱动头文件里出现，`application.*` 内为零。
3. `HandleXxxEvent()` 先取局部副本，再写缓冲、重挂接收、比较字节。
4. `Write` 先检查 `size == 0`、容量与发送空闲，再 `memcpy`，避免覆盖仍在发送的缓冲。
5. 回调注册用函数指针 + `void *ctx`，禁止 `std::function`。
6. 析构里不写厂商 `*_DeInit`：静态对象在裸机上永不析构，只会拖入 `__cxa_atexit` 白占 flash。
7. 驱动不定义事件号、不解析协议、不写业务逻辑。

`Board` 的增量：

```cpp
// board.hpp
#pragma once

#include "serial.hpp"

class Board {
public:
	static Board &GetInstance() {
		static Board instance;
		return instance;
	}

	Board(const Board &) = delete;
	Board &operator=(const Board &) = delete;

	Serial<128, 128> *GetSerial();

	void Init();   // arm 各驱动中断，由 Application::Start() 调用

private:
	Board();

	Serial<128, 128> serial_;
};

// board.cpp
#include "board.hpp"

#include "usart.h"   // 厂商外设初始化头，提供 husart1 等句柄

Board::Board()
	: serial_(&husart1) {}   // 只保存句柄

Serial<128, 128> *Board::GetSerial() {
	return &serial_;
}

void Board::Init() {
	serial_.Init();
}

// 厂商弱回调只在这里实现一次，按句柄路由
extern "C" void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	if (huart->Instance != USART1) {
		return;
	}
	Board::GetInstance().GetSerial()->HandleRxCpltEvent();
}
```

## 工程结构（固定）

```text
<project>/
├── main/                       # 自有代码的唯一根目录
│   ├── application.hpp / .cpp
│   ├── mcu_cmsis.h             # 唯一的 MCU 依赖点
│   ├── board/                  # 每个工程必须有
│   │   ├── board.hpp
│   │   └── board.cpp
│   └── serial/ key/ led/ ...   # 按外设创建，一外设一目录
├── utility/                    # queue.hpp / stream_buffer.hpp / timers.hpp / irq_lock.hpp
├── <厂商生成目录>/              # 名称随厂商与 IDE：Core/、Drivers/、Src/、Inc/、startup、.ld/.sct
└── <IDE 工程目录>/              # 名称随 IDE：.eide/、MDK-ARM/、Makefile、CMakeLists.txt
```

| 路径 | 职责 |
| --- | --- |
| `main/application.*` | 事件号、`QueueItem`、`PostXxx()`、`HandleXxx()`、事件循环 |
| `main/board/` | 持有并初始化 HAL 句柄，构造驱动对象，暴露 `GetXxx()`，转发 HAL 回调 |
| `main/<periph>/` | 具体外设驱动：缓冲、回调注册、`Read` / `Write` / `Init` |
| `main/mcu_cmsis.h` | include 目标器件 CMSIS 头，并按需补齐中断原语转接 |
| `utility/` | header-only，不认识 MCU |

规则：

1. `main/` 是所有自有代码的唯一根目录；`app/`、`bsp/`、`devices/`、`port/` 一律不建。
2. `main/board/` 必须有。`Board` 是应用获取板载设备的唯一入口。
3. 一外设一目录：`main/serial/serial.hpp`、`main/key/key.hpp`；目录名与类名对应。
4. `main/mcu_cmsis.h` 是移植点，不单独建 `port/` 目录。
5. `application.hpp` / `application.cpp` 只 include `application.hpp`、`board.hpp`、`queue.hpp`、`timers.hpp`、`irq_lock.hpp`、`mcu_cmsis.h` 与标准库。
6. `board.hpp` / `board.cpp` 负责构造并持有具体设备对象（`Serial`、`Key`、`Led`）。
7. 驱动头文件可以 include 厂商 HAL；它只被 `board.hpp` / `board.cpp` include。
8. 每块板一份 `Board`；换板时改 `main/board/` 与 `main/mcu_cmsis.h`，`application.*` 不动。
9. 厂商生成目录、启动文件、链接脚本、IDE 工程文件只做最小追加（1 行入口调用、include 路径、源文件列表），不改既有源码。
10. 所有工程用这一种结构，生成前不做架构选型。

## 三条铁律

1. **中断只做三件事**：清标志、搬数据、投递事件（`push`）。禁止在 ISR 里延时、阻塞发送、`printf`、跑业务循环。
2. **静态内存**：禁止 `new` / `malloc` / `std::vector` / `std::string` / `std::function` / 智能指针。
   设备对象是 `Board` 的成员；事件队列是 `Application` 的成员。
   函数内 `static` 只用于 `Application::GetInstance()` 与 `Board::GetInstance()` 的单例实例，且必须在开中断前首次调用。
3. **共享可变状态一律用 `IrqLock`**：临界区只包住几条语句，不要在锁内调用 HAL 阻塞接口。

## 初始化顺序（固定）

```text
Board 构造           只保存 HAL 句柄、配置时钟与 GPIO，不 arm 中断
Application 构造     创建 Timer、向 Board 注册回调、PostEvent(START_EVENT)
Application::Start   先 Board::Init()（各驱动此刻才 arm 中断）→ 进入事件循环
```

回调注册因此一定早于中断使能，注册前到达的数据不会丢失，单例也不会在 ISR 里首次构造。

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
3. **`Application::Start()` 永不返回**：`pop` 失败可以 `continue`，也可以用 `__WFI()` 之类睡眠指令（1ms tick 会唤醒，最坏唤醒延迟 1ms）。
4. **事件号集中定义**在 `Application` 里（`static constexpr uint32_t XXXX_EVENT`），其他文件不得自定义事件号。
5. **载荷只放 POD**，优先 ≤ 8 字节，按值放进 `QueueItem` 的 `union`；不放指针（生命周期无法保证）。
6. **事件是 FIFO，没有优先级**：高频事件会挤压低频事件。忙时用位图合并同类事件，或用"读取可用字节数"代替"每字节一个事件"。
7. **handler 必须快速返回且幂等**：handler 的执行时间就是其它事件的等待时间；同一事件可能连续到达，长流程拆成状态机，由后续事件或定时器推进。
8. **handler 内再次 `PostXxx()` 是允许的**（状态机推进），但不要形成自激循环；在中断之外主动触发时同样从 `PostXxx()` 进入，不直接调用 `HandleXxx()`。

## Board：具体类，板级唯一入口

`Board` 是一个具体类，每块板一份实现。同一 MCU 但引脚、外设、器件不同，就改 `Board`，`application.*` 不动。

职责（且只有这些）：

1. 持有并初始化 HAL 句柄，完成板级初始化（时钟使能、GPIO 初始化、外设 `*_Init`）。
2. 构造并持有驱动对象（`Serial`、`Key`、`Led`…），通过 `GetXxx()` 暴露。
3. **独占并转发厂商弱回调**，按句柄 / 引脚路由到对应驱动。
4. 提供板级常量与能力查询（引脚定义、时钟、是否支持 DMA、`RefreshWatchdog()`）。
5. 提供 `Init()`，集中 arm 各驱动的中断（需要撤除时配 `Stop()`）。

固定 API 表（所有工程同名同义）：

| 成员 | 用途 |
| --- | --- |
| `static Board &GetInstance()` | 全工程唯一实例 |
| `Serial<kRx, kTx> *GetSerial()` | 串口驱动 |
| `Key *GetKey(size_t index)` / `Led *GetLed(size_t index)` | 同类型多实例按 index 取 |
| `void Init()` | 初始化各驱动并 arm 中断，由 `Application::Start()` 调用 |
| `void RefreshWatchdog()` | 板上有看门狗时提供 |

不做：

- 不放业务逻辑、不做协议解析、不定义事件编号。
- 不缓存事件、不持有事件队列。
- 不在构造函数里 arm 中断或调用驱动 `Init()`。

**中断回调契约**：驱动在中断里发现"有新数据 / 状态变化"时，只调用上层注册的通知回调（带上字节数或标志），不解析内容；上层回调再把结果 `Post` 成事件。

## 代码规范（只约束 `main/**` 与 `utility/**`）

厂商生成目录、IDE 工程文件与第三方代码不动。

| 项目 | 规则 |
| --- | --- |
| 目录 | `main/application.*`、`main/mcu_cmsis.h`、`main/board/` 必有；`main/<periph>/` 按外设可选 |
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
```

## 完成标准（DoD）

骨架阶段：

- [ ] `main/board/` 存在，`Board` 是应用获取板载设备的唯一入口
- [ ] 入口调用已接入厂商 `main()`，只新增一行
- [ ] 实际工具链编译通过，`START_EVENT` 命中一次
- [ ] `Board` 构造不 arm 中断，`Application::Start()` 注册回调后再 `Board::Init()`
- [ ] 自写头文件都有 `#pragma once`，`GetInstance()` 拼写正确
- [ ] `Board` 私有构造 + 删除拷贝，设备对象是 `Board` 成员

外设与业务阶段：

- [ ] `application.*` 内没有驱动头、HAL 头与厂商句柄
- [ ] 驱动构造只存句柄，`Init()` 里才 arm 中断
- [ ] `HandleXxxEvent()` 先取局部字节，再重挂接收与比较
- [ ] `dropped_++` 在 `IrqLock` 内
- [ ] 所有 `queue_.Push` / `sb_.Write` / `Write` 返回值有处理
- [ ] 用实际工具链编译通过，无新增告警
- [ ] 上电 smoke test：串口有输出、tick 周期正确、无 HardFault

## utility 头文件用法

| 头文件 | 谁 include | 用途 |
| --- | --- | --- |
| `queue.hpp` | `application.hpp` | 事件队列 |
| `timers.hpp` | `application.hpp`（用到 `Timer` 时） | 1ms 软件定时器 |
| `irq_lock.hpp` | `application.cpp`、驱动内部 | 临界区 |
| `stream_buffer.hpp` | 驱动头文件（如 `serial.hpp`） | 字节流环形缓冲 |

| 组件 | 签名 | 说明 |
|---|---|---|
| `Queue<T, N>` | `Push(const T&)` / `Pop(T&)` / `Empty()` / `Full()` / `Size()` / `Clear()` | 单生产者单消费者事件队列，满时 `Push` 返回 `false` |
| `StreamBuffer<N>` | `Write(const void*, size_t)` / `Read(void*, size_t)` / `Peek` / `Skip` / `Size` / `Space` / `Empty` / `Clear` | 字节流环形缓冲，保留一个空位区分满 / 空 |
| `Timer` | `Create(period_ms, reload, cb, ctx)` / `Start` / `Stop` / `Reset` / `ChangePeriod` / `Destroy` / `IsCreated` / `IsActive` / `PeriodMs` | 静态链表软件定时器，`period_ms >= 1` |
| `Timer::OnMilliSecondTick()` | `static void` | 由 1ms 中断调用，回调在 ISR 上下文执行 |
| `IrqLock` | RAII，构造关中断、析构恢复 | 不可拷贝；不要跨越长逻辑持有 |

新代码统一使用 `timers.hpp` 的 `Timer`，不另写毫秒计数。

## 生成新功能的标准流程

1. **需要新资源**：在 `Board` 增加 `GetXxx()`，在 `main/board/` 里完成厂商初始化与回调转发。应用层不接触句柄。
2. **需要新外设驱动**：在 `main/<periph>/` 建具体类，由 `Board` 持有并暴露；构造函数只存句柄，`Init()` 里 arm。
3. **定事件**：在 `Application` 里加 `static constexpr uint32_t XXX_EVENT`。带载荷的走 `queue`，纯电平 / 标志类事件用 `volatile` 位图。
4. **定载荷**：在 `QueueItem` 的 `union` 里加 POD 成员，优先 ≤ 8 字节。
5. **写投递函数**：`PostXxx()` 只构造 `QueueItem` 并 `Push`，失败时在 `IrqLock` 内计数。
6. **写 handler**：`Application::Start()` 的 `switch` 加 case 与 `HandleXxx()`，立即返回；等待过程拆状态机。
7. **要定时就挂 Timer**：回调里只 `PostXxx()`，不写阻塞延时，不自己数 tick。
8. **验证**：用工程实际工具链编译（C++17、`-fno-exceptions -fno-rtti`；GCC 额外 `-Wall -Wextra`），以构建日志为准。

## 平台适配与工具链

- 移植点、中断原语、1ms tick、回调上下文 → [references/porting.md](./references/porting.md)
- EIDE / Keil MDK（AC6）/ GCC 配置与验证 → [references/toolchains.md](./references/toolchains.md)
- 平台契约模板 → [references/platform-profile.md](./references/platform-profile.md)

## 移植三步

1. 提供 `main/mcu_cmsis.h`，include 目标器件的 CMSIS 头。
2. 目标平台没有 `__get_PRIMASK` / `__disable_irq` / `__set_PRIMASK` 时，在 `mcu_cmsis.h` 里补齐同名转接，**不要改 `irq_lock.hpp`**。
3. 在 1ms 中断里调用一次 `Timer::OnMilliSecondTick()`：找到工程已有的 1ms 位置（SysTick 中断或 HAL 的 tick 函数）追加这一次调用，不要新建重复的 tick 源。

> 本仓库是 header-only，加入工程的 **include 路径**即可（`main/`、`main/board/`、`utility/`）。
> 若必须加入 `srcDirs`，把 `main/` 与 `utility/` 加进去即可，`utility/` 里没有 `.cpp`。

## 反模式

| 反模式 | 做法 |
|---|---|
| 建 `app/`、`bsp/`、`devices/`、`port/` 多级目录 | 自有代码全部放 `main/`，外设按 `main/<periph>/` 分目录 |
| 把示例里的具体句柄、头文件名、回调名直接抄到新 MCU | 先读基线取值，再换成目标工程的实际名字 |
| 一次生成全部外设再编译 | 先空骨架编译通过，再逐个外设 |
| `Board` 写成抽象基类 + 工厂 / 注册宏 | `Board` 直接持有具体驱动成员并返回具体类型指针 |
| 在 `board.cpp` 之外创建驱动对象 | 设备对象作为 `Board` 成员，经 `GetXxx()` 取出 |
| `Board` 构造里启动接收 / 调用驱动 `Init()` | 构造只配置，中断在 `Board::Init()` 里 arm |
| 驱动构造里 arm 接收 | 构造只存句柄，`Init()` 里 arm |
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
