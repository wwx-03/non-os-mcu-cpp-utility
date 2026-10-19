# 移植与中断接入

本文件说明本仓库唯一的平台依赖点（`port/mcu_cmsis.h`）、1 ms tick 接入方式，以及
HAL 回调的上下文约束。平台契约见 [platform-profile.md](./platform-profile.md)。

## 1. 唯一的移植点：`port/mcu_cmsis.h`

`irq_lock.hpp` 只依赖 `mcu_cmsis.h`。目标工程必须提供这个文件：

```cpp
// port/mcu_cmsis.h
#pragma once

// 换 MCU 时只改这一行
#include "py32f0xx.h"   // 或 "stm32f1xx.h" / "stm32f4xx.h" ...
```

`utility/` 层的 `queue`、`stream_buffer`、`timer`、`irq_lock` 都**不应该**知道具体 MCU。

## 2. 中断控制原语

Cortex-M 的 CMSIS 头通常已经提供：

- `__get_PRIMASK()`
- `__disable_irq()`
- `__set_PRIMASK(state)`

如果目标平台没有这三个内建函数（非 Cortex-M，或厂商头未提供），
**不要修改 `irq_lock.hpp`**，而是在 `mcu_cmsis.h` 里补齐同名转接：

```cpp
// port/mcu_cmsis.h
#pragma once

#include "xxx_device.h"

static inline uint32_t __get_PRIMASK() { return xxx_get_irq_state(); }
static inline void __disable_irq() { xxx_disable_irq(); }
static inline void __set_PRIMASK(uint32_t state) { xxx_restore_irq(state); }
```

这样 `irq_lock` 的 RAII 语义保持不变，`utility` 层不需要为任何平台分叉。

> `irq_lock` 只适用于单核裸机。多核共享数据必须使用平台提供的原子操作或硬件信号量。

## 3. 1 ms tick 接入

`timer` 依赖一个稳定的 1 ms tick。接入点**全工程只能有一个**。

### 3.1 厂商已定义 `SysTick_Handler`

CubeMX / 厂商 SDK 生成的 `*_it.c` 通常已经定义了 `SysTick_Handler`（内部调用
`HAL_IncTick`）。此时**不要重复定义**，改为覆盖 HAL 的弱符号：

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

### 3.2 使用独立 TIM 或自定义中断

如果工程没有现成的 `SysTick_Handler`，或使用 TIM 产生 1 ms tick，可以直接在该中断
入口里调用 `timer::OnMilliSecondTick()`：

```cpp
extern "C" void TIMx_IRQHandler(void)
{
	// 清中断标志 ...
	timer::OnMilliSecondTick();
}
```

要求：

- tick 周期必须是 1 ms；
- 全工程只能接入一次；
- 回调运行在中断上下文，只允许投递事件（见下一节）。

## 4. 回调上下文

| 上下文 | 能否阻塞 | 该做什么 |
| --- | --- | --- |
| HAL UART / EXTI / TIM 回调 | 否 | 清标志、搬数据、调用上层注册的通知回调 |
| `timer` 回调 | 否 | 只调用 `Application::PostXxx()` |
| BSP 驱动通知回调 | 否 | 只上报"有数据 / 状态变化"，不解析协议 |
| `Application` 事件 handler | 可以（但不建议久等） | 业务、状态机、调用 BSP 接口 |

规则：

1. **中断里只做三件事**：清标志、搬数据、投递事件；
2. 中断里禁止 `printf`、`HAL_Delay`、阻塞式发送、动态分配、长循环；
3. 厂商 HAL 回调名必须与实际使用的 API 对齐，例如
   `HAL_UART_Receive_IT` 对应 `HAL_UART_RxCpltCallback`；
4. 同一外设的 HAL 回调只能由 `Board` 实现一次，再按句柄 / 引脚路由，
   不要多个模块各自抢同一个弱符号。

## 5. 接入方式：include 路径，不是源码目录

本仓库是 header-only，正确做法是把仓库根目录加入 **include 路径**
（例如 EIDE 的 `incList`），不需要编译任何 `.cpp`。

如果工程必须把仓库根目录加入 `srcDirs`（源目录扫描），必须同时排除范例：

```yaml
targets:
  Debug:
    excludeList:
      - ../non-os-mcu-cpp-utility/examples
```

否则 `examples/` 下的范例 `.cpp` 会被一起编译。

## 6. 验证

完成移植后至少确认：

- `mcu_cmsis.h` 指向正确的器件头；
- 工程能以目标工具链编译通过（C++17，`-fno-exceptions -fno-rtti`）；
- `SysTick_Handler` / `HAL_IncTick` / `timer::OnMilliSecondTick()` 只有一条调用链；
- 上电后 `timer` 周期准确（用 GPIO 翻转或串口时间戳验证）；
- 中断里没有阻塞调用，队列 `dropped_` 计数可观测。
