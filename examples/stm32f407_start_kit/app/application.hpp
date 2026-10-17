#pragma once

#include <stddef.h>
#include <stdint.h>

#include "queue.hpp"
#include "timers.hpp"

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

	void Start();

private:
	static constexpr size_t QUEUE_LENGTH = 16;

	static constexpr uint32_t START_EVENT = 0x00000001u;
	static constexpr uint32_t SERIAL_RECEIVED_EVENT = 0x00000002u;
	static constexpr uint32_t TIMER_ELAPSED_EVENT = 0x00000003u;
	static constexpr uint32_t WATCHDOG_REFRESH_EVENT = 0x00000004u;

	struct QueueItem {
		uint32_t event;
		union {
			struct {
				size_t size;
			} serial_received;
		} data;
	};

	queue<QueueItem, QUEUE_LENGTH> queue_;
	timer tick_timer_;
	timer watchdog_timer_;
	volatile uint32_t dropped_ = 0;

	Application();
	~Application() = default;

	void PostEvent(uint32_t event);
	void PostSerialReceivedEvent(size_t size);

	void HandleStart();
	void HandleSerialReceived(const QueueItem &item);
	void HandleTimerElapsed();
	void HandleWatchdogRefresh();
};
