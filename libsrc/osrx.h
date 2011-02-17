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

// Maximum number of nibbles in a message. Determines buffer size.
// maximum message length in bits is four times this value
#define MAX_MSG_LEN                 64 

//
// MILLIS_CMP(a,b) returns the sign of (a-b) -- or zero if a and b are identical
// in essence, this does a signed comparison of unsigned long numbers and makes the assumption
// that when two numbers differ by more than mm_diff, that an overflow or underflow must have 
// occurred. the over/underflow is "fixed" and the proper answer is returned
//
#define MILLIS_CMP(a,b) ( (a==b) ? 0 : ( (a>b) ? (((a-b)>mm_diff) ? -1 : 1) : (((b-a)>mm_diff) ? 1 : -1) ) )



void osrx_init();
boolean osrx_data_available();
boolean get_osrx_data(byte *buffer, byte length, byte *protocol_version);
void start_receiving();
