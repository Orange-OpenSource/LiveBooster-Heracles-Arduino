/*
 * === Heracles Sample Basic ===
 *
 * This sketch connects to website arduino.cc to get file http://www.arduino.cc/asciilogo.txt,
 * using the Heracles modem. The modem is connected to the Arduino board with serial interface.
 *
 * You may need to change the Serial port used on your board for modem interface.
 *
 */

#include <HeraclesGsmModem.h>

/*
 * Use Serial1 UART for modem interface:
 *    o Pins D0 (RX), D1 (TX) on LinkIt One board.
 *    o Pins 18 (TX1), 19 (RX1) on Arduino MEGA ADK.
 * By default, DNS is enabled.
 */
HeraclesGsmModem modem(Serial1);

/*
 * Selected mux is 1, and SSL is enabled (this sketch connects to HTTPS server).
 */
HeraclesGsmModem::GsmClient gsmClient(modem, 1, true);


void setup() {

    /* Initialize serial debug and wait for port to open */
    Serial.begin(115200);
    while (!Serial) {
      delay(100);
    }

    /* Initializing serial port for modem interface */
    Serial1.begin(115200);

    /* Initializing Heracles modem */
    Serial.print("Initializing Heracles modem... ");
    modem.restart();

    /* Check for modem firmware version (optional) */
    String modemInfo = modem.getModemInfo();
    if (modemInfo == "") {
        Serial.println("FAIL");
        while (true);
    }
    Serial.print("OK: ");
    Serial.println(modemInfo);

    /* Wait for network */
    Serial.print("Waiting for network... ");
    if (!modem.waitForNetwork()) {
        Serial.println("FAIL");
        while (true);
    }
    Serial.println("OK");

    /* attachGPRS() used without any parameters for internal SIM.
     * For external SIM, use attachGPRS("your apn", "user", "pswd") */
    Serial.print("Connecting to APN... ");
    if (!modem.attachGPRS()) {
        Serial.println("FAIL");
        while (true);
    }
    Serial.println("OK");

    /* Connecting to arduino.cc and get file. Port 443 is the default
     * port number for HTTPS servers. */
    Serial.print("Connecting to arduino.cc... ");
    if (gsmClient.connect("www.arduino.cc", 443)) {
      Serial.println("OK");
      gsmClient.println("GET /asciilogo.txt HTTP/1.1");
      gsmClient.println("Host: www.arduino.cc");
      gsmClient.println();
    } else {
      Serial.println("FAIL");
    }

}

void loop() {

    /* Wait for answer from server */
    while (!gsmClient.available() && gsmClient.connected());

    /* Get received answer */
    while (gsmClient.available()) {
      char c = gsmClient.read();
      Serial.print(c);
    }

    /* Disconnecting */
    Serial.println();
    Serial.print("Disconnecting... ");

    gsmClient.stop();
    modem.radioOff();

    Serial.println("FINISHED");
    while (true);
}
