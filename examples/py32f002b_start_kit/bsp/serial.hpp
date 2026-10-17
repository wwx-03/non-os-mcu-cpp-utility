#pragma once

#include <array>
#include <tuple>

#include "py32f0xx_hal.h"

#include "stream_buffer.hpp"

// BSP 驱动：中断接收 + stream_buffer 缓冲 + 非阻塞发送。
// 对外只提供"读 / 写 / 注册通知"三种能力，具体实现按目标板裁剪。
template <size_t kRxSize, size_t kTxSize>
class Serial {
public:
	using character_callback_t = void (*)(size_t, void *);
	using full_callback_t = void (*)(size_t, void *);

	Serial(UART_HandleTypeDef *huart)
		: huart_(huart) {
		HAL_UART_Receive_IT(huart_, &temp_, sizeof(temp_));
	}

	~Serial() = default;

	void SetRxBufferFullCallback(full_callback_t cb, void *ctx) {
		full_entry_ = std::make_tuple(cb, ctx);
	}

	void SetSpecificCharactersCallback(char sc, character_callback_t cb, void *ctx) {
		character_entry_ = std::make_tuple(sc, cb, ctx);
	}

	size_t Write(const void *src, size_t size) {
		if (size > kTxSize) {
			return 0; // 超过缓冲区大小，直接返回0
		}
		memcpy(tx_buffer_.data(), src, size);
		HAL_StatusTypeDef status = HAL_UART_Transmit_IT(huart_, tx_buffer_.data(), static_cast<uint16_t>(size));
		return status == HAL_OK ? size : 0;
	}

	size_t Read(void *const dest, size_t size) {
		return sb_.read(dest, size);
	}

	void HandleRxCpltEvent() {
		sb_.write(&temp_, sizeof(temp_));
		HAL_UART_Receive_IT(huart_, &temp_, sizeof(temp_));

		{ // 执行特定字符接收回调函数
			const auto &[c, cb, ctx] = character_entry_;
			if (c && c == temp_ && cb) {
				cb(sb_.size(), ctx);
			}
		}

		{ // 执行缓冲区满回调函数
			const auto &[cb, ctx] = full_entry_;
			if (!sb_.space() && cb) {
				cb(sb_.size(), ctx);
			}
		}
	}

	// 溢出等错误后重新挂起接收，避免接收通道永久失效（已在接收中时 HAL 返回 HAL_BUSY）
	void HandleErrorEvent() {
		HAL_UART_Receive_IT(huart_, &temp_, sizeof(temp_));
	}

private:
	UART_HandleTypeDef *huart_ = nullptr;
	stream_buffer<kRxSize> sb_;
	std::array<uint8_t, kTxSize> tx_buffer_ = {};
	uint8_t temp_ = 0;

	std::tuple<char, character_callback_t, void *> character_entry_ = {};
	std::tuple<full_callback_t, void *> full_entry_ = {};
};
