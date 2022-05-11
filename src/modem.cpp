#include "Modem.h"
#include "socket.h"

ModemClass::ModemClass(Uart& uart, unsigned long baud):
    _uart(&uart),
    _baud(baud),
    _lowPowerMode(false),
    _lastResponseOrUrcMillis(0),
    _init(false),
    _ready(1),
	_sent(false),
    _responseDataStorage(NULL),
    _initSocks(0),
    _atCommandState(AT_IDLE)

{
    _buffer.reserve(64); //reserve 64 chars
}


//Copy this code before calling init()

/*
        pinMode(GSM_PWR_PIN, OUTPUT);
        pinMode(GSM_RST_PIN, OUTPUT);
        pinMode(GSM_LOW_PWR_PIN, OUTPUT);
        digitalWrite(GSM_RST_PIN, LOW);
        digitalWrite(GSM_LOW_PWR_PIN, HIGH);
        digitalWrite(GSM_PWR_PIN, HIGH);

                                //send a pulse to start the module
        digitalWrite(GSM_PWR_PIN, LOW);
        delay(3000);
        digitalWrite(GSM_PWR_PIN, HIGH);

*/

bool ModemClass::init()
{
    if(!_init){
        _uart->begin(_baud > 115200 ? 115200 : _baud);

        send(F("ATV1")); //set verbose mode
        if(waitForResponse() != 1) return false;

        if (!autosense()){
            return false;
        }

        #ifdef GSM_DEBUG
        send(F("AT+CMEE=2"));  // turn on verbose error codes
        #else
        send(F("AT+CMEE=0"));  // turn off error codes
        #endif
        if(waitForResponse() != 1) return false;

        send(F("AT+CIPSPRT=0")); //turn off TCP prompt ">" 
        waitForResponse();
        
        //check if baud can be set higher than default 115200

        if (_baud > 115200){
            sendf("AT+IPR=%ld", _baud);
            if (waitForResponse() != 1){
                return false;
            }
            _uart->end();
            delay(100);
            _uart->begin(_baud);

            if (!autosense()){
                return false;
            }
        }
        _init = true;
    }
    return true;
}

bool ModemClass::restart()
{
    if(_init){
        send(F("AT+RST=1"));
        return (waitForResponse(1000) == 1);
    }
    else{
        init();
    }
}

bool ModemClass::factoryReset()
{
    send(F("AT&FZ&W"));
    return waitForResponse(1000) == 1;
}

bool ModemClass::powerOff()
{
    if(_init){
        _init = false;
        send(F("AT+CPOF"));
        uint8_t stat = waitForResponse();
        _uart->end();
        return stat == 1;
    }
    return true;
}

bool ModemClass::autosense(unsigned int timeout)
{
    for (unsigned long start = millis(); (millis() - start) < timeout;){
        if (noop() == 1){
            return true;
        }
        delay(100);
    }
    return false;
}

bool ModemClass::noop()
{
    send(F("AT"));
    return (waitForResponse() == 1);
}

void ModemClass::lowPowerMode()
{
    _lowPowerMode = true;
    digitalWrite(GSM_LOW_PWR_PIN, LOW);
}

void ModemClass::noLowPowerMode()
{
    _lowPowerMode = false;
    digitalWrite(GSM_LOW_PWR_PIN, HIGH);
}

bool ModemClass::turnEcho(bool on)
{
    sendf("ATE%d", on? 1:0);
    uint8_t resp = waitForResponse();
    if (resp != 1){
        DBG("#DEBUG# setting echo mode failed!");
        return false;
    }
    return true;
}

uint16_t ModemClass::write(uint8_t c)
{
    //make sure to turn off echo, because this is not intended to be used as a send method!
    //so we don't want the modem to echo back c
   return _uart->write(c);
}

uint16_t ModemClass::write(const uint8_t* buf, uint16_t size)
{
    //make sure to turn off echo, because this is not intended to be used as a send method!
    //so we don't want the modem to echo back the content of buffer
    return _uart->write(buf, size);
}

void ModemClass::flush(){
    _uart->flush();
}

void ModemClass::send(const char* command)
{
    /* The chain Command -> Response shall always be respected and a new command must not be issued
    before the module has terminated all the sending of its response result code (whatever it may be).
    This applies especially to applications that ?sense? the OK text and therefore may send the next
    command before the complete code <CR><LF>OK<CR><LF> is sent by the module.
    It is advisable anyway to wait for at least 20ms between the end of the reception of the response and
    the issue of the next AT command.
    If the response codes are disabled and therefore the module does not report any response to the
    command, then at least the 20ms pause time shall be respected.
    */

    if (_lowPowerMode){
        digitalWrite(GSM_LOW_PWR_PIN, HIGH); //turn off low power mode if on
        delay(5);
    }

    unsigned long delta = millis() - _lastResponseOrUrcMillis;
    if(delta < MODEM_MIN_RESPONSE_OR_URC_WAIT_TIME_MS){
        delay(MODEM_MIN_RESPONSE_OR_URC_WAIT_TIME_MS - delta);
    }


    _ready = 0;
	_sent = true;
    _atCommandState = AT_IDLE;
    _uart->println(command);
    _uart->flush();
}

void ModemClass::send(__FlashStringHelper* command)
{
    if (_lowPowerMode){
        digitalWrite(GSM_LOW_PWR_PIN, HIGH); //turn off low power mode if on
        delay(5);
    }

    // compare the time of the last response or URC and ensure
    // at least 20ms have passed before sending a new command
    unsigned long delta = millis() - _lastResponseOrUrcMillis;
    if(delta < MODEM_MIN_RESPONSE_OR_URC_WAIT_TIME_MS){
        delay(MODEM_MIN_RESPONSE_OR_URC_WAIT_TIME_MS - delta);
    }

    _ready = 0;
	_sent = true;
    _atCommandState = AT_IDLE;
    _uart->println(command);
    _uart->flush();
}

void ModemClass::sendf(const char *fmt, ...)
{
    char buf[BUFSIZ];
    va_list ap;
    va_start((ap), (fmt));
    vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
    va_end(ap);
    send(buf);
}

//call this only after send!
int ModemClass::waitForResponse(unsigned long timeout, String* responseDataStorage)
{
    _responseDataStorage = responseDataStorage;
    unsigned long start = millis();
    while ((millis() - start) < timeout){
        uint8_t r = ready();
        if(r != 0) return r;
    }
    //clean up in case timeout occured
    DBG("#DEBUG# response timeout!");
    _responseDataStorage = NULL;
    _ready = 1;
    _atCommandState = AT_IDLE;
	_sent = false;
    _buffer = ""; //clean buffer in case we got some bytes but didn't complete in time
    return -1;
}

uint8_t ModemClass::ready()
{
    poll();
    return _ready;
}

void ModemClass::poll()
{
    //DBG("*** POLL");
    while(_uart->available()){
        char c = _uart->read();
        _buffer += c;
        //DBG("#DEBUG BUFFER#", _buffer);
        //DBG("#DEBUG CHAR#", c);
        switch(_atCommandState){
            default:
            case AT_IDLE:{
                if (_sent && (_buffer.startsWith("AT") || _buffer.startsWith("\r\nAT"))
                            && _buffer.endsWith("\r\n")){ //we use _sent check in case some URC contains the AT string!
                    _atCommandState = AT_RECV_RESP;
                    _buffer.trim();
                    DBG("#DEBUG# command sent: \"", _buffer, "\"");
                    _buffer = "";
                    _sent = false;
                    break;
                }
                else{
                    switch(_urcState){
                        default:
                        case URC_IDLE:{
                            checkUrc();
                            break;
                        }
                        case URC_RECV_SOCK_CHUNK:{
                            _buffer = "";
                            //send to correct socket!
                            _sockets[_sock]->handleUrc(&c, 1);
                            if(--_chunkLen <= 0){
                                //done receiving chunk
                                _lastResponseOrUrcMillis = millis();
                                _urcState = URC_IDLE;
                                _ready = 1;
                                bool skip = streamSkipUntil('\n');
                                #ifdef GSM_DEBUG
                                if (!skip){
                                    DBG("#DEBUG# TCP missing END mark!");
                                }
                                #endif
                            }
                            break;
                        }
                    }
                }
                break;
            }
            case AT_RECV_RESP:{
                int responseResultIndex;
                #ifdef GSM_DEBUG
                    String error;
                    String response;
                #endif
                if (_buffer.endsWith(GSM_OK)){
                    _ready = 1;
                    #ifdef GSM_DEBUG
                        response = _buffer;
                    #endif
                }
                else if (_buffer.endsWith(GSM_ERROR)){
                    _ready = 2;
                    #ifdef GSM_DEBUG
                        response = _buffer;
                    #endif
                }
                #ifdef GSM_DEBUG
                else if (_buffer.endsWith(GSM_CME_ERROR)){
                    streamSkipUntil('\n', &error);
                    _ready = 3;
                    response = GSM_CME_ERROR + error; 
                }
                else if (_buffer.endsWith(GSM_CMS_ERROR)){
                    streamSkipUntil('\n', &error);
                    _ready = 4;
                    response = GSM_CMS_ERROR + error; 
                }
                #endif
                if (_ready != 0){ 
                    _lastResponseOrUrcMillis = millis();
                    if (_lowPowerMode){ //after receiving the response, bring back low power mode if it were on
                        digitalWrite(GSM_LOW_PWR_PIN, LOW);
                    }
                    #ifdef GSM_DEBUG
                    response.trim();
                    DBG("#DEBUG# response received: \"", response, "\"");
                    #endif
                    if (_responseDataStorage != NULL){
                        _buffer.trim();
                        *_responseDataStorage = _buffer;
                    }
                    _buffer = ""; 
                    _responseDataStorage = NULL;
                    _atCommandState = AT_IDLE;
                    break;
                }
            }
        } //end switch _atCommandState
    } //end while
}

void ModemClass::checkUrc()
{
    //############################################################################ +CIPRCV
    if (_buffer.endsWith("+CIPRCV,")){
        _sock = streamGetIntBefore(',');
        _chunkLen = streamGetIntBefore(':');
        _urcState = URC_RECV_SOCK_CHUNK;
        _ready = 0;
        _buffer = "";
    }
    //############################################################################ UNHANDLED
    else if(_buffer.endsWith("\r\n") && _buffer.length() > 2){
        _lastResponseOrUrcMillis = millis();
        #ifdef GSM_DEBUG
        //can get URC not starting with \r\n+ but only with +
        if (_buffer.startsWith("+") || _buffer.startsWith("\r\n+")){
            //DBG("#DEBUG# sent: ", _sent);
            _buffer.trim();
            DBG("#DEBUG# unhandled URC received: \"", _buffer, "\"");
        }
        else {
           // DBG("#DEBUG# sent: ", _sent);
           _buffer.trim();
           DBG("#DEBUG# unhandled data: \"", _buffer, "\"");
        }
        #endif
        _buffer = "";
    }
    //############################################################################
}

inline int16_t ModemClass::streamGetIntBefore(const char& lastChar)
{
    char buf[7];
    int16_t bytesRead = _uart->readBytesUntil(lastChar, buf, 7);
    if (bytesRead && bytesRead < 7) {
        buf[bytesRead] = '\0';
        int16_t res = atoi(buf);
        return res;
    }
    return -999;
}

inline bool ModemClass::streamSkipUntil(const char& c, String* save, const uint32_t timeout_ms)
{
    uint32_t startMillis = millis();
    while (millis() - startMillis < timeout_ms){
        while (_uart->available()){
            char r = _uart->read();
            //DBG("#DEBUG#", r);
            if (save != NULL) *save += r; 
            if (r == c) return true;
        }
    }
    return false;
}

void ModemClass::setBaudRate(unsigned long baud)
{
    _baud = baud;
}

void ModemClass::addUrcHandler(ModemUrcHandler* handler)
{
    for (int i = 0; i < MAX_URC_HANDLERS; i++) {
        if (_urcHandlers[i] == NULL) {
            _urcHandlers[i] = handler;
            break;
        }
    }
}

void ModemClass::removeUrcHandler(ModemUrcHandler* handler)
{
    for (int i = 0; i < MAX_URC_HANDLERS; i++) {
        if (_urcHandlers[i] == handler) {
            _urcHandlers[i] = NULL;
            break;
        }
    }
}

ModemClass MODEM(Serial1, 115200);

uint8_t GSM_PWR_PIN = 9;
uint8_t GSM_RST_PIN = 6;
uint8_t GSM_LOW_PWR_PIN = 5;
