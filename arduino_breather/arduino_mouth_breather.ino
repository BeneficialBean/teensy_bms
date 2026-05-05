// basically the UART example code on arduino website
byte readByte;

void setup() {
  Serial.begin(1000000);    // Initialize the Serial monitor for debugging
}

void loop() {
  if (Serial.available() > 0) {
    readByte = Serial.read();
    Serial.println(readByte, HEX);
  } else {
    Serial.println("no uart detected");
    delay(1000);
  }
}