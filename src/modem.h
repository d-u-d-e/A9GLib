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
static const char GSM_CME_ERROR[] PROGMEM = "+CME ERROR";
static const char GSM_CMS_ERROR[] PROGMEM = "+CMS ERROR";
static const char CLOCK_FORMAT[] PROGMEM = "+CCLK: \"%y/%m/%d,%H:%M:%S\"";
static const char PROMPT[] PROGMEM = "\r\n>";

typedef const __FlashStringHelper* GsmConstStr;
#define GFP(x) (reinterpret_cast<GsmConstStr>(x))
#define GF(x) F(x)

class GSM_Socket;
class GPRS;

class ModemUrcHandler {
    public:
    virtual void handleUrc(const void* data, uint16_t len) = 0;
};

class ModemClass
{
public:
    friend class GPRS;
    friend class GSM_Socket;
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
    uint16_t write(const uint8_t* buf, uint16_t len);
    void flush();
    void send(__FlashStringHelper* command);
    void sendf(const char* fmt, ...);
    int8_t waitForResponse(uint32_t timeout = 1000L,
                                   GsmConstStr r1 = GFP(GSM_OK),
                                   GsmConstStr r2 = GFP(GSM_ERROR),
                                   #ifdef GSM_DEBUG
                                   GsmConstStr r3 = GFP(GSM_CME_ERROR),
                                   GsmConstStr r4 = GFP(GSM_CMS_ERROR),
                                   #else
                                   GsmConstStr r3 = NULL,
                                   GsmConstStr r4 = NULL,
                                   #endif
                                   GsmConstStr r5 = NULL)
    void poll();
    void setBaudRate(unsigned long baud);
    void removeUrcHandler(ModemUrcHandler* handler);
    void addUrcHandler(ModemUrcHandler* handler);
    bool streamSkipUntil(const char& c, String* save = NULL, const uint32_t timeout_ms = 10000L);
    int16_t streamGetIntBefore(const char& lastChar);

private:
    Uart* _uart;
    unsigned long _baud;
    bool _lowPowerMode;
    unsigned long _lastResponseOrUrcMillis;
    bool _init;
    uint16_t _chunkLen;
    uint8_t _sock; //socket that will receive the chunk
    #define MAX_SOCKETS 3
    GSM_Socket* _sockets[MAX_SOCKETS] = {NULL};
    uint8_t _initSocks;
    
    enum 
    {
        URC_IDLE,
        URC_RECV_SOCK_CHUNK
    } _urcState;

    String _buffer;
    #define MAX_URC_HANDLERS 1
    ModemUrcHandler* _urcHandlers[MAX_URC_HANDLERS] = {NULL};
};

extern ModemClass MODEM;
extern uint8_t GSM_PWR_PIN;
extern uint8_t GSM_RST_PIN;
extern uint8_t GSM_LOW_PWR_PIN;

#endif
