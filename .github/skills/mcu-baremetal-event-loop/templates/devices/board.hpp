#pragma once

#include <stddef.h>

// 抽象 Board（仅 DIP / Hybrid 模式使用）。
//
// 只暴露 devices 层接口：不出现 HAL 类型、引脚号、外设句柄与板级常量。
// 具体板类**只在 bsp/<board>_board.cpp 内部定义**，不提供 .hpp；
// 板级实现通过 IMPLEMENT_BOARD_CLASS() 注册到下面的工厂。
//
// Layered 模式不要使用本文件：直接用具体 Board 单例即可。

class Button;
class Led;
class Network;
class Board;

// 由具体板的 .cpp 通过 IMPLEMENT_BOARD_CLASS() 提供。
// 全工程只能有一份定义：多份会链接期重复符号，缺失会 undefined reference，
// 这正好把"选板"变成编译/链接期约束。
Board *create_new_board();

class Board {
public:
	// 首次调用必须在中断使能之前完成（见 SKILL.md 的初始化顺序）。
	// 之后所有模块只通过本接口取资源，不关心具体板类型。
	static Board &GetInstance() {
		static Board *instance = create_new_board();
		return *instance;
	}

	Board(const Board &) = delete;
	Board &operator=(const Board &) = delete;

	virtual Led *GetLed(size_t index) = 0;
	virtual Button *GetButton(size_t index) = 0;
	virtual Network *GetNetwork() = 0;

protected:
	// 子类只能通过基类构造；声明拷贝构造会抑制隐式默认构造，必须显式保留
	Board() = default;
	// 实例是函数内静态对象，不会通过基类指针 delete，析构无需虚函数
	~Board() = default;
};

// 在具体板 .cpp 的末尾调用一次：注册本板的唯一实例。
//
//   IMPLEMENT_BOARD_CLASS(Stm32F407StartKitBoard);
//
// 使用函数内静态对象，不引入 new / malloc（静态内存铁律）；
// 首次调用发生在 main() 里、中断使能之前。
#define IMPLEMENT_BOARD_CLASS(class_name) \
	Board *create_new_board() {            \
		static class_name instance;         \
		return &instance;                   \
	}
