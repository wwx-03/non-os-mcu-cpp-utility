#pragma once

#include <stdint.h>

#include "irq_lock.hpp"

// 电平型事件位图：按键按住、缓冲区满、故障标志这类"状态"用它，
// 不要为每个状态都塞一条队列事件。
class PendingEvents {
public:
	void set(const uint32_t bits) {
		irq_lock lock;
		bits_ |= bits;
	}

	uint32_t take() {
		irq_lock lock;
		const uint32_t value = bits_;
		bits_ = 0;
		return value;
	}

	bool any() const {
		irq_lock lock;
		return bits_ != 0;
	}

private:
	volatile uint32_t bits_ = 0;
};
