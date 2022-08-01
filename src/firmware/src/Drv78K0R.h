#include <Arduino.h>

class Drv78K0R
{
public:
    enum Result 
    {
        ERROR_NONE = 0x06,
        ERROR_TIMEOUT = 0x01,
        ERROR_INVALID_COMMAND = 0x04,
        ERROR_INVALID_PARAMETER = 0x05,
        ERROR_CHECKSUM = 0x07,
        ERROR_VERIFY = 0x0f,
        ERROR_PROTECTED = 0x10,
        ERROR_NOT_ACCEPTED = 0x15,
        ERROR_ERASE_VERIFY = 0x1a,
        ERROR_INTERNAL_VERIFY = 0x1b,
        ERROR_WRITE = 0x1c,
        ERROR_BUSY = 0xff     
    };

    Drv78K0R(int pinFLMD, int pinReset);

    Result begin();
    void end();

    Result eraseFlash();

    Result beginWrite(uint32_t startAddress, uint32_t len);
    Result writeFlash(uint8_t* data, uint8_t len);
    Result endWrite();
    
private:
    struct Result2
    {
        Result a;
        Result b;
    };

    void sendCmd(const uint8_t* buffer, uint8_t length, bool end = true);

    Result readStatusResponse();
    Result2 readStatusResponse2();
    
    Result readSiliconSignatureResponse();

    uint8_t readResponse();

private:
    int _pinFLMD;
    int _pinReset;
    uint8_t _msgbuf[64];

    uint32_t _endAddress = 0x00;
    uint32_t _bytesWritten = 0x00;
};