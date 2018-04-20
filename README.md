# LiveBooster-Heracles-Arduino Library

Please refers to the [Changelog](ChangeLog.md) to check the latest change and improvement.

---

LiveBooster-Heracles-Arduino is an [Arduino](https://www.arduino.cc/) library for [Heracles modem](https://www.avnet.com/wps/portal/ebv/solutions/ebvchips/heracles/), based on the [TinyGSM](https://github.com/vshymanskyy/TinyGSM) library.

## Arduino Client interface support

This library provides an Arduino standard [Client interface](https://www.arduino.cc/en/Reference/ClientConstructor), so that it is easy to integrate with lots of usages based on [TCP](https://fr.wikipedia.org/wiki/Transmission_Control_Protocol) (MQTT, HTTP, ...).

As an example, this library can be used with the [IoTSoftBox library](https://github.com/Orange-OpenSource/LiveObjects-iotSoftbox-mqtt-arduino) to connect devices to [Live Objects](https://liveobjects.orange-business.com) server.

## Supported boards

Tested with:
 * [Arduino MEGA ADK](https://store.arduino.cc/arduino-mega-adk-rev3)
 * [Mediatek LinkIt ONE](https://labs.mediatek.com/en/platform/linkit-one)
 * ... should work on other Arduino boards.

## Supported modem

This library targets the [EBV Heracles modem](https://www.avnet.com/wps/portal/ebv/solutions/ebvchips/heracles/).

## Getting Started

The Heracles modem shall be connected to the Arduino board with serial interface (Ground, Tx, Rx).

Once the library has been [imported into your Arduino IDE](https://www.arduino.cc/en/Guide/Libraries#toc4), two example sketchs are accessible by menu File -> Examples -> LiveBooster-Heracles-Arduino :

### "Soft Serial Test" example sketch

This sketch doesn't really use the LiveBooster-Heracles-Arduino library. It should be used to check correct communication between the Arduino board and the Heracles modem: all AT commands sent on Arduino Serial Monitor are transmitted to modem, and answers from modem are transmitted to Arduino Serial Monitor.

You may need to change the Serial port used on your board for modem interface.

Here are some basic AT commands and expected modem answers you can use:

   ```c
   AT
   OK
   AT+CPIN?
   +CPIN: READY
   AT+COPS?
   +COPS: 0,0,"Orange F"
   ```

If not working, check:
 * That your modem is correctly powered. Try to reset it.
 * Hardware connection between the modem and your board (Ground, Rx, Tx).
 * Serial UART and baud rates used in example sketch and in Arduino Serial monitor.

### "Heracles Sample Basic" example sketch

This sketch connects to website arduino.cc to get file [asciilogo.txt](http://www.arduino.cc/asciilogo.txt), using the Heracles modem.

## License
This project is released under The GNU Lesser General Public License (LGPL-3.0).
