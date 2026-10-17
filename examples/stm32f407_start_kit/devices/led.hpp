#pragma once

// 器件层：灯的抽象（NVI，非虚接口）。
// 派生类只实现 SetIo()，状态缓存与对外接口由基类统一维护；
// 引脚、极性等板级细节留在 BSP，本层不依赖任何厂商头文件。
class Led {
public:
	void SetState(bool state) {
		state_ = state;
		SetIo(state_);
	}

	void Toggle() {
		SetState(!state_);
	}

	bool GetState() const {
		return state_;
	}

protected:
	// 实例都是静态对象，不会通过基类指针 delete，析构无需虚函数
	~Led() = default;

	bool state_ = false;

	// 写硬件电平：on == true 表示"点亮"
	virtual void SetIo(bool on) = 0;
};
