/*
BBC micro:bit VGM player

Version: 0.1 NK 10-Feb-2018

Copyright 2018 N Kendrick (nigel-dot-kendrick-at-gmail-dotcom)

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the Software
is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

Inspired by Arduino code produced by Artkasser (https://github.com/artkasser)

This code plays BBC micro (SN76489) VGM sound files on a BBC micro:bit.

Note that in this code version, the VGM data is stored as a byte array file
(minus the header info), and not read-in from anywhere. Maybe later!?

External circuitry is needed to make this work:

1 x BBC micro:bit breakout board (eg: Kitronik) to get to the required pins
1 x SN74HC595 shift register IC - easy to obtain
1 x SN76489 programmable sound generator (PSG) IC (From a reputable[!]
    supplier. You can get them on Ebay and elsewhere but beware of fakes.
2 x 0.1uF ceramic capacitors
1 x 4MHz oscillator module - the connections list below assumes a '14-pin'
    unit - such as something similar to QX14T50B4.000000B50TT.
    Note, you want a 4MHz crystal MODULE, not a 4MHz crystal.
1 x Breadboard to build the circuit, unless there is room on the breakout board
    or you use perfboard/stripboard or make your own PCB etc.
2 x 16-pin IC sockets (optional, and not needed for a breadboard)
1 x audio amplifier or lead to connect PSG audio out to a set of active
    speakers, you could instead connect a pieze element (not a buzzer) to the
    audio out pin of the PSG. If connecting to a piezo sounder, you'll need a
    1K resistor.
1 x Assorted wiring

Connections:

    The micro:bit takes power as usual from the connected micro USB cable.

    Everything else i powered from a separate 5V supply, such as a USB phone
    charger. You *could* try powering these parts from the 3V connection on
    the micro:bit - it might work, but the PSG and the oscillator module are
    officially 5V parts so YMMV.

    Remember to ensure that the micro:bit's 0V/GND line is connected to the
    GND line of all the other parts to ensure proper circuit operation. DO NOT
    connect the 5V power line to the micro:bit's 3V line as you'll break
    something!

    Hookup overview:

    From                 Function      Connect to
    micro:bit P15          MOSI     74HC595 pin 14 (SER)
    micro:bit P13          CE       74HC595 pin 12 (RCLK)
    micro:bit P2           SER      74HC595 pin 11 (SRCLK)
    micro:bit P8           PSG ~WE  SN76489 pin 5 (~WE)
    74HC595 Data out       Data bus PSG Data lines - Note that the PSG data
                                    lines are labelled in REVERSE, so the
                                    wiring between the 595 and the 76489 should
                                    be 74HC595 D0 --- D7 SN76489
                                       74HC595 D1 --- D6 SN76489 etc
    Oscillator P7          GND      To common ground with all other GND pins
    Oscillator P8          CLK      SN76489 pin 16
    Audio out              AUDIO    SN76489 pin 7 to an external amplifier,
                                    or a piezo sounder through a 1K resistor
    +5V                    VCC      Both ICs, pin 16, Oscillator pin 14; all
                                    active components EXCEPT the micro:bit
    micro:bit GND          GND      connect to the common GND on the external
                                    parts

For stability, connect 0.1uF capacitors between VCC and GND near the both the
74HC595 and SN76489 chips.

Double-check all wiring before powering up anything!

*/

#include "MicroBit.h"
#include "nkpins.h"

//VGM file as a byte array with header info removed...
#include "sonic.h"

MicroBit uBit;
MicroBitDisplay display;

const uint16_t SampleTime = 23; //VGM format = 44,100Hz sampling = 23uS/sample

// General program variables

uint16_t Samples = 0;
uint16_t vgmpos = 0;

// Microbit SPI port setup
// Using the default SPI pins defined in the micro:bit docs...
SPI spi(mbit_p15, mbit_p14, mbit_p13); // mosi, miso (not used), sclk
DigitalOut cs(mbit_p1);     //Chip select pin for the shift register
DigitalOut PSG_WE(mbit_p8); //~WE pin for PSG

void PutByte (uint8_t b)
// Write data to the shift register via SPI interface
{
    cs = 1;
    spi.write(b);
    cs = 0;
}

void SendByte(uint8_t b)
{
    PSG_WE = 1;
    PutByte(b);
    PSG_WE = 0;
    wait_us(SampleTime);
    PSG_WE = 1;
}

void SilenceAllChannels()
{
    SendByte(0x9f);
    SendByte(0xbf);
    SendByte(0xdf);
    SendByte(0xff);
}

void Playloop()
//Process the VGM data
{

    bool runstop = false; 
    // True when we get to the 'end of data' value in the array. 
    // There's no current trap for if the last byte in the array isn't 0x66
    
    do {
        uint8_t vgmdata = (*(VGMDataArray + vgmpos));

        // Process VGM codes.

        /* Some ranges are reserved for future use, with different numbers of
           operands:

           0x30..0x3F dd          : one operand, reserved for future use
                                 Note: used for dual-chip support
           0x40..0x4E dd dd       : two operands, reserved for future use
                                 Note: was one operand only til v1.60
           0xA1..0xAF dd dd       : two operands, reserved for future use
                                 Note: used for dual-chip support
           0xBC..0xBF dd dd       : two operands, reserved for future use
           0xC5..0xCF dd dd dd    : three operands, reserved for future use
           0xD5..0xDF dd dd dd    : three operands, reserved for future use
           0xE1..0xFF dd dd dd dd : four operands, reserved for future use

           This programming does NOT currently cater for those codes
          */

        if ((vgmdata & 0xF0) == 0x70) {
            // 0x7n : wait n+1 samples, n can range from 0 to 15
            //Samples = 1;
            vgmpos++;
            wait_us(((vgmdata & 0x0F)+1) * SampleTime);
        } else {
            switch (vgmdata) {
                case 0x50: // 0x50 dd : PSG (SN76489/SN76496) write value dd
                    vgmpos++;
                    vgmdata = (*(VGMDataArray + vgmpos));
                    SendByte(vgmdata);
                    vgmpos++;
                    break;

                case 0x61: // 0x61 nn nn : Wait n samples, n can range from 0 to 65535
                    vgmpos++;
                    Samples = (uint16_t)( (*(VGMDataArray + vgmpos)) & 0x00FF );
                    vgmpos++;
                    Samples |= (uint16_t)(( (*(VGMDataArray + vgmpos)) << 8) & 0xFF00 );
                    vgmpos++;
                    wait_us(Samples * SampleTime);
                    break;

                case 0x62: // wait 735 samples (60th of a second)
                    vgmpos++;
                    wait_ms(17);
                    break;

                case 0x63: // wait 882 samples (50th of a second)
                    vgmpos++;
                    wait_ms(20);
                    break;

                case 0x66: // 0x66 : end of sound data
                    vgmpos = 0;
                    SilenceAllChannels();
                    wait_ms(2000);
                    runstop = true;
                    break;

                default:
                    break;
            } //end switch

        } // end else


    } while (!runstop);
}
int main()
{
    // Initialise the micro:bit runtime.
    uBit.init();

    //SPI setup
    // Chip must be deselected
    cs = 1;

    // Setup the spi for 8 bit data, high steady state clock,
    // second edge capture, with a 1MHz clock rate (microbit and shift
    // register can handle it - tested).
    spi.format(8,3);
    spi.frequency(1000000);

    // Silence the PSG

    PSG_WE = 1;
    SilenceAllChannels();
    wait_ms(500);

    //Play music
    Playloop();
    //Mute the last tone
    SilenceAllChannels();

    display.scroll("** DONE ** :)");

    //Tidy up and we're done
    release_fiber();

}

