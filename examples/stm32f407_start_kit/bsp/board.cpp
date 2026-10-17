#include "board.hpp"

#include "iwdg.h"
#include "usart.h"

#include "timers.hpp"

Serial<Board::_SERIAL1_RX_SIZE, Board::_SERIAL1_TX_SIZE> *Board::GetSerial() {
	static Serial<_SERIAL1_RX_SIZE, _SERIAL1_TX_SIZE> serial(&huart1);
	return &serial;
}

void Board::RefreshWatchdog() {
	HAL_IWDG_Refresh(&hiwdg);
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
