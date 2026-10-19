#pragma once

#include <stddef.h>
#include <stdint.h>

#include "queue.hpp"
#include "timers.hpp"

class Board;

// 应用层：全工程唯一的事件循环（单例）。
// 中断只通过 PostXxx() 投递事件，业务全部在 Start() 的派发循环里处理。
class Application {
public:
	static Application &GetInstance() {
		static Application instance;
		return instance;
	}

	Application(const Application &) = delete;
	Application &operator=(const Application &) = delete;

	// Layered 与 DIP/Hybrid 统一签名：传具体 Board 或抽象 Board 的引用
	void Start(Board &board);

private:
	static constexpr size_t QUEUE_LENGTH = 16;

	static constexpr uint32_t EVT_START = 0x00000001u;
	static constexpr uint32_t EVT_SERIAL_DATA = 0x00000002u;
	static constexpr uint32_t EVT_TICK = 0x00000003u;
	static constexpr uint32_t EVT_KEY = 0x00000004u;

	struct QueueItem {
		uint32_t event;
		union {
			struct {
				size_t size;
			} serial;
			struct {
				uint8_t index;
			} key;
		} data;
	};

	queue<QueueItem, QUEUE_LENGTH> queue_;
	timer tick_timer_;
	// 只保存注入进来的板级接口；具体类型由 main() 决定
	Board *board_ = nullptr;
	volatile uint32_t dropped_ = 0;

	Application();
	~Application() = default;

	// 投递入口：可在中断上下文调用，只做 POD 构造 + push
	void PostEvent(uint32_t event);
	void PostSerialData(size_t size);
	void PostKey(uint8_t index);

	// 业务入口：只在 Start() 里调用，必须快速返回
	void HandleStart();
	void HandleSerialData(const QueueItem &item);
	void HandleTick();
	void HandleKey(const QueueItem &item);
};
