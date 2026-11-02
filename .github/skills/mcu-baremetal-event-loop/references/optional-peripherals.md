# 可选外设模板（按需复制）

本文件只在用户**明确要求**某个外设时使用。需求里没有的外设：

- 不建 `main/<periph>/` 目录；
- 不在 `Board` 里加成员、`GetXxx()` 或 `Init()` 调用；
- 不在 `Application` 里加事件号、`PostXxx()` 或注册代码；
- 不加 include 路径。

空骨架阶段（见 [SKILL.md](../SKILL.md) 第 1 步）不包含本文件里的任何内容。

## 0. 先列出需求清单

生成代码前，把用户请求逐条翻译成"要不要某个外设"：

| 用户请求 | 生成 | 不生成 |
| --- | --- | --- |
| 只生成空骨架 / 先搭框架 | `main/` 骨架 + `board/` | 任何 `main/<periph>/` |
| 加串口 / 收命令行 | `main/serial/serial.hpp` + `Board::GetSerial()` | `Key` / `Led` |
| 加按键 / 长按 | `main/key/key.hpp` + `Board::GetKey(index)` | `Serial` / `Led` |
| 加 LED 指示 | `main/led/led.hpp` + `Board::GetLed(index)` | `Serial` / `Key` |
| 只加周期任务 | `Timer` 成员（放 `Application` 或 `Board`） | 任何外设目录 |

一次只加一个外设：加完编译通过、事件能进 handler，再加下一个。

## 1. Serial（UART 中断收发）

适用：需要串口输入输出、需要按字符或按行触发事件。

### 1.1 驱动

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

	void Init() {                      // 只在这里 arm 接收
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
		const bool stored = sb_.Write(&byte, sizeof(byte)) == sizeof(byte);

		HAL_UART_Receive_IT(huart_, &temp_, sizeof(temp_));

		{ // 执行特定字符接收回调函数
			const auto &[c, cb, ctx] = character_entry_;
			if (c && c == byte && cb) {
				cb(sb_.Size(), ctx);
			}
		}

		{ // 缓冲已满：本字节没进缓冲，通知上层尽快取走
			const auto &[cb, ctx] = full_entry_;
			if (!stored && cb) {
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

容量 `kRxSize` / `kTxSize` 由本工程的 RAM 与最大帧长决定，示例的 128 只是取值示例。

### 1.2 Board 增量

```cpp
// board.hpp —— 只在本工程有串口时出现
#include "serial.hpp"

class Board {
public:
	Serial<128, 128> *GetSerial();

private:
	Serial<128, 128> serial_;   // 成员，替换掉空骨架里的私有段
};

// board.cpp
Board::Board()
	: serial_(&husart1) {}      // 只保存句柄；时钟/引脚由厂商 MX_*_Init 配好

Serial<128, 128> *Board::GetSerial() {
	return &serial_;
}

void Board::Init() {
	serial_.Init();             // 只调用本工程实际存在的设备
}

// 厂商弱回调只在这里实现一次，按句柄路由
extern "C" void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	if (huart->Instance != USART1) {
		return;
	}
	Board::GetInstance().GetSerial()->HandleRxCpltEvent();
}
```

### 1.3 Application 注册（在 `Board::Init()` 之前）

```cpp
auto *serial = board.GetSerial();
serial->SetSpecificCharactersCallback('\n', +[](size_t size, void *ctx) {
	static_cast<Application *>(ctx)->PostSerialReceivedEvent(size);
}, this);
serial->SetRxBufferFullCallback(+[](size_t size, void *ctx) {
	static_cast<Application *>(ctx)->PostSerialReceivedEvent(size);
}, this);

board.Init();
```

### 1.4 include 路径

加一条 `main/serial/`（`board.hpp` 里写的是短名 `#include "serial.hpp"`）。

## 2. Key（GPIO 按键，消抖 + 长按）

适用：需要按键的按下 / 松开 / 长按事件。消抖与长按都在 `Key` 内部完成。

```cpp
// main/key/key.hpp —— 契约
class Key {
public:
	enum class Event : uint8_t { Pressed, Released, LongPressed };
	using _Callback = void (*)(Event e, void *ctx);

	// Port / Pin 按厂商类型定义，见下面的映射表
	Key(Port port, Pin pin, bool active_high,
	    uint32_t debounce_ms = 10, uint32_t long_press_ms = 1000);

	void Init();                                  // Timer::Start()，开始按 debounce_ms 采样
	void SetCallback(_Callback cb, void *ctx);
	bool IsPressed() const;                       // 消抖后的稳定状态
};
```

`Port` / `Pin` 的取值：

| 厂商 | Port | Pin |
| --- | --- | --- |
| STM32 / PY32（HAL） | `GPIO_TypeDef *` | `uint16_t` |
| 其他 SDK | 按厂商 GPIO 句柄 | 按厂商引脚类型 |

语义：

- 采样用 `Timer`：`Create` 在 `Key` 构造函数里，`Start` 在 `Key::Init()` 里；
- 电平稳定达到 `debounce_ms` 才算状态变化；持续按住超过 `long_press_ms` 产生一次 `LongPressed`；松开产生 `Released`；
- 回调只在状态**变化**时调用，运行在 1ms 中断上下文，只允许 `PostXxx()`；
- 一个 `Key` 对应一个 GPIO 与一个 `Timer`；
- 长按判定留在 `Key` 内部，`Application` 不做按键计时。

`Board` 侧：

```cpp
// board.hpp —— 只在本工程有按键时出现
#include "key.hpp"

class Board {
public:
	Key *GetKey(size_t index);

private:
	Key key0_;
};

// board.cpp
Board::Board()
	: key0_(GPIOB, GPIO_PIN_0, false) {}   // 引脚与极性按本板填写

Key *Board::GetKey(size_t index) {
	return index == 0 ? &key0_ : nullptr;
}

void Board::Init() {
	key0_.Init();      // 开始采样
}
```

`Application` 侧：

```cpp
board.GetKey(0)->SetCallback(+[](Key::Event e, void *ctx) {
	static_cast<Application *>(ctx)->PostKeyEvent(e);
}, this);

board.Init();
```

`PostKeyEvent(Key::Event e)` 把设备枚举翻成 `Application` 自己的事件号与载荷；事件号只在 `Application` 里定义。

## 3. Led（GPIO 输出，无中断）

适用：状态指示、心跳灯。

```cpp
// main/led/led.hpp —— 契约
class Led {
public:
	void Init();              // 由 Board::Init() 调用：设置初始熄灭状态
	void SetState(bool on);   // on == true 表示点亮，极性在实现内部处理
	void Toggle();
	bool IsOn() const;
};
```

- 极性（共阳 / 共阴）只写在 `led.hpp` 里，`SetState` 的参数是语义而非电平；
- 不设回调、不占 `Timer`、不进事件循环；
- `Board` 侧同样只加 `Led *GetLed(size_t index)`、成员与 `Init()` 调用。

## 4. Timer（周期任务与设备内部时序）

`Timer` 不是外设目录，按任务归属挂在持有者身上：

| 任务 | 持有者 | 例子 |
| --- | --- | --- |
| 应用层周期事件 | `Application` | 每 1s 投递 `TIMER_ELAPSED_EVENT` |
| 设备内部时序 | 该设备 | `Key` 的 10ms 采样、传感器上电延时 |
| 板级维护 | `Board` | 看门狗喂狗、心跳灯 |

固定时序：

```text
构造函数          Timer::Create(period_ms, reload, cb, ctx)   只登记回调与周期
Start() / Init()  Timer::Start()                              此刻才开始计时
```

```cpp
// application.hpp
Timer timer_;

// application.cpp
Application::Application() {
	timer_.Create(1000, true, +[](void *ctx) {
		static_cast<Application *>(ctx)->PostEvent(TIMER_ELAPSED_EVENT);
	}, this);
	PostEvent(START_EVENT);
}

void Application::HandleStart() {
	timer_.Start();      // 事件循环已启动，开始计时
}
```

```cpp
// main/key/key.hpp —— 设备内部同样遵守"构造 Create、Init Start"
Key::Key(Port port, Pin pin, bool active_high, uint32_t debounce_ms, uint32_t long_press_ms)
	: /* … */ {
	timer_.Create(debounce_ms, true, +[](void *ctx) {
		static_cast<Key *>(ctx)->OnSampleTick();
	}, this);
}

void Key::Init() {
	timer_.Start();
}
```

规则：

1. `Create` 只登记，不启动；同一对象只 `Create` 一次；
2. `Start` 必须在所有者的 `Start()` / `Init()` 里，保证第一拍中断到达时对象与回调都已就绪；
3. `Timer` 回调运行在 1ms 中断上下文，只允许 `PostXxx()`；
4. 全工程只在一处调用 `Timer::OnMilliSecondTick()`，位置见 [porting.md](./porting.md)。

## 5. 每加一个外设的落地清单

- [ ] 用户明确要求了这个外设
- [ ] `main/<periph>/<periph>.hpp` 已建，构造函数只存配置，`Init()` 里才 arm
- [ ] `Board` 加了成员 + `GetXxx()` + `Init()` 调用 + 弱回调转发
- [ ] `Application::Start()` 里在 `Board::Init()` 之前注册回调
- [ ] `Application` 加了事件号、`PostXxx()`、`case` 与 `HandleXxx()`
- [ ] include 路径加了 `main/<periph>/`
- [ ] 其他外设没有被顺手加进来

## 6. 负向检查

```powershell
# Board 里出现的外设 getter，必须与需求一致
rg -n 'GetSerial|GetKey|GetLed|RefreshWatchdog' main/board/

# main/ 下实际存在的外设目录
Get-ChildItem main -Directory | Select-Object -ExpandProperty Name
```

需求里没有的外设，上面两条命令都不应命中对应项。
