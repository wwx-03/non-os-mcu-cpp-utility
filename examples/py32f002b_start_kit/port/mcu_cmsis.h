#pragma once

// 唯一的 MCU 依赖点：换 MCU 时只改这一行。
// 如果目标平台没有 __get_PRIMASK / __disable_irq / __set_PRIMASK，在这里补齐同名转接。
#include "py32f0xx.h"