#pragma once

#include <stdint.h>

#include "mcu_cmsis.h" // 桥接头：由工程提供，包含具体 MCU 的 CMSIS

class IrqLock {
public:
	IrqLock() { primask_ = __get_PRIMASK(); __disable_irq(); }
	~IrqLock() { __set_PRIMASK(primask_); }
	IrqLock(const IrqLock &) = delete;
	IrqLock &operator=(const IrqLock &) = delete;
private:
	uint32_t primask_;
};