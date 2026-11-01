// SPDX-License-Identifier: MIT
// Copyright (c) 2026 wwx-03

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "irq_lock.hpp"

template <size_t N>
class StreamBuffer {
public:
	size_t Write(const void *src, size_t size) {
		IrqLock lock;
		size_t space = SpaceUnlocked();
		if (size > space) {
			size = space;
		}
		if (size == 0) {
			return 0;
		}

		const uint8_t *p = static_cast<const uint8_t *>(src);
		size_t first = N - head_;
		if (first > size) {
			first = size;
		}
		memcpy(buffer_ + head_, p, first);
		if (size > first) {
			memcpy(buffer_, p + first, size - first);
		}
		head_ = (head_ + size) % N;
		return size;
	}

	// 读取数据，返回实际读取的字节数
	// 数据不足时读取部分数据并返回已读取量
	size_t Read(void *dest, size_t size) {
		IrqLock lock;
		size_t avail = SizeUnlocked();
		if (size > avail) {
			size = avail;
		}
		if (size == 0) {
			return 0;
		}

		uint8_t *p = static_cast<uint8_t *>(dest);
		size_t first = N - tail_;
		if (first > size) {
			first = size;
		}
		memcpy(p, buffer_ + tail_, first);
		if (size > first) {
			memcpy(p + first, buffer_, size - first);
		}
		tail_ = (tail_ + size) % N;
		return size;
	}

	// 查看数据但不移动读指针
	size_t Peek(void *dest, size_t size) const {
		IrqLock lock;
		size_t avail = SizeUnlocked();
		if (size > avail) {
			size = avail;
		}
		if (size == 0) {
			return 0;
		}

		uint8_t *p = static_cast<uint8_t *>(dest);
		size_t first = N - tail_;
		if (first > size) {
			first = size;
		}
		memcpy(p, buffer_ + tail_, first);
		if (size > first) {
			memcpy(p + first, buffer_, size - first);
		}
		return size;
	}

	// 丢弃已读取的数据
	size_t Skip(size_t size) {
		IrqLock lock;
		size_t avail = SizeUnlocked();
		if (size > avail) {
			size = avail;
		}
		tail_ = (tail_ + size) % N;
		return size;
	}

	// 查询状态
	size_t Size() const { IrqLock lock; return SizeUnlocked(); }
	size_t Space() const { IrqLock lock; return SpaceUnlocked(); }
	bool Empty() const { IrqLock lock; return head_ == tail_; }

	// 清空缓冲区
	void Clear() {
		IrqLock lock;
		head_ = 0;
		tail_ = 0;
	}

private:
	// 以下无锁版本仅供已持有锁的内部函数调用
	size_t SizeUnlocked() const {
		if (head_ >= tail_) {
			return head_ - tail_;
		}
		return N - tail_ + head_;
	}

	size_t SpaceUnlocked() const {
		// 保留一个空位以区分“满”和“空”
		return N - 1 - SizeUnlocked();
	}

	uint8_t buffer_[N] = {};
	volatile size_t head_ = 0;  // 写指针
	volatile size_t tail_ = 0;  // 读指针
};