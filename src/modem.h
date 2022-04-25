#ifndef _MODEM_INCLUDED_H
#define _MODEM_INCLUDED_H

#include <stdarg.h>
#include <stdio.h>

#include <Arduino.h>


#define MODEM_MIN_RESPONSE_OR_URC_WAIT_TIME_MS 20

//uncomment next line to debug on SerialUSB
#define GSM_DEBUG SerialUSB

/*If defined, all commands X sent to the modem are printed on the uart with the following syntax:
    "#DEBUG# command sent: X"

    All responses to commands are printed with the following syntax:
    1) if no buffer is provided to save the response Y along with result code C: "#DEBUG# response received"
    2) otherwise: "#DEBUG# response received: Y, code: C"

    URC X are printed as well, with the following syntax:
    "#DEBUG# URC received: X"
*/

#ifdef GSM_DEBUG
namespace {
template <typename T>
static void DBG_PLAIN(T last) {
    GSM_DEBUG.println(last);
}

template <typename T, typename... Args>
static void DBG_PLAIN(T head, Args... tail) {
    GSM_DEBUG.print(head);
    DBG_PLAIN(tail...);
}

template <typename... Args>
static void DBG(Args... args) {
    GSM_DEBUG.print(F("["));
    GSM_DEBUG.print(millis());
    GSM_DEBUG.print(F("] "));
    DBG_PLAIN(args...);
}
}  // namespace
#else
#define DBG_PLAIN(...)
#define DBG(...)
#endif


static const char GSM_OK[] PROGMEM = "\r\nOK\r\n";
static const char GSM_ERROR[] PROGMEM = "\r\nERROR\r\n";
static const char GSM_DBG_ERROR[] PROGMEM = "+CME ERROR";
static const char GSM_NO_CARRIER[] PROGMEM = "\r\nNO CARRIER\r\n";
static const char CLOCK_FORMAT[] PROGMEM = "+CCLK: \"%y/%m/%d,%H:%M:%S\"";
static const char TCP_PROMPT[] PROGMEM = "\r\n>";

class ModemUrcHandler {
public:
    virtual void handleUrc(const String& urc) = 0;
};

class ModemClass
{
public:
    ModemClass(Uart& uart, unsigned long baud);
    bool init();
    bool powerOff();
    bool autosense(unsigned int timeout = 10000);
    bool noop();
    bool factoryReset();
    bool restart();

    void lowPowerMode();
    void noLowPowerMode();
    uint16_t write(uint8_t c);
    uint16_t write(const uint8_t*, uint16_t);
    void flush();
    void send(const char* command);
    void send(__FlashStringHelper* command);
    void send(const String& command)
    {
        send(command.c_str());
    }
    void sendf(const char *fmt, ...);

    int waitForResponse(unsigned long timeout = 100, String* responseDataStorage = NULL, String* expected = NULL);
    uint8_t ready();
    void poll();
    void setResponseDataStorage(String* responseDataStorage);
    void setBaudRate(unsigned long baud);

    void removeUrcHandler(ModemUrcHandler* handler);
    void addUrcHandler(ModemUrcHandler* handler);

private:
    Uart* _uart;
    unsigned long _baud;
    bool _lowPowerMode;
    unsigned long _lastResponseOrUrcMillis;
    bool _init;

    void beginSend();
    void handleUrc();

    enum
    {
        AT_COMMAND_IDLE,
        AT_RECEIVING_RESPONSE,
        AT_CONSUME
    } _atCommandState;

    uint8_t _ready;
    String _buffer;
    String* _responseDataStorage;
    String* _expected;
    unsigned int _stopConsumingAtLen;

#define SOCKETS_MAX 4
#define MAX_URC_HANDLERS (SOCKETS_MAX + 1) // 4 Sockets + Location TODO to define
    static ModemUrcHandler* _urcHandlers[MAX_URC_HANDLERS];

};

extern ModemClass MODEM;
extern uint8_t GSM_PWR_PIN;
extern uint8_t GSM_RST_PIN;
extern uint8_t GSM_LOW_PWR_PIN;

#endif
