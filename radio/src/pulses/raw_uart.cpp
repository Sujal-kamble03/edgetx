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
#if defined(RADIO_BOXER) && defined(BLUETOOTH)
#include "bluetooth_driver.h"
#endif

#define RAW_UART_BAUDRATE 115200
#define RAW_UART_FRAME_MARKER 0xA5
#define RAW_UART_CHANNELS 16
#define RAW_UART_PERIOD 20000
#define RAW_UART_ESTOP_MARKER 0x5A
#define RAW_UART_ESTOP_TIMEOUT 30

static const etx_serial_init rawUartParams = {
  .baudrate = RAW_UART_BAUDRATE,
  .encoding = ETX_Encoding_8N1,
  .direction = ETX_Dir_TX_RX,
  .polarity = ETX_Pol_Normal,
};

#if defined(RADIO_BOXER) && defined(BLUETOOTH)
static uint8_t rawUartContext;
static uint8_t rawUartModule;
#endif

static uint8_t estopParserState = 0;
static uint8_t estopReceivedState = RAW_UART_ESTOP_NO_LINK;
static uint8_t estopState = RAW_UART_ESTOP_NO_LINK;
static tmr10ms_t estopLastRx = 0;

static void rawUartParseByte(uint8_t byte)
{
  if (estopParserState == 0) {
    if (byte == RAW_UART_ESTOP_MARKER) estopParserState = 1;
    return;
  }

  if (estopParserState == 1) {
    estopReceivedState = byte;
    estopParserState = 2;
    return;
  }

  estopParserState = 0;
  if (estopReceivedState <= RAW_UART_ESTOP_WAIT_ARM &&
      byte == (uint8_t)(RAW_UART_ESTOP_MARKER ^ estopReceivedState)) {
    estopState = estopReceivedState;
    estopLastRx = get_tmr10ms();
  }
}

uint8_t rawUartGetEstopState()
{
  if (estopState == RAW_UART_ESTOP_NO_LINK ||
      (tmr10ms_t)(get_tmr10ms() - estopLastRx) > RAW_UART_ESTOP_TIMEOUT)
    return RAW_UART_ESTOP_NO_LINK;

  return estopState;
}

static void* rawUartInit(uint8_t module)
{
  estopParserState = 0;
  estopState = RAW_UART_ESTOP_NO_LINK;
  estopLastRx = 0;
#if defined(RADIO_BOXER) && defined(BLUETOOTH)
  rawUartModule = module;
  if (!bluetoothRawUartInit(RAW_UART_BAUDRATE)) return nullptr;
  mixerSchedulerSetPeriod(module, RAW_UART_PERIOD);
  return &rawUartContext;
#else
#if defined(HARDWARE_INTERNAL_MODULE)
  if (module == INTERNAL_MODULE) return nullptr;
#endif

  auto mod_st = modulePortInitSerial(module, ETX_MOD_PORT_UART,
                                     &rawUartParams, false);
  if (!mod_st) return nullptr;

  mixerSchedulerSetPeriod(module, RAW_UART_PERIOD);
  return mod_st;
#endif
}

static void rawUartDeInit(void* ctx)
{
  estopParserState = 0;
  estopState = RAW_UART_ESTOP_NO_LINK;
#if defined(RADIO_BOXER) && defined(BLUETOOTH)
  (void)ctx;
  bluetoothRawUartDeInit();
#else
  modulePortDeInit((etx_module_state_t*)ctx);
#endif
}

static void rawUartSendPulses(void* ctx, uint8_t* buffer, int16_t*, uint8_t)
{
  uint8_t receivedByte;
#if defined(RADIO_BOXER) && defined(BLUETOOTH)
  (void)ctx;
  auto module = rawUartModule;
  auto p_buf = buffer;

  while (bluetoothRead(&receivedByte) > 0) rawUartParseByte(receivedByte);
#else
  auto mod_st = (etx_module_state_t*)ctx;
  auto module = modulePortGetModule(mod_st);
  auto drv = modulePortGetSerialDrv(mod_st->tx);
  auto drv_ctx = modulePortGetCtx(mod_st->tx);
  auto p_buf = buffer;

  auto rx_drv = modulePortGetSerialDrv(mod_st->rx);
  auto rx_ctx = modulePortGetCtx(mod_st->rx);
  if (rx_drv && rx_drv->getByte) {
    while (rx_drv->getByte(rx_ctx, &receivedByte) > 0)
      rawUartParseByte(receivedByte);
  }
#endif

  *p_buf++ = RAW_UART_FRAME_MARKER;
  uint8_t start = g_model.moduleData[module].channelsStart;
  for (uint8_t i = 0; i < RAW_UART_CHANNELS; i++) {
    int16_t value = 0;
    if (start + i < MAX_OUTPUT_CHANNELS) value = channelOutputs[start + i];
    *p_buf++ = (uint8_t)value;
    *p_buf++ = (uint8_t)(value >> 8);
  }

#if defined(RADIO_BOXER) && defined(BLUETOOTH)
  bluetoothRawUartSend(buffer, p_buf - buffer);
#else
  drv->sendBuffer(drv_ctx, buffer, p_buf - buffer);
#endif
}

static bool rawUartTxCompleted(void* ctx)
{
#if defined(RADIO_BOXER) && defined(BLUETOOTH)
  (void)ctx;
  return bluetoothRawUartTxCompleted();
#else
  return modulePortSerialTxCompleted(ctx);
#endif
}

const etx_proto_driver_t RawUartDriver = {
  .protocol = PROTOCOL_CHANNELS_RAW_UART,
  .init = rawUartInit,
  .deinit = rawUartDeInit,
  .sendPulses = rawUartSendPulses,
  .processData = nullptr,
  .processFrame = nullptr,
  .onConfigChange = nullptr,
  .txCompleted = rawUartTxCompleted,
};