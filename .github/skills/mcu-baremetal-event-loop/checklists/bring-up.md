# Bring-up 检查清单（裸机事件驱动）

生成或改造工程后逐项核对。平台契约见 [platform-profile.md](../references/platform-profile.md)，
移植与中断见 [porting.md](../references/porting.md)，工程配置见 [toolchains.md](../references/toolchains.md)。

## 平台与移植

- [ ] 架构模式（Layered / DIP / Hybrid）已选定并记录理由
- [ ] 构建期已强制依赖方向；在 `app/` include `bsp/` 会编译失败（DIP/Hybrid）
- [ ] 平台契约已填写并记录到工程（`docs/platform.md` 或 README）
- [ ] `port/mcu_cmsis.h` 指向正确的器件头
- [ ] `__get_PRIMASK` / `__disable_irq` / `__set_PRIMASK` 可用；缺失时在 `mcu_cmsis.h` 中转接，
      `irq_lock.hpp` 未被修改
- [ ] 1 ms tick 只有一个来源，`timer::OnMilliSecondTick()` 只被调用一次
- [ ] 没有重复定义 `SysTick_Handler`
- [ ] 中断优先级符合 HAL 与自身代码的假设（没有跨平台套用数值）

## 结构与事件链路

- [ ] `Application`、`Board` 都是私有构造 + `GetInstance()`，没有第二个实例
- [ ] `Application` 是唯一的事件循环，唯一持有事件队列
- [ ] 所有中断只做：清标志、搬数据、`PostXxx()` 投递
- [ ] 中断里没有 `printf`、`HAL_Delay`、阻塞发送、动态分配、长循环
- [ ] 事件号集中定义在 `Application`，载荷是 POD 且尽量 ≤ 8 字节
- [ ] handler 快速返回，长流程拆状态机
- [ ] 队列满时 `dropped_` 计数可观测，没有静默丢弃
- [ ] 电平型状态用 `PendingEvents` 位图，没有为每个状态塞事件

## 内存与并发

- [ ] 没有 `new` / `malloc` / `std::vector` / `std::string` / `std::function` / 智能指针
- [ ] `irq_lock` 只包住几条共享变量读写，没有跨阻塞调用
- [ ] 所有共享可变状态都有 `irq_lock` 或单写者保证
- [ ] 器件对象都是静态对象（成员或命名空间作用域），没有函数内 `static` 惰性构造
- [ ] 所有单例在开中断之前完成构造

## 分层

- [ ] 应用层没有 include 厂商 HAL 头，只通过 `Board` 取资源
- [ ] `devices/` 没有厂商头、没有引脚号 / 极性
- [ ] DIP/Hybrid 下 `app` 不 include `bsp/`，只看到 `devices` 接口
- [ ] 抽象 `Board` 只含 `GetXxx()`，没有喂狗 / 板号 / 引脚常量
- [ ] 具体板命名为 `<MCU><BoardName><Version>Board`，`final` + `override`
- [ ] 全工程只编译一个 `bsp/*_board.cpp`；多个会链接期重复符号，缺失会 undefined reference
- [ ] 具体板类只在 `.cpp` 内定义，没有为它提供 `.hpp`，也没有多处 `#ifdef` 选板
- [ ] `IMPLEMENT_BOARD_CLASS()` 用函数内静态对象，没有 `new` / `malloc`
- [ ] `devices` 接口析构是 `protected` 非虚，没有 `delete` 基类
- [ ] `Application::Start(Board &)` 是统一签名；Layered 传具体板引用
- [ ] BSP 初始化（时钟、GPIO、外设）只在 `Board`，驱动构造 / 析构里没有
      `HAL_GPIO_Init` / `HAL_GPIO_DeInit`
- [ ] 器件基类构造函数里没有调用纯虚函数；初值采样放在 `Start()`
- [ ] HAL 回调名与实际 API 匹配（`HAL_UART_Receive_IT` → `HAL_UART_RxCpltCallback`）
- [ ] 同一外设的 HAL 弱回调只有一个实现，按句柄 / 引脚路由

## 工程配置与验证

- [ ] 工具链与工程来源一致（Keil/MDK-ARM → AC6；GCC → `.ld`），没有擅自切换
- [ ] 启动文件语法与工具链一致
- [ ] 链接脚本 / scatter 的 Flash、RAM 与芯片一致
- [ ] C++17，`-fno-exceptions -fno-rtti`；GCC 额外 `-Wall -Wextra`
- [ ] EIDE `srcDirs` 含仓库根目录时，`excludeList` 已排除 `examples/**`
- [ ] Keil 工程同步：`<Target>` 下有 `<pCCUsed>` + `<uAC6>1</uAC6>`，
      `.cpp` 以 `FileType 8` 入组，`IncludePath` 含 `app/bsp/port/devices/仓库根目录`
- [ ] 用工程实际工具链编译通过（AC6 看 `build successfully !` 或 `0 Error(s)`，
      GCC 看实际编译 / 链接结果）
- [ ] 上电 smoke test：串口有启动输出、tick 周期正确、无 HardFault / 栈溢出
- [ ] 已启用 IWDG 的工程有周期喂狗
