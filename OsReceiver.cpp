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
// set this to "1" to disable checksum verification
// sometimes useful for analyzing messages from new sensors
// or new protocols.
//
#define NO_VERIFY_CHECKSUMS 0

#include "OsReceiver.h"

extern "C" {
#include "osrx.h"
}

const unsigned long mm_diff = 0x7fffffffUL; 

#if ENABLE_DEBUG_PRINT
const char hexChars[16] = { 
  '0', '1', '2', '3', '4', '5', '6', '7',
  '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'   };
#endif

  OsRx::OsRx()
  {

  }

  void OsRx::init()
  {
    osrx_init();
  }

  boolean OsRx::data_available()
  {
    return osrx_data_available();
  }

  byte OsRx::get_data(byte *packet, byte length, byte *protocol)
  {
    if (!osrx_data_available()) return 0;

    byte duplicateIndex = 0;
    byte duplicateLength = 0;
    byte protocol_version;
    byte msgLen = get_osrx_data(packet, length, &protocol_version);
    start_receiving();

    //
    // validate the sync nibble. it is the same for version 2 and 3 protocols
    //
    bool msgOk = packet[0] == 0x0A;
    //
    // for protocol version 2, there may be two concatenated copies of the same message 
    // this is indicated if the pattern "FFFFA" occurs in the message. "FFFF" is the 
    // preamble of the 2nd message and "A" is the sync nibble. time can be saved by 
    // limiting this check to messages over 27 nibbles in length.
    //
    if (msgLen > 27)
    {
      int k;
      unsigned long sr = 0; // shift register
      boolean foundHdr = false;

      for (k=10; k<msgLen; k++)
      {
        sr = (sr << 4) | (unsigned long)packet[k];
        if ((sr & 0x000FFFFFUL) == 0x000FFFFAUL)
        {
          foundHdr = true;
          break;
        }
      }

      if (foundHdr) // there are two messages here
      {
        // k points to the sync nibble of the 2nd message. 
        // the last nibble of the first message is packet[k-5]
        // and the length of the first message is (k-4)
        // the 2nd message begins at "k" and is only if interest if the
        // checksum in the first message is bad. make a record of the
        // existence and location of the duplicate message if needed 
        // later on below.
        //
        duplicateIndex = k;
        duplicateLength = msgLen - k;
        msgLen = k - 4;
        //
        // if by chance, the sync nibble in the first message copy
        // is wrong, then immediately switch to the 2nd one, because
        // we've already verified that it's sync nibble is correct.
        //
        if (!msgOk && (duplicateIndex > 0))
        {
          // copy the 2nd message on top of the first, destroying it
          memcpy(packet, packet+duplicateIndex, duplicateLength);
          // reset the pointers and we're done
          duplicateIndex = duplicateLength = 0;
          msgLen = duplicateLength;
          msgOk = true;
        }
      } // if (foundHdr)

    }   // if (msgLen > 27)

    do 
    {
      if (msgOk)
      {
        //
        // attempt to validate the message checksum. make some attempt to handle the loss of up
        // to 8 trailing message bits.
        //
        unsigned int cksumIndex = msgLen - 4;
        msgOk &= ValidChecksum(packet, cksumIndex);

        if (!msgOk)
        {
          // if between 4 and 7 bits were lost from the end of the message, it might still be valid, so increase
          // the length by one and try again        
          msgOk = ValidChecksum(packet, ++cksumIndex);
        }

        if (!msgOk)
        {
          // if exactly 8 bits were lost from the end of the message, it might still be valid, so increase
          // the length by one more and try again
          msgLen++;
          msgOk = ValidChecksum(packet, ++cksumIndex);
        }

        msgLen = cksumIndex + 4;

        // for both version 2.1 and 3.0 protocols, msgLen is includes two nibbles
        // after the checksum, whether they were actually part of the message or not.
        // this is done so that the meaning of msgLen is consistent regardless of 
        // protocol version. it might be cleaner to make msgLen only include the checksum...?
      }

      if (msgOk || (duplicateIndex == 0)) break;
      //
      // copy the duplicate message on top of the first one and see
      // if that one is valid
      //
      memcpy(packet, packet+duplicateIndex, duplicateLength);
      msgLen = duplicateLength;
      duplicateIndex = duplicateLength = 0;

    } while (true);


#if ENABLE_DEBUG_PRINT && !ENABLE_HEX_OUTPUT
    if (!msgOk)
    {
      // for debug, print out the message nibbles
      for (int i=0; i<msgLen; i++)
      {
        Serial.print(hexChars[packet[i]]);			
      }
      Serial.println(".");
    }
#endif

    //
    // version 2.1 protocol messages include a full repeat of the 
    // message with every transmission.
    // detect repeated version 2.1 protocol messages here
    // and get rid of one of them if it matches the previous one
    //
    if (msgOk && protocol_version == 2)
    {
      unsigned long now = millis();
      unsigned long tlim = previous_packet_time + 1000UL;
      if ( MILLIS_CMP(now, tlim) == -1 )
      {
        // this packet was received less than one second after the
        // previous packet, so this might be a repeated packet. 
        // the only way to know for sure is to compare data.
        // if the data is equal, memcmp will return zero.
        msgOk = memcmp(packet, previous_packet, msgLen-2) != 0; 
      }
      // log the time of this packet and save the packet data 
      previous_packet_time = now;
      memcpy(previous_packet, packet, msgLen);
    }

    if (msgOk)
    {
      *protocol = protocol_version;
      return msgLen;
    }
    else
    {
      return 0;
    }

  }

  boolean OsRx::ValidChecksum(byte *packet, int Pos)
  {
#if NO_VERIFY_CHECKSUMS
    return true;
#else
    bool ok = false;
    byte check = packet[Pos] | (byte)(packet[Pos+1] << 4);

    byte Checksum = 0;
    for (int x = 1; x < Pos; Checksum += packet[x++]);	

    return (Checksum == check);
#endif
  }

  OsRx OsReceiver = OsRx();
