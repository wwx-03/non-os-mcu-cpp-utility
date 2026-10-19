#pragma once

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

	Serial<128, 128> *GetSerial();
	void RefreshWatchdog();

private:
	Board();

	// 成员对象在 Board 构造时初始化，不使用函数内 static
	Serial<128, 128> serial_;
};
