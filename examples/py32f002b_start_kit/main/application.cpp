#include "application.hpp"
#include "board.hpp"

Application::Application() {
	timer_.create(1000, true, +[](void *ctx) {
		auto *thiz = static_cast<Application *>(ctx);
		thiz->PostEvent(TIMER_ELAPSED_EVENT);
	}, this);

	auto &board = Board::GetInstance();
	auto *serial = board.GetSerial();
	serial->SetSpecificCharactersCallback('\n', +[](size_t size, void *ctx) {
		auto *thiz = static_cast<Application *>(ctx);
		thiz->PostSerialReceivedEvent(size);
	}, this);

	serial->SetRxBufferFullCallback(+[](size_t size, void *ctx) {
		auto *thiz = static_cast<Application *>(ctx);
		thiz->PostSerialReceivedEvent(size);
	}, this);

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
		}
	}
}

void Application::PostEvent(const uint32_t e) {
	QueueItem item = {};
	item.event = e;
	queue_.push(item);
}

void Application::PostSerialReceivedEvent(const size_t size) {
	QueueItem item = {};
	item.event = SERIAL_RECEIVED_EVENT;
	item.data.serial_received.size = size;
	queue_.push(item);
}

void Application::HandleStart() {
	timer_.start();
}

void Application::HandleSerialReceived(const QueueItem &item) {
	auto &board = Board::GetInstance();
	auto *serial = board.GetSerial();
	uint8_t buffer[128] = {};
	size_t n = serial->Read(buffer, item.data.serial_received.size);
	serial->Write(buffer, n);
}

void Application::HandleTimerElapsed() {
	auto &board = Board::GetInstance();
	auto *serial = board.GetSerial();
	serial->Write("seconds elapsed\r\n", sizeof("seconds elapsed\r\n") - 1);
}

extern "C" void app_main() {
	Application::GetInstance().Start();
}
