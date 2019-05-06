/*
 * blink.c:
 *      blinks the first LED
 *      Gordon Henderson, projects@drogon.net
 */

#include <stdio.h>
#include <wiringPi.h>
#include <iostream>
#include <cstdlib>
#include "RtMidi.h"

volatile int state = 1;

void
midi_callback( double deltatime, std::vector< unsigned char > *message, void *userData )
{
	if ( message->size() >= 3 && message->at(1) == 36 && message->at(2) > 0 ) {
		if (state) {
			state = 0;
		} else {
                        state = 1;
		}
		digitalWrite(1, state);
	}
}

int main()
{
  printf ("Raspberry Pi MIDI blink\n");
  if (wiringPiSetup () == -1)
    return 1;
  pinMode(1, OUTPUT);        // aka BCM_GPIO pin 18
  digitalWrite (1, state);   // On

  RtMidiIn *midiin = new RtMidiIn();
  // Check available ports.
  unsigned int nPorts = midiin->getPortCount();
  if ( nPorts == 0 ) {
    std::cout << "No ports available!\n";
    goto cleanup;
  }

  // FIXME: Hardcoded Port 1  :D :D :D
  midiin->openPort( 1 );
  // Set our callback function.  This should be done immediately after
  // opening the port to avoid having incoming messages written to the
  // queue.
  midiin->setCallback( &midi_callback );

  // Don't ignore sysex, timing, or active sensing messages.
  midiin->ignoreTypes( false, false, false );

  std::cout << "\nReading MIDI input ... press <enter> to quit.\n";
  char input;
  std::cin.get(input);

  // Clean up
 cleanup:
  delete midiin;
  return 0;
}

