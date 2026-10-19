// Board 骨架：复制后按目标板完成 TODO。
// 这是一块板子一份的实现，不属于 utility 库。
#include "board.hpp"

#include "board_hal.h"   // TODO: 换成本工程的 HAL 初始化头，例如 "usart.h" / "gpio.h" / "iwdg.h"
#include "serial.hpp"    // TODO: 本板 BSP 驱动，需定义 BoardSerial

#include "timers.hpp"

namespace {
// 静态对象：命名空间作用域，无 __cxa_guard，在 main() 之前构造
BoardSerial serial(&huart1);   // TODO: 换成实际 HAL 句柄与缓冲大小
}

Board::Board() : serial_(&serial) {
	// TODO: 使能时钟、配置 GPIO / 外设；初始化完成后设置初始电平
}

BoardSerial *Board::GetSerial() {
	return serial_;
}

size_t Board::ReadSerial(void *dest, size_t size) {
	return serial_->Read(dest, size);   // TODO: 对齐本板驱动的读取接口
}

void Board::OnSerialData(const serial_data_callback_t cb, void *ctx) {
	serial_cb_ = cb;
	serial_ctx_ = ctx;
}

void Board::OnKeyPressed(const key_callback_t cb, void *ctx) {
	key_cb_ = cb;
	key_ctx_ = ctx;
}

void Board::DispatchSerialRx() {
	if (serial_cb_ != nullptr && serial_ != nullptr) {
		serial_cb_(serial_->Available(), serial_ctx_);   // TODO: 对齐可用字节数接口
	}
}

void Board::DispatchExti(const uint16_t pin) {
	(void)pin;
	if (key_cb_ != nullptr) {
		key_cb_(0, key_ctx_);   // TODO: 按引脚映射到按键索引
	}
}

void Board::RefreshWatchdog() {
	// TODO: HAL_IWDG_Refresh(&hiwdg);
}

// HAL 弱回调：只路由，不解析
extern "C" void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	(void)huart;
	Board::GetInstance().DispatchSerialRx();
}

extern "C" void HAL_GPIO_EXTI_Callback(uint16_t pin) {
	Board::GetInstance().DispatchExti(pin);
}

// 1ms tick 的唯一接入点：SysTick_Handler -> HAL_IncTick -> 软件定时器
extern "C" void HAL_IncTick(void) {
	extern __IO uint32_t uwTick;
	extern HAL_TickFreqTypeDef uwTickFreq;
	uwTick += uwTickFreq;
	timer::OnMilliSecondTick();
}
