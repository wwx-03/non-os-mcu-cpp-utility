#include "application.hpp"

#include "board.hpp"

Application::Application() {
	tick_timer_.create(1000, true, +[](void *ctx) {
		static_cast<Application *>(ctx)->PostEvent(TIMER_ELAPSED_EVENT);
	}, this);

	// IWDG 溢出约 512 ms（LSI 32 kHz / 4 * 4096），这里每 100 ms 喂一次狗
	watchdog_timer_.create(100, true, +[](void *ctx) {
		static_cast<Application *>(ctx)->PostEvent(WATCHDOG_REFRESH_EVENT);
	}, this);

	// 每 250 ms 切换一次 LED 状态
	blink_timer_.create(250, true, +[](void *ctx) {
		static_cast<Application *>(ctx)->PostEvent(BLINK_EVENT);
	}, this);

	auto *serial = Board::GetInstance().GetSerial();

	serial->SetSpecificCharactersCallback('\n', +[](size_t size, void *ctx) {
		static_cast<Application *>(ctx)->PostSerialReceivedEvent(size);
	}, this);

	serial->SetRxBufferFullCallback(+[](size_t size, void *ctx) {
		static_cast<Application *>(ctx)->PostSerialReceivedEvent(size);
	}, this);

	// 按键回调运行在 1ms tick 中断里，这里只投递事件
	Board::GetInstance().GetButton(Board::BUTTON0_INDEX)->SetReleasedCallback(
		+[](void *ctx) {
			static_cast<Application *>(ctx)->PostButtonReleasedEvent(
				static_cast<uint8_t>(Board::BUTTON0_INDEX));
		},
		this);

	PostEvent(START_EVENT);
}

void Application::Start() {
	QueueItem item = {};
	while (true) {
		if (!queue_.pop(item)) {
			continue;
		}

		switch (item.event) {
		case START_EVENT:
			HandleStart();
			break;
		case SERIAL_RECEIVED_EVENT:
			HandleSerialReceived(item);
			break;
		case TIMER_ELAPSED_EVENT:
			HandleTimerElapsed();
			break;
		case WATCHDOG_REFRESH_EVENT:
			HandleWatchdogRefresh();
			break;
		case BLINK_EVENT:
			HandleBlink();
			break;
		case BUTTON_RELEASED_EVENT:
			HandleButtonReleased(item);
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

void Application::PostSerialReceivedEvent(const size_t size) {
	QueueItem item = {};
	item.event = SERIAL_RECEIVED_EVENT;
	item.data.serial_received.size = size;
	if (!queue_.push(item)) {
		++dropped_;
	}
}

void Application::PostButtonReleasedEvent(const uint8_t index) {
	QueueItem item = {};
	item.event = BUTTON_RELEASED_EVENT;
	item.data.button_released.index = index;
	if (!queue_.push(item)) {
		++dropped_;
	}
}

void Application::HandleStart() {
	tick_timer_.start();
	watchdog_timer_.start();
	blink_timer_.start();
	Board::GetInstance().RefreshWatchdog();
}

void Application::HandleSerialReceived(const QueueItem &item) {
	auto *serial = Board::GetInstance().GetSerial();
	uint8_t buffer[128] = {};
	size_t size = item.data.serial_received.size;
	if (size > sizeof(buffer)) {
		size = sizeof(buffer);
	}
	size = serial->Read(buffer, size);
	serial->Write(buffer, size);
}

void Application::HandleTimerElapsed() {
	static uint32_t seconds = 0;
	++seconds;

	auto &board = Board::GetInstance();
	auto *serial = board.GetSerial();
	serial->Write("Tick: %lu, dropped: %lu\r\n", seconds, dropped_);
}

void Application::HandleWatchdogRefresh() {
	Board::GetInstance().RefreshWatchdog();
}

void Application::HandleBlink() {
	auto &board = Board::GetInstance();
	auto *led = board.GetLed(Board::RED_LED_INDEX);
	if (led) {
		led->Toggle();
	}
	led = board.GetLed(Board::GREEN_LED_INDEX);
	if (led) {
		led->Toggle();
	}
}

void Application::HandleButtonReleased(const QueueItem &item) {
	if (item.data.button_released.index != Board::BUTTON0_INDEX) {
		return;
	}

	auto *serial = Board::GetInstance().GetSerial();
	serial->Write("Button0 released\r\n");
}

extern "C" void app_main(void) {
	Application::GetInstance().Start();
}
