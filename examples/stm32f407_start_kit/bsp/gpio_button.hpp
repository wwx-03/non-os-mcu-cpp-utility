#pragma once

#include "stm32f4xx_hal.h"

#include "button.hpp"

// BSP：GPIO 直连按键。
// 引脚的时钟与输入模式由 Board 统一初始化（见 bsp/board.cpp），
// active_high 表示"按下时为高电平"。
class GpioButton final : public Button {
public:
	GpioButton(GPIO_TypeDef *port, uint16_t pin, bool active_high = true)
		: port_(port), pin_(pin), active_high_(active_high) {}

private:
	GPIO_TypeDef *port_;
	uint16_t pin_;
	const bool active_high_;

	bool Read() const override {
		const bool level = HAL_GPIO_ReadPin(port_, pin_) == GPIO_PIN_SET;
		return level == active_high_;
	}
};