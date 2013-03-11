#include <nds.h>
#include <dswifi9.h>
#include "nifi.h"
#include "mmu.h"
#include "main.h"
#include "gameboy.h"
#include "gbcpu.h"
#include "console.h"

volatile int packetData=-1;
volatile int sendData;
volatile bool transferWaiting = false;

bool nifiEnabled=true;

volatile bool readyToSend=true;

u8 lastSendid = 0xff;

void packetHandler(int packetID, int readlength)
{
    if (isConsoleEnabled())
        return;
    static char data[4096];
    static int bytesRead;

    // Wifi_RxRawReadPacket:  Allows user code to read a packet from within the WifiPacketHandler function
    //  long packetID:		a non-unique identifier which locates the packet specified in the internal buffer
    //  long readlength:		number of bytes to read (actually reads (number+1)&~1 bytes)
    //  unsigned short * data:	location for the data to be read into
    bytesRead = Wifi_RxRawReadPacket(packetID, readlength, (unsigned short *)data);

    // Check this is the right kind of packet
    if (data[32] == 'Y' && data[33] == 'O') {
        u8 command = data[34];
        u8 val = data[35];
        u8 sendid = data[36];

        if (lastSendid == sendid)
            return;
        lastSendid = sendid;

        if (command == 55 || command == 56) {
            printLog("%d: Received %x\n", ioRam[0x02]&1, val);
        }

        //packetData = 0;
        switch(command) {
            // Command sent from "internal clock"
            case 55:
                if (ioRam[0x02] & 0x80) {
                    // Falls through to case 56
                }
                else {
                    printLog("Not ready!\n");
                    transferWaiting = true;
                    break;
                }
                // Internal clock receives a response from external clock
            case 56:
                packetData = val;
                cyclesToExecute = 0;
                break;
            default:
                //printLog("Unknown packet\n");
                break;
        }
    }
}



void enableNifi()
{
	Wifi_InitDefault(false);

// Wifi_SetPromiscuousMode: Allows the DS to enter or leave a "promsicuous" mode, in which 
//   all data that can be received is forwarded to the arm9 for user processing.
//   Best used with Wifi_RawSetPacketHandler, to allow user code to use the data
//   (well, the lib won't use 'em, so they're just wasting CPU otherwise.)
//  int enable:  0 to disable promiscuous mode, nonzero to engage
	Wifi_SetPromiscuousMode(1);

// Wifi_EnableWifi: Instructs the ARM7 to go into a basic "active" mode, not actually
//   associated to an AP, but actively receiving and potentially transmitting
	Wifi_EnableWifi();

// Wifi_RawSetPacketHandler: Set a handler to process all raw incoming packets
//  WifiPacketHandler wphfunc:  Pointer to packet handler (see WifiPacketHandler definition for more info)
	Wifi_RawSetPacketHandler(packetHandler);

// Wifi_SetChannel: If the wifi system is not connected or connecting to an access point, instruct
//   the chipset to change channel
//  int channel: the channel to change to, in the range of 1-13
	Wifi_SetChannel(10);

    transferWaiting = false;
    nifiEnabled = true;
}

void disableNifi() {
    Wifi_DisableWifi();
    nifiEnabled = false;
}


u8 sendid = 0;
void sendPacketByte(u8 command, u8 data)
{
    if (!nifiEnabled || isConsoleEnabled())
        return;
    unsigned char buffer[6];
    buffer[0] = 'Y';
    buffer[1] = 'O';
    buffer[2] = command;
    buffer[3] = data;
    buffer[4] = sendid++;
    printLog("%d: Sent %x\n", ioRam[0x02]&1, data);
    if (Wifi_RawTxFrame(6, 0x0014, (unsigned short *)buffer) != 0)
        printLog("Nifi send error\n");
    Wifi_RawTxFrame(6, 0x0014, (unsigned short *)buffer);
    Wifi_RawTxFrame(6, 0x0014, (unsigned short *)buffer);
}
