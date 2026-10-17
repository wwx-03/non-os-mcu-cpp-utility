#include "board.hpp"

#include "usart.h"

#include "timers.hpp"

Serial<128, 128> *Board::GetSerial() {
	static Serial<128, 128> serial(&husart1);
	return &serial;
}

extern "C" void HAL_USART_RxCpltCallback(USART_HandleTypeDef *husart)
{
	(void)husart;
	auto &board = Board::GetInstance();
	auto *serial = board.GetSerial();
	serial->HandleRxCpltEvent();
}

extern "C" void HAL_IncTick(void)
{
	extern __IO uint32_t uwTick;
	uwTick += uwTickFreq;
	timer::OnMilliSecondTick();
}
