#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "irq_lock.hpp"

template <size_t N>
class stream_buffer {
public:
	size_t write(const void *src, size_t size) {
		irq_lock lock;
		size_t space = space_unlocked();
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
	size_t read(void *dest, size_t size) {
		irq_lock lock;
		size_t avail = size_unlocked();
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
	size_t peek(void *dest, size_t size) const {
		irq_lock lock;
		size_t avail = size_unlocked();
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
	size_t skip(size_t size) {
		irq_lock lock;
		size_t avail = size_unlocked();
		if (size > avail) {
			size = avail;
		}
		tail_ = (tail_ + size) % N;
		return size;
	}

	// 查询状态
	size_t size() const { irq_lock lock; return size_unlocked(); }
	size_t space() const { irq_lock lock; return space_unlocked(); }
	bool empty() const { irq_lock lock; return head_ == tail_; }

	// 清空缓冲区
	void clear() {
		irq_lock lock;
		head_ = 0;
		tail_ = 0;
	}

private:
	// 以下无锁版本仅供已持有锁的内部函数调用
	size_t size_unlocked() const {
		if (head_ >= tail_) {
			return head_ - tail_;
		}
		return N - tail_ + head_;
	}

	size_t space_unlocked() const {
		// 保留一个空位以区分“满”和“空”
		return N - 1 - size_unlocked();
	}

	uint8_t buffer_[N] = {};
	volatile size_t head_ = 0;  // 写指针
	volatile size_t tail_ = 0;  // 读指针
};
