#include <iostream>
#include <wiringPi.h>
#include <stdio.h>  
#include <cstdlib>
#include <stdint.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <poll.h>
#include <alsa/asoundlib.h>

#include "RtMidi.h"

#define PCM_DEVICE "default"
#define NFRAMES 4
#define CHANNELS 1
#define BUFSIZE CHANNELS*NFRAMES
#define BITRATE 44100
      
typedef struct clip {
	size_t size;
   	int16_t *buffer;
} clip;

clip c0;
clip c1;
int state = 1;
volatile unsigned int clip_select = 0;
volatile unsigned int playing = 0;
volatile size_t offset = 0;
pthread_mutex_t lock;
snd_pcm_t *playback_handle;

clip
readClip(const char *name)
{
    // FIXME: clip must be longer than skip bytes!
    size_t skip = 44;
    FILE *file = fopen(name, "r");
    fseek(file, 0, SEEK_END);
    size_t size = (ftell(file) - skip) / 2; // 16 bit samples
    int16_t *buffer = (int16_t *)malloc(2 * size);
    fseek(file, skip, SEEK_SET);
    fread(buffer, 2, size, file); // both wave files and the raspberrypi is Little Endian so...
    fclose(file);
    clip c;
    c.size = size;
    c.buffer = buffer;
    return c;
}

void
midi_callback( double deltatime, std::vector< unsigned char > *message, void *userData )
{
	if ( message->size() >= 3 && message->at(1) == 36 && message->at(2) > 0 ) {
		pthread_mutex_lock(&lock);
		clip_select = 0; // kick		
		playing = 1;
		offset = 0;
		pthread_mutex_unlock(&lock);
	} else if ( message->size() >= 3 && message->at(1) == 38 && message->at(2) > 0 ) {
		pthread_mutex_lock(&lock);
		clip_select = 1; // snare		
		playing = 1;
		offset = 0;
		pthread_mutex_unlock(&lock);
	}
}

char buf[BUFSIZE];

int
playback_callback (snd_pcm_sframes_t nframes)
{
	int err;

	// printf ("playback callback called with %ld frames\n", nframes);
	pthread_mutex_lock(&lock);
	int i=0;
	if (playing)
	{
		clip c;
		if (clip_select == 0) {
			c = c0;
		} else {
			c = c1;
		}
		for (; i < BUFSIZE && i+offset < c.size; i++) {
			buf[i] = c.buffer[i+offset];		
		}
		offset += i;
		if (offset >= c.size) {
			// std::cout << "clip ended" << std::endl;
			playing = 0;
			offset = 0;
		}
	}
	for (; i < BUFSIZE; i++) {
		// fill the rest of the buffer with silence
		buf[i] = 0;
	}
	pthread_mutex_unlock(&lock);

	/* ... fill buf with data ... */

	/* Regardless of how many frames it asked for,
	we only have a buffer for NFRAMES frames (*2 channels * 2 bytes (16 bits)
	*/
	if ((err = snd_pcm_writei (playback_handle, buf, NFRAMES)) < 0) {
		fprintf (stderr, "write failed (%s)\n", snd_strerror (err));
	}

	return err;
}

void* midiThread(void *arg)
{
	RtMidiIn *midiin = new RtMidiIn();
	// Check available ports.
	unsigned int nPorts = midiin->getPortCount();
	if ( nPorts == 0 )
	{
		std::cout << "No ports available!\n";
		exit(1);
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
	delete midiin;
	return NULL;
}

void* pcmThread(void *arg)
{
	snd_pcm_hw_params_t *hw_params;
	snd_pcm_sw_params_t *sw_params;
	snd_pcm_sframes_t frames_to_deliver;
	int err;

	if ((err = snd_pcm_open (&playback_handle, PCM_DEVICE, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
		fprintf (stderr, "cannot open audio device %s (%s)\n", 
			 PCM_DEVICE,
			 snd_strerror (err));
		exit (1);
	}
	   
	if ((err = snd_pcm_hw_params_malloc (&hw_params)) < 0) {
		fprintf (stderr, "cannot allocate hardware parameter structure (%s)\n",
			 snd_strerror (err));
		exit (1);
	}
			 
	if ((err = snd_pcm_hw_params_any (playback_handle, hw_params)) < 0) {
		fprintf (stderr, "cannot initialize hardware parameter structure (%s)\n",
			 snd_strerror (err));
		exit (1);
	}

	if ((err = snd_pcm_hw_params_set_access (playback_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		fprintf (stderr, "cannot set access type (%s)\n",
			 snd_strerror (err));
		exit (1);
	}

	if ((err = snd_pcm_hw_params_set_format (playback_handle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
		fprintf (stderr, "cannot set sample format (%s)\n",
			 snd_strerror (err));
		exit (1);
	}

	unsigned int rate = BITRATE;
	if ((err = snd_pcm_hw_params_set_rate_near (playback_handle, hw_params, &rate, 0)) < 0) {
		fprintf (stderr, "cannot set sample rate (%s)\n",
			 snd_strerror (err));
		exit (1);
	}

	if ((err = snd_pcm_hw_params_set_channels (playback_handle, hw_params, CHANNELS)) < 0) {
		fprintf (stderr, "cannot set channel count (%s)\n",
			 snd_strerror (err));
		exit (1);
	}

	if ((err = snd_pcm_hw_params (playback_handle, hw_params)) < 0) {
		fprintf (stderr, "cannot set parameters (%s)\n",
			 snd_strerror (err));
		exit (1);
	}

	snd_pcm_hw_params_free (hw_params);

	/* tell ALSA to wake us up whenever NFRAMES or more frames
	   of playback data can be delivered. Also, tell
	   ALSA that we'll start the device ourselves.
	*/

	if ((err = snd_pcm_sw_params_malloc (&sw_params)) < 0) {
		fprintf (stderr, "cannot allocate software parameters structure (%s)\n",
			 snd_strerror (err));
		exit (1);
	}
	if ((err = snd_pcm_sw_params_current (playback_handle, sw_params)) < 0) {
		fprintf (stderr, "cannot initialize software parameters structure (%s)\n",
			 snd_strerror (err));
		exit (1);
	}
	if ((err = snd_pcm_sw_params_set_avail_min (playback_handle, sw_params, NFRAMES)) < 0) {
		fprintf (stderr, "cannot set minimum available count (%s)\n",
			 snd_strerror (err));
		exit (1);
	}
	if ((err = snd_pcm_sw_params_set_start_threshold (playback_handle, sw_params, 0U)) < 0) {
		fprintf (stderr, "cannot set start mode (%s)\n",
			 snd_strerror (err));
		exit (1);
	}
	if ((err = snd_pcm_sw_params (playback_handle, sw_params)) < 0) {
		fprintf (stderr, "cannot set software parameters (%s)\n",
			 snd_strerror (err));
		exit (1);
	}

	/* the interface will interrupt the kernel every NFRAMES frames, and ALSA
	   will wake up this program very soon after that.
	*/

	if ((err = snd_pcm_prepare (playback_handle)) < 0) {
		fprintf (stderr, "cannot prepare audio interface for use (%s)\n",
			 snd_strerror (err));
		exit (1);
	}

	while (1) {

		/* wait till the interface is ready for data, or 1 second
		   has elapsed.
		*/

		if ((err = snd_pcm_wait (playback_handle, 1000)) < 0) {
			fprintf (stderr, "poll failed (%s)\n", strerror (errno));
			break;
		}	           

		/* find out how much space is available for playback data */

		if ((frames_to_deliver = snd_pcm_avail_update (playback_handle)) < 0) {
			if (frames_to_deliver == -EPIPE) {
				fprintf (stderr, "an xrun occured\n");
				break;
			} else {
				fprintf (stderr, "unknown ALSA avail update return value (%ld)\n", 
					 frames_to_deliver);
				break;
			}
		}

		frames_to_deliver = frames_to_deliver > NFRAMES ? NFRAMES : frames_to_deliver;

		/* deliver the data */

		if (playback_callback (frames_to_deliver) != frames_to_deliver) {
			fprintf (stderr, "playback callback failed\n");
			break;
		}
	}

	snd_pcm_close (playback_handle);
	return NULL;
}

int     
main (int argc, char *argv[])
{
	int err;
	if ((err = pthread_mutex_init(&lock, NULL)) != 0)
	{
		std::cout << "mutex init failed" << std::endl;
		return 1;
	}
	c0 = readClip("/home/pi/yass/wav/kick2.wav");
	std::cout << "Kick loaded: " << c0.size << " samples" << std::endl;	

	c1 = readClip("/home/pi/yass/wav/snare.wav");
	std::cout << "Snare loaded: " << c1.size << " samples" << std::endl;	
	size_t i;
	for (i = 0; i < c0.size; i++) {
		printf("%d\n", c0.buffer[i]);
	}
	return 0;

	if (wiringPiSetup () == -1)
		return 1;

	pinMode(1, OUTPUT);        // aka BCM_GPIO pin 18
	digitalWrite (1, state);   // On

	pthread_t t0;
	if ((err = pthread_create(&t0, NULL, &midiThread, NULL)) != 0 )
	{
		printf("\ncan't create thread :[%s]", strerror(err));
		return 1;
	}

	pthread_t t1;
	if ((err = pthread_create(&t1, NULL, &pcmThread, NULL)) != 0 )
	{
		printf("\ncan't create thread :[%s]", strerror(err));
		return 1;
	}

	pthread_join(t0, NULL);
	pthread_join(t1, NULL);
	pthread_mutex_destroy(&lock);
	return 0;
}
