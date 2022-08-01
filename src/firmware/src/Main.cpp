#include <Arduino.h>
#include "Drv78K0R.h"

#define STX             0xf1
#define ETX             0xf2

//                                 LEN
//                         .-------------------.
// ---------------------------------------------------
// | STX | LEN(H) | LEN(L) | CMD |    DATA     | ETX |
// ---------------------------------------------------
#define CMD_INIT        0x01
#define CMD_RESET       0x02
#define CMD_ERASE       0x03
// --------------------------------------------
// | ADDR(H) | ADDR(M) | ADDR(L) | LEN | DATA | 
// --------------------------------------------
#define CMD_WRITE       0x04

// ----------------------
// | STX | STATUS | ETX |
// ----------------------
#define RESP_STATUS     0x16

#define ERROR_OK        0x00
#define ERROR_TIMEOUT   0x01
#define ERROR_ERASE     0x02
#define ERROR_WRITE     0x03
#define ERROR_VERIFY    0x04


#define PIN_RESET   9
#define PIN_FLMD    10

Drv78K0R drv78k0r(PIN_FLMD, PIN_RESET);
uint8_t rxbuf[300];

void setup()
{
    SerialUSB.begin(115200);
    while(!SerialUSB);
}


uint8_t receiveCmd()
{
    uint32_t now = millis();

    uint8_t i = 0;
    uint16_t len = sizeof(rxbuf);
    bool err = false;
    while(i < (len + 4) && millis() - now < 1000)
    {
        int b = Serial1.read();
        if (!err && b != -1)
        {
            if (i == 0 && b != STX)
            {
                err = true;
            }
            else if (i == 1)
            {
                len |= (((uint16_t)b) << 8);
            }
            else if (i == 2)
            {
                len |= b;
            }
            else if (i == (len + 3))
            {
                if (b != ETX)
                {
                    err = true;
                }
            }

            rxbuf[i++] = b;
        }
    }

    if (err)
    {
        return 0;
    }

    return rxbuf[3];
}

void writeStatus(uint8_t err)
{
    uint8_t resp[] = { STX, err, ETX };
    SerialUSB.write(resp, sizeof(resp));
}

void processInitCmd()
{
    Drv78K0R::Result res = drv78k0r.begin();

    if (res == Drv78K0R::ERROR_NONE)
    {
        writeStatus(ERROR_OK);     
    } 
    else
    {
        writeStatus(ERROR_TIMEOUT);
    }
}

void processResetCmd()
{
    drv78k0r.end();
    writeStatus(ERROR_OK);
}

void processEraseCmd()
{
    Drv78K0R::Result res = drv78k0r.eraseFlash();
    if (res == Drv78K0R::ERROR_NONE)
    {
        writeStatus(ERROR_OK);      
    }
    else
    {
        writeStatus(ERROR_ERASE);
    }
}

void processWriteCmd()
{
    uint16_t addr = ((uint16_t)rxbuf[4]) << 16 | ((uint16_t)rxbuf[5] << 8) | rxbuf[6];
    uint8_t len = rxbuf[7];

    Drv78K0R::Result res = drv78k0r.beginWrite(addr, len);

    if (res != Drv78K0R::ERROR_NONE)
    {
       writeStatus(ERROR_WRITE);
       return;
    }

    if (drv78k0r.writeFlash(&rxbuf[8], len) != Drv78K0R::ERROR_NONE)
    {
        writeStatus(ERROR_WRITE);
        return;
    }

    if (drv78k0r.endWrite() != Drv78K0R::ERROR_NONE)
    {
        writeStatus(ERROR_VERIFY);
        return;
    }

    writeStatus(ERROR_OK);
}

void loop()
{
    switch (receiveCmd())
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
    case CMD_WRITE:
        processWriteCmd();
        break;
    default:
        break;
    }
}