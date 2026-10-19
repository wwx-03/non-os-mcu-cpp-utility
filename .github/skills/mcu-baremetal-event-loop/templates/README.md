# 代码模板

本目录是**可复制的骨架**，不是完整可编译工程。复制到目标工程后，需要按该板的
HAL 与引脚完成 `TODO` 标记的位置；完整可运行工程见 `examples/`。

| 模板 | 复制到 | 说明 |
| --- | --- | --- |
| `port/mcu_cmsis.h` | `<project>/port/mcu_cmsis.h` | 唯一的 MCU 依赖点，换成目标器件头 |
| `app/application.hpp` | `<project>/app/application.hpp` | 事件循环单例与事件号（**Layered**） |
| `app/application.cpp` | `<project>/app/application.cpp` | `Start()` 派发循环、`PostXxx()`、handler（**Layered**） |
| `app/application_dip.hpp` | `<project>/app/application.hpp` | DIP/Hybrid 版声明：只依赖 `devices/board.hpp`，`Start(Board &)` |
| `bsp/board.hpp` | `<project>/bsp/board.hpp` | 板级资源与 HAL 回调入口 |
| `bsp/board.cpp` | `<project>/bsp/board.cpp` | 单例、驱动对象、HAL 弱回调转发、1 ms tick |
| `devices/led.hpp` | `<project>/devices/led.hpp` | 与 MCU 无关的 LED 抽象（NVI） |
| `devices/button.hpp` | `<project>/devices/button.hpp` | 与 MCU 无关的按键消抖（NVI） |
| `devices/board.hpp` | `<project>/devices/board.hpp` | 抽象 Board（**仅 DIP / Hybrid**） |
| `bsp/gpio_led.hpp` | `<project>/bsp/gpio_led.hpp` | `Led` 的 GPIO 实现 |
| `bsp/gpio_button.hpp` | `<project>/bsp/gpio_button.hpp` | `Button` 的 GPIO 实现 |
| `bsp/board_select.hpp` | `<project>/bsp/board_select.hpp` | 唯一选板点（**仅 DIP / Hybrid**） |
| `bsp/board_example.hpp` | `<project>/bsp/<board>_board.hpp` | 具体板骨架，按 `<MCU><BoardName><Version>Board` 重命名 |
| `utility/pending_events.hpp` | `<project>/utility/pending_events.hpp` | 电平型事件位图 |

> `bsp/board.hpp`（具体 `Board` 单例）用于 **Layered**；
> `devices/board.hpp` + `bsp/board_select.hpp` + `bsp/board_example.hpp` 用于 **DIP / Hybrid**。
> 两种形态不要同时出现在一个工程里。

生成步骤：

1. 先选定架构模式（Layered / DIP / Hybrid），再填写
   [platform-profile.md](../references/platform-profile.md) 的平台契约；
2. 复制 `port/`、`app/`、`bsp/`、`devices/`、`utility/`；
   - Layered：用 `bsp/board.hpp`，删除 `devices/board.hpp`、`bsp/board_select.hpp`；
   - DIP/Hybrid：用 `devices/board.hpp` + `bsp/board_example.hpp`（改名）+ `bsp/board_select.hpp`，
     并删除具体 `bsp/board.hpp` 单例模板；
3. 把 `board_hal.h` 换成目标厂商 HAL 头（例如 `stm32f4xx_hal.h`），按板子重写 `board.cpp` 中的引脚与外设；
4. 按 [toolchains.md](../references/toolchains.md) 配置工程，并用实际工具链编译；
5. 按 [bring-up.md](../checklists/bring-up.md) 逐项核对。

不要复制 `Core/`、`Drivers/`、`startup_*.s`：它们必须由目标 MCU 的厂商工具生成。
