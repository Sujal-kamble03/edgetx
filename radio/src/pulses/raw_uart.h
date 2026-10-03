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

#define RAW_UART_SOC_UNKNOWN 0xFF
bool rawUartPlcConnected();
uint8_t rawUartGetBatterySoc();

#define RAW_UART_MODE_UNKNOWN 0xFF
enum RawUartPlcMode : uint8_t {
	RAW_UART_MODE_WAITING = 0,
	RAW_UART_MODE_MU_READY = 1,
	RAW_UART_MODE_CSU_READY = 2,
	RAW_UART_MODE_GO_TO_CW = 3,
};

uint8_t rawUartGetPlcModeState();

#define RAW_UART_SYSTEM_UNKNOWN 0xFF
enum RawUartPlcSystem : uint8_t {
	RAW_UART_SYSTEM_INIT = 0,
	RAW_UART_SYSTEM_SELF_CHECK = 1,
	RAW_UART_SYSTEM_IDLE = 2,
	RAW_UART_SYSTEM_MODE_TRANS = 3,
	RAW_UART_SYSTEM_RUN = 4,
	RAW_UART_SYSTEM_FAULT = 5,
	RAW_UART_SYSTEM_ESTOP = 6,
};

uint8_t rawUartGetPlcSystemState();

#define RAW_UART_STEERING_UNKNOWN 0xFF
enum RawUartPlcSteering : uint8_t {
	RAW_UART_STEERING_IDLE_HOLD = 0,
	RAW_UART_STEERING_STRAIGHT = 1,
	RAW_UART_STEERING_TURN = 2,
	RAW_UART_STEERING_SPIN = 3,
	RAW_UART_STEERING_CRAB = 4,
	RAW_UART_STEERING_ACKERMANN_FWD = 5,
	RAW_UART_STEERING_ACKERMANN_LEFT = 6,
};

uint8_t rawUartGetPlcSteeringState();
