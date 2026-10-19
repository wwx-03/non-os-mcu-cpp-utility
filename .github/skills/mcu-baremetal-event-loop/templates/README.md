# 代码模板

本目录是**可复制的骨架**，不是完整可编译工程。复制到目标工程后，需要按该板的
HAL 与引脚完成 `TODO` 标记的位置；完整可运行工程见 `examples/`。

| 模板 | 复制到 | 说明 |
| --- | --- | --- |
| `port/mcu_cmsis.h` | `<project>/port/mcu_cmsis.h` | 唯一的 MCU 依赖点，换成目标器件头 |
| `app/application.hpp` | `<project>/app/application.hpp` | 事件循环单例与事件号 |
| `app/application.cpp` | `<project>/app/application.cpp` | `Start()` 派发循环、`PostXxx()`、handler |
| `bsp/board.hpp` | `<project>/bsp/board.hpp` | 板级资源与 HAL 回调入口 |
| `bsp/board.cpp` | `<project>/bsp/board.cpp` | 单例、驱动对象、HAL 弱回调转发、1 ms tick |
| `devices/led.hpp` | `<project>/devices/led.hpp` | 与 MCU 无关的 LED 抽象（NVI） |
| `devices/button.hpp` | `<project>/devices/button.hpp` | 与 MCU 无关的按键消抖（NVI） |
| `bsp/gpio_led.hpp` | `<project>/bsp/gpio_led.hpp` | `Led` 的 GPIO 实现 |
| `bsp/gpio_button.hpp` | `<project>/bsp/gpio_button.hpp` | `Button` 的 GPIO 实现 |
| `utility/pending_events.hpp` | `<project>/utility/pending_events.hpp` | 电平型事件位图 |

生成步骤：

1. 先填写 [platform-profile.md](../references/platform-profile.md) 的平台契约；
2. 复制 `port/`、`app/`、`bsp/`、`devices/`、`utility/`；
3. 把 `board_hal.h` 换成目标厂商 HAL 头（例如 `stm32f4xx_hal.h`），按板子重写 `board.cpp` 中的引脚与外设；
4. 按 [toolchains.md](../references/toolchains.md) 配置工程，并用实际工具链编译；
5. 按 [bring-up.md](../checklists/bring-up.md) 逐项核对。

不要复制 `Core/`、`Drivers/`、`startup_*.s`：它们必须由目标 MCU 的厂商工具生成。
