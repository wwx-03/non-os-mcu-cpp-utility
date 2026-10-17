# 适配层与骨架模板

本文件只给**骨架与契约**：`Serial`、`Timer` 这类驱动实现属于具体板子的 BSP，本仓库不提供，也不建议照抄。换板时按新硬件与 HAL 能力重写，保持接口语义即可。

## 1. 移植点：mcu_cmsis.h

```cpp
#pragma once

#include "py32f0xx.h"   // 换 MCU 时只改这一行，例如 "stm32f1xx.h"
```

`irq_lock.hpp` 只依赖这个头。若目标架构没有 `__get_PRIMASK/__disable_irq/__set_PRIMASK`，在 `irq_lock.hpp` 内部改写实现，保持 RAII 接口不变：

```cpp
class irq_lock {
public:
	irq_lock() { /* 保存并关闭全局中断 */ }
	~irq_lock() { /* 恢复 */ }
	irq_lock(const irq_lock &) = delete;
	irq_lock &operator=(const irq_lock &) = delete;
};
```

## 2. tick 接入（1ms）

```cpp
// tick.cpp —— 覆盖 startup 里的弱符号
#include "timers.hpp"
#include "py32f0xx_hal.h"

extern "C" void SysTick_Handler(void)
{
	HAL_IncTick();                 // HAL 自己的毫秒计数
	timer::OnMilliSecondTick();    // 驱动所有软件定时器
}
```

## 3. BSP：Board 契约

一块板子一份 `Board`。它负责**板级资源与 HAL 回调**，不负责业务。

### 3.1 单例约定（Application / Board 固定）

`Application` 与 `Board` **只能**通过 `GetInstance()` 获取，形态固定为"私有构造 + 函数内 `static` 实例"，不要引入其它单例写法：

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
	Board() = default;   // 构造函数必须私有
};
```

- 禁止：全局对象、自由函数、宏单例、工厂、依赖注入、注册表、允许 public 构造。
- 首次 `GetInstance()` 必须发生在中断使能之前，避免 ISR 触发首次构造。

### 3.2 Board 骨架

```cpp
// board.hpp —— 每个板子自己的实现
#pragma once

#include <stddef.h>
#include <stdint.h>

class Uart;   // 板级驱动：由本板 BSP 提供
class Key;    // 同上

class Board {
public:
	static Board &GetInstance() {       // 单例：唯一获取入口
		static Board instance;
		return instance;
	}

	Board(const Board &) = delete;
	Board &operator=(const Board &) = delete;

	// ---- 板级资源：返回类型与数量由这块板子决定 ----
	Uart *GetUart();
	Key *GetKey();

	// ---- 通知注册：驱动在中断里调用，只上报"有事件"，不解析数据 ----
	void OnUartData(void (*cb)(size_t size, void *ctx), void *ctx);
	void OnKeyPressed(void (*cb)(void *ctx), void *ctx);

	// ---- 仅供 HAL 弱回调调用 ----
	void DispatchUartRx();
	void DispatchExti(uint16_t pin);

private:
	Board() = default;
	// 驱动对象、回调指针、板级常量……
};
```

```cpp
// board.cpp
#include "board.hpp"

#include "gpio.h"
#include "usart.h"

Uart *Board::GetUart() {
	static Uart uart(&huart1);   // 驱动实现由本板 BSP 提供
	return &uart;
}

extern "C" void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	(void)huart;
	Board::GetInstance().DispatchUartRx();   // 只路由，不解析
}

extern "C" void HAL_GPIO_EXTI_Callback(uint16_t pin)
{
	Board::GetInstance().DispatchExti(pin);
}
```

约定：

- 多串口时按 `huart` 指针路由，多 EXTI 时按 `pin` 路由；**不要让多个模块各自实现同一个 HAL 回调**。
- 回调链上任何一层都不得阻塞、不得解析协议、不得直接调用业务函数。
- 板级驱动（串口、按键、LED…）在收到数据时调用注册的通知回调，参数只带"字节数 / 标识"这类信息。
- 应用层通过 `Board` 获取资源，不 `#include` 厂商 HAL 头，也不直接操作 `huart`。

## 4. Application：事件循环骨架

`Application` 是唯一的事件循环，也是唯一持有事件队列的地方。

```cpp
// application.hpp
#pragma once

#include "queue.hpp"
#include "timers.hpp"

class Application {
public:
	static Application &GetInstance() {   // 单例：唯一获取入口
		static Application instance;
		return instance;
	}

	Application(const Application &) = delete;
	Application &operator=(const Application &) = delete;

	void Start();

private:
	static constexpr size_t QUEUE_LENGTH = 16;

	static constexpr uint32_t EVT_START = 0x00000001u;
	static constexpr uint32_t EVT_UART_DATA = 0x00000002u;
	static constexpr uint32_t EVT_TICK = 0x00000003u;
	static constexpr uint32_t EVT_KEY = 0x00000004u;

	struct QueueItem {
		uint32_t event = 0;
		union {
			struct {
				size_t size;
			} uart;
			struct {
				uint8_t id;
			} key;
		} data = {};
	};

	queue<QueueItem, QUEUE_LENGTH> queue_;
	timer tick_;
	uint32_t dropped_ = 0;

	Application();
	~Application() = default;

	// 投递入口：可在中断上下文调用，只做 POD 构造 + push
	void PostEvent(uint32_t e);
	void PostUartData(size_t size);
	void PostKey(uint8_t id);

	// 业务入口：只在 Start() 里调用，必须快速返回
	void HandleStart();
	void HandleUartData(const QueueItem &item);
	void HandleTick();
	void HandleKey(const QueueItem &item);
};
```

```cpp
// application.cpp
#include "application.hpp"
#include "board.hpp"

Application::Application() {
	// 定时器回调运行在 1ms 中断里：只投递事件
	tick_.create(1000, true, +[](void *ctx) {
		static_cast<Application *>(ctx)->PostEvent(EVT_TICK);
	}, this);

	// BSP 回调运行在中断里：只投递事件
	Board::GetInstance().OnUartData(+[](size_t size, void *ctx) {
		static_cast<Application *>(ctx)->PostUartData(size);
	}, this);

	Board::GetInstance().OnKeyPressed(+[](void *ctx) {
		static_cast<Application *>(ctx)->PostEvent(EVT_KEY);
	}, this);

	PostEvent(EVT_START);   // 初始事件，主循环启动后第一个处理
}

void Application::Start() {
	QueueItem item;

	while (true) {
		if (!queue_.pop(item)) {
			continue;   // 需要省电时改为 __WFI()：SysTick 每 1ms 唤醒一次
		}

		switch (item.event) {
		case EVT_START:
			HandleStart();
			break;
		case EVT_UART_DATA:
			HandleUartData(item);
			break;
		case EVT_TICK:
			HandleTick();
			break;
		case EVT_KEY:
			HandleKey(item);
			break;
		default:
			break;
		}
	}
}

void Application::PostEvent(const uint32_t e) {
	QueueItem item;
	item.event = e;
	if (!queue_.push(item)) {
		++dropped_;   // 仅统计用；需要严格计数时用 irq_lock 包住自增
	}
}

void Application::PostUartData(const size_t size) {
	QueueItem item;
	item.event = EVT_UART_DATA;
	item.data.uart.size = size;
	if (!queue_.push(item)) {
		++dropped_;
	}
}

void Application::PostKey(const uint8_t id) {
	QueueItem item;
	item.event = EVT_KEY;
	item.data.key.id = id;
	if (!queue_.push(item)) {
		++dropped_;
	}
}

void Application::HandleStart() {
	tick_.start();
}

void Application::HandleUartData(const QueueItem &item) {
	uint8_t buffer[64];
	// UartRead/UartWrite 是 BSP 暴露的接口，具体实现由该板提供
	const size_t n = Board::GetInstance().UartRead(buffer, item.data.uart.size);
	Board::GetInstance().UartWrite(buffer, n);
}

void Application::HandleTick() {
	// 周期业务放在这里
}

void Application::HandleKey(const QueueItem &) {
	// 按键业务放在这里
}

extern "C" void app_main() {
	Application::GetInstance().Start();   // 单例：唯一启动入口
}
```

关键点：

- 投递链路固定为 `ISR → PostXxx() → queue → Start() 的 pop → HandleXxx()`，不存在第二条路径。
- `PostXxx()` 会被中断调用，因此绝不能调用 HAL、不能延时、不能解析数据。
- handler 的执行时间就是其它事件的等待时间，长流程拆成状态机。
- 需要主动触发业务时，也从 `PostXxx()` 进入，不要直接调用 `HandleXxx()`。

## 5. main() 集成

```c
int main(void)
{
	HAL_Init();
	SystemClock_Config();
	MX_GPIO_Init();
	MX_USART1_UART_Init();

	app_main();          /* C++ 入口，不返回 */

	while (1) {
	}
}
```

**顺序要求**：所有单例（`Application`、`Board`）必须在开中断之前完成构造，驱动对象由 `Board` 在构造时初始化，避免中断里触发首次构造。

## 6. 电平型事件用位图，不要用队列

按键按住、缓冲区满、故障标志这类"状态"适合位图，处理函数做成幂等：

```cpp
class PendingEvents {
public:
	void set(const uint32_t bits) {
		irq_lock lock;
		bits_ |= bits;
	}

	uint32_t take() {
		irq_lock lock;
		const uint32_t v = bits_;
		bits_ = 0;
		return v;
	}

	bool any() const { irq_lock lock; return bits_ != 0; }

private:
	volatile uint32_t bits_ = 0;
};
```

`take()` 之后再置位的位不会被清掉，会在下一轮被处理，因此对电平事件是安全的；带载荷的事件（收到 N 字节、ID 号）仍应走 `queue`。

## 7. 队列容量与溢出策略

- 容量下限 = 最坏事件率 × 最长 handler 阻塞时间 × 2。
- 溢出必须可观测：至少维护 `dropped_` 计数，调试期直接断言。
- 如果某个事件不能丢，就不要用"丢最旧"策略，改为在源头限流（例如串口满时暂停接收）。

## 8. 新增外设的检查清单

- [ ] 资源初始化与 HAL 回调都在 `Board`（BSP），应用层没有 HAL 头
- [ ] ISR 里没有阻塞调用、没有业务逻辑，只调用 `PostXxx()`
- [ ] 事件号定义在 `Application`，载荷是 POD
- [ ] handler 立即返回，长流程拆状态机
- [ ] 所有共享变量都有 `irq_lock` 或单写者保证
- [ ] 没有新增动态内存
- [ ] 没有把其它板子的 `Serial` / `Timer` 实现直接搬过来
- [ ] 用 `-Wall -Wextra -fno-exceptions -fno-rtti -std=c++17` 编译通过
