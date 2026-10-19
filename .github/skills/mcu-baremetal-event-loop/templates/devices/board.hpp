#pragma once

#include <stddef.h>

// 抽象 Board（仅 DIP / Hybrid 模式使用）。
//
// 只暴露 devices 层接口：不出现 HAL 类型、引脚号、外设句柄与板级常量。
// 具体板类放在 bsp/，命名 <MCU><BoardName><Version>Board，
// 由 bsp/board_select.hpp 选定，main() 注入 Application::Start(Board &)。
//
// Layered 模式不要使用本文件：直接用具体 Board 单例即可。

class Button;
class Led;
class Network;

class Board {
public:
	Board(const Board &) = delete;
	Board &operator=(const Board &) = delete;

	virtual Led *GetLed(size_t index) = 0;
	virtual Button *GetButton(size_t index) = 0;
	virtual Network *GetNetwork() = 0;

protected:
	// 实例都是静态对象，不会通过基类指针 delete，析构无需虚函数
	~Board() = default;
};
