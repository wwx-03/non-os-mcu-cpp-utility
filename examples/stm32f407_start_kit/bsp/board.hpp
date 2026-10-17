#pragma once

#include "serial.hpp"

// BSP：板级资源与 HAL 回调的唯一入口（单例）。
// 应用层只通过这里获取资源，不直接接触厂商 HAL。
class Board {
public:
	static constexpr size_t _SERIAL1_RX_SIZE = 1024u;
	static constexpr size_t _SERIAL1_TX_SIZE = 1024u;

	static Board &GetInstance() {
		static Board instance;
		return instance;
	}

	Board(const Board &) = delete;
	Board &operator=(const Board &) = delete;

	Serial<_SERIAL1_RX_SIZE, _SERIAL1_TX_SIZE> *GetSerial();
	void RefreshWatchdog();

private:

	Board() = default;
};
