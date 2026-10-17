# PY32F002B Start Kit 范例

本目录是 `non-os-mcu-cpp-utility` 的完整范例工程，演示仓库约定的工程模式：

- 1ms SysTick → `timer::OnMilliSecondTick()`，驱动静态链表软件定时器；
- UART1 中断接收 → `stream_buffer` 缓冲 → BSP 回调通知；
- `queue` 事件队列 + `Application` 单例事件循环派发；
- `Board` 单例作为 BSP，独占 HAL 句柄与 HAL 弱回调。

## 硬件与现象

| 项目 | 值 |
|---|---|
| 芯片 | PY32F002Bx5，HSI 24 MHz |
| 串口 | USART1：PA2 = RX，PA3 = TX，115200 8N1 |
| 看门狗 | IWDG（约 512 ms 溢出），事件循环每 100 ms 喂狗 |
| 现象 | 上电后每秒输出 `seconds elapsed`；发送以 `\n` 结尾的数据会原样回显 |

## 目录结构

```
examples/py32f002b_start_kit/
├── app/         # 应用层：Application 单例与事件循环（唯一的业务入口）
├── bsp/         # 板级支持包：Board 单例、HAL 回调转发、UART 驱动
├── port/        # mcu_cmsis.h：唯一的 MCU 依赖点
├── Core/        # 厂商生成的外设初始化与中断入口（main.c / *_it.c）
├── Drivers/     # CMSIS + PY32F002B HAL（只保留本范例用到的文件）
├── EIDE/        # EIDE 工程：.eide / .vscode / 链接脚本 / 启动文件
└── README.md
```

分层与仓库 `SKILL.md` 中的定义完全一致：

| 目录 | 层 | 职责 |
|---|---|---|
| `app/` | 应用 | 事件号、事件队列、handler、状态机；只认识 `Board` 暴露的接口 |
| `bsp/` | BSP | 持有 HAL 句柄、构造驱动、独占并转发 HAL 弱回调、接入 1ms tick |
| `port/` | 移植 | `mcu_cmsis.h`，换 MCU 只改这里 |
| `Core/`、`Drivers/` | 厂商 | 时钟/外设初始化、CMSIS、HAL |
| 仓库根目录 | 工具库 | `queue.hpp` / `stream_buffer.hpp` / `timers.hpp` / `irq_lock.hpp` |

`queue.hpp`、`stream_buffer.hpp`、`timers.hpp`、`irq_lock.hpp` **直接来自仓库根目录，本目录没有复制一份**。EIDE 工程把仓库根目录 `../../..` 同时放进 `srcDirs` 与 `incList`，并在 `excludeList` 中排除 `../../../examples`，所以 `#include "queue.hpp"` 能正常解析、范例的 `.cpp` 也不会被重复编译；如果移动了本目录的位置，需要同步修改这三处路径。

## 构建与下载

1. 安装 VS Code、EIDE 插件与 ARM GCC 工具链（仓库开发时使用 `arm-none-eabi-g++`）。
2. 打开 `examples/py32f002b_start_kit/EIDE/py32f002b_start_kit.code-workspace`。
3. 执行构建（EIDE 构建按钮，或工作区任务 `build`）。
4. 下载方式见 `.eide/eide.yml` 的 `uploadConfigMap`（默认 pyOCD，另有 OpenOCD / J-Link）。

构建产物位于 `EIDE/build/`，已被仓库根目录的 `.gitignore` 忽略。

> 如果宿主工程把这个仓库目录也加进了 `srcDirs`（源目录扫描），请排除 `examples/**`，否则本范例的 `.cpp` 会被重复编译。

## 代码导读（一条完整调用链）

1. `Core/SRC/main.c`：初始化时钟与外设，最后调用 `app_main()`。
2. `app/application.cpp`
   - 构造函数：创建 1000 ms 周期 `timer`、注册 BSP 通知回调、投递 `START_EVENT`；
   - 另有一个 100 ms `timer` 投递 `WATCHDOG_REFRESH_EVENT`，在事件循环里喂 IWDG（本工程的 IWDG 约 512 ms 溢出）；
   - `Start()`：全工程唯一的事件循环，`pop` → `switch` → `HandleXxx()`。
3. `bsp/board.cpp`
   - `Board::GetInstance()` 单例，持有 UART 驱动对象，并暴露 `RefreshWatchdog()`；
   - `HAL_UART_RxCpltCallback` / `HAL_UART_ErrorCallback` 只把事件转给 `Serial`，与 `HAL_UART_*` API 匹配；
   - `HAL_IncTick` 里调用 `timer::OnMilliSecondTick()` —— 这就是 1ms tick 的唯一接入点。
4. `bsp/serial.hpp`：1 字节中断接收 + `stream_buffer` + `HAL_UART_Transmit_IT` 非阻塞发送。
5. `port/mcu_cmsis.h`：`#include "py32f0xx.h"`，`irq_lock` 的唯一 MCU 依赖点。

## 作为新工程的模板

新建工程时直接复制本目录的 `app/`、`bsp/`、`port/` 三层，再按目标板替换实现即可，步骤见仓库 `SKILL.md` 的「用范例生成工程骨架」一节。不要复制 `Drivers/`、`Core/`，它们应当由目标 MCU 的厂商工具生成。

## 注意

- `bsp/` 是**这块板子的 BSP 实现**，用于演示运行效果；换板时请按仓库 `SKILL.md` 的契约重新实现，不要直接复制。
- 事件回调运行在中断上下文，只能投递事件，不要在里面解析协议或阻塞。
- 厂商的 `Studio_IWDG_Init()` 已启动看门狗，`Application` 用 100 ms `timer` 周期喂狗；如果目标工程没有 IWDG，删除 `Board::RefreshWatchdog()` 与对应的定时器事件。
