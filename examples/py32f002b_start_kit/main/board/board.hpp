#pragma once

#include "serial.hpp"

class Board {
public:
	static Board &GetInstance() {
		static Board instance;
		return instance;
	}

	Board(const Board &) = delete;
	Board &operator=(const Board &) = delete;

	Serial<128, 128> *GetSerial();

private:
	Board() = default;
};
