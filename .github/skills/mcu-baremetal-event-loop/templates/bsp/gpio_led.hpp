#pragma once

#include "board_hal.h"   // TODO: 换成目标厂商 HAL 头，例如 "stm32f4xx_hal.h" / "py32f0xx_hal.h"

#include "led.hpp"

// BSP：GPIO 直连 LED。
// 引脚的时钟与输出模式由 Board 统一初始化（见 bsp/board.cpp），
// 驱动自身不碰 GPIO 配置，只负责电平与极性。
class GpioLed final : public Led {
public:
	explicit GpioLed(GPIO_TypeDef *port, uint16_t pin, bool active_high = true)
		: port_(port), pin_(pin), active_high_(active_high) {}

private:
	GPIO_TypeDef *port_;
	uint16_t pin_;
	const bool active_high_;

	void SetIo(bool on) override {
		const GPIO_PinState level = (on == active_high_) ? GPIO_PIN_SET : GPIO_PIN_RESET;
		HAL_GPIO_WritePin(port_, pin_, level);
	}
};
