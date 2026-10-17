#include "board.hpp"

#include "iwdg.h"
#include "usart.h"

#include "timers.hpp"

Serial<128, 128> *Board::GetSerial() {
	static Serial<128, 128> serial(&husart1);
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

extern "C" void HAL_IncTick(void) {
	extern __IO uint32_t uwTick;
	uwTick += uwTickFreq;
	timer::OnMilliSecondTick();
}
