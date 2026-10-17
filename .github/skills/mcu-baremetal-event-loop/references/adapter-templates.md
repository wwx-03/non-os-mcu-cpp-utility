# 适配层与骨架模板

本文件只给**骨架与契约**：`Serial`、`Timer` 这类驱动实现属于具体板子的 BSP，本仓库不提供实现，按 §3.3 的最小契约在各板 BSP 中自行实现即可。

## 1. 移植点：mcu_cmsis.h

```cpp
#pragma once

#include "py32f0xx.h"   // 换 MCU 时只改这一行，例如 "stm32f1xx.h"
```

`irq_lock.hpp` 只依赖这个头。若目标平台没有 `__get_PRIMASK` / `__disable_irq` / `__set_PRIMASK`（非 CMSIS 架构，或厂商头未提供），**不要改 `irq_lock.hpp`**，在 `mcu_cmsis.h` 里补同名转接函数即可：

```cpp
// mcu_cmsis.h
#pragma once

#include "xxx_device.h"

// 目标平台没有 CMSIS 风格内建函数时，在这里补齐同名转接
static inline uint32_t __get_PRIMASK() { return xxx_get_irq_state(); }
static inline void __disable_irq() { xxx_disable_irq(); }
static inline void __set_PRIMASK(uint32_t state) { xxx_restore_irq(state); }
```

把这三个函数映射到平台自己的原语即可（RISC-V 用 `mstatus`/`mie`，AVR 用 `SREG`，等等）。`irq_lock.hpp` 的 RAII 接口保持不变，`utility` 层不需要为任何平台分叉。

## 2. tick 接入（1ms）

厂商工具生成的 `*_it.c` 通常已经定义了 `SysTick_Handler`，此时**不要重复定义**，改为覆盖 HAL 的弱符号 `HAL_IncTick`（范例就是这么做的）：

```cpp
// board.cpp —— 挂在厂商的 SysTick_Handler 调用链上
#include "timers.hpp"
#include "py32f0xx_hal.h"

extern "C" void HAL_IncTick(void)
{
	extern __IO uint32_t uwTick;
	uwTick += uwTickFreq;          // HAL 自己的毫秒计数
	timer::OnMilliSecondTick();    // 驱动所有软件定时器
}
```

如果工程里没有现成的 `SysTick_Handler`（或使用 TIM 产生 1ms tick），也可以直接在该中断入口里调用 `timer::OnMilliSecondTick()`；**全工程只能接入一次**。回调运行在中断上下文，只允许 `PostXxx()`。

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

### 3.3 板级驱动最小契约

BSP 里的驱动（串口、按键、传感器…）不需要与本仓库保持相同的类结构，只要能提供下面三种能力，`Application` 就能按同一模式使用：

| 能力 | 语义 |
|---|---|
| 读 | 从接收缓冲取走数据，返回实际字节数；由主循环调用，数据不足时返回 0 |
| 写 | 启动一次发送后立即返回；不得阻塞调用者，忙时返回 0 或自己排队 |
| 注册通知 | 提供 `(回调, ctx)` 注册；在中断里只回调"有数据 / 状态变化"，不解析内容 |

接口形态示意（**只有声明，实现由各板 BSP 完成**）：

```cpp
class Uart {
public:
	using data_callback_t = void (*)(size_t size, void *ctx);

	void OnData(data_callback_t cb, void *ctx);    // 中断里调用 cb
	size_t Read(void *dest, size_t size);          // 主循环调用
	size_t Write(const void *src, size_t size);    // 非阻塞
};
```

类名、是否用模板、缓冲大小、是否用 DMA，全部由这块板子决定；只要语义一致，`Application` 的写法不变。

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
- [ ] `devices/` 里没有厂商 HAL 头、没有引脚号 / 极性等板级细节
- [ ] 器件对象是静态对象（`Board` 成员或命名空间作用域静态），没有 `new`、没有函数内 `static` 惰性构造
- [ ] 驱动构造/析构里没有 `HAL_GPIO_Init` / `HAL_GPIO_DeInit`（初始化归 `Board`，静态对象永不析构）
- [ ] HAL 回调名与所用 API 匹配（`HAL_UART_Receive_IT` → `HAL_UART_RxCpltCallback`）
- [ ] 厂商初始化里已启动 IWDG 时，有周期喂狗（`timer` 投递事件 → `Board::RefreshWatchdog()`）
- [ ] EIDE `srcDirs` 含仓库根目录时已用 `excludeList` 排除 `examples/**`
- [ ] 工程工具链与来源一致（Keil/MDK-ARM → AC6 + `.sct`；GCC → `.ld`），没有擅自切换
- [ ] Keil 工程同步：`<Target>` 下有 `<pCCUsed>` + `<uAC6>1</uAC6>`，`app/`、`bsp/` 的 `.cpp` 以 `FileType 8` 入组，`IncludePath` 含 `../app;../bsp;../port;../devices;../non-os-mcu-cpp-utility`
- [ ] 用工程实际工具链（AC6 或 GCC）以 C++17 + `-fno-exceptions -fno-rtti` 编译通过

## 8b. devices 层骨架

`devices/` 只放与 MCU 无关的器件行为；实现落在 `bsp/`，由 `Board` 暴露基类指针。

```cpp
// devices/led.hpp —— 不含任何厂商头
class Led {
public:
	void SetState(bool state) {
		state_ = state;
		SetIo(state_);
	}
	void Toggle() { SetState(!state_); }
	bool GetState() const { return state_; }

protected:
	~Led() = default;          // 静态对象，不会通过基类 delete
	bool state_ = false;
	virtual void SetIo(bool on) = 0;
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
		HAL_GPIO_WritePin(port_, pin_, (on == active_high_) ? GPIO_PIN_SET : GPIO_PIN_RESET);
	}
};
```

```cpp
// bsp/board.cpp —— 静态对象 + Board 内完成板级初始化
namespace {
GPIO_TypeDef *const RED_LED_PORT = GPIOF;   // GPIOx 是强制转换，不能用 constexpr
constexpr uint16_t RED_LED_PIN = GPIO_PIN_9;
GpioLed red_led(RED_LED_PORT, RED_LED_PIN, false);   // 无 __cxa_guard
}

Board::Board() {
	__HAL_RCC_GPIOF_CLK_ENABLE();

	GPIO_InitTypeDef init = {};
	init.Mode = GPIO_MODE_OUTPUT_PP;
	init.Pull = GPIO_NOPULL;
	init.Speed = GPIO_SPEED_FREQ_LOW;
	init.Pin = RED_LED_PIN;
	HAL_GPIO_Init(RED_LED_PORT, &init);

	red_led.SetState(false);    // 时钟与引脚就绪后再确定电平
}
```

要点：

- `GPIOx` 展开为 `((GPIO_TypeDef *)基址)`，**不是 C++ 常量表达式**，声明为 `GPIO_TypeDef *const`，写成 `constexpr` 会报 `must be initialized by a constant expression`。
- 构造里只保存 `port/pin/polarity`，不写寄存器；初始电平在 `Board` 初始化完成后设置。
- 引脚由 `Board` 初始化（CubeMX 若已配置该引脚，就不要重复 `HAL_GPIO_Init`）。

## 9. EIDE 工程配置模板

EIDE 工程目录固定为 `<project>/EIDE`，`.eide/eide.yml` 的关键字段如下（路径相对 `EIDE/`；范例位于 `examples/py32f002b_start_kit`，所以仓库根目录是 `../../..`，普通工程通常是 `../non-os-mcu-cpp-utility`）。

> **先选对工具链**：Keil / CubeMX 导入的 `MDK-ARM` 工程用 AC6 + EIDE 生成的 `.sct`；GCC 工程才用下面的 GCC 模板 + 厂商 `.ld`。不要把两者混用。

```yaml
version: "4.1"
name: <project>
type: ARM
deviceName: PY32F002Bx5
srcDirs:
  - ../app
  - ../bsp
  - ../port
  - ../non-os-mcu-cpp-utility
virtualFolder:            # Core / Drivers / startup 用虚拟目录引用，不复制
  name: <virtual_root>
  files: []
  folders:
    - name: Application
      files: []
      folders:
        - name: EIDE
          files:
            - path: startup_py32f002bxx.s
          folders: []
        - name: User
          files: []
          folders:
            - name: Core
              files:
                - path: ../Core/SRC/main.c
                # … gpio.c / tim.c / usart.c / iwdg.c / rcc.c /
                #   py32f002b_it.c / py32f002b_hal_msp.c
              folders: []
    - name: Drivers
      files: []
      folders:
        - name: CMSIS
          files:
            - path: ../Core/SRC/system_py32f002b.c
          folders: []
        - name: PY32F002B_HAL_Driver
          files:
            # 只列用到的 HAL 模块，必须与 py32f002b_hal_conf.h 打开的模块一致
            - path: ../Drivers/PY32F002B_HAL_Driver/Src/py32f002b_hal.c
          folders: []
outDir: build
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
    excludeList:
      - ../non-os-mcu-cpp-utility/examples
    toolchain: GCC
    toolchainConfigMap:
      GCC:
        cpuType: Cortex-M0+
        floatingPointHardware: none
        options:
          c/cpp-compiler:
            CXX_FLAGS: -Wno-register
            language-c: c11
            language-cpp: c++17
            no-exceptions: true
            no-rtti: true
            one-elf-section-per-data: true
            one-elf-section-per-function: true
            optimization: level-2
            warnings: all-warnings
          global:
            $float-abi-type: softfp
            arm-thumb-mode: thumb
            misc-control: --specs=nosys.specs --specs=nano.specs
            output-debug-info: enable
          linker:
            $toolName: auto
            LD_FLAGS: -Wl,--no-warn-rwx-segments
            LIB_FLAGS: -lm
            output-format: elf
            remove-unused-input-sections: true
        scatterFilePath: PY32F002BF15U6.ld
        useCustomScatterFile: true
    uploader: pyOCD
```

如果工程是从 Keil MDK / CubeMX 的 `MDK-ARM` 工程导入的，**不要用上面的 GCC 模板**，改用 AC6：

```yaml
srcDirs:
  - ../app
  - ../bsp
  - ../port
  - ../non-os-mcu-cpp-utility
virtualFolder:
  name: <virtual_root>
  files: []
  folders:
    - name: Application
      files: []
      folders:
        - name: EIDE
          files:
            - path: ../MDK-ARM/startup_stm32f407xx.s     # ARM 语法启动文件
          folders: []
        # … Core / Drivers 同上
targets:
  Debug:
    cppPreprocessAttrs:
      defineList: [USE_HAL_DRIVER, STM32F407xx]
      incList:
        - ../Core/Inc
        - ../Drivers/STM32F4xx_HAL_Driver/Inc
        - ../Drivers/CMSIS/Device/ST/STM32F4xx/Include
        - ../Drivers/CMSIS/Include
        - ../app
        - ../bsp
        - ../non-os-mcu-cpp-utility
        - ../port
    excludeList:
      - ../non-os-mcu-cpp-utility/examples
    toolchain: AC6
    toolchainConfigMap:
      AC6:
        cpuType: Cortex-M4
        floatingPointHardware: single
        options:
          asm-compiler:
            $use: asm-auto
          c/cpp-compiler:
            CXX_FLAGS: -fno-exceptions -fno-rtti
            language-c: c99
            language-cpp: c++17
            one-elf-section-per-function: true
            optimization: level-2
            plain-char-is-signed: true
            warnings: ac5-like-warnings
          linker:
            misc-controls: --diag_suppress=L6329
            output-format: elf
        scatterFilePath: ""
        storageLayout:            # 按芯片真实内存填写
          RAM:
            - { id: 1, isChecked: true, mem: { size: "0x20000", startAddr: "0x20000000" }, noInit: false, tag: IRAM }
          ROM:
            - { id: 1, isChecked: true, isStartup: true, mem: { size: "0x100000", startAddr: "0x08000000" }, tag: IROM }
        useCustomScatterFile: false   # EIDE 按 storageLayout 生成 .sct
    uploader: STLink
```

检查点：

- **工具链与工程来源一致**：`MDK-ARM/*.uvprojx` 工程用 AC6 + `../MDK-ARM/startup_*.s` + `useCustomScatterFile: false`；GCC 工程用 GCC + CMSIS `Templates/gcc/startup_*.s` + 厂商 `.ld`（`useCustomScatterFile: true`）。
- AC6 的 `CXX_FLAGS` 只能写 armclang 参数（`-fno-exceptions -fno-rtti`），不要混入 `--specs=nosys.specs`、`-Wl,...` 等 GCC 选项。
- `storageLayout` 的 RAM/ROM 按芯片数据手册填写，构建后核对生成的 `.sct`。
- `srcDirs` 放进仓库根目录时，**必须**同步排除 `examples/**`；否则范例的 `.cpp` 会被编译进当前工程。
- `../port` 必须同时出现在 `incList`，否则 `irq_lock.hpp` 找不到 `mcu_cmsis.h`。
- `language-cpp` 必须是 `c++17`；`-fno-exceptions -fno-rtti` 必须打开。
- `py32f002b_hal_conf.h` / `stm32f4xx_hal_conf.h` 打开的每个 HAL 模块都要在 `virtualFolder` 的 `Src` 列表里有对应 `.c`。
- `.vscode/tasks.json` 与 `uploadConfigMap` 的 ELF 路径要和 `outDir/<target>/<name>.elf` 一致。
