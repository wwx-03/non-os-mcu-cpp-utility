#pragma once

#include <stddef.h>

#include "led.hpp"
#include "serial.hpp"

// BSP：板级资源与 HAL 回调的唯一入口（单例）。
// 应用层只通过这里获取资源，不直接接触厂商 HAL。
class Board {
public:
	static Board &GetInstance() {
		static Board instance;
		return instance;
	}

	Board(const Board &) = delete;
	Board &operator=(const Board &) = delete;

	static constexpr size_t SERIAL1_RX_SIZE = 1024u;
	static constexpr size_t SERIAL1_TX_SIZE = 1024u;

	static constexpr size_t RED_LED_INDEX = 0;
	static constexpr size_t GREEN_LED_INDEX = 1;

	Serial<SERIAL1_RX_SIZE, SERIAL1_TX_SIZE> *GetSerial();
	Led *GetLed(size_t index = RED_LED_INDEX);
	void RefreshWatchdog();

private:
	Board();
};
