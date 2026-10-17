#include "board.hpp"

#include "gpio_led.hpp"
#include "iwdg.h"
#include "usart.h"

#include "timers.hpp"

namespace {
// GPIOx 展开为 (GPIO_TypeDef *)基址，不是 C++ 常量表达式，这里用 const 指针
GPIO_TypeDef *const RED_LED_PORT = GPIOF;
constexpr uint16_t RED_LED_PIN = GPIO_PIN_9;
GPIO_TypeDef *const GREEN_LED_PORT = GPIOF;
constexpr uint16_t GREEN_LED_PIN = GPIO_PIN_10;

// 命名空间作用域的静态对象：没有 __cxa_guard 守卫，且在 main() 之前构造
GpioLed red_led(RED_LED_PORT, RED_LED_PIN, false);
GpioLed green_led(GREEN_LED_PORT, GREEN_LED_PIN, false);
}

Board::Board() {
	__HAL_RCC_GPIOF_CLK_ENABLE();

	GPIO_InitTypeDef init = {};
	init.Mode = GPIO_MODE_OUTPUT_PP;
	init.Pull = GPIO_NOPULL;
	init.Speed = GPIO_SPEED_FREQ_LOW;

	init.Pin = RED_LED_PIN;
	HAL_GPIO_Init(RED_LED_PORT, &init);

	init.Pin = GREEN_LED_PIN;
	HAL_GPIO_Init(GREEN_LED_PORT, &init);

	// 外设就绪后再确定初始电平，避免在时钟未使能时写寄存器
	red_led.SetState(false);
	green_led.SetState(false);
}

Led *Board::GetLed(const size_t index) {
	switch (index) {
	case RED_LED_INDEX:
		return &red_led;
	case GREEN_LED_INDEX:
		return &green_led;
	default:
		return nullptr;
	}
}

void Board::RefreshWatchdog() {
	HAL_IWDG_Refresh(&hiwdg);
}

Serial<Board::SERIAL1_RX_SIZE, Board::SERIAL1_TX_SIZE> *Board::GetSerial() {
	static Serial<SERIAL1_RX_SIZE, SERIAL1_TX_SIZE> serial(&huart1);
	return &serial;
}

extern "C" void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	if (huart->Instance != USART1) {
		return;
	}
	Board::GetInstance().GetSerial()->HandleRxCpltEvent();
}

extern "C" void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
	if (huart->Instance != USART1) {
		return;
	}
	Board::GetInstance().GetSerial()->HandleErrorEvent();
}

// 1ms tick 的唯一接入点：SysTick_Handler -> HAL_IncTick -> 软件定时器
extern "C" void HAL_IncTick(void) {
	extern __IO uint32_t uwTick;
	extern HAL_TickFreqTypeDef uwTickFreq;
	uwTick += uwTickFreq;
	timer::OnMilliSecondTick();
}
