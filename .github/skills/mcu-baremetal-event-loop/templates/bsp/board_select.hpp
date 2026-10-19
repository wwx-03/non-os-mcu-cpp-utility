#pragma once

// 全工程唯一的选板点。
//
// 换板只改这里：include 目标板的头，并把 ActiveBoard 指向它。
// 不要在其它文件里用 #ifdef 选板，也不要让 app/ 认识具体板类型。
//
// 具体板命名约定：<MCU><BoardName><Version>Board
//   例：Stm32F407StartKitBoard / Py32F002BStartKitBoard / Esp32SmartLockV01Board

#include "bsp/board_example.hpp"

using ActiveBoard = BoardExample;
