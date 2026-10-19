#pragma once

#include <stddef.h>

#include <tuple>

#include "timers.hpp"

// 器件层：按键消抖（NVI）。
// 派生类只实现 Read()；采样由内部 timer 完成，回调运行在 1ms tick 上下文中，
// 因此上层回调里只允许投递事件，不能做业务处理。
class Button {
public:
	using released_callback_t = void (*)(void *ctx);

	Button() {
		timer_.create(25, true, +[](void *ctx) {
			static_cast<Button *>(ctx)->OnTimerTick();
		}, this);
	}

	void SetReleasedCallback(released_callback_t callback, void *ctx) {
		released_entry_ = std::make_tuple(callback, ctx);
	}

	// 引脚配置完成后调用：记录初始电平并开始采样。
	// 不能在构造函数里调用 Read()——此时派生类尚未构造，纯虚调用是未定义行为。
	void Start() {
		last_ = Read();
		timer_.start();
	}

	void Stop() {
		timer_.stop();
	}

protected:
	// 实例都是静态对象，不会通过基类指针 delete
	~Button() = default;

	// 返回 true 表示按键处于按下状态
	virtual bool Read() const = 0;

private:
	timer timer_;
	std::tuple<released_callback_t, void *> released_entry_ = {};
	bool last_ = false;

	void OnTimerTick() {
		const bool now = Read();
		if (last_ && !now) {
			const auto &[cb, ctx] = released_entry_;
			if (cb) {
				cb(ctx);
			}
		}
		last_ = now;
	}
};
