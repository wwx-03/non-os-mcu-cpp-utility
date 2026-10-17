# 范例工程

每个子目录都是一个**独立可编译**的工程，直接引用仓库根目录的头文件，不复制库代码。

| 范例 | MCU | 工具链 / IDE | 演示内容 |
|---|---|---|---|
| [py32f002b_start_kit](./py32f002b_start_kit/README.md) | PY32F002Bx5 | GCC + EIDE | 1ms tick 软件定时器、UART 中断收 + `stream_buffer`、`queue` 事件循环、`Board` BSP 单例 |
| [stm32f407_start_kit](./stm32f407_start_kit/README.md) | STM32F407ZGTx | **AC6（armclang）** + Keil MDK / EIDE | 同上，另演示「Keil 工程 + EIDE 工程共用同一份源码」

## 目录约定

所有范例都按同一分层组织，与仓库 `SKILL.md` 一致：

```
<example>/
├── app/        # 应用层：Application 单例、事件号、handler
├── bsp/        # 板级支持包：Board 单例、驱动、HAL 回调转发
├── devices/    # 器件抽象：只描述行为，不含任何厂商头
├── port/       # mcu_cmsis.h：唯一的 MCU 依赖点
├── Core/       # 厂商生成的初始化与中断入口
├── Drivers/    # 厂商 CMSIS + HAL
├── EIDE/       # IDE 工程（本仓库使用 EIDE）
└── README.md
```

`Core/`、`Drivers/` 由厂商工具生成，不要手改；新增范例时复制一个现有范例的 `app/`、`bsp/`、`devices/`、`port/` 四层即可。

依赖方向：`app → bsp → devices → utility → port → CMSIS`。`devices/` 是"器件是什么"（`Led`、`Button`…），`bsp/` 是"它在这块板上怎么接"（引脚、极性、初始化）。

## 作为新工程模板

先按工程来源选模板：

| 工程来源 | 用哪个模板 |
|---|---|
| Keil MDK / CubeMX 生成的 `MDK-ARM/*.uvprojx` | [stm32f407_start_kit](./stm32f407_start_kit/README.md)（AC6 + `.sct`） |
| CubeMX Makefile 或已有 `.ld` | [py32f002b_start_kit](./py32f002b_start_kit/README.md)（GCC + `.ld`） |

选定后复制对应范例的 `app/`、`bsp/`、`devices/`、`port/` 到新工程，按目标板重写：

1. `port/mcu_cmsis.h` 换成目标器件的 CMSIS 头；
2. `bsp/board.*` 换成目标板的资源与 HAL 回调；
3. `devices/` 只保留与 MCU 无关的器件抽象，引脚/极性/初始化留在 `bsp/`；
4. `app/application.*` 修改事件号、载荷与 handler；
5. 在 1ms 中断里调用 `timer::OnMilliSecondTick()`；
6. 工程配置按 [SKILL.md](../.github/skills/mcu-baremetal-event-loop/SKILL.md) 的「EIDE 工程配置」设置：`srcDirs` 加 `app/`、`bsp/`、`devices/`、`port/`，`incList` 加仓库根目录与 `devices/`，C++ 用 `c++17` 并关闭异常/RTTI，**工具链沿用工程原有那套**。

详细规则与反模式见 [SKILL.md](../.github/skills/mcu-baremetal-event-loop/SKILL.md)。
