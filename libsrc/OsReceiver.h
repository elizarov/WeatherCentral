//============================================================================
//Weather Station Data Logger : Weather Shield for Arduino
//Copyright © 2010, Weber Anderson
// 
//This application is free software; you can redistribute it and/or
//modify it under the terms of the GNU Lesser General Public
//License as published by the Free Software Foundation; either
//version 3 of the License, or (at your option) any later version.
//
//This application is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR When PARTICULAR PURPOSE. See the GNU
//Lesser General Public License for more details.
//
//You should have received a copy of the GNU Lesser General Public
//License along with this library; if not, see <http://www.gnu.org/licenses/>
//
//=============================================================================
//
// Hardware connections and notes:
//
// Connect the data line from the receiver to Port B0 (Digital 8 on the Duemilanove).
// Optionally, an LED may be connected to Port D6 (Digital 6 on the Duemilanove).
// The receiver may be powered from the Arduino's 5V supply and works with the 5V logic
// levels of the Duemilanove  -- level translation is not required.
//

#ifndef OsReceiver_h
#define OsReciever_h

#include "WProgram.h"

class OsRx
{
public:
  OsRx();

  void init();
  boolean data_available();
  byte get_data(byte *buffer, byte length, byte *protocol);

private:
  //
  // these used to detect version 2.1 protocol repeated packets
  // so one of them can be discarded
  //
  byte previous_packet[64];
  unsigned long previous_packet_time;

  boolean ValidChecksum(byte *packet, int Pos);

};

extern OsRx OsReceiver;

#endif
