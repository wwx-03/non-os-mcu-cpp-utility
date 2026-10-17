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
