// basically the UART example code on arduino website

// software design ref (SDR): https://www.ti.com/lit/an/slvae86b/slvae86b.pdf?ts=1775108056517
// datasheet: https://www.ti.com/lit/gpn/bq79616-q1

//hi evan

/*---- command frame ----
{
initialization,
device id (only for single device read/write),
register addr 1,
register addr 2,
data (reading = num bytes requested [max 128], writing = bytes to be written [max 8]),
CRC 1,
CRC 2,
}*/
uint8_t message[] = {0x80, 0x02, 0x02, 0x15, 0x0B, 0xCB, 0x49};
// single device read command frame (table 1-2) (SDR p2)

#define UART_SERIAL Serial1

const int TX_PIN = 1; // not actual
const int RX_PIN = 2; // not actual

// datasheet p56
uint16_t calculateCRC(uint8_t* frame, uint8_t len) {
  return 0;
}

void sendFrame(uint8_t* frame, uint8_t len) {
  UART_SERIAL.write(frame, len);
}

void wakePing() {
  digitalWrite(TX_PIN, HIGH);
  digitalWrite(TX_PIN, LOW);
  delayMicroseconds(2500);
  digitalWrite(TX_PIN, HIGH);
}

void setup() {
  Serial.begin(1000000);    // Initialize the Serial monitor for debugging
  UART_SERIAL.begin(1000000);
  pinMode(TX_PIN, OUTPUT);
  pinMode(RX_PIN, INPUT);
}

void loop() {
  sendFrame(message, sizeof(message));
  delay(1000);
}