#pragma once

#include <stddef.h>
#include <stdint.h>

// 板级驱动：由本板 BSP 提供（例如 serial.hpp 中的 Serial<N, M>）。
class BoardSerial;

// BSP：板级资源与 HAL 回调的唯一入口（单例）。
// 应用层只通过这里获取资源，不直接接触厂商 HAL。
class Board {
public:
	using serial_data_callback_t = void (*)(size_t size, void *ctx);
	using key_callback_t = void (*)(uint8_t index, void *ctx);

	static Board &GetInstance() {
		static Board instance;
		return instance;
	}

	Board(const Board &) = delete;
	Board &operator=(const Board &) = delete;

	BoardSerial *GetSerial();
	size_t ReadSerial(void *dest, size_t size);

	// 驱动在中断里调用：只上报"有数据 / 状态变化"，不解析内容
	void OnSerialData(serial_data_callback_t cb, void *ctx);
	void OnKeyPressed(key_callback_t cb, void *ctx);

	// 仅供 HAL 弱回调调用
	void DispatchSerialRx();
	void DispatchExti(uint16_t pin);
	void RefreshWatchdog();

private:
	Board();

	BoardSerial *serial_ = nullptr;
	serial_data_callback_t serial_cb_ = nullptr;
	void *serial_ctx_ = nullptr;
	key_callback_t key_cb_ = nullptr;
	void *key_ctx_ = nullptr;
};
