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
#define RAW_UART_STATUS_MARKER 0xC5
#define RAW_UART_STATUS_SIZE 8
#define RAW_UART_STATUS_TIMEOUT 30
#define RAW_UART_FLAG_PLC_LINK 0x02

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

static uint8_t statusBuf[RAW_UART_STATUS_SIZE];
static uint8_t statusIdx = 0;
static bool statusValid = false;
static uint8_t statusEstop = RAW_UART_ESTOP_NO_LINK;
static uint8_t statusFlags = 0;
static uint8_t statusSoc = RAW_UART_SOC_UNKNOWN;
static uint8_t statusMode = RAW_UART_MODE_UNKNOWN;
static uint8_t statusSystem = RAW_UART_SYSTEM_UNKNOWN;
static uint8_t statusSteering = RAW_UART_STEERING_UNKNOWN;
static tmr10ms_t statusLastRx = 0;

static void rawUartParseByte(uint8_t b)
{
  if (statusIdx == 0) {
    if (b == RAW_UART_STATUS_MARKER) {
      statusBuf[0] = b;
      statusIdx = 1;
    }
    return;
  }

  statusBuf[statusIdx++] = b;
  if (statusIdx < RAW_UART_STATUS_SIZE) return;

  statusIdx = 0;
  if (statusBuf[1] <= RAW_UART_ESTOP_WAIT_ARM && statusBuf[2] <= 3 &&
      (statusBuf[3] <= 100 || statusBuf[3] == RAW_UART_SOC_UNKNOWN) &&
      (statusBuf[4] <= RAW_UART_MODE_GO_TO_CW ||
       statusBuf[4] == RAW_UART_MODE_UNKNOWN) &&
      (statusBuf[5] <= RAW_UART_SYSTEM_ESTOP ||
       statusBuf[5] == RAW_UART_SYSTEM_UNKNOWN) &&
      (statusBuf[6] <= RAW_UART_STEERING_ACKERMANN_LEFT ||
       statusBuf[6] == RAW_UART_STEERING_UNKNOWN) &&
      statusBuf[7] == (uint8_t)(statusBuf[0] ^ statusBuf[1] ^ statusBuf[2] ^
                                statusBuf[3] ^ statusBuf[4] ^ statusBuf[5] ^
                                statusBuf[6])) {
    statusEstop = statusBuf[1];
    statusFlags = statusBuf[2];
    statusSoc = statusBuf[3];
    statusMode = statusBuf[4];
    statusSystem = statusBuf[5];
    statusSteering = statusBuf[6];
    statusLastRx = get_tmr10ms();
    statusValid = true;
    return;
  }

  uint8_t markerIndex = 1;
  while (markerIndex < RAW_UART_STATUS_SIZE &&
         statusBuf[markerIndex] != RAW_UART_STATUS_MARKER) {
    markerIndex++;
  }
  if (markerIndex < RAW_UART_STATUS_SIZE) {
    statusIdx = RAW_UART_STATUS_SIZE - markerIndex;
    for (uint8_t index = 0; index < statusIdx; index++) {
      statusBuf[index] = statusBuf[markerIndex + index];
    }
  }
}

static bool rawUartStatusFresh()
{
  return statusValid &&
         (tmr10ms_t)(get_tmr10ms() - statusLastRx) <= RAW_UART_STATUS_TIMEOUT;
}

uint8_t rawUartGetEstopState()
{
  return rawUartStatusFresh() ? statusEstop : RAW_UART_ESTOP_NO_LINK;
}

bool rawUartPlcConnected()
{
  return rawUartStatusFresh() && (statusFlags & RAW_UART_FLAG_PLC_LINK);
}

uint8_t rawUartGetBatterySoc()
{
  return rawUartStatusFresh() ? statusSoc : RAW_UART_SOC_UNKNOWN;
}

uint8_t rawUartGetPlcModeState()
{
  return rawUartStatusFresh() ? statusMode : RAW_UART_MODE_UNKNOWN;
}

uint8_t rawUartGetPlcSystemState()
{
  return rawUartStatusFresh() ? statusSystem : RAW_UART_SYSTEM_UNKNOWN;
}

uint8_t rawUartGetPlcSteeringState()
{
  return rawUartStatusFresh() ? statusSteering : RAW_UART_STEERING_UNKNOWN;
}

static void* rawUartInit(uint8_t module)
{
  statusIdx = 0;
  statusValid = false;
  statusEstop = RAW_UART_ESTOP_NO_LINK;
  statusFlags = 0;
  statusSoc = RAW_UART_SOC_UNKNOWN;
  statusMode = RAW_UART_MODE_UNKNOWN;
  statusSystem = RAW_UART_SYSTEM_UNKNOWN;
  statusSteering = RAW_UART_STEERING_UNKNOWN;
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
  statusIdx = 0;
  statusValid = false;
  statusEstop = RAW_UART_ESTOP_NO_LINK;
  statusFlags = 0;
  statusSoc = RAW_UART_SOC_UNKNOWN;
  statusMode = RAW_UART_MODE_UNKNOWN;
  statusSystem = RAW_UART_SYSTEM_UNKNOWN;
  statusSteering = RAW_UART_STEERING_UNKNOWN;
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