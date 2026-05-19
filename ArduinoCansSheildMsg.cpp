/*
 * CAN Receiver — Arduino + MCP2515 CAN Shield
 * =============================================
 * Library : arduino-CAN  (by sandeepmistry)
 *           Install via Arduino Library Manager:
 *           Sketch → Include Library → Manage Libraries → search "CAN"
 *
 * Configuration:
 *   Bitrate  : 500 kbps  ← MUST match Teensy transmitter
 *   CS pin   : 10        (standard for most MCP2515 shields)
 *   INT pin  : 2         (MCP2515 interrupt → Arduino D2)
 *
 * Expected frame:
 *   ID      : 0x123  (standard 11-bit)
 *   DLC     : 8
 *   Byte 0  : rolling counter  (0–255)
 *   Byte 1  : status sentinel  (0xA5)
 *   Bytes 2–5: 32-bit timestamp from Teensy (ms since boot, big-endian)
 *   Bytes 6–7: 0xBE 0xEF  (fixed test pattern)
 *
 * Wiring reminder:
 *   CANH ──────────── CANH  (both nodes)
 *   CANL ──────────── CANL
 *   GND  ──────────── GND   (common ground mandatory)
 *   120 Ω across CANH–CANL at each physical end of the bus.
 */

#include <CAN.h>   /* sandeepmistry arduino-CAN library */

/* ── Configuration ────────────────────────────────────────────── */
#define CAN_BITRATE     500E3   /* 500 kbps — must match transmitter     */
#define EXPECTED_ID     0x123   /* accept only frames with this ID       */
#define MCP2515_CS_PIN  10      /* SPI chip-select for CAN shield        */
#define MCP2515_INT_PIN 2       /* interrupt pin from MCP2515            */

/* ── State tracking ───────────────────────────────────────────── */
static uint8_t  last_counter   = 0;
static uint32_t frames_received = 0;
static uint32_t frames_dropped  = 0;   /* counter wrap mismatches */

/* ── Helper: print a received frame to Serial ─────────────────── */
static void print_frame(int id, int dlc, uint8_t *data)
{
    Serial.print("RX  ID=0x");
    Serial.print(id, HEX);
    Serial.print("  DLC=");
    Serial.print(dlc);
    Serial.print("  DATA=[ ");
    for (int i = 0; i < dlc; i++) {
        if (data[i] < 0x10) Serial.print("0");
        Serial.print(data[i], HEX);
        Serial.print(" ");
    }
    Serial.print("]");
}

/* ── Helper: decode and validate the payload ──────────────────── */
static void decode_payload(uint8_t *data, int dlc)
{
    if (dlc < 8) {
        Serial.println("  [WARN: short frame]");
        return;
    }

    uint8_t  counter   = data[0];
    uint8_t  sentinel  = data[1];
    uint32_t timestamp = ((uint32_t)data[2] << 24) |
                         ((uint32_t)data[3] << 16) |
                         ((uint32_t)data[4] <<  8) |
                         ((uint32_t)data[5]);
    uint8_t  pat0      = data[6];
    uint8_t  pat1      = data[7];

    /* Check sentinel byte */
    if (sentinel != 0xA5) {
        Serial.print("  [WARN: bad sentinel 0x");
        Serial.print(sentinel, HEX);
        Serial.print("]");
    }

    /* Check fixed test pattern */
    if (pat0 != 0xBE || pat1 != 0xEF) {
        Serial.print("  [WARN: bad pattern]");
    }

    /* Check for dropped frames (counter skips) */
    uint8_t expected = (uint8_t)(last_counter + 1);
    if (frames_received > 0 && counter != expected) {
        frames_dropped++;
        Serial.print("  [WARN: counter jump ");
        Serial.print(last_counter);
        Serial.print("→");
        Serial.print(counter);
        Serial.print("]");
    }
    last_counter = counter;

    /* Print decoded fields */
    Serial.print("  cnt=");
    Serial.print(counter);
    Serial.print("  ts=");
    Serial.print(timestamp);
    Serial.print("ms");
}

/* ────────────────────────────────────────────────────────────────
 * setup()
 * ──────────────────────────────────────────────────────────────── */
void setup()
{
    Serial.begin(115200);
    while (!Serial);   /* wait for USB Serial on Leonardo/Mega; skip for Uno */

    Serial.println("=== CAN Receiver (Arduino + MCP2515) ===");
    Serial.print("Bitrate: ");
    Serial.print((long)CAN_BITRATE / 1000);
    Serial.println(" kbps");

    /* ── Initialise MCP2515 ─────────────────────────────────── *
     *  CAN.setPins(cs, irq) must be called BEFORE CAN.begin()   *
     *  if your shield uses non-default pins.                      *
     * ─────────────────────────────────────────────────────────*/
    CAN.setPins(MCP2515_CS_PIN, MCP2515_INT_PIN);

    if (!CAN.begin(CAN_BITRATE)) {
        Serial.println("ERROR: CAN controller failed to initialise!");
        Serial.println("Check wiring, CS pin, and 3.3 V / 5 V compatibility.");
        while (1);   /* halt */
    }

    Serial.println("CAN controller ready — waiting for frames...");
    Serial.println();
}

/* ────────────────────────────────────────────────────────────────
 * loop()
 * ──────────────────────────────────────────────────────────────── */
void loop()
{
    /* Non-blocking poll — returns number of bytes available */
    int packet_size = CAN.parsePacket();

    if (packet_size == 0) {
        return;   /* nothing yet */
    }

    int  frame_id  = (int)CAN.packetId();
    int  dlc       = CAN.packetDlc();
    bool is_rtr    = CAN.packetRtr();

    /* ── Filter: accept only our expected ID ─────────────────── */
    if (frame_id != EXPECTED_ID) {
        /* Silently drain and ignore other IDs */
        while (CAN.available()) CAN.read();
        return;
    }

    if (is_rtr) {
        Serial.println("RTR frame received (ignored)");
        return;
    }

    /* ── Read payload bytes ───────────────────────────────────── */
    uint8_t data[8] = {0};
    int     idx     = 0;
    while (CAN.available() && idx < 8) {
        data[idx++] = (uint8_t)CAN.read();
    }

    frames_received++;

    /* ── Print frame + decoded fields ────────────────────────── */
    print_frame(frame_id, dlc, data);
    decode_payload(data, dlc);
    Serial.print("  [total=");
    Serial.print(frames_received);
    Serial.print(" dropped=");
    Serial.print(frames_dropped);
    Serial.println("]");
}
