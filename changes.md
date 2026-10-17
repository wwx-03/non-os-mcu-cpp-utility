# 变更记录

## 0002 — 2026-09-29

### 新增
- `main/mcu_cmsis.h`：MCU 桥接头，仅转发 `eg. #include "py32f0xx.h"`。让上层头文件通过它间接依赖当前 MCU 的 CMSIS，避免写死具体器件头文件。

### 修改
- `irq_lock.hpp`：
  - 改为 `#include "mcu_cmsis.h"`（此前未包含任何 CMSIS 头）。
  - 修复 `__get_PRIMASK()` / `__disable_irq()` / `__set_PRIMASK()` 未声明的问题——这些内建函数定义在 `cmsis_gcc.h`，需经厂商提供的头文件逐级引入。
  - 清理已不再需要的空实现兜底宏注释。

## 0003 — 2026-10-17

### 新增
- `timers.hpp`：header-only 软件定时器，由 `main/timer` 移植而来。
  - 只需在 1ms 中断中调用 `timer::OnMilliSecondTick()`，即可驱动所有已创建的定时器实例。
  - 支持周期/单次触发、运行中修改周期、启停与重置；无动态内存，实例可以是全局、静态或成员对象。
  - `start()` / `change_period()` 会重新计时；单次触发在回调前失活，因此可在回调内重新 `start()`。
  - 链表操作与状态修改均受 `irq_lock` 保护，可在主循环和中断间安全调用。
  - 回调运行于中断上下文，应保持简短；定时器对象的生命周期需覆盖其绑定期间，释放对象前请先调用 `destroy()`（析构函数会自动处理）。

## 0004 — 2026-10-17

### 新增
- `README.md`：库说明、快速开始、1ms tick 接入方式、工程模式概要，以及 Agent Skill 的三种接入方式。
- `.github/skills/mcu-baremetal-event-loop/`：Agent Skill，约定引入本仓库后必须沿用的工程模式。
  - `SKILL.md`：分层与依赖方向、三条铁律（中断只投递、静态内存、`irq_lock`）、`Application` 事件循环与投递链路、`Board`（BSP）板级资源契约、单例约定（`Application` / `Board` 只允许 `GetInstance()`）、API 速查、新增功能的标准流程、命名风格与反模式对照表。
  - `references/adapter-templates.md`：`mcu_cmsis.h`、tick 接入、`Board` 契约、`Application` 事件循环骨架、电平事件位图、队列容量与溢出策略。`Serial` / `Timer` 只作为接口命名示意，不提供实现。
