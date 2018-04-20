/*
 * Copyright (C) 2018 Orange
 *
 * This software is distributed under the terms and conditions of the GNU Lesser
 * General Public License (LGPL-3.0) which can be found in the file 'LICENSE.txt'
 * in this package distribution.
 */

#ifndef __HeraclesGsmModem_h
#define __HeraclesGsmModem_h

#if defined(ARDUINO)
  #if ARDUINO >= 100
    #include "Arduino.h"
  #else
    #include "WProgram.h"
  #endif
#endif

#include <Client.h>
#include <GsmFifo.h>

#if defined(__AVR__)
  #define GSM_PROGMEM PROGMEM
  typedef const __FlashStringHelper* GsmConstStr;
  #define GFP(x) (reinterpret_cast<GsmConstStr>(x))
  #define GF(x)  F(x)
#else
  #define GSM_PROGMEM
  typedef const char* GsmConstStr;
  #define GFP(x) x
  #define GF(x)  x
#endif

#define GSM_YIELD() { delay(0); }

#define GSM_MUX_COUNT 2

#define GSM_NL "\r\n"
static const char GSM_OK[] GSM_PROGMEM = "OK" GSM_NL;
static const char GSM_ERROR[] GSM_PROGMEM = "ERROR" GSM_NL;


enum SimStatus {
    SIM_ERROR = 0,
    SIM_READY = 1,
    SIM_LOCKED = 2,
};

enum RegStatus {
    REG_UNREGISTERED = 0,
    REG_SEARCHING = 2,
    REG_DENIED = 3,
    REG_OK_HOME = 1,
    REG_OK_ROAMING = 5,
    REG_UNKNOWN = 4,
};

/**
 * @startuml
 *   package "Arduino core" {
 *      class Client {
 *        {abstract} connect(host or IP, port)
 *        {abstract} write(buf, size)
 *        {abstract} available()
 *        {abstract} read(buf, size)
 *        {abstract} peek()
 *        {abstract} flush()
 *        {abstract} stop()
 *        {abstract} connected()
 *      }
 *      class Stream
 *   }
 *
 *   package "LiveBooster-Heracles-Arduino library" {
 *      class GsmClient {
 *          -rx : GsmFifo
 *          +connect(host or IP, port)
 *          +write(buf, size)
 *          +available()
 *          +read(buf, size)
 *          +<s>peek()</s>
 *          +flush()
 *          +stop()
 *          +connected()
 *      }
 *
 *      class HeraclesGsmModem {
 *        +setBaud(baud)
 *        +init()
 *        +attachGPRS()
 *        +getOperator()
 *        +...()
 *      }
 *
 *      GsmClient "0..2" --o "1" HeraclesGsmModem
 *   }
 *
 *   Client <|-right- GsmClient
 *   Stream "1" --o HeraclesGsmModem
 * @enduml
 */
class HeraclesGsmModem {

public:

    class GsmClient: public Client {
        friend class HeraclesGsmModem;

    public:

        GsmClient(HeraclesGsmModem& modem, uint8_t mux = 0, bool sslEnabled = true) {
            init(&modem, mux, sslEnabled);
        }

        /*
         * @startuml
         *    hide footbox
         *    participant UserApp as "User\nApplication"
         *    participant GsmClient as "GsmClient\n(library)"
         *    participant HeraclesGsmModem as "HeraclesGsmModem\n(library)"
         *    UserApp -> GsmClient : connect(<host>, <port>)
         *    GsmClient -> HeraclesGsmModem : modemConnect(<host>, <port>, <mux>, <ssl>)
         *    HeraclesGsmModem -> Stream : "AT+CIPSSL=<ssl>"
         *    note right : Enable or disable SSL function
         *    HeraclesGsmModem <-- Stream : "OK"
         *    HeraclesGsmModem -> Stream : "AT+CIPSTART=<mux>,TCP,<host>,<port>"
         *    note right : Start up the connection
         *    HeraclesGsmModem <-- Stream : "CONNECT OK"
         *    note left : The TCP connection has been established successfully.\nSSL certificate handshake finished.
         *    GsmClient <-- HeraclesGsmModem : status
         *    UserApp <-- GsmClient : status
         * @enduml
         */
        virtual int connect(const char *host, uint16_t port) {
            GSM_YIELD();
            rx.clear();

            sock_connected = at->modemConnect(host, port, mux, ssl_enabled);
            return sock_connected;
        }

        virtual int connect(IPAddress ip, uint16_t port) {
            String host;
            host.reserve(16);
            host += ip[0];
            host += ".";
            host += ip[1];
            host += ".";
            host += ip[2];
            host += ".";
            host += ip[3];
            return connect(host.c_str(), port);
        }

        /*
         * @startuml
         *    hide footbox
         *    participant UserApp as "User\nApplication"
         *    participant GsmClient as "GsmClient\n(library)"
         *    participant HeraclesGsmModem as "HeraclesGsmModem\n(library)"
         *    UserApp -> GsmClient : stop()
         *    GsmClient -> HeraclesGsmModem : sendAT("+CIPCLOSE=<mux>")
         *    HeraclesGsmModem -> Stream : "AT+CIPCLOSE=<mux>"
         *    note right : Close connection
         *    HeraclesGsmModem <-- Stream : "CLOSE OK"
         *    GsmClient <-- HeraclesGsmModem : "CLOSE OK"
         * @enduml
         */
        virtual void stop() {
            GSM_YIELD();
            at->sendAT(GF("+CIPCLOSE="), mux);
            sock_connected = false;
            at->waitResponse();
            rx.clear();
        }

        /*
         * @startuml
         *    hide footbox
         *    participant UserApp as "User\nApplication"
         *    participant GsmClient as "GsmClient\n(library)"
         *    participant HeraclesGsmModem as "HeraclesGsmModem\n(library)"
         *    UserApp -> GsmClient : write(<buf>, <size>)
         *    GsmClient -> HeraclesGsmModem : maintain()
         *    GsmClient -> HeraclesGsmModem : modemSend(<buf>, <size>, <mux>)
         *    HeraclesGsmModem -> Stream : "AT+CIPSEND=<mux>,<size>"
         *    note right : Write command
         *    HeraclesGsmModem <-- Stream : ">"
         *    HeraclesGsmModem -> Stream : write(<buf>, <size>)
         *    note right : Provide data to send
         *    HeraclesGsmModem -> Stream : flush()
         *    HeraclesGsmModem <-- Stream : "DATA ACCEPT:"
         *    note left : Sending is successful
         *    GsmClient <-- HeraclesGsmModem : status
         *    UserApp <-- GsmClient : status
         * @enduml
         */
        virtual size_t write(const uint8_t *buf, size_t size) {
            GSM_YIELD();
            at->maintain();
            return at->modemSend(buf, size, mux);
        }

        virtual size_t write(uint8_t c) {
            return write(&c, 1);
        }

        virtual int available() {
            GSM_YIELD();
            if (!rx.size() && sock_connected) {
                at->maintain();
            }

            return rx.size() + sock_available;
        }

        /*
         * @startuml
         *    hide footbox
         *    participant UserApp as "User\nApplication"
         *    participant GsmClient as "GsmClient\n(library)"
         *    participant HeraclesGsmModem as "HeraclesGsmModem\n(library)"
         *    UserApp -> GsmClient : read(<buf>, size)
         *    GsmClient -> HeraclesGsmModem : maintain()
         *    GsmClient -> HeraclesGsmModem : modemRead(...)
         *    HeraclesGsmModem -> Stream : "AT+CIPRXGET=2,<mux>,<size>"
         *    note right : Get Data from Network Manually
         *    loop while a char is available
         *      HeraclesGsmModem -> Stream : stream.read()
         *      HeraclesGsmModem <-- Stream : received char
         *    end loop
         *    HeraclesGsmModem <-- Stream : "OK"
         *    GsmClient <-- HeraclesGsmModem : number of bytes read
         *    UserApp <-- GsmClient : number of bytes read
         * @enduml
         */
        virtual int read(uint8_t *buf, size_t size) {
            GSM_YIELD();
            at->maintain();
            size_t cnt = 0;
            while (cnt < size)
            {
                size_t chunk = (rx.size() < (size-cnt)) ? rx.size() : (size-cnt);
                if (chunk > 0) {
                    rx.get(buf, chunk);
                    buf += chunk;
                    cnt += chunk;
                    continue;
                }
                at->maintain();
                if (sock_available > 0) {
                    at->modemRead(rx.free(), mux);
                }
                else {
                    break;
                }
            }
            return cnt;
        }

        virtual int read() {
            uint8_t c;
            if (read(&c, 1) == 1) {
                return c;
            }
            return -1;
        }

        virtual int peek() {
            return -1; // TODO
        }

        virtual void flush() {
            at->stream.flush();
        }

        virtual uint8_t connected() {
            if (available()) {
                return true;
            }

            return sock_connected;
        }

        virtual operator bool() {
            return connected();
        }

    private:

        bool init(HeraclesGsmModem* modem, uint8_t mux, bool sslEnabled) {
            this->at = modem;
            this->mux = mux;
            ssl_enabled = sslEnabled;
            sock_available = 0;
            sock_connected = false;

            at->sockets[mux] = this;

            return true;
        }

        typedef GsmFifo<uint8_t, 64> RxFifo;

        HeraclesGsmModem* at;
        uint8_t mux;
        uint16_t sock_available;
        bool sock_connected;
        bool ssl_enabled;
        RxFifo rx;
    };

public:

    HeraclesGsmModem(Stream& stream, bool dnsEnabled = true) : stream(stream), dns_enabled(dnsEnabled)
    {
        memset(sockets, 0, sizeof(sockets));
        prev_check = 0;
    }

    /*
     * Basic functions
     */

    bool init() {
        if (!testAT()) {
            return false;
        }
        sendAT(GF("&F0"));  // Set all TA parameters to manufacturer defaults
        if (waitResponse(10000L) != 1) {
            return false;
        }
        sendAT(GF("E0"));   // Echo Off
        if (waitResponse() != 1) {
            return false;
        }
        getSimStatus();
        return true;
    }

    void setBaud(unsigned long baud) {
        sendAT(GF("+IPR="), baud);
    }

    bool testAT(unsigned long timeout = 10000L) {
        for (unsigned long start = millis(); millis() - start < timeout;) {
            sendAT(GF(""));
            if (waitResponse(200) == 1) {
                delay(100);
                return true;
            }
            delay(100);
        }
        return false;
    }

    void maintain() {
        if (millis() - prev_check > 500) {
            prev_check = millis();
            for (int mux = 0; mux < GSM_MUX_COUNT; mux++) {
                GsmClient* sock = sockets[mux];
                if (sock) {
                    sock->sock_available = modemGetAvailable(mux);
                }
            }
        }

        while (stream.available()) {
            waitResponse(10, NULL, NULL);
        }
    }

    bool factoryDefault() {
        sendAT(GF("&FZE0&W"));  // Factory + Reset + Echo Off + Write
        waitResponse();
        sendAT(GF("+IPR=0"));   // Auto-baud
        waitResponse();
        sendAT(GF("+IFC=0,0")); // No Flow Control
        waitResponse();
        sendAT(GF("+ICF=3,3")); // 8 data 0 parity 1 stop
        waitResponse();
        sendAT(GF("+CSCLK=0")); // Disable Slow Clock
        waitResponse();
        sendAT(GF("&W"));       // Write configuration
        return waitResponse() == 1;
    }

    String getModemInfo() {
        sendAT(GF("I"));
        String res;
        if (waitResponse(1000L, res) != 1) {
            return "";
        }
        res.replace(GSM_NL "OK" GSM_NL, "");
        res.replace(GSM_NL, " ");
        res.trim();
        return res;
    }

    /*
     * Power functions
     */

    bool restart() {
        if (!testAT()) {
            return false;
        }

        sendAT(GF("+CFUN=0"));
        if (waitResponse(10000L) != 1) {
            return false;
        }
        sendAT(GF("+CFUN=1,1"));
        if (waitResponse(10000L) != 1) {
            return false;
        }
        delay(3000);
        return init();
    }

    bool poweroff() {
        sendAT(GF("+CPOWD=1"));
        return waitResponse(GF("NORMAL POWER DOWN")) == 1;
    }

    bool radioOff() {
        sendAT(GF("+CFUN=0"));
        if (waitResponse(10000L) != 1) {
            return false;
        }
        delay(3000);
        return true;
    }

    /*
     During sleep, the SIM800 module has its serial communication disabled. In order to reestablish communication
     pull the DRT-pin of the SIM800 module LOW for at least 50ms. Then use this function to disable sleep mode.
     The DTR-pin can then be released again.
     */
    bool sleepEnable(bool enable = true) {
        sendAT(GF("+CSCLK="), enable);
        return waitResponse() == 1;
    }

    /*
     * SIM card functions
     */

    bool setInternalSim() {
        sendAT(GF("+SSIM=0"));
        if (waitResponse() != 1) {
            return false;
        }
        sendAT(GF("&W"));
        if (waitResponse() != 1) {
            return false;
        }
        if (!poweroff()) {
            return false;
        }
        return true;
    }

    bool setExternalSim() {
        sendAT(GF("+SSIM=1"));
        if (waitResponse() != 1) {
            return false;
        }
        sendAT(GF("&W"));
        if (waitResponse() != 1) {
            return false;
        }
        if (!poweroff()) {
            return false;
        }
        return true;
    }

    bool simUnlock(const char *pin) {
        sendAT(GF("+CPIN=\""), pin, GF("\""));
        return waitResponse() == 1;
    }

    String getSimCCID() {
        sendAT(GF("+ICCID"));
        if (waitResponse(GF(GSM_NL "+ICCID:")) != 1) {
            return "";
        }
        String res = stream.readStringUntil('\n');
        waitResponse();
        res.trim();
        return res;
    }

    String getIMEI() {
        sendAT(GF("+GSN"));
        if (waitResponse(GF(GSM_NL)) != 1) {
            return "";
        }
        String res = stream.readStringUntil('\n');
        waitResponse();
        res.trim();
        return res;
    }

    SimStatus getSimStatus(unsigned long timeout = 10000L) {
        for (unsigned long start = millis(); millis() - start < timeout;) {
            sendAT(GF("+CPIN?"));
            if (waitResponse(GF(GSM_NL "+CPIN:")) != 1) {
                delay(1000);
                continue;
            }
            int status = waitResponse(GF("READY"), GF("SIM PIN"), GF("SIM PUK"), GF("NOT INSERTED"));
            waitResponse();
            switch (status) {
            case 2:
            case 3:
                return SIM_LOCKED;
            case 1:
                return SIM_READY;
            default:
                return SIM_ERROR;
            }
        }
        return SIM_ERROR;
    }

    RegStatus getRegistrationStatus() {
        sendAT(GF("+CREG?"));
        if (waitResponse(GF(GSM_NL "+CREG:")) != 1) {
            return REG_UNKNOWN;
        }
        streamSkipUntil(','); // Skip format (0)
        int status = stream.readStringUntil('\n').toInt();
        waitResponse();
        return (RegStatus) status;
    }

    String getOperator() {
        sendAT(GF("+COPS?"));
        if (waitResponse(GF(GSM_NL "+COPS:")) != 1) {
            return "";
        }
        streamSkipUntil('"'); // Skip mode and format
        String res = stream.readStringUntil('"');
        waitResponse();
        return res;
    }

    /*
     * Generic network functions
     */

    int getSignalQuality() {
        sendAT(GF("+CSQ"));
        if (waitResponse(GF(GSM_NL "+CSQ:")) != 1) {
            return 99;
        }
        int res = stream.readStringUntil(',').toInt();
        waitResponse();
        return res;
    }

    bool isNetworkConnected() {
        RegStatus s = getRegistrationStatus();
        return (s == REG_OK_HOME || s == REG_OK_ROAMING);
    }

    bool waitForNetwork(unsigned long timeout = 60000L) {
        for (unsigned long start = millis(); millis() - start < timeout;) {
            if (isNetworkConnected()) {
                return true;
            }
            delay(250);
        }
        return false;
    }

    /*
     * GPRS functions for all external SIM CARD
     */
    bool attachGPRS(const char* apn, const char* user, const char* pwd) {
        gprsDisconnect();

        // Set the connection type to GPRS
        sendAT(GF("+SAPBR=3,1,\"Contype\",\"GPRS\""));
        waitResponse();

        sendAT(GF("+SAPBR=3,1,\"APN\",\""), apn, '"');  // Set the APN
        waitResponse();

        if (user && strlen(user) > 0) {
            sendAT(GF("+SAPBR=3,1,\"USER\",\""), user, '"');  // Set the user name
            waitResponse();
        }

        if (pwd && strlen(pwd) > 0) {
            sendAT(GF("+SAPBR=3,1,\"PWD\",\""), pwd, '"');  // Set the password
            waitResponse();
        }

        // Define the PDP context
        sendAT(GF("+CGDCONT=1,\"IP\",\""), apn, '"');
        waitResponse();

        // Activate the PDP context
        sendAT(GF("+CGACT=1,1"));
        waitResponse(60000L);

        // Open the definied GPRS bearer context
        sendAT(GF("+SAPBR=1,1"));
        waitResponse(85000L);

        // Query the GPRS bearer context status
        sendAT(GF("+SAPBR=2,1"));
        if (waitResponse(30000L) != 1)
            return false;

        // Attach to GPRS
        sendAT(GF("+CGATT=1"));
        if (waitResponse(60000L) != 1)
            return false;

        // Set mode TCP
        sendAT(GF("+CIPMODE=0"));
        if (waitResponse() != 1) {
            return false;
        }

        // Set to multiple-IP
        sendAT(GF("+CIPMUX=1"));
        if (waitResponse() != 1) {
            return false;
        }

        // Put in "quick send" mode (thus no extra "Send OK")
        sendAT(GF("+CIPQSEND=1"));
        if (waitResponse() != 1) {
            return false;
        }

        // Set to get data manually
        sendAT(GF("+CIPRXGET=1"));
        if (waitResponse() != 1) {
            return false;
        }

        // Start Task and Set APN, USER NAME, PASSWORD
        sendAT(GF("+CSTT=\""), apn, GF("\",\""), user, GF("\",\""), pwd, GF("\""));
        if (waitResponse(60000L) != 1) {
            return false;
        }

        // Bring Up Wireless Connection with GPRS or CSD
        sendAT(GF("+CIICR"));
        if (waitResponse(60000L) != 1) {
            return false;
        }

        // Get Local IP Address, only assigned after connection
        sendAT(GF("+CIFSR;E0"));
        if (waitResponse(10000L) != 1) {
            return false;
        }

        // Configure Domain Name Server (DNS)
        if (dns_enabled) {
            sendAT(GF("+CDNSCFG=\"8.8.8.8\",\"8.8.4.4\""));
            if (waitResponse() != 1) {
                return false;
            }
        }

        return true;
    }

    /*
     * GPRS function for Heracles Internal SIM card (APN fixed by default)
     */

    /*
     * @startuml
     *    hide footbox
     *    participant UserApp as "User\nApplication"
     *    participant Library as "HeraclesGsmModem\n(library)"
     *    UserApp -> Library : attachGPRS()
     *    Library -> Stream : "AT+SAPBR=3,1,"CONTYPE","GPRS""
     *    note right : Set the connection type to GPRS
     *    Library <-- Stream : "OK"
     *    Library -> Stream : "AT+CGACT=1,1"
     *    note right : Activate the PDP context
     *    Library <-- Stream : "OK"
     *    Library -> Stream : "AT+SAPBR=1,1"
     *    note right : Open the defined GPRS bearer context
     *    Library <-- Stream : "OK"
     *    Library -> Stream : "AT+SAPBR=2,1"
     *    note right : Query the GPRS bearer context status
     *    Library <-- Stream : "OK"
     *    Library -> Stream : "AT+CGATT=1"
     *    note right : Attach to GPRS.\nMax response time is 75sec.
     *    activate Stream
     *    Library <-- Stream : "OK"
     *    deactivate Stream
     *    Library -> Stream : "AT+CIPMODE=0"
     *    note right : Set mode TCP
     *    Library <-- Stream : "OK"
     *    Library -> Stream : "AT+CIPMUX=1"
     *    note right : Set to multiple-IP
     *    Library <-- Stream : "OK"
     *    Library -> Stream : "AT+CIPQSEND=1"
     *    note right : Put in "quick send" mode (thus no extra "Send OK")
     *    Library <-- Stream : "OK"
     *    Library -> Stream : "AT+CIPRXGET=1"
     *    note right : Set to get data manually
     *    Library <-- Stream : "OK"
     *    Library -> Stream : "AT+CSTT"
     *    note right : Default configuration for Heracles board: just AT+CSTT
     *    Library <-- Stream : "OK"
     *    Library -> Stream : "AT+CIICR"
     *    note right : Bring Up Wireless Connection with GPRS or CSD
     *    Library <-- Stream : "OK"
     *    Library -> Stream : "AT+CIFSR;E0"
     *    note right : Get Local IP Address, only assigned after connection
     *    Library <-- Stream : "OK"
     *    alt If DNS is enabled
     *       Library -> Stream : "+CDNSCFG="8.8.8.8","8.8.4.4""
     *       note right : Configure Domain Name Server (DNS)
     *       Library <-- Stream : "OK"
     *    end cond
     *    UserApp <-- Library : status
     * @enduml
     */
    bool attachGPRS() {
        gprsDisconnect();

        // Set the connection type to GPRS
        sendAT(GF("+SAPBR=3,1,\"CONTYPE\",\"GPRS\""));
        waitResponse();

        // Activate the PDP context
        sendAT(GF("+CGACT=1,1"));
        waitResponse(60000L);

        // Open the defined GPRS bearer context
        sendAT(GF("+SAPBR=1,1"));
        waitResponse(85000L);

        // Query the GPRS bearer context status
        sendAT(GF("+SAPBR=2,1"));
        if (waitResponse(30000L) != 1)
            return false;

        // Attach to GPRS
        sendAT(GF("+CGATT=1"));
        if (waitResponse(75000L) != 1)
            return false;

        // Set mode TCP
        sendAT(GF("+CIPMODE=0"));
        if (waitResponse() != 1) {
            return false;
        }

        // Set to multiple-IP
        sendAT(GF("+CIPMUX=1"));
        if (waitResponse() != 1) {
            return false;
        }

        // Put in "quick send" mode (thus no extra "Send OK")
        sendAT(GF("+CIPQSEND=1"));
        if (waitResponse() != 1) {
            return false;
        }

        // Set to get data manually
        sendAT(GF("+CIPRXGET=1"));
        if (waitResponse() != 1) {
            return false;
        }

        // Default configuration for Heracles board: just AT+CSTT
        sendAT(GF("+CSTT"));
        if (waitResponse(60000L) != 1) {
            return false;
        }

        // Bring Up Wireless Connection with GPRS or CSD
        sendAT(GF("+CIICR"));
        if (waitResponse(60000L) != 1) {
            return false;
        }

        // Get Local IP Address, only assigned after connection
        sendAT(GF("+CIFSR;E0"));
        if (waitResponse(10000L) != 1) {
            return false;
        }

        // Configure Domain Name Server (DNS)
        if (dns_enabled) {
            sendAT(GF("+CDNSCFG=\"8.8.8.8\",\"8.8.4.4\""));
            if (waitResponse() != 1) {
                return false;
            }
        }

        return true;
    }

    bool gprsDisconnect() {
        sendAT(GF("+CIPSHUT"));  // Shut the TCP/IP connection
        if (waitResponse(60000L) != 1)
            return false;

        sendAT(GF("+CGATT=0"));  // Deactivate the bearer context
        if (waitResponse(60000L) != 1)
            return false;

        return true;
    }

    bool isGprsConnected() {
        sendAT(GF("+CGATT?"));
        if (waitResponse(GF(GSM_NL "+CGATT:")) != 1) {
            return false;
        }
        int res = stream.readStringUntil('\n').toInt();
        waitResponse();
        if (res != 1)
            return false;

        sendAT(GF("+CIFSR;E0")); // Another option is to use AT+CGPADDR=1
        if (waitResponse() != 1)
            return false;

        return true;
    }

    String getLocalIP() {
        sendAT(GF("+CIFSR;E0"));
        String res;
        if (waitResponse(10000L, res) != 1) {
            return "";
        }
        res.replace(GSM_NL "OK" GSM_NL, "");
        res.replace(GSM_NL, "");
        res.trim();
        return res;
    }

    IPAddress localIP() {
        String strIP = getLocalIP();

        int Parts[4] = { 0, };
        int Part = 0;
        for (uint8_t i = 0; i < strIP.length(); i++) {
            char c = strIP[i];
            if (c == '.') {
                Part++;
                if (Part > 3) {
                    return IPAddress(0, 0, 0, 0);
                }
                continue;
            }
            else if (c >= '0' && c <= '9') {
                Parts[Part] *= 10;
                Parts[Part] += c - '0';
            }
            else {
                if (Part == 3)
                    break;
            }
        }
        return IPAddress(Parts[0], Parts[1], Parts[2], Parts[3]);
    }

    /*
     * Phone Call functions
     */

    bool setGsmBusy(bool busy = true) {
        sendAT(GF("+GSMBUSY="), busy ? 1 : 0);
        return waitResponse() == 1;
    }

    bool callAnswer() {
        sendAT(GF("A"));
        return waitResponse() == 1;
    }

    // Returns true on pick-up, false on error/busy
    bool callNumber(const String& number) {
        if (number == GF("last")) {
            sendAT(GF("DL"));
        }
        else {
            sendAT(GF("D"), number, ";");
        }
        int status = waitResponse(60000L,
                                  GFP(GSM_OK),
                                  GF("BUSY" GSM_NL),
                                  GF("NO ANSWER" GSM_NL),
                                  GF("NO CARRIER" GSM_NL));

        switch (status) {
            case 1:
                return true;
            case 2:
            case 3:
                return false;
            default:
                return false;
        }
    }

    bool callHangup() {
        sendAT(GF("H"));
        return waitResponse() == 1;
    }

    // 0-9,*,#,A,B,C,D
    bool dtmfSend(char cmd, int duration_ms = 100) {
        duration_ms = constrain(duration_ms, 100, 1000);

        sendAT(GF("+VTD="), duration_ms / 100); // VTD accepts in 1/10 of a second
        waitResponse();

        sendAT(GF("+VTS="), cmd);
        return waitResponse(10000L) == 1;
    }

    /*
     * Messaging functions
     */

    String sendUSSD(const String& code) {
        sendAT(GF("+CMGF=1"));
        waitResponse();
        sendAT(GF("+CSCS=\"HEX\""));
        waitResponse();
        sendAT(GF("+CUSD=1,\""), code, GF("\""));
        if (waitResponse() != 1) {
            return "";
        }
        if (waitResponse(10000L, GF(GSM_NL "+CUSD:")) != 1) {
            return "";
        }
        stream.readStringUntil('"');
        String hex = stream.readStringUntil('"');
        stream.readStringUntil(',');
        int dcs = stream.readStringUntil('\n').toInt();

        if (dcs == 15) {
            return gsmDecodeHex8bit(hex);
        }
        else if (dcs == 72) {
            return gsmDecodeHex16bit(hex);
        }
        else {
            return hex;
        }
    }

    bool sendSMS(const String& number, const String& text) {
        sendAT(GF("+CMGF=1"));
        waitResponse();
        sendAT(GF("+CMGS=\""), number, GF("\""));
        if (waitResponse(GF(">")) != 1) {
            return false;
        }
        stream.print(text);
        stream.write((char) 0x1A);
        stream.flush();
        return waitResponse(60000L) == 1;
    }

    bool sendSMS_UTF16(const String& number, const void* text, size_t len) {
        sendAT(GF("+CMGF=1"));
        waitResponse();
        sendAT(GF("+CSCS=\"HEX\""));
        waitResponse();
        sendAT(GF("+CSMP=17,167,0,8"));
        waitResponse();

        sendAT(GF("+CMGS=\""), number, GF("\""));
        if (waitResponse(GF(">")) != 1) {
            return false;
        }

        uint16_t* t = (uint16_t*) text;
        for (size_t i = 0; i < len; i++) {
            uint8_t c = t[i] >> 8;
            if (c < 0x10) {
                stream.print('0');
            }
            stream.print(c, HEX);
            c = t[i] & 0xFF;
            if (c < 0x10) {
                stream.print('0');
            }
            stream.print(c, HEX);
        }
        stream.write((char) 0x1A);
        stream.flush();
        return waitResponse(60000L) == 1;
    }

    /*
     * Location functions
     */

    String getGsmLocation() {
        sendAT(GF("+CIPGSMLOC=1,1"));
        if (waitResponse(10000L, GF(GSM_NL "+CIPGSMLOC:")) != 1) {
            return "";
        }
        String res = stream.readStringUntil('\n');
        waitResponse();
        res.trim();
        return res;
    }

    /*
     * Battery functions
     */

    // Use: float vBatt = modem.getBattVoltage() / 1000.0;
    uint16_t getBattVoltage() {
        sendAT(GF("+CBC"));
        if (waitResponse(GF(GSM_NL "+CBC:")) != 1) {
            return 0;
        }
        streamSkipUntil(','); // Skip
        streamSkipUntil(','); // Skip

        uint16_t res = stream.readStringUntil(',').toInt();
        waitResponse();
        return res;
    }

    int getBattPercent() {
        sendAT(GF("+CBC"));
        if (waitResponse(GF(GSM_NL "+CBC:")) != 1) {
            return false;
        }
        stream.readStringUntil(',');
        int res = stream.readStringUntil(',').toInt();
        waitResponse();
        return res;
    }

protected:

    bool modemConnect(const char* host, uint16_t port, uint8_t mux, bool sslEnabled) {
        sendAT(GF("+CIPSSL="), sslEnabled);
        int rsp = waitResponse();
        if (sslEnabled && (rsp != 1)) {
            return false;
        }
        sendAT(GF("+CIPSTART="), mux, ',', GF("\"TCP"), GF("\",\""), host, GF("\","), port);
        rsp = waitResponse(75000L,
                           GF("CONNECT OK" GSM_NL),
                           GF("CONNECT FAIL" GSM_NL),
                           GF("ALREADY CONNECT" GSM_NL),
                           GF("ERROR" GSM_NL),
                           GF("CLOSE OK" GSM_NL)   // Happens when HTTPS handshake fails
                          );
        return (1 == rsp);
    }

    int modemSend(const void* buff, size_t len, uint8_t mux) {
        sendAT(GF("+CIPSEND="), mux, ',', len);
        if (waitResponse(GF(">")) != 1) {
            return 0;
        }
        stream.write((uint8_t*) buff, len);
        stream.flush();
        if (waitResponse(GF(GSM_NL "DATA ACCEPT:")) != 1) {
            return 0;
        }
        streamSkipUntil(','); // Skip mux
        return stream.readStringUntil('\n').toInt();
    }

    size_t modemRead(size_t size, uint8_t mux) {
        sendAT(GF("+CIPRXGET=2,"), mux, ',', size);
        if (waitResponse(GF("+CIPRXGET:")) != 1) {
            return 0;
        }

        streamSkipUntil(','); // Skip mode 2
        streamSkipUntil(','); // Skip mux
        size_t len = stream.readStringUntil(',').toInt();
        sockets[mux]->sock_available = stream.readStringUntil('\n').toInt();

        for (size_t i = 0; i < len; i++) {
            while (!stream.available()) {
                GSM_YIELD();
            }
            char c = stream.read();
            sockets[mux]->rx.put(c);
        }
        waitResponse();
        return len;
    }

    size_t modemGetAvailable(uint8_t mux) {
        sendAT(GF("+CIPRXGET=4,"), mux);
        size_t result = 0;
        if (waitResponse(GF("+CIPRXGET:")) == 1) {
            streamSkipUntil(','); // Skip mode 4
            streamSkipUntil(','); // Skip mux
            result = stream.readStringUntil('\n').toInt();
            waitResponse();
        }
        if (!result) {
            sockets[mux]->sock_connected = modemGetConnected(mux);
        }
        return result;
    }

    bool modemGetConnected(uint8_t mux) {
        sendAT(GF("+CIPSTATUS="), mux);
        int res = waitResponse(GF(",\"CONNECTED\""), GF(",\"CLOSED\""), GF(",\"CLOSING\""), GF(",\"INITIAL\""));
        waitResponse();
        return 1 == res;
    }

public:

    /* Utilities */

    template<typename T>
    void streamWrite(T last) {
        stream.print(last);
    }

    template<typename T, typename ... Args>
    void streamWrite(T head, Args ... tail) {
        stream.print(head);
        streamWrite(tail...);
    }

    bool streamSkipUntil(char c) { // TODO: timeout
        while (true) {
            while (!stream.available()) {
                GSM_YIELD();
            }
            if (stream.read() == c)
                return true;
        }
        return false;
    }

    template<typename ... Args>
    void sendAT(Args ... cmd) {
        streamWrite("AT", cmd..., GSM_NL);
        stream.flush();
        GSM_YIELD();
    }

    uint8_t waitResponse(uint32_t timeout, String& data, GsmConstStr r1 = GFP(GSM_OK), GsmConstStr r2 = GFP(GSM_ERROR),
            GsmConstStr r3 = NULL, GsmConstStr r4 = NULL, GsmConstStr r5 = NULL)
    {
        data.reserve(64);
        int index = 0;
        unsigned long startMillis = millis();
        do {
            GSM_YIELD();
            while (stream.available() > 0) {
                int a = stream.read();
                if (a <= 0) {
                    continue; // Skip 0x00 bytes, just in case
                }
                data += (char) a;
                if (r1 && data.endsWith(r1)) {
                    return 1;
                }
                else if (r2 && data.endsWith(r2)) {
                    return 2;
                }
                else if (r3 && data.endsWith(r3)) {
                    return 3;
                }
                else if (r4 && data.endsWith(r4)) {
                    return 4;
                }
                else if (r5 && data.endsWith(r5)) {
                    return 5;
                }
                else if (data.endsWith(GF(GSM_NL "+CIPRXGET:"))) {
                    String mode = stream.readStringUntil(',');
                    if (mode.toInt() == 1) {
                        int mux = stream.readStringUntil('\n').toInt();
                        if (mux >= 0 && mux < GSM_MUX_COUNT && sockets[mux]) {
                            prev_check = 0;
                        }
                        data = "";
                    }
                    else {
                        data += mode;
                    }
                }
                else if (data.endsWith(GF("CLOSED" GSM_NL))) {
                    int nl = data.lastIndexOf(GSM_NL, data.length() - 8);
                    int coma = data.indexOf(',', nl + 2);
                    int mux = data.substring(nl + 2, coma).toInt();
                    if (mux >= 0 && mux < GSM_MUX_COUNT && sockets[mux]) {
                        sockets[mux]->sock_connected = false;
                        sockets[mux]->sock_available = 0;
                    }
                    data = "";
                }
            }
        } while (millis() - startMillis < timeout);

        return index;
    }

    uint8_t waitResponse(uint32_t timeout, GsmConstStr r1 = GFP(GSM_OK), GsmConstStr r2 = GFP(GSM_ERROR),
            GsmConstStr r3 = NULL, GsmConstStr r4 = NULL, GsmConstStr r5 = NULL)
    {
        String data;
        return waitResponse(timeout, data, r1, r2, r3, r4, r5);
    }

    uint8_t waitResponse(GsmConstStr r1 = GFP(GSM_OK), GsmConstStr r2 = GFP(GSM_ERROR), GsmConstStr r3 = NULL,
            GsmConstStr r4 = NULL, GsmConstStr r5 = NULL)
    {
        return waitResponse(1000, r1, r2, r3, r4, r5);
    }

private:

    Stream& stream;
    GsmClient* sockets[GSM_MUX_COUNT];
    bool dns_enabled;
    uint32_t prev_check;

    static inline
    String gsmDecodeHex8bit(String &instr) {
      String result;
      for (unsigned i=0; i<instr.length(); i+=2) {
        char buf[4] = { 0, };
        buf[0] = instr[i];
        buf[1] = instr[i+1];
        char b = strtol(buf, NULL, 16);
        result += b;
      }
      return result;
    }

    static inline
    String gsmDecodeHex16bit(String &instr) {
      String result;
      for (unsigned i=0; i<instr.length(); i+=4) {
        char buf[4] = { 0, };
        buf[0] = instr[i];
        buf[1] = instr[i+1];
        char b = strtol(buf, NULL, 16);
        if (b) { // If high byte is non-zero, we can't handle it ;(
          result += "?";
        } else {
          buf[0] = instr[i+2];
          buf[1] = instr[i+3];
          b = strtol(buf, NULL, 16);
          result += b;
        }
      }
      return result;
    }
};

#endif
