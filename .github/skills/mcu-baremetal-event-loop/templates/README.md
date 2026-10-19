# 代码模板

本目录是**可复制的骨架**，不是完整可编译工程。复制到目标工程后，需要按该板的
HAL 与引脚完成 `TODO` 标记的位置；完整可运行工程见 `examples/`。

| 模板 | 复制到 | 说明 |
| --- | --- | --- |
| `port/mcu_cmsis.h` | `<project>/port/mcu_cmsis.h` | 唯一的 MCU 依赖点，换成目标器件头 |
| `app/application.hpp` | `<project>/app/application.hpp` | 事件循环单例与事件号（**Layered**） |
| `app/application.cpp` | `<project>/app/application.cpp` | `Start(Board &)` 派发循环、`PostXxx()`、handler（**Layered**） |
| `app/application_dip.hpp` | `<project>/app/application.hpp` | DIP/Hybrid 版声明：只依赖 `devices/board.hpp`，`Start(Board &)` |
| `bsp/board.hpp` | `<project>/bsp/board.hpp` | 板级资源与 HAL 回调入口 |
| `bsp/board.cpp` | `<project>/bsp/board.cpp` | 单例、驱动对象、HAL 弱回调转发、1 ms tick |
| `devices/led.hpp` | `<project>/devices/led.hpp` | 与 MCU 无关的 LED 抽象（NVI） |
| `devices/button.hpp` | `<project>/devices/button.hpp` | 与 MCU 无关的按键消抖（NVI） |
| `devices/board.hpp` | `<project>/devices/board.hpp` | 抽象 Board（**仅 DIP / Hybrid**） |
| `bsp/gpio_led.hpp` | `<project>/bsp/gpio_led.hpp` | `Led` 的 GPIO 实现 |
| `bsp/gpio_button.hpp` | `<project>/bsp/gpio_button.hpp` | `Button` 的 GPIO 实现 |
| `bsp/board_example.cpp` | `<project>/bsp/<board>_board.cpp` | 具体板骨架：**类只在 .cpp 内定义**，末尾 `IMPLEMENT_BOARD_CLASS()` |
| `utility/pending_events.hpp` | `<project>/utility/pending_events.hpp` | 电平型事件位图 |

> **Layered**：`bsp/board.hpp` + `bsp/board.cpp`（具体 `Board` 单例）。
> **DIP / Hybrid**：`devices/board.hpp`（抽象 + 工厂 + 宏）+ `bsp/<board>_board.cpp`（具体板）。
> 两种形态不要同时出现在一个工程里。

生成步骤：

1. 先选定架构模式（Layered / DIP / Hybrid），再填写
   [platform-profile.md](../references/platform-profile.md) 的平台契约；
2. 复制 `port/`、`app/`、`bsp/`、`devices/`、`utility/`；
   - Layered：用 `bsp/board.hpp` + `bsp/board.cpp`，删除 `devices/board.hpp`；
   - DIP/Hybrid：用 `devices/board.hpp` + `bsp/board_example.cpp`（改名为
     `bsp/<board>_board.cpp` 并按板重命名类），删除 `bsp/board.hpp` / `bsp/board.cpp` 单例模板；
3. 把 `board_hal.h` 换成目标厂商 HAL 头（例如 `stm32f4xx_hal.h`），按板子重写板级初始化；
4. 按 [toolchains.md](../references/toolchains.md) 配置工程，并用实际工具链编译；
5. 按 [bring-up.md](../checklists/bring-up.md) 逐项核对。

DIP/Hybrid 的选板方式：`devices/board.hpp` 声明 `create_new_board()`，
具体板 `.cpp` 末尾用 `IMPLEMENT_BOARD_CLASS(<BoardClass>)` 提供唯一实现。

- 只编译一个 `bsp/*_board.cpp`；编进两个会链接期重复符号；
- 一个都没编会 undefined reference；
- 因此不需要 `board_select.hpp`，也没有 `#ifdef` 选板。

不要复制 `Core/`、`Drivers/`、`startup_*.s`：它们必须由目标 MCU 的厂商工具生成。
