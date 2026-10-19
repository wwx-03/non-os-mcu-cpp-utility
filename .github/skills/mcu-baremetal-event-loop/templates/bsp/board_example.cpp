// 具体板骨架（DIP / Hybrid）。
//
// 复制到 bsp/<board_name>_board.cpp 后按目标板重命名类：
//   <MCU><BoardName><Version>Board，例如 Stm32F407StartKitBoard。
//
// 关键约定：
// - 类只在本 .cpp 内可见，**不提供 .hpp**；外部只能通过抽象 Board 访问。
// - 文件末尾用 IMPLEMENT_BOARD_CLASS() 注册唯一实例。
// - 选板 = 构建时只编译一个 bsp/*_board.cpp（多份会链接期重复符号）。
// - 板级专用能力（喂狗、板号、自检）留在本类，不上抽象接口。
// - 所有器件对象都是成员，不做动态分配。
// - 构造函数里完成板级初始化，且必须早于中断使能。

#include "devices/board.hpp"

#include "bsp/gpio_button.hpp"
#include "bsp/gpio_led.hpp"

// 厂商初始化头：换板时替换成新工程的（例如 usart.h / iwdg.h）
#include "board_hal.h"

namespace {

class BoardExample final : public Board {
public:
	BoardExample();
	~BoardExample() = default;

	BoardExample(const BoardExample &) = delete;
	BoardExample &operator=(const BoardExample &) = delete;

	Led *GetLed(size_t index) override;
	Button *GetButton(size_t index) override;
	Network *GetNetwork() override { return &network_; }

	// 板级专用能力：不放进抽象 Board
	void RefreshWatchdog();

private:
	GpioLed led_red_;
	GpioLed led_green_;
	GpioButton button0_;
	Network network_;
};

BoardExample::BoardExample()
	: led_red_(gpio::LED_RED_PORT, gpio::LED_RED_PIN, true),
	  led_green_(gpio::LED_GREEN_PORT, gpio::LED_GREEN_PIN, true),
	  button0_(gpio::BUTTON0_PORT, gpio::BUTTON0_PIN) {
	// TODO: 板级初始化（时钟、GPIO、外设）。必须早于中断使能。
}

Led *BoardExample::GetLed(size_t index) {
	switch (index) {
	case 0:
		return &led_red_;
	case 1:
		return &led_green_;
	default:
		return nullptr;
	}
}

Button *BoardExample::GetButton(size_t index) {
	return index == 0 ? &button0_ : nullptr;
}

void BoardExample::RefreshWatchdog() {
	// TODO: 刷新 IWDG；目标板没有看门狗时删除本方法与对应定时任务
}

}  // namespace

// 注册本板：全工程唯一的一份 create_new_board()
IMPLEMENT_BOARD_CLASS(BoardExample);
