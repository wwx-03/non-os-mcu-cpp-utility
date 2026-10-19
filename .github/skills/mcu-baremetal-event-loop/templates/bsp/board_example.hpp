#pragma once

#include "devices/board.hpp"

#include "bsp/gpio_button.hpp"
#include "bsp/gpio_led.hpp"

// 具体板骨架（DIP / Hybrid）。
//
// 复制到 bsp/<board_name>_board.hpp 后按目标板重命名类：
//   <MCU><BoardName><Version>Board，例如 Stm32F407StartKitBoard。
// 同时把 board_select.hpp 里的 ActiveBoard 指过来。
//
// 规则：
// - final + override 显式写出；
// - 只实现抽象 Board 的 GetXxx()，板级专用能力（喂狗、板号、自检）留在本类；
// - 所有器件对象都是成员，不做动态分配；
// - 构造函数里完成板级初始化，且必须早于中断使能。

class BoardExample final : public Board {
public:
	static BoardExample &GetInstance() {
		static BoardExample instance;
		return instance;
	}

	BoardExample(const BoardExample &) = delete;
	BoardExample &operator=(const BoardExample &) = delete;

	Led *GetLed(size_t index) override;
	Button *GetButton(size_t index) override;
	Network *GetNetwork() override;

	// 板级专用能力：不放进抽象 Board
	void RefreshWatchdog();

private:
	BoardExample();

	GpioLed led_red_;
	GpioLed led_green_;
	GpioButton button0_;
	Network network_;
};
