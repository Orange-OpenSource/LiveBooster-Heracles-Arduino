/*
 * === Soft Serial Test ===
 *
 * This sketch doesn't really use the LiveBooster-Heracles-Arduino library. It should be
 * used to check correct communication between the Arduino board and the Heracles modem:
 * all AT commands sent on Arduino Serial Monitor are transmitted to modem, and answers
 * from modem are transmitted to Arduino Serial Monitor.
 *
 * You may need to change the Serial port used on your board for modem interface.
 *
 */

/*
 * Use Serial1 UART for modem interface:
 *    o Pins D0 (RX), D1 (TX) on LinkIt One board.
 *    o Pins 18 (TX1), 19 (RX1) on Arduino MEGA ADK.
 */
#define HeraclesSerial Serial1

void setup() {

  /* Open serial communication and wait for port to open */
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  Serial.println("TEST AT COMMANDS WITH HERACLES MODEM !");

  /* Set the data rate for the HeraclesSerial port */
  HeraclesSerial.begin(115200);
  HeraclesSerial.println("AT\r\n");
}

void loop() {
   
  if (HeraclesSerial.available()) {
    Serial.write(HeraclesSerial.read());
  }
  if (Serial.available()) {
      HeraclesSerial.write(Serial.read());
  }
}
