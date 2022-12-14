#ifndef DRV78K0R_H
#define DRV78K0R_H

#include <Arduino.h>

class Drv78K0R
{
public:
    const static uint8_t DEVICE_ID_LENGTH = 10;

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

    Drv78K0R(HardwareSerial& serial, int pinRESET, int pinFLMD);

    Result begin();
    void end();

    uint32_t getFlashSize() const;
    const char* getDeviceId() const;

    Result eraseFlash();

    Result beginWrite(uint32_t startAddress, uint32_t endAddress);
    Result writeFlash(uint8_t* data, uint16_t len);
    Result endWrite();
 
private:
    void sendCmd(const uint8_t* buffer, uint8_t length);

    Result readStatusResponse(uint32_t timeout_ms);
    Result readStatusResponse2(uint32_t timeout_ms);
    Result readSiliconSignatureResponse();
    uint8_t readResponse(uint32_t timeout_ms);

    void flush();

private:
    HardwareSerial& _serial;
    int _pinReset;
    int _pinFlmd;

    uint8_t _msgbuf[64];

    char _deviceId[DEVICE_ID_LENGTH];
    uint32_t _flashSize = 0;
    int32_t _blockToWrite = 0;
    int32_t _bytesLeftToWrite = 0;
};

#endif
