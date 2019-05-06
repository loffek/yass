all: pcm_push pcm_pull midi_probe midi_callback trigger

pcm_push: pcm_push.c
	gcc -o pcm_push pcm_push.c -lasound

pcm_pull: pcm_pull.c
	gcc -o pcm_pull pcm_pull.c -lasound

silenece: silence.c
	gcc -o silence silence.c -lasound

midi_probe: midi_probe.o RtMidi.o
	g++ -o midi_probe midi_probe.o RtMidi.o -lasound -lpthread

midi_probe.o: midi_probe.cpp
	g++ -c midi_probe.cpp

midi_callback: midi_callback.o RtMidi.o
	g++ -o midi_callback midi_callback.o RtMidi.o -lasound -lpthread

midi_callback.o: midi_callback.cpp
	g++ -c midi_callback.cpp

trigger: trigger.o
	g++ -o trigger trigger.o RtMidi.o -lasound -lpthread -lwiringPi

trigger.o: trigger.cpp
	g++ -Wall -c trigger.cpp

RtMidi.o: RtMidi.cpp
	g++ -Wall -D__LINUX_ALSA__ -c RtMidi.cpp
     
clean:
	rm *.o pcm_push pcm_pull midi_probe midi_callback trigger

