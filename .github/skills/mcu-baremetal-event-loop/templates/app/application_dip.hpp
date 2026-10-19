#pragma once

#include <stddef.h>
#include <stdint.h>

#include "devices/board.hpp"
#include "queue.hpp"
#include "timers.hpp"

// 应用层（DIP / Hybrid）：全工程唯一的事件循环（单例）。
//
// 与 Layered 版的区别只有一点：
//   Application 只 include devices/board.hpp，不 include 任何 bsp/ 头；
//   main() 用 Start(Board::GetInstance()) 注入，具体板由 IMPLEMENT_BOARD_CLASS 注册。
//
// 中断只通过 PostXxx() 投递事件，业务全部在 Start() 的派发循环里处理。

class Application {
public:
	static Application &GetInstance() {
		static Application instance;
		return instance;
	}

	Application(const Application &) = delete;
	Application &operator=(const Application &) = delete;

	// Layered 也使用同一签名：传具体 Board 的引用即可
	void Start(Board &board);

private:
	static constexpr size_t QUEUE_LENGTH = 16;

	static constexpr uint32_t EVT_START = 0x00000001u;
	static constexpr uint32_t EVT_TICK = 0x00000002u;
	static constexpr uint32_t EVT_KEY = 0x00000003u;

	struct QueueItem {
		uint32_t event;
		union {
			struct {
				uint8_t index;
			} key;
		} data;
	};

	queue<QueueItem, QUEUE_LENGTH> queue_;
	timer tick_timer_;
	// 只保存抽象指针；具体板类型在 main() 里决定
	Board *board_ = nullptr;
	volatile uint32_t dropped_ = 0;

	Application();
	~Application() = default;

	// 投递入口：可在中断上下文调用，只做 POD 构造 + push
	void PostEvent(uint32_t event);
	void PostKey(uint8_t index);

	// 业务入口：只在 Start() 里调用，必须快速返回
	void HandleStart();
	void HandleTick();
	void HandleKey(const QueueItem &item);
};
