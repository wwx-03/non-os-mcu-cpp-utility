# 适配层契约与设计说明

本文件说明骨架背后的**契约与设计约束**，不再重复完整代码：

- 可复制代码 → [../templates/](../templates/README.md)
- 完整可运行工程 → [../../../../examples/](../../../../examples/README.md)
- 平台契约 → [platform-profile.md](./platform-profile.md)
- 移植与中断 → [porting.md](./porting.md)
- 工具链与完整 `eide.yml` → [toolchains.md](./toolchains.md)
- 上板检查 → [../checklists/bring-up.md](../checklists/bring-up.md)

`Serial`、`Timer` 这类驱动实现属于具体板子的 BSP，本仓库不提供实现，
按 §3.3 的最小契约在各板 BSP 中自行实现。

## 1. 移植点：`mcu_cmsis.h`

`irq_lock.hpp` 只依赖 `port/mcu_cmsis.h`。该头的内容就是所选器件的 CMSIS 头：

```cpp
// port/mcu_cmsis.h
#pragma once
#include "py32f0xx.h"   // 换成目标器件头
```

目标平台没有 `__get_PRIMASK` / `__disable_irq` / `__set_PRIMASK` 时，
在 `mcu_cmsis.h` 里补齐同名转接，**不要修改 `irq_lock.hpp`**。
完整说明见 [porting.md](./porting.md)。

## 2. 1 ms tick

`timer` 依赖稳定的 1 ms tick，全工程只能接入一次：

- 厂商已定义 `SysTick_Handler`：覆盖弱符号 `HAL_IncTick`，在里面调用
  `timer::OnMilliSecondTick()`；
- 使用独立 TIM 或自定义中断：直接在入口里调用；
- 回调运行在中断上下文，只允许 `Application::PostXxx()`。

详见 [porting.md](./porting.md)。

## 3. BSP：Board 契约

一块板子一份 `Board`。它负责**板级资源与 HAL 回调**，不负责业务。

### 3.1 单例约定

`Application` 固定为"私有构造 + 函数内 `static` 实例 + `GetInstance()`"。

`Board` 的形态由架构模式决定（见 SKILL.md「架构模式」）：

- **Layered**：具体 `Board` 同为"私有构造 + 函数内 `static` 实例 + `GetInstance()`"；
  `Application` 通过 `Start(Board &)` 注入的引用取资源（内部保存 `Board *board_`）。
- **DIP / Hybrid**：抽象 `Board` 提供 `GetInstance()`，内部转调 `create_new_board()`；
  具体板 `<MCU><BoardName><Version>Board` **只在 `bsp/<board>_board.cpp` 内定义**，
  末尾 `IMPLEMENT_BOARD_CLASS(<BoardClass>)` 提供唯一实现。
  选板 = 只编译一个板级 `.cpp`（多个 → 重复符号，缺失 → undefined reference）。

共同要求：

- 禁止全局对象、自由函数、宏单例、注册表、public 构造；
- 首次 `GetInstance()` / 具体板构造必须发生在**中断使能之前**，避免 ISR 触发首次构造；
- 删除拷贝构造与拷贝赋值；
- 抽象 `Board` 的析构设为 `protected` 非虚，禁止通过基类指针 `delete`；
- `IMPLEMENT_BOARD_CLASS` 内部用函数内静态对象，**不使用 `new` / `malloc`**。

"函数内 static"只允许用于 `Application`、具体板类与 `IMPLEMENT_BOARD_CLASS`；
器件对象不要用函数内 static（见 §8）。

### 3.2 职责

1. 持有并初始化 HAL 句柄（`huart`、`htim`、`hadc`…），完成板级初始化；
2. 构造板级驱动对象（串口、按键、LED、传感器…），通过接口暴露；
3. **独占并转发 HAL 弱回调**，按句柄 / 引脚路由到对应驱动；
4. 提供板级常量与能力查询（引脚、时钟、DMA 能力等）。

不做：业务逻辑、协议解析、事件编号、事件缓存。

### 3.3 板级驱动最小契约

驱动器不需要与本仓库保持相同的类结构，只要能提供：

| 能力 | 语义 |
| --- | --- |
| 读 | 从接收缓冲取走数据，返回实际字节数；主循环调用，不足时返回 0 |
| 写 | 启动一次发送后立即返回；不得阻塞调用者，忙时返回 0 或自行排队 |
| 注册通知 | 提供 `(回调, ctx)` 注册；中断里只回调"有数据 / 状态变化"，不解析内容 |

类名、是否用模板、缓冲大小、是否用 DMA 全部由板子决定；语义一致即可。

### 3.4 HAL 回调

- 同一外设的 HAL 弱回调只能由 `Board` 实现一次；
- 多串口按 `huart` 指针路由，多 EXTI 按 `pin` 路由；
- 回调链上任何一层都不得阻塞、不得解析协议、不得直接调用业务函数；
- 回调名必须与实际 API 匹配（`HAL_UART_Receive_IT` → `HAL_UART_RxCpltCallback`）。

代码骨架见 [templates/bsp/board.hpp](../templates/bsp/board.hpp) 与
[templates/bsp/board.cpp.txt](../templates/bsp/board.cpp.txt)。

## 4. Application：事件循环契约

事件从产生到处理只有一条链路：

```text
任一 ISR（UART / GPIO / TIM / SysTick）
        │  只做三件事：清标志、搬数据、调用 PostXxx()
        ▼
Application::PostXxx()   ──►  queue<QueueItem, N>（irq_lock 保护）
                                      │
                                      ▼
Application::Start(Board&)  while (true)：pop → switch 事件号 → HandleXxx(item)
                                      │
                                      ▼
                              业务 / 状态机 / 调用 BSP 接口
```

规则：

1. 投递入口只有 `PostXxx()`：只允许 POD 构造 + `push`，不得调用 HAL、延时或解析数据；
2. `Start(Board &)` 永不返回：`pop` 失败可以 `continue`，或 `__WFI()` 睡眠；
3. 事件号集中定义在 `Application`，BSP 与驱动不得自定义；
4. 载荷只放 POD，优先 ≤ 8 字节；不要放指针；
5. 事件是 FIFO、无优先级：高频事件会挤压低频事件，忙时用位图合并同类事件；
6. handler 必须快速返回且幂等；长流程拆状态机；
7. 队列满必须可观测：`push` 返回 `false` 时计数，调试期断言；
8. 构造函数只做"注册 BSP 回调 + 投递初始事件"，所有单例必须在开中断前构造；
9. 单例形态固定，`main()` 里只调用 `Application::GetInstance().Start(Board::GetInstance())`；两种模式的 `Board::GetInstance()` 返回具体板或抽象接口。

handler 内再次 `PostXxx()` 是允许的，但不要形成自激循环；主动触发业务也从
`PostXxx()` 进入，不要直接调用 `HandleXxx()`。

代码骨架见 [templates/app/application.hpp](../templates/app/application.hpp) 与
[templates/app/application.cpp.txt](../templates/app/application.cpp.txt)。

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

顺序要求：所有单例（`Application`、`Board`）必须在开中断之前完成构造；
驱动对象由 `Board` 在构造时初始化，避免中断里触发首次构造。

## 6. 电平型事件用位图

按键按住、缓冲区满、故障标志这类"状态"适合位图，处理函数做成幂等：

- `set(bits)` 置位；
- `take()` 取出并清零；取出后再置位的位会在下一轮处理，不会丢失；
- 带载荷的事件（收到 N 字节、ID 号）仍走 `queue`。

代码见 [templates/utility/pending_events.hpp](../templates/utility/pending_events.hpp)。

## 7. 队列容量与溢出策略

- 容量下限 = 最坏事件率 × 最长 handler 阻塞时间 × 2；
- 溢出必须可观测：至少维护 `dropped_` 计数，调试期直接断言；
- 不能丢的事件不要用"丢最旧"，改为在源头限流（例如串口满时暂停接收）。

## 8. devices 层要点

`devices/` 回答"这个器件是什么"，`bsp/` 回答"它在这块板上怎么接"。

- 基类用 NVI：public 方法维护状态与不变量，派生类只实现一个硬件钩子；
- 派生类只持有端口 / 引脚 / 极性 / 总线句柄，不初始化外设；
- 需要读初值的器件提供 `Start()`，由 `Board` 在引脚配置后调用；
- 实例放在命名空间作用域或 `Board` 成员，**不要用函数内 `static`**；
- 析构里不要 `HAL_*_DeInit`（静态对象永不析构，只会拖入 `__cxa_atexit`）；
- 只有需要运行时替换或 PC 端 mock 时才用虚接口，否则用函数指针 / 模板 policy。

完整写法见 `SKILL.md` 的「devices 层：设备类怎么写」。
代码见 [templates/devices/](../templates/devices/) 与 [templates/bsp/](../templates/bsp/)。

## 9. 检查清单

移植、事件链路、内存并发、分层、工程配置与上板验证的完整清单见
[../checklists/bring-up.md](../checklists/bring-up.md)。
