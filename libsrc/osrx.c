//============================================================================
//Weather Station Data Logger : Weather Shield for Arduino
//Copyright © 2010, brian@lostbyte.com, Weber Anderson
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
// Portions of this application were originally developed by brian@lostbyte.com and
// the original header comments from that version are included below.
// 
// The program has been modified significantly and in its current
// form is specifically tailored to work in conjuction with the
// Weather Station Data Logger weather logging program,
// Web Site: http://wmrx00.sourceforge.net
// Contact: Visit one of the forums at source forge for this project.
//
//=============================================================================
// ****** Original comment header from brian@lostbyte.com ******
//
// Arduino Oregon Scientific V3 Sensor receiver v0.3
// Updates: http://www.lostbyte.com/Arduino-OSV3
// Contact: brian@lostbyte.com
// 
// Receives and decodes 433.92MHz signals sent from the Oregon Scientific
// Version 2.1 and 3.0 sensors.
//
// For more info: http://lostbyte.com/Arduino-OSV3
//
// Hardware based on the Practical Arduino Weather Station Receiver project
// http://www.practicalarduino.com/projects/weather-station-receiver
// 
// Special thanks to Kayne Richens and his ThermorWeatherRx code.  
// http://github.com/kayno/ThermorWeatherRx
//

#include <wiring.h>
#include <avr/interrupt.h>

#include "osrx.h"

//
// *** BEGIN DEFINES FOR RF PROTOCOL DECODING
//

#define INPUT_CAPTURE_IS_RISING_EDGE()    ((TCCR1B & _BV(ICES1)) != 0)
#define INPUT_CAPTURE_IS_FALLING_EDGE()   ((TCCR1B & _BV(ICES1)) == 0)
#define SET_INPUT_CAPTURE_RISING_EDGE()   (TCCR1B |=  _BV(ICES1))
#define SET_INPUT_CAPTURE_FALLING_EDGE()  (TCCR1B &= ~_BV(ICES1))

// Control LED on pin 6
#define RECEIVING_LED_PIN   6
#define LED_ON()            digitalWrite(RECEIVING_LED_PIN, LOW);
#define LED_OFF()           digitalWrite(RECEIVING_LED_PIN, HIGH);

// macro to reset the receive state machine state
#define WEATHER_RESET() { protocol_version = short_count = long_count = 0; rx_state = RX_STATE_IDLE; }  
//
// values for timer 2 control registers
//
#define PULSE_TIMEOUT_ENABLE  _BV(TOIE2)
#define PULSE_TIMEOUT_DISABLE 0
#define T2_PRESCALE_X128 ( _BV(CS22) | _BV(CS21) )
//
// Preamble information
//
// minimum number of short sync periods required to begin a version 3 RF message:
#define SYNC_COUNT		               20
// min number of long sync periods to begin a version 2.1 RF message:
#define LONG_SYNC_COUNT              25

// Rx States
#define RX_STATE_IDLE               0  /* ready to go */
#define RX_STATE_RECEIVING_V3       1  /* receiving version 3 protocol messsage */
#define RX_STATE_PACKET_RECEIVED    2  /* complete message received */
#define RX_STATE_RECEIVING_V2       3  /* receiving version 2 protocol message */

#define BIT_ZERO                    0
#define BIT_ONE                     1



// *** RF Protocol Decoding Variables ***

//
// timer value at last edge capture event. used to compute time interval
// between previous event and current event.
//
static unsigned int timer1_ovfl_count;
static unsigned int previous_captured_time;
//
// value of most recently decoded bit.
//
static unsigned int current_bit;
//
// receive buffer pointer 
// using bit fields like this is platform dependent and generally not recommended.
// however, since this code will never (never say never :-O ) run on anything but an Atmel processor,
// the risks are minimized. on the plus side, this union with bit fields makes the code
// cleaner and easier to understand.
//
static union {
  unsigned int value;  
  struct 
  {
    unsigned int bit:2;      // two LSBs refer to a bit within the nibble
    unsigned int nibble:14;  // 14 MSBs refer to a nibble 
  } portion;
} bufptr;
//
// receive buffer
// only the lower nibble of each byte is used. The upper nibbles
// are zeroed in the reset() function and should not be modified
// thereafter.
//
static byte packet[MAX_MSG_LEN];
//
// while looking for a valid preamble, these track the number of 
// short and long periods seen. for now, preambles are much less
// than 256 pulses long, but use 16-bit counters just to be safe
//
static unsigned int short_count;
static unsigned int long_count;

static byte protocol_version; // protocol version of the current message
//
// for the version 2 protocol, this is used as a toggle to cause every
// other bit to be "dumped" -- since each bit is repeated once every
// other bit is not stored in the buffer.
//
static boolean dump_bit;

static byte rx_state; // current state of the receive state machine
//
// short periods must occur in pairs in a valid signal. this variable 
// toggles every time a short period is found and is used to 
// enforce this requirement. also, this lets us know that the 2nd
// period of a short pair has occurred -- this is one event that defines
// a data bit.
//
static boolean previous_period_was_short = false;

const unsigned long mm_diff = 0x7fffffffUL; 

//
// thresholds for short and long timer periods. the thresholds are different
// when the RF signal is on and off. the three values in each array are, in order:
// minimum short period, threshold between short and long periods, and maximum long period.
// if the timer value is exactly equal to the short/long threshold it does not matter 
// which choice is made -- this is probably a noise reading anyway. 
// periods less then the minimum short or maximum long periods are considered noise and
// rejected.
//
const unsigned int rf_off_thresholds[3] = { 100, 212, 350 };  // for a 4usec timer tick, 400,848,1400 usec
const unsigned int rf_on_thresholds[3]  = {  50, 137, 275 };  // for a 4usec timer tick, 200,548,1100 usec
//
// Overflow interrupt routine for timer 2
// When the last bit of a message has been received by the event capture ISR, the state machine will just 
// sit there waiting for the next RF transition -- which won't occur until a new message begins.
// This timer is designed to expire shortly after the last message bit, so the new message
// can be signalled to background software. This will then happen much sooner than if we waited
// for a new message to begin arriving.
//
ISR(TIMER2_OVF_vect)
{
  TIMSK2 =  PULSE_TIMEOUT_DISABLE; // disable further interrupts
  TIFR2 = 0; // this may be redundant -- the interrupt is probably cleared automatically for us
  boolean rx_state = (rx_state == RX_STATE_RECEIVING_V2) || (rx_state == RX_STATE_RECEIVING_V3);
  if (rx_state && bufptr.value > 40)
  { 
    rx_state = RX_STATE_PACKET_RECEIVED;
  }
}
//
// Overflow interrupt vector
// this is need to keep track of overflow events on timer 1 which allows detection
// of very long time periods  while looking for preambles when there might be a lot
// of time between data transitions
//
ISR(TIMER1_OVF_vect)
{
  timer1_ovfl_count++;
}

//
// Event capture interrupt routine for timer 1. 
// This is the heart of RF protocol decoding.
//
// This is fired every time the received RF signal goes on or off. Since every 
// transition (up or down) is followed by an opposite transition (down or up), the edge detection
// bit for the timer is flipped with every transition.
//
// Decoding of the Manchester-coded ASK is performed in this ISR. The technique works by
// examining the time between adjacent RF transitions. This is easy to do using the Atmel
// processor's "timer 1" which has an input specifically designed to detect the timing of transitions
// on input bit 0 of port B. This input even has a de-glitching feature which helps to filter
// out noise on the RF transitions. This idea apparently came from one or more of the 
// sources quoted above at the top of this file.
//
// It may take a little thinking, but you can prove to yourself these facts about normal
// Manchester-coded signals:
// 
// 1) If the next message bit is the same as the previous message bit, there will need to 
//    be an RF transition (on-to-off or vice versa) in the middle of the bit period.
// 2) If the next message bit is the opposite of the previous bit, there will be no RF transition
//    in the middle of the bit period.
// 3) The time between transitions is either short (1/2 bit period) or long (a full bit period).
// 4) When short transitions occur, they always occur in pairs.
// 
// The ISR below uses these ideas to decode the signal, with the additional knowledge that all 
// bits in the preamble of short transitions are "1" bits. When a long transition occurs, it 
// represents a bit that is the opposite of the preceeding bit. When two short transitions occur,
// it represents a bit that is identical to the preceeding bit.
//
// Things are reversed for the version 2.1 RF protocol. See the document for details.
//
// In fact, RF transmissions from OS units do not have a 50% duty cycle so the definition of
// short and long bit periods are a bit skewed.
//
ISR(TIMER1_CAPT_vect)
{ 
  // do the time-sensitive things first
  TIMSK2 = PULSE_TIMEOUT_DISABLE;
  unsigned int ovfl = timer1_ovfl_count;
  timer1_ovfl_count = 0;
  // grab the event time
  unsigned int captured_time = ICR1;
  //
  // depending on which edge (rising/falling) caused this interrupt, setup to receive the opposite
  // edge (falling/rising) as the next event.
  //
  boolean rf_was_on = INPUT_CAPTURE_IS_FALLING_EDGE();

  if(!rf_was_on)
    SET_INPUT_CAPTURE_FALLING_EDGE();
  else 
    SET_INPUT_CAPTURE_RISING_EDGE();
  //
  // detect and deal with timer overflows. the timer will overflow about once every
  // 0.26 seconds so it is not all that rare of an occurance. as long as there has only been 
  // one overflow this will work. If there is more than one overflow, just set the period to maximum.
  // there IS a race condition where this logic will sometimes fail but the window is very small.
  // here's the situation: an input edge happens about the same time as timer1 overflows,
  // and the timer gets latche before the overflow, but the timer overflow ISR runs before the
  // edge capture ISR (don't know if this is really possible). In this case, the timer will not
  // have overflowed (it will be equal to 0xFFFF) but the overflow counter WILL indicate an 
  // overflow. Again, the window to create this problem is so narrow this should rarely happen.
  // the worst thing that will happen is that we'll occasionally loose a preamble bit or maybe
  // even a whole message (even more rare). It's certainly better than ignoring overflows altogether.
  //
  // the other thing that can happen if there is no activity on the DATA line for 4.7 hours, is that
  // the overflow counter itself will overflow. again, this might cause us to miss one message at the 
  // worst case, so this is an acceptable risk.
  //
  unsigned int captured_period;
  if (ovfl > 1)
  {
    captured_period = 0xFFFFL;
  }
  else
  {
    //
    // computing the period with an int (16 bits) can only go about 1/4 of a second before 
    // overflowing. however, the timeout provided by timer 2 ensures that we'll never see
    // a period that long. the overflow ISR will force the rx state back to IDLE, which does
    // not care about the result of this calculation.
    //
    if (captured_time < previous_captured_time)
    {
      // do the math using signed 32-bit integers, but convert the result back to a 16-bit integer
      captured_period = (unsigned int)((long)captured_time + 0x10000L - (long)previous_captured_time);    
    }
    else
    {
      captured_period = captured_time - previous_captured_time;
    }
  }

  unsigned int *thresholds = rf_was_on ? rf_on_thresholds : rf_off_thresholds;
  boolean short_period = false;
  boolean long_period = false;
  if ((captured_period >= thresholds[0]) && (captured_period <= thresholds[1]))
  {
    short_period = true;
  }
  else
  {
    if ((captured_period > thresholds[1]) && (captured_period <= thresholds[2]))
    {
      long_period = true;
    }
  }

  switch (rx_state)
  {
  case RX_STATE_IDLE:
    //
    // Version 3 Protocol:
    // When idle, we're looking for a minimum number of short transitions which will 
    // signify the beginning of a message. It is possible the receiver will miss a few
    // of the initial pulses, so we don't want to require all of them to be received.
    // After receiving the required minimum, the first long pulse received will kick
    // us into the V3 receiving state. this is also the first bit of the sync nibble,
    // and is always a zero bit.
    //
    // Version 2.1 protocol:
    // The preample here is one short pulse (which we often miss) followed by 16 long
    // pulses. The first short pulse received after a minimum set of long ones will
    // kick us into the V2 receiving state.
    //
    if (short_period)
    {
      if ((short_count <= 1) && (long_count > LONG_SYNC_COUNT))
      {
        protocol_version = 2;
        dump_bit = false; //true;
        rx_state = RX_STATE_RECEIVING_V2;
        previous_period_was_short = true;
        // this is actually the first bit, which is always a one so record it.
        // it will be repeated, so take that into account also
        bufptr.value = 0;
        packet[0] = 0;
        current_bit = BIT_ONE;        
      }
      else if (long_count == 0)
      {
        short_count++;  
      }
      else
      {
        WEATHER_RESET();
      }
    }
    else if (long_period)
    { 
      if(short_count > SYNC_COUNT) 
      {
        rx_state = RX_STATE_RECEIVING_V3;
        protocol_version = 3;
        previous_period_was_short = false;
        // this is actually the first bit, which is always a zero so record it.
        bufptr.value = 1;
        packet[0] = 0;
        current_bit = BIT_ZERO;
        // LED_ON();
      } 
      else if (short_count <= 1)
      {
        long_count++;
      }
      else 
      {
        WEATHER_RESET();
      }
    } 
    else 
    {
      WEATHER_RESET();
    }
    break;

  case RX_STATE_RECEIVING_V3:  
    //
    // while receiving message bits, examine the time between this RF transition and the 
    // previous one. there are three possibilities, a "short" period, a "long" period, 
    // and a period which does not meet either of these criteria.
    //
    // for properly formatted messages, short periods occur in pairs. the first short
    // period of a pair is registered but conveys no data at that time. the second short
    // period in a pair indicates that the message bit for this period is the same as the
    // bit transmitted in the previous period.
    //
    if (short_period)
    { 
      if(previous_period_was_short) 
      {      
        if(current_bit)
          packet[bufptr.portion.nibble] |= 1 << bufptr.portion.bit;          
        else 
          packet[bufptr.portion.nibble] &= ~(1 << bufptr.portion.bit);

        bufptr.value++;
        previous_period_was_short = false;
      } 
      else 
        previous_period_was_short = true;
    }
    //
    // long periods convey a single transmitted bit each. in this case the transmitted bit
    // is the opposite of the previously transmitted bit.
    //
    else if (long_period)
    {
      if (previous_period_was_short)
      {
        WEATHER_RESET();       
      }

      current_bit = 1 - current_bit;

      if(current_bit) 
        packet[bufptr.portion.nibble] |=  (0x01 << bufptr.portion.bit);
      else 
        packet[bufptr.portion.nibble] &= ~(0x01 << bufptr.portion.bit);

      bufptr.value++;
    }
    //
    // transition periods outside the valid ranges for long or short periods occur in two
    // situations: (a) a new message has begun before timer2 can produce a timeout to end
    // the first message, and (b) data corruption through interference, RF noise, etc.
    // In both cases, if there are enough bits to form a message, signal a message complete
    // status and let the downstream software try to decode it. Otherwise, perform a reset
    // and wait for another message.
    //
    else
    {
      if (bufptr.value > 40) 
        rx_state = RX_STATE_PACKET_RECEIVED;
      else
        WEATHER_RESET();
    }
    break;

  case RX_STATE_RECEIVING_V2:  
    //
    // while receiving message bits, examine the time between this RF transition and the 
    // previous one. there are three possibilities, a "short" period, a "long" period, 
    // and a period which does not meet either of these criteria.
    //
    // for properly formatted messages, short periods occur in pairs. the first short
    // period of a pair is registered but conveys no data at that time. the second short
    // period in a pair indicates that the message bit for this period is the same as the
    // bit transmitted in the previous period.
    //
    if (short_period)
    { 
      if(previous_period_was_short) 
      {      
        current_bit = 1 - current_bit;
        if (dump_bit)
        {
          // V2 protocol sends bits in pairs. The fact that there are these
          // repeated bits means that the dumped bit must always be the same as the previous bit.
          // A short pair flips the bit, and the following bit must be the same -- this implies
          // that a pair of short periods must always be followed by a long period. If two pairs
          // of short pulses occur together, the bits won't be repeated; this is an error.
          WEATHER_RESET();
        }
        else
        {
          if(current_bit) 
            packet[bufptr.portion.nibble] |=  (0x01 << bufptr.portion.bit);
          else
            packet[bufptr.portion.nibble] &= ~(0x01 << bufptr.portion.bit);

          bufptr.value++;
          dump_bit = true; // dump the next bit -- it s/b a repeat of this one
        }
        previous_period_was_short = false;
      } 
      else 
        previous_period_was_short = true;
    }
    //
    // long periods convey a single transmitted bit each. in this case the transmitted bit
    // is the opposite of the previously transmitted bit.
    //
    else if (long_period)
    {
      //
      // short periods must appear in pairs. if "previous_period_was_short" is true, then this long period was
      // preceeded by a single short period -- this is an error.
      //
      if (previous_period_was_short)
      {
        WEATHER_RESET();       
      }      

      if (dump_bit)
      {
        //
        // here, the current bit is identical to the previous bit -- this meets
        // the repeated-bit criteria.
        //
        dump_bit = false;
      }
      else
      {  
        if(current_bit) 
          packet[bufptr.portion.nibble] |=  (0x01 << bufptr.portion.bit);
        else
          packet[bufptr.portion.nibble] &= ~(0x01 << bufptr.portion.bit);

        bufptr.value++;
        dump_bit = true; // dump the next bit -- it s/b a repeat
      }

    }
    //
    // transition periods outside the valid ranges for long or short periods occur in two
    // situations: (a) a new message has begun before timer2 can produce a timeout to end
    // the first message, and (b) data corruption through interference, RF noise, etc.
    // In both cases, if there are enough bits to form a message, signal a message complete
    // status and let the downstream software try to decode it. Otherwise, perform a reset
    // and wait for another message.
    //
    else
    {
      if (bufptr.value > 40) 
        rx_state = RX_STATE_PACKET_RECEIVED;
      else
        WEATHER_RESET();
    }
    break;

  case RX_STATE_PACKET_RECEIVED:
  default:
    // this most often will happen when a new message begins before the background loop has
    // had a chance to read the current message. in this situation, we just let the new message
    // bits go into the bit bucket...
    break;
  }

  previous_captured_time = captured_time;

  // guard against buffer overflows this could happen if two messages overlap
  // just right -- probably very rare.
  if (bufptr.value >= (MAX_MSG_LEN << 2))
  {
    WEATHER_RESET();
  }

  if ((rx_state == RX_STATE_RECEIVING_V3) || (rx_state == RX_STATE_RECEIVING_V2)) 
  {
    // when waiting for another transition, set timer 2 for a timeout
    // in case we have reached the end of the message
    TCNT2 = 0;
    TIFR2 = 0;
    TIMSK2 = PULSE_TIMEOUT_ENABLE;
  }

}


void osrx_init()
{
  int i;
  // we never write to the high nibble of packet[] elements so clear them all now.
  for (i=0; i<MAX_MSG_LEN; packet[i++] = 0);

  WEATHER_RESET();

  // 
  // configure ports:
  //
  // ports are initialized with all pins as inputs. 
  // when changed to outputs, all bits are defaulted to zero
  // all pullup resistors are disabled by default
  //

  // pinMode(RECEIVING_LED_PIN, OUTPUT);

  //
  // setup timer 1 to be triggered by transitions on the DATA  line
  // from the 433.92MHz receiver module. Arduino uses timer 1 for PWM
  // so this code will not work at the same time as some PWM applications
  //
  TCCR1A = 0; // Select normal (simple count-up) mode
  //
  // ICNC1 enables the noise canceller for timer event captures
  // CS10/11/12 bits select the timer clock to be the system clock/64
  // for the Duemillinova, this is 16MHz/64 or 250kHz.
  //
  TCCR1B = ( _BV(ICNC1) | _BV(CS11) | _BV(CS10) );
  SET_INPUT_CAPTURE_RISING_EDGE();
  //
  // enable timer 1 interrupts for input capture events only.
  //
  TIMSK1 = (_BV(ICIE1)) | (_BV(TOIE1));
  // 
  // setup timer 2 to provide a timeout when waiting for receiver DATA transitions
  // timer2 is also used for PWM by Arduino, so this code may not be compatible with
  // some PWM applications.
  //
  TCCR2A = 0; // select normal mode for the timer
  TCCR2B = T2_PRESCALE_X128;  // select clk/128. 8-bit timer will overflow every 2 msec
  TCNT2 = 0;  // clear the timer count
  TIMSK2 = PULSE_TIMEOUT_DISABLE; // interrupts are disabled to start with...
}

boolean osrx_data_available()
{
  return rx_state == RX_STATE_PACKET_RECEIVED;
}

byte get_osrx_data(byte *buffer, byte length, byte *protocol)
{
  if (rx_state != RX_STATE_PACKET_RECEIVED) return 0;
  *protocol = protocol_version;
  if (bufptr.portion.bit != 0)
  {
    // bump up to an even multiple of 4 bits
    bufptr.value += 4 - bufptr.portion.bit;
  }
  byte cnt = bufptr.portion.nibble;
  if (length < cnt) return 0;
  memcpy(buffer, packet, cnt);
  return cnt;
}

void start_receiving()
{
  if (rx_state != RX_STATE_PACKET_RECEIVED) return;
  WEATHER_RESET();
}

