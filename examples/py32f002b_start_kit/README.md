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
| 现象 | 上电后每秒输出 `seconds elapsed`；发送以 `\n` 结尾的数据会原样回显 |

## 目录结构

```
examples/py32f002b_start_kit/
├── Core/        # 厂商生成的外设初始化与中断入口（main.c / *_it.c）
├── Drivers/     # CMSIS + PY32F002B HAL（只保留本范例用到的文件）
├── main/        # 应用层与 BSP
│   ├── application.cpp/.hpp   # 事件循环（Application 单例）
│   ├── mcu_cmsis.h            # MCU 桥接头
│   ├── board/                 # BSP：Board 单例、HAL 回调转发、1ms tick 接入
│   └── serial/                # 板级 UART 驱动（中断收 + 流缓冲 + 非阻塞发）
├── EIDE/        # EIDE 工程：.eide / .vscode / 链接脚本 / 启动文件
└── README.md
```

`queue.hpp`、`stream_buffer.hpp`、`timers.hpp`、`irq_lock.hpp` **直接来自仓库根目录，本目录没有复制一份**。EIDE 工程通过 `incList` 里的 `../../..` 指向仓库根目录，所以 `#include "queue.hpp"` 能正常解析；如果移动了本目录的位置，需要同步修改该路径。

## 构建与下载

1. 安装 VS Code、EIDE 插件与 ARM GCC 工具链（仓库开发时使用 `arm-none-eabi-g++`）。
2. 打开 `examples/py32f002b_start_kit/EIDE/py32f002b_start_kit.code-workspace`。
3. 执行构建（EIDE 构建按钮，或工作区任务 `build`）。
4. 下载方式见 `.eide/eide.yml` 的 `uploadConfigMap`（默认 pyOCD，另有 OpenOCD / J-Link）。

构建产物位于 `EIDE/build/`，已被仓库根目录的 `.gitignore` 忽略。

> 如果宿主工程把这个仓库目录也加进了 `srcDirs`（源目录扫描），请排除 `examples/**`，否则本范例的 `.cpp` 会被重复编译。

## 代码导读（一条完整调用链）

1. `Core/SRC/main.c`：初始化时钟与外设，最后调用 `app_main()`。
2. `main/application.cpp`
   - 构造函数：创建 1000 ms 周期 `timer`、注册 BSP 通知回调、投递 `START_EVENT`；
   - `Start()`：全工程唯一的事件循环，`pop` → `switch` → `HandleXxx()`。
3. `main/board/board.cpp`
   - `Board::GetInstance()` 单例，持有 UART 驱动对象；
   - `HAL_USART_RxCpltCallback` 只把事件转给 `Serial::HandleRxCpltEvent()`；
   - `HAL_IncTick` 里调用 `timer::OnMilliSecondTick()` —— 这就是 1ms tick 的唯一接入点。
4. `main/serial/serial.hpp`：1 字节中断接收 + `stream_buffer` + `HAL_UART_Transmit_IT` 非阻塞发送。
5. `main/mcu_cmsis.h`：`#include "py32f0xx.h"`，`irq_lock` 的唯一 MCU 依赖点。

## 注意

- `main/board`、`main/serial` 是**这块板子的 BSP 实现**，用于演示运行效果；换板时请按仓库 `SKILL.md` 的契约重新实现，不要直接复制。
- 事件回调运行在中断上下文，只能投递事件，不要在里面解析协议或阻塞。
