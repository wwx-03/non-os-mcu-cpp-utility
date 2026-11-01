// SPDX-License-Identifier: MIT
// Copyright (c) 2026 wwx-03

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "irq_lock.hpp"

template <typename T, size_t N>
class Queue {
public:
	bool Push(const T &src) {
		IrqLock lock;
		if (num_ >= N) return false;
		queue_[(head_ + num_) % N] = src;
		++num_;
		return true;
	}

	bool Pop(T &dest) {
		IrqLock lock;
		if (num_ == 0) return false;
		dest = queue_[head_];
		head_ = (head_ + 1) % N;
		--num_;
		return true;
	}

	bool Empty() const { IrqLock lock; return num_ == 0; }

	bool Full() const { IrqLock lock; return num_ >= N; }

	size_t Size() const { IrqLock lock; return num_; }

	void Clear() { IrqLock lock; head_ = 0; num_ = 0; }

private:
	T queue_[N] = {};
	volatile size_t num_ = 0;
	volatile size_t head_ = 0;
};