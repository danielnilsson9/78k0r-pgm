# 78k0r-pgm
Arduino based Renesas 78K0R microcontroller programmer.

## Usage
You will need an Arduino board with > 1 Serial port to run this flashing firmware.
Arduino Micro was used during the development of this project which has one physical UART and another virtual port on the USB connection.

The physical port (Serial1) is used for communication with the MCU.
The virtual port (Serial) is used for communication with the computer (flash tool).

You need to make the following connections to the Renesas MCU:

Arduino | Renesas MCU | Comment
------- | ----------- | -------
RX      | TOOL0       | Single wire UART
TX      | TOOL0       | Single wire UART
D9      | RESET       |
D10     | FLMD        | 
GND     | GND         |
+5V     | +5V         | Only if Renesas MCU is not powered by other circuit

You can change the pins in firmware/Config.h if needed.

Initiate flash operation using:
```
78k0r-pgm.exe --com COM7 --input program.hex
```


## Disclaimer
I do not know if this flash tool is compatible with the modern Renesas 78K0R series MCU:s, if not, then minor adjustments may be needed.

This programmer has only been tested on NEC D79F9211 which I suspect is equivalent to Renesas UPD78F1000. I don't know if I have an original NEC MCU or if it's some  form of clone. A few commands such as baudrate change and silicone signature does not return the expected response. Silicon signature has a few extra bytes compared to the specification document and the baudrate change command does not work at all causing the programmering interface to be stuck at 9600 buad (which is good enough).
