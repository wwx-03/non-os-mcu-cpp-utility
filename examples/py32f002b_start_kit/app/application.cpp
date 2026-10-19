#include "application.hpp"
#include "board.hpp"

Application::Application()
	: board_(&Board::GetInstance()) {
	tick_timer_.create(1000, true, +[](void *ctx) {
		static_cast<Application *>(ctx)->PostEvent(TIMER_ELAPSED_EVENT);
	}, this);

	// IWDG 的溢出时间约 512ms，这里每 100ms 喂一次狗
	watchdog_timer_.create(100, true, +[](void *ctx) {
		static_cast<Application *>(ctx)->PostEvent(WATCHDOG_REFRESH_EVENT);
	}, this);

	auto *serial = board_->GetSerial();
	serial->SetSpecificCharactersCallback('\n', +[](size_t size, void *ctx) {
		static_cast<Application *>(ctx)->PostSerialReceivedEvent(size);
	}, this);

	serial->SetRxBufferFullCallback(+[](size_t size, void *ctx) {
		static_cast<Application *>(ctx)->PostSerialReceivedEvent(size);
	}, this);

	PostEvent(START_EVENT);
}

void Application::Start(Board &board) {
	board_ = &board;

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

void Application::HandleStart() {
	tick_timer_.start();
	watchdog_timer_.start();
	board_->RefreshWatchdog();
}

void Application::HandleSerialReceived(const QueueItem &item) {
	auto *serial = board_->GetSerial();
	uint8_t buffer[128] = {};
	size_t n = serial->Read(buffer, item.data.serial_received.size);
	serial->Write(buffer, n);
}

void Application::HandleTimerElapsed() {
	board_->GetSerial()->Write("seconds elapsed\r\n", sizeof("seconds elapsed\r\n") - 1);
}

void Application::HandleWatchdogRefresh() {
	board_->RefreshWatchdog();
}

extern "C" void app_main(void) {
	Application::GetInstance().Start(Board::GetInstance());
}
