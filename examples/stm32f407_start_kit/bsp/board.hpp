#pragma once

#include <stddef.h>

#include "button.hpp"
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

	static constexpr size_t BUTTON0_INDEX = 0;

	Serial<SERIAL1_RX_SIZE, SERIAL1_TX_SIZE> *GetSerial();
	Led *GetLed(size_t index = RED_LED_INDEX);
	Button *GetButton(size_t index = BUTTON0_INDEX);
	void RefreshWatchdog();

private:
	Board();

	// 命名空间作用域替代品：成员在 Board 构造时初始化，
	// 不使用函数内 static（否则首次访问若发生在 ISR 里就等于在中断里初始化外设）
	Serial<SERIAL1_RX_SIZE, SERIAL1_TX_SIZE> serial_;
};
