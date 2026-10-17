#pragma once

// 唯一的 MCU 依赖点：换 MCU 时只改这一行。
// STM32F4 的 CMSIS 头已提供 __get_PRIMASK / __disable_irq / __set_PRIMASK，无需转接。
#include "stm32f4xx.h"
