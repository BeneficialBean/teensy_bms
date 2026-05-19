/*
 * CAN Transmitter — Teensy 4.1 + Zephyr RTOS
 * ============================================
 * Transmits a structured CAN frame every 100 ms on CAN1.
 *
 * Configuration:
 *   Bitrate  : 500 kbps  (change CAN_BITRATE below for 125k / 250k)
 *   CAN ID   : 0x123     (standard 11-bit identifier)
 *   Payload  : 8 bytes   (incrementing counter + fixed test pattern)
 *
 * Physical pins (Teensy 4.1):
 *   CAN1_TX  → pin 22  (connect to CAN transceiver TXD)
 *   CAN1_RX  → pin 23  (connect to CAN transceiver RXD)
 *   Transceiver CANH / CANL → bus
 *
 * Wiring reminder:
 *   • Place a 120 Ω resistor across CANH–CANL at each end of the bus.
 *   • Tie Teensy GND to Arduino GND.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(can_tx, LOG_LEVEL_INF);

/* ── Configuration ────────────────────────────────────────────── */
#define CAN_BITRATE        500000U   /* 500 kbps — must match receiver  */
#define CAN_TX_ID          0x123U    /* Standard 11-bit frame ID         */
#define CAN_TX_INTERVAL_MS 100       /* Transmit one frame every 100 ms  */

/* ── Device alias defined in the board overlay (see below) ────── */
#define CAN_DEVICE_NODE    DT_CHOSEN(zephyr_canbus)

/* ── Payload layout (8 bytes) ─────────────────────────────────── *
 *  Byte 0   : rolling counter  (0x00 – 0xFF, wraps)               *
 *  Byte 1   : status flags     (0xA5 = all-good sentinel)         *
 *  Bytes 2–5: 32-bit timestamp (ms since boot, big-endian)        *
 *  Bytes 6–7: fixed test pattern  0xBE 0xEF                       *
 * ───────────────────────────────────────────────────────────────*/

static void build_payload(uint8_t *buf, uint8_t counter, uint32_t timestamp_ms)
{
    buf[0] = counter;
    buf[1] = 0xA5U;                          /* status sentinel           */
    buf[2] = (uint8_t)(timestamp_ms >> 24);  /* MSB of timestamp          */
    buf[3] = (uint8_t)(timestamp_ms >> 16);
    buf[4] = (uint8_t)(timestamp_ms >>  8);
    buf[5] = (uint8_t)(timestamp_ms      );  /* LSB of timestamp          */
    buf[6] = 0xBEU;                          /* fixed test pattern byte 0 */
    buf[7] = 0xEFU;                          /* fixed test pattern byte 1 */
}

int main(void)
{
    /* ── 1. Obtain CAN device handle ──────────────────────────── */
    const struct device *can_dev = DEVICE_DT_GET(CAN_DEVICE_NODE);

    if (!device_is_ready(can_dev)) {
        LOG_ERR("CAN device not ready");
        return -ENODEV;
    }
    LOG_INF("CAN device: %s", can_dev->name);

    /* ── 2. Set bitrate ───────────────────────────────────────── *
     *  Call before can_start().  Zephyr will program the timing   *
     *  registers of the i.MX RT FlexCAN peripheral automatically. *
     * ─────────────────────────────────────────────────────────── */
    int ret = can_set_bitrate(can_dev, CAN_BITRATE);
    if (ret != 0) {
        LOG_ERR("Failed to set bitrate: %d", ret);
        return ret;
    }

    /* ── 3. Set mode: normal (no loopback) ───────────────────── */
    ret = can_set_mode(can_dev, CAN_MODE_NORMAL);
    if (ret != 0) {
        LOG_ERR("Failed to set CAN mode: %d", ret);
        return ret;
    }

    /* ── 4. Start the controller ─────────────────────────────── */
    ret = can_start(can_dev);
    if (ret != 0) {
        LOG_ERR("Failed to start CAN: %d", ret);
        return ret;
    }
    LOG_INF("CAN started at %u bps", CAN_BITRATE);

    /* ── 5. Build the frame template ─────────────────────────── */
    struct can_frame frame = {
        .flags  = 0,           /* Standard frame (not FD, not RTR, not extended) */
        .id     = CAN_TX_ID,   /* 11-bit identifier 0x123                        */
        .dlc    = 8,           /* Data Length Code — 8 bytes                     */
    };

    uint8_t  counter = 0;

    /* ── 6. Transmit loop ─────────────────────────────────────── */
    while (1) {
        uint32_t now_ms = (uint32_t)k_uptime_get();

        /* Fill payload with fresh data */
        build_payload(frame.data, counter, now_ms);

        /* Blocking send — waits for a free TX mailbox */
        ret = can_send(can_dev, &frame, K_MSEC(50), NULL, NULL);
        if (ret == 0) {
            LOG_INF("TX  ID=0x%03X  cnt=%3u  ts=%u ms",
                    frame.id, counter, now_ms);
        } else {
            LOG_WRN("TX failed: %d", ret);
        }

        counter++;   /* wraps at 256 automatically (uint8_t) */

        k_msleep(CAN_TX_INTERVAL_MS);
    }

    return 0;  /* unreachable */
}
