#pragma once

#include <stdint.h>

#include "irq_lock.hpp"

// 软件定时器（单链表，无动态内存）。
// 移植：把 timer::OnMilliSecondTick() 放进 1ms 中断即可，所有已创建的定时器都会原位更新。
// 回调在中断上下文中执行，应保持简短；回调内可以安全调用 start/stop/reset/change_period/destroy。
class timer {
public:
	using callback_t = void (*)(void *context);

	timer() = default;
	~timer() { destroy(); }

	timer(const timer &) = delete;
	timer &operator=(const timer &) = delete;

	// period_ms 必须 >= 1；reload 为 true 时周期触发，为 false 时单次触发
	bool create(const uint32_t period_ms, const bool reload, callback_t cb, void *context = nullptr) {
		if (period_ms == 0) {
			return false;
		}

		irq_lock lock;
		if (bound_) {
			return false;
		}

		period_ms_ = period_ms;
		reload_ = reload;
		cb_ = cb;
		context_ = context;
		cnt_ms_ = 0;
		active_ = false;
		bound_ = true;

		next_ = head_;
		head_ = this;

		return true;
	}

	void destroy() {
		irq_lock lock;
		if (!bound_) {
			return;
		}

		timer **link = &head_;
		while (*link && *link != this) {
			link = &(*link)->next_;
		}
		if (*link) {
			*link = next_;
		}

		// next_ 保留原值：若回调中销毁了后续节点，本 tick 的遍历仍能沿链继续
		cb_ = nullptr;
		context_ = nullptr;
		period_ms_ = 0;
		cnt_ms_ = 0;
		reload_ = false;
		active_ = false;
		bound_ = false;
	}

	// 启动并重新计时
	bool start() {
		irq_lock lock;
		if (!bound_) {
			return false;
		}
		cnt_ms_ = 0;
		active_ = true;
		return true;
	}

	void stop() {
		irq_lock lock;
		active_ = false;
	}

	// 清零已计时长，不改变运行状态
	void reset() {
		irq_lock lock;
		cnt_ms_ = 0;
	}

	// 修改周期并重新计时
	bool change_period(const uint32_t period_ms) {
		if (period_ms == 0) {
			return false;
		}

		irq_lock lock;
		if (!bound_) {
			return false;
		}
		period_ms_ = period_ms;
		cnt_ms_ = 0;
		return true;
	}

	bool is_created() const { irq_lock lock; return bound_; }
	bool is_active() const { irq_lock lock; return active_; }
	uint32_t period_ms() const { irq_lock lock; return period_ms_; }

	// 由 1ms 中断调用
	static void OnMilliSecondTick() {
		timer *current = head_;
		while (current) {
			timer *next = current->next_;

			if (current->bound_ && current->active_) {
				if (++current->cnt_ms_ >= current->period_ms_) {
					current->cnt_ms_ = 0;
					// 先失活再回调：单次定时器的回调里可以重新 start()
					if (!current->reload_) {
						current->active_ = false;
					}

					callback_t cb = current->cb_;
					void *context = current->context_;
					if (cb) {
						cb(context);
					}
				}
			}

			current = next;
		}
	}

private:
	static inline timer *head_ = nullptr;

	timer *next_ = nullptr;
	callback_t cb_ = nullptr;
	void *context_ = nullptr;
	uint32_t period_ms_ = 0;
	volatile uint32_t cnt_ms_ = 0;
	bool reload_ = false;
	volatile bool active_ = false;
	bool bound_ = false;
};
