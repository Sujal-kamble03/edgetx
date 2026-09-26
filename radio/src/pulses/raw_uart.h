/*
 * Copyright (C) EdgeTX
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 */

#pragma once

#include "hal/module_driver.h"

extern const etx_proto_driver_t RawUartDriver;

enum RawUartEstop : uint8_t {
	RAW_UART_ESTOP_NORMAL = 0,
	RAW_UART_ESTOP_PRESSED = 1,
	RAW_UART_ESTOP_WAIT_DISARM = 2,
	RAW_UART_ESTOP_WAIT_ARM = 3,
	RAW_UART_ESTOP_NO_LINK = 0xFF,
};

uint8_t rawUartGetEstopState();