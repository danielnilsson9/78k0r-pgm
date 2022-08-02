#include <Arduino.h>
#include "Drv78K0R.h"

#define STX             0xf1
#define ETX             0xf2

// CMD                             LEN
//                         .-------------------.
// ---------------------------------------------------
// | STX | LEN(H) | LEN(L) | CMD |    DATA     | ETX |
// ---------------------------------------------------

#define CMD_INIT        0x01
#define CMD_RESET       0x02
#define CMD_ERASE       0x03

// ---------------------------------
// | START_ADDR(3B) | END_ADDR(3B) |
// ---------------------------------
#define CMD_WRITE_BEGIN 0x04

// Max 256 bytes
// --------
// | DATA | 
// --------
#define CMD_WRITE_DATA  0x05

#define CMD_WRITE_END   0x06


// RESP                      LEN
//                         .------.
// --------------------------------------------
// | STX | LEN(H) | LEN(L) | RSP | DATA | ETX |
// --------------------------------------------

// ----------
// | STATUS |
// ----------
#define RSP_STATUS      0x11

// -----------------------------------------
// | STATUS | DEVICE(10B) | FLASH_SIZE(3B) |
// -----------------------------------------
#define RSP_INIT        0x12


#define STATUS_OK                       0x00
#define STATUS_ERROR_TIMEOUT            0x01
#define STATUS_ERROR_ERASE              0x02
#define STATUS_ERROR_WRITE              0x03
#define STATUS_ERROR_VERIFY             0x04
#define STATUS_ERROR_INVALID_PARAM      0x05


#define PIN_RESET       9
#define PIN_FLMD        10

Drv78K0R drv78k0r(PIN_FLMD, PIN_RESET);
uint8_t rxbuf[300];

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);

    Serial.begin(115200);
    while(!Serial);
}


uint8_t receiveCmd()
{
    uint32_t lastRecv = millis();

    uint8_t i = 0;
    uint16_t len = 0;
    bool err = false;

    while(i < (len + 4) && millis() - lastRecv < 1000)
    {
        int b = Serial.read();

        if (!err && b != -1)
        {
            lastRecv = millis();

            if (i == 0 && b != STX)
            {
                err = true;
            }
            else if (i == 1)
            {
                len = (((uint16_t)b) << 8);
            }
            else if (i == 2)
            {
                len |= (uint8_t)b;
            }
            else if (i == (len + 3) && b != ETX)
            {              
                err = true;
            }

            rxbuf[i++] = (uint8_t)b;
        }
    }

    if (err)
    {
        return 0;
    }
    else if (i < (len + 4))
    {
        return 0;
    }
    else
    {
        return rxbuf[3];
    }
}

void writeResponse(uint8_t* data, uint16_t len)
{
    Serial.write(STX);
    Serial.write((uint8_t)(len >> 8));
    Serial.write((uint8_t)len);
    Serial.write(data, len);
    Serial.write(ETX);

    Serial.flush();
}

void writeStatusResponse(uint8_t err)
{
    uint8_t resp[] = { RSP_STATUS, err };
    writeResponse(resp, sizeof(resp));
}

void writeInitResponse(uint8_t err)
{
    uint8_t resp[15];
    resp[0] = RSP_INIT;
    resp[1] = err;

    memcpy(&resp[2], drv78k0r.getDeviceId(), Drv78K0R::DEVICE_ID_LENGTH);

    resp[12] = (uint8_t)(drv78k0r.getFlashSize() >> 16);
    resp[13] = (uint8_t)(drv78k0r.getFlashSize() >> 8);
    resp[14] = (uint8_t)(drv78k0r.getFlashSize());

    writeResponse(resp, sizeof(resp));
}

void processInitCmd()
{
    Drv78K0R::Result res = drv78k0r.begin();

    if (res == Drv78K0R::ERROR_NONE)
    {
        writeInitResponse(STATUS_OK);     
    } 
    else
    {
        writeInitResponse(STATUS_ERROR_TIMEOUT);
    }
}

void processResetCmd()
{
    drv78k0r.end();
    writeStatusResponse(STATUS_OK);
}

void processEraseCmd()
{
    Drv78K0R::Result res = drv78k0r.eraseFlash();
    if (res == Drv78K0R::ERROR_NONE)
    {
        writeStatusResponse(STATUS_OK);      
    }
    else
    {
        writeStatusResponse(STATUS_ERROR_ERASE);
    }
}

void processWriteBeginCmd()
{
    uint32_t startAddr = ((uint32_t)rxbuf[4] << 16) | ((uint32_t)rxbuf[5] << 8) | rxbuf[6];
    uint32_t endAddr = ((uint32_t)rxbuf[7] << 16) | ((uint32_t)rxbuf[8] << 8) | rxbuf[9];

    Drv78K0R::Result res;
    
    res = drv78k0r.beginWrite(startAddr, endAddr);
    if (res != Drv78K0R::ERROR_NONE)
    {
       writeStatusResponse(res);
    }
    else
    {
        writeStatusResponse(STATUS_OK);
    }
}

void processWriteDataCmd()
{
    uint16_t len = (((uint16_t)rxbuf[1] << 8) | rxbuf[2]) - 1;

    Drv78K0R::Result res = drv78k0r.writeFlash(&rxbuf[4], len);
    if (res != Drv78K0R::ERROR_NONE)
    {
        writeStatusResponse(res);
    }
    else
    {
        writeStatusResponse(STATUS_OK);  
    }
}

void processWriteEndCmd()
{
    Drv78K0R::Result res = drv78k0r.endWrite();
    if (res != Drv78K0R::ERROR_NONE)
    {
        writeStatusResponse(STATUS_ERROR_VERIFY);
    }
    else
    {
        writeStatusResponse(STATUS_OK);
    }
}

void loop()
{
    uint8_t cmd = receiveCmd();
    switch (cmd)
    {
    case CMD_INIT:
        processInitCmd();
        break;
    case CMD_RESET:
        processResetCmd();
        break;
    case CMD_ERASE:
        processEraseCmd();
        break;
    case CMD_WRITE_BEGIN:
        processWriteBeginCmd();
        break;
    case CMD_WRITE_DATA:
        processWriteDataCmd();
        break;
    case CMD_WRITE_END:
        processWriteEndCmd();
        break;
    default:
        break;
    }
}