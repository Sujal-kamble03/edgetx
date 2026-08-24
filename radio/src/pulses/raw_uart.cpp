/*
 * Copyright (C) EdgeTX
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 */

#include "raw_uart.h"

#include "edgetx.h"
#include "hal/module_port.h"
#include "mixer_scheduler.h"
#include "pulses/pulses.h"

#define RAW_UART_BAUDRATE 115200
#define RAW_UART_FRAME_MARKER 0xA5
#define RAW_UART_CHANNELS 16
#define RAW_UART_PERIOD 20000

static const etx_serial_init rawUartParams = {
  .baudrate = RAW_UART_BAUDRATE,
  .encoding = ETX_Encoding_8N1,
  .direction = ETX_Dir_TX,
  .polarity = ETX_Pol_Normal,
};

static void* rawUartInit(uint8_t module)
{
#if defined(HARDWARE_INTERNAL_MODULE)
  if (module == INTERNAL_MODULE) return nullptr;
#endif

  auto mod_st = modulePortInitSerial(module, ETX_MOD_PORT_UART,
                                     &rawUartParams, false);
  if (!mod_st) return nullptr;

  mixerSchedulerSetPeriod(module, RAW_UART_PERIOD);
  return mod_st;
}

static void rawUartDeInit(void* ctx)
{
  modulePortDeInit((etx_module_state_t*)ctx);
}

static void rawUartSendPulses(void* ctx, uint8_t* buffer, int16_t*, uint8_t)
{
  auto mod_st = (etx_module_state_t*)ctx;
  auto module = modulePortGetModule(mod_st);
  auto drv = modulePortGetSerialDrv(mod_st->tx);
  auto drv_ctx = modulePortGetCtx(mod_st->tx);
  auto p_buf = buffer;

  *p_buf++ = RAW_UART_FRAME_MARKER;
  uint8_t start = g_model.moduleData[module].channelsStart;
  for (uint8_t i = 0; i < RAW_UART_CHANNELS; i++) {
    int16_t value = 0;
    if (start + i < MAX_OUTPUT_CHANNELS) value = channelOutputs[start + i];
    *p_buf++ = (uint8_t)value;
    *p_buf++ = (uint8_t)(value >> 8);
  }

  drv->sendBuffer(drv_ctx, buffer, p_buf - buffer);
}

const etx_proto_driver_t RawUartDriver = {
  .protocol = PROTOCOL_CHANNELS_RAW_UART,
  .init = rawUartInit,
  .deinit = rawUartDeInit,
  .sendPulses = rawUartSendPulses,
  .processData = nullptr,
  .processFrame = nullptr,
  .onConfigChange = nullptr,
  .txCompleted = modulePortSerialTxCompleted,
};