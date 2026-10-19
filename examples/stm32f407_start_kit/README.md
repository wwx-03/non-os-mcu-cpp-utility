# STM32F407 Start Kit 范例（Keil / AC6 模板）

本目录是 `non-os-mcu-cpp-utility` 的 STM32 范例，也是**从 Keil MDK 导入 EIDE 的工程模板**。与 PY32 范例演示同一套模式，区别在于它的工具链是 Keil 自带的 **AC6（armclang）**，工程里同时保留了可用的 Keil 工程和 EIDE 工程。

- 1ms SysTick → `timer::OnMilliSecondTick()`，驱动静态链表软件定时器；
- USART1 中断接收 → `stream_buffer` 缓冲 → BSP 回调通知；
- `queue` 事件队列 + `Application` 单例事件循环派发；
- `Board` 单例作为 BSP，独占 HAL 句柄与 HAL 弱回调。

## 硬件与现象

| 项目 | 值 |
|---|---|
| 芯片 | STM32F407ZGTx（144 pin，1 MB Flash / 128 KB SRAM） |
| 时钟 | HSE 25 MHz → PLL 168 MHz |
| 串口 | USART1：PA9 = TX，PA10 = RX，115200 8N1 |
| LED | PF9（红）/ PF10（绿），低电平点亮，由 `Board` 初始化 |
| 按键 | PA0，内部下拉、按下为高（WK_UP 风格），`Button` 每 25 ms 采样一次消抖 |
| 看门狗 | IWDG（LSI 32 kHz / 4 × 4095 ≈ 512 ms），事件循环每 100 ms 喂狗 |
| 现象 | 每秒输出 `Tick: <秒数>, dropped: <丢事件数>`；每 250 ms 翻转 LED；**松开 PA0 时输出 `Button0 released`**；发送以 `\n` 结尾的数据会原样回显 |

## 目录结构

```
examples/stm32f407_start_kit/
├── app/         # 应用层：Application 单例与事件循环（唯一的业务入口）
├── bsp/         # 板级支持包：Board 单例、HAL 回调转发、GPIO 驱动
├── devices/     # 器件抽象：Led、Button（不含任何厂商头）
├── port/        # mcu_cmsis.h：唯一的 MCU 依赖点
├── Core/        # CubeMX 生成的外设初始化与中断入口（main.c / *_it.c）
├── Drivers/     # CMSIS + STM32F4 HAL（只保留本范例用到的文件）
├── MDK-ARM/     # Keil 工程（AC6）+ ARM 语法启动文件
├── EIDE/        # EIDE 工程：.eide/eide.yml、.vscode、code-workspace
└── README.md
```

## 两套工程，同一份源码

| 入口 | 工具链 | 启动文件 | 链接脚本 |
|---|---|---|---|
| `MDK-ARM/stm32f407_start_kit.uvprojx` | Keil **AC6**（`uAC6=1`） | `MDK-ARM/startup_stm32f407xx.s` | Keil 按 `Cpu` 内存模型自动生成 |
| `EIDE/stm32f407_start_kit.code-workspace` | EIDE **AC6**（同一套 armclang） | `../MDK-ARM/startup_stm32f407xx.s` | `useCustomScatterFile: false`，EIDE 按 `storageLayout` 生成 `.sct` |

两者都不使用 GCC，也不需要 `.ld` 链接脚本。`app/`、`bsp/`、`port/` 是唯一实现，两个工程都只是引用它们。

## 构建与下载

### EIDE

1. 安装 VS Code、EIDE 插件，以及 KEIL MDK（提供 `armclang`）。
2. 打开 `EIDE/stm32f407_start_kit.code-workspace`。
3. 执行构建（EIDE 构建按钮或任务 `build`），产物在 `EIDE/build/Debug/`。
4. 下载方式见 `.eide/eide.yml` 的 `uploadConfigMap`（默认 ST-Link）。

EIDE 首次构建时会根据 `eide.yml` 生成 `build/Debug/builder.params` 与 `build/Debug/*.sct`；`.sct` 的 RAM/ROM 来自 `storageLayout`（`0x20000000 + 128 KB` / `0x08000000 + 1024 KB`），换芯片时改这里。

### Keil MDK

1. 打开 `MDK-ARM/stm32f407_start_kit.uvprojx`。
2. 工程已设置 `uAC6=1`（ARM Compiler 6），`C/C++` 页含 `-fno-exceptions -fno-rtti`。
3. Build（F7）→ 产物在 `MDK-ARM/stm32f407_start_kit/`，可直接下载。

> 若 Keil 用的是其它 AC6 小版本，`<pCCUsed>` 里的版本号在首次打开工程时会被 µVision 自动修正，无需手改。

## 代码导读（一条完整调用链）

1. `Core/Src/main.c`：`HAL_Init()` → `SystemClock_Config()` → 外设初始化 → `app_main()`。
2. `app/application.cpp`
   - 构造函数：创建 1000 ms `tick_timer_`、100 ms 喂狗定时器、250 ms 闪灯定时器，注册 BSP 通知回调（串口收满 / 按键释放），投递 `START_EVENT`；
   - 按键回调只调用 `PostButtonReleasedEvent()`，业务在 `HandleButtonReleased()` 里输出 `Button0 released`；
   - `Start()`：全工程唯一的事件循环，`pop` → `switch` → `HandleXxx()`。
3. `bsp/board.cpp`
   - `Board::GetInstance()` 单例，持有 UART 驱动、两个 `GpioLed` 与一个 `GpioButton`，并暴露 `RefreshWatchdog()`；
   - `Board` 构造函数里使能 GPIOF 时钟、`HAL_GPIO_Init` PF9/PF10（输出）与 PA0（输入下拉），随后确定 LED 初始电平并 `button0.Start()` 建立采样基线；
   - `HAL_UART_RxCpltCallback` / `HAL_UART_ErrorCallback` 只把事件转给 `Serial`；
   - `HAL_IncTick` 里调用 `timer::OnMilliSecondTick()` —— 1ms tick 的唯一接入点。
4. `bsp/gpio_led.hpp` / `devices/led.hpp`：`Led` 是 NVI 接口（`SetState`/`Toggle`/`GetState` 非虚，派生类只实现 `SetIo`），`GpioLed` 负责电平与极性。
5. `bsp/gpio_button.hpp` / `devices/button.hpp`：`Button` 用 `timer` 每 25 ms 采样做消抖，`Start()` 里记录初始电平并启动采样，检测到"按下 → 松开"时回调；`GpioButton` 只实现 `Read()`（含 `active_high` 极性）。
6. `bsp/serial.hpp`：1 字节中断接收 + `stream_buffer` + `HAL_UART_Transmit_IT` 非阻塞发送，另有 `vsnprintf` 格式化输出重载。
7. `port/mcu_cmsis.h`：`#include "stm32f4xx.h"`，`irq_lock` 的唯一 MCU 依赖点。

## 作为新工程模板

**从 Keil / CubeMX 导入的工程**请以本目录为模板（GCC 工程用 `py32f002b_start_kit/`）：

1. 复制 `app/`、`bsp/`、`devices/`、`port/` 四层；
2. `Core/`、`Drivers/` 由 CubeMX 重新生成，不要复制（本目录只保留运行本范例必需的 HAL 模块，供直接编译验证）；
3. `MDK-ARM/` 保留你自己的 `.uvprojx`，但要做三件事：
   - `<pCCUsed>` + `<uAC6>1</uAC6>` 放在 `<Target>` 下（不是 `<TargetCommonOption>` 里），否则 µVision 仍用 AC5；
   - `IncludePath` 追加 `../app;../bsp;../port;../devices;../non-os-mcu-cpp-utility`；
   - `Groups` 里新增一个组，把 `../app/application.cpp`、`../bsp/board.cpp` 以 `FileType 8`（C++ Source）加入；
4. `EIDE/.eide/eide.yml` 按本目录的 `AC6` 配置改写（`toolchain: AC6`、`useCustomScatterFile: false`，`srcDirs`/`incList` 含 `../devices`），详见仓库 [references/toolchains.md](../../.github/skills/mcu-baremetal-event-loop/references/toolchains.md)。

## 注意

- `bsp/` 是**这块板子的 BSP 实现**；换板时按 `SKILL.md` 的契约重写，不要直接复制。
- 事件回调运行在中断上下文，只能投递事件，不要在里面解析协议或阻塞。
- `Core/Src` 里的 `main.c` 只在 `USER CODE` 区域调用 `app_main()`；重新用 CubeMX 生成时这段会被保留。
- 若目标工程没有 IWDG，删除 `Board::RefreshWatchdog()` 与 `WATCHDOG_REFRESH_EVENT`。
