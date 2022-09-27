/*

Copyright (c) 2022 Paul Anderson

This work would not have been possible without the excellent NMEA 2000 library produced
by Timo Lappalainen

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <Arduino.h>
#include <N2kMessages.h>
#include <N2kMsg.h>
#include <NMEA2000.h>
#include <NMEA2000_CAN.h>


// Defined constants

#define CzUpdatePeriod127501 10000
#define BinaryDeviceInstance 0x00  // Instance of 127501 switch state message
#define NumberOfSwitches 8  // change to 4 for bit switch bank

// Function prototypes

void HandleNMEA2000Msg(const tN2kMsg& N2kMsg);
void N2kSendUpdates(void);
void N2kChangeOutputState(uint8_t, bool);
void N2kSendCurrentState127501(unsigned char);  // N2K compatability
void SetN2kSwitchBankCommand(tN2kMsg&, unsigned char, tN2kBinaryStatus);
void N2kParsePGN127502(const tN2kMsg&);

typedef struct {
  unsigned long PGN;
  void (*Handler)(const tN2kMsg& N2kMsg);
} tNMEA2000Handler;

tNMEA2000Handler NMEA2000Handlers[] = {{127502L, N2kParsePGN127502}, {0, 0}};
const unsigned long TransmitMessages[] PROGMEM = {127501L, 127502L, 0};

//  Global variables

uint8_t N2kRelayGpioMap[] = {
    13, 14, 15, 16, 17,
    18, 19, 20};  // arduino pins driving relays i.e CzRelayPinMap[0] returns
                  // the pin number of Relay 1

tN2kBinaryStatus n2KSwitchState = 0;

void setup() {
#ifdef Debug
  Serial.begin(115200);
  delay(500);
  Serial.println("Starting Up CzSwitches");
#endif

  // sets the digital relay driver pins as output
  for (uint8_t i = 0; i < NumberOfSwitches; i++)
    pinMode(N2kRelayGpioMap[i], OUTPUT);

  // setup the N2k parameters

  NMEA2000.SetN2kCANSendFrameBufSize(150);
  NMEA2000.SetN2kCANReceiveFrameBufSize(150);
  // Set Product information
  NMEA2000.SetProductInformation("00260001", 0001, "Switch Bank",
                                 "1.000 27/09/22", "8 Bit N2K Relay Control");
  // Set device information
  NMEA2000.SetDeviceInformation(260001, 140, 30, 717);
  NMEA2000.SetMode(tNMEA2000::N2km_ListenAndNode, 169);
  NMEA2000.ExtendTransmitMessages(TransmitMessages);
  NMEA2000.SetMsgHandler(HandleNMEA2000Msg);
  NMEA2000.Open();
  // Send initial state to any listerning devices
  N2kSendCurrentState127501(BinaryDeviceInstance);
}

void loop() {
  NMEA2000.ParseMessages();
  N2kSendUpdates();
}

//*****************************************************************************
// send periodic updates to maintain sync and as a "heatbeat" to the MFD
//*****************************************************************************

void N2kSendUpdates(void) {
  static unsigned long N2kUpdate127501 = millis();
  if (N2kUpdate127501 + CzUpdatePeriod127501 < millis()) {
    N2kUpdate127501 = millis();
    N2kSendCurrentState127501(BinaryDeviceInstance);
  }
}

//*****************************************************************************
// NMEA 2000 message handler
//*****************************************************************************

void HandleNMEA2000Msg(const tN2kMsg& N2kMsg) {
  int iHandler;
  for (iHandler = 0; NMEA2000Handlers[iHandler].PGN != 0 &&
                     !(N2kMsg.PGN == NMEA2000Handlers[iHandler].PGN);
       iHandler++);
  if (NMEA2000Handlers[iHandler].PGN != 0) {
    NMEA2000Handlers[iHandler].Handler(N2kMsg);
  }
}

//*****************************************************************************
// Transmit the current state to the bus perodically and "on change of state"
//*****************************************************************************

void N2kSendCurrentState127501(unsigned char DeviceInstance) {
  tN2kMsg N2kMsg;
  tN2kBinaryStatus bankStatus;
  N2kResetBinaryStatus(bankStatus);
  bankStatus = bankStatus & n2KSwitchState;
  bankStatus = bankStatus | 0xffffffffffff0000ULL; // set unused bits to "unavailable"
  SetN2kPGN127501(N2kMsg, DeviceInstance, bankStatus);
  NMEA2000.SendMsg(N2kMsg);
}

//*****************************************************************************
// Change the state of the relay requested 
//*****************************************************************************

void N2kChangeOutputState(uint8_t SwitchIndex, bool ItemStatus) {
  if (SwitchIndex < NumberOfSwitches) {
       digitalWrite(N2kRelayGpioMap[SwitchIndex], ItemStatus);
  }
}

//*****************************************************************************
// A Switch control message has arrived, parse and make state change
//*****************************************************************************

void N2kParsePGN127502(const tN2kMsg& N2kMsg) {
  tN2kOnOff State;
  tN2kBinaryStatus mask = 1;
  int ChangeIndex, Index = 0;

  uint8_t DeviceBankInstance = N2kMsg.GetByte(Index);

  if (N2kMsg.PGN != 127502L && DeviceBankInstance != BinaryDeviceInstance)
    return;  // Nothing to see here, 127502 not meant for this switch bank
  uint16_t BankStatus = N2kMsg.Get4ByteUInt(Index);

  for (ChangeIndex = 0; ChangeIndex < NumberOfSwitches; ChangeIndex++) {
    State = (tN2kOnOff)(BankStatus & 0x03);
    BankStatus >>= 2;
    if (State != N2kOnOff_Unavailable) {
      n2KSwitchState ^= mask;
      N2kChangeOutputState(ChangeIndex, State);  // send the change out
    }
    mask <<= 2;
  }
  N2kSendCurrentState127501(BinaryDeviceInstance);
}
