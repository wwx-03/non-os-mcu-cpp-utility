#include "application.hpp"

#include "board.hpp"

Application::Application() {
	// 定时器回调运行在 1ms 中断里：只投递事件
	tick_timer_.create(1000, true, +[](void *ctx) {
		static_cast<Application *>(ctx)->PostEvent(EVT_TICK);
	}, this);

	// BSP 回调运行在中断里：只投递事件
	Board::GetInstance().OnSerialData(+[](size_t size, void *ctx) {
		static_cast<Application *>(ctx)->PostSerialData(size);
	}, this);

	Board::GetInstance().OnKeyPressed(+[](uint8_t index, void *ctx) {
		static_cast<Application *>(ctx)->PostKey(index);
	}, this);

	PostEvent(EVT_START);   // 初始事件：主循环启动后第一个处理
}

void Application::Start() {
	QueueItem item = {};
	while (true) {
		if (!queue_.pop(item)) {
			continue;   // 需要省电时改为 __WFI()：SysTick 每 1ms 唤醒一次
		}

		switch (item.event) {
		case EVT_START:
			HandleStart();
			break;
		case EVT_SERIAL_DATA:
			HandleSerialData(item);
			break;
		case EVT_TICK:
			HandleTick();
			break;
		case EVT_KEY:
			HandleKey(item);
			break;
		default:
			break;
		}
	}
}

void Application::PostEvent(const uint32_t event) {
	QueueItem item = {};
	item.event = event;
	if (!queue_.push(item)) {
		++dropped_;
	}
}

void Application::PostSerialData(const size_t size) {
	QueueItem item = {};
	item.event = EVT_SERIAL_DATA;
	item.data.serial.size = size;
	if (!queue_.push(item)) {
		++dropped_;
	}
}

void Application::PostKey(const uint8_t index) {
	QueueItem item = {};
	item.event = EVT_KEY;
	item.data.key.index = index;
	if (!queue_.push(item)) {
		++dropped_;
	}
}

void Application::HandleStart() {
	tick_timer_.start();
}

void Application::HandleSerialData(const QueueItem &item) {
	uint8_t buffer[64] = {};
	const size_t n = Board::GetInstance().ReadSerial(buffer, item.data.serial.size);
	(void)n;   // TODO: 按协议解析或回显
}

void Application::HandleTick() {
	// TODO: 周期业务
}

void Application::HandleKey(const QueueItem &item) {
	(void)item;   // TODO: 按键业务
}

extern "C" void app_main(void) {
	Application::GetInstance().Start();
}
