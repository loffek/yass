#include <stdio.h>  
#include <stdlib.h>
#include <stdint.h>

typedef struct WaveHeader {
	uint32_t chunk_id;
	uint32_t chunk_size;
	uint32_t format;
	uint32_t subchunk1_id;
	uint32_t subchunk1_size;
	uint16_t audio_format;
	uint16_t num_channels;
	uint32_t sample_rate;
	uint32_t byte_rate;
	uint16_t block_align;
	uint16_t bits_per_sample;
	uint32_t subchunk2_id;
	uint32_t subchunk2_size;
} WaveHeader;

#define SAMPLE_RATE 44100
#define BITS_PER_SAMPLE 16

int
main (int argc, char *argv[])
{
	if (argc < 2) {
		printf("Usage: readwav <filepath>\n");
		return 1;
	}
	char *path = argv[1];

	// http://soundfile.sapp.org/doc/WaveFormat/
	WaveHeader header;
	// TODO; make sure to close file even if early exit!
	FILE *file = fopen(path, "r");
	fseek(file, 0, SEEK_END);
	size_t size = ftell(file);

	printf("Reading: %s, %d bytes\n", path, size);

	// check that we can at least read a proper header
	if (size < 44) {
		printf("not a WAVE file");
		return 1;
	}
	// read it!
	fseek(file, 0, SEEK_SET);
	
	size_t nread;
	if ((nread = fread(&header, sizeof(WaveHeader), 1, file)) != 1) {
		printf("Error reading file header\n");
		return 1;
	}

	printf("ChunkID: %#08x\n", header.chunk_id); 
	// check chunk_id == "RIFF" in ASCII backwards (RaspberryPI is LE)
	if (header.chunk_id != 0x46464952) {
		printf("not a RIFF header");
		return 1;
	}

	printf("ChunkSize: %d\n", header.chunk_size);

	printf("Format: %#08x\n", header.format);
	// check format == "WAVE" in ASCII backwards (RaspberryPI is LE)
	if (header.format != 0x45564157) {
		printf("not a WAVE file");
		return 1;
	}

	printf("Subchunk1ID: %#08x\n", header.subchunk1_id);
	// check subchunk1_id == "fmt " in ASCII backwards (RaspberryPI is LE)
	if (header.subchunk1_id != 0x20746d66) {
		printf("malformed WAVE file");
		return 1;
	}

	printf("Subchunk1Size: %d\n", header.subchunk1_size);
	if (header.subchunk1_size != 16) {
		printf("malformed WAVE file (or not PCM?)");
		return 1;
	}

	if (header.audio_format == 1) {	
		printf("AudioFormat: PCM\n");
	} else {
		printf("AudioFormat: %d\n", header.audio_format);
		printf("Format not supported");
		return 1;
	}

	printf("NumChannels: %d\n", header.num_channels);
	if (header.num_channels != 1) {
		printf("Only mono is supported currently\n");
		return 1;
	}

	printf("SampleRate: %d sps\n", header.sample_rate);
	if (header.sample_rate != SAMPLE_RATE) {
		printf("Only %d sps supported currently\n", SAMPLE_RATE);
		return 1;
	}

	printf("ByteRate: %d B/s\n", header.byte_rate);
	if (header.byte_rate*8 != header.sample_rate*header.num_channels*header.bits_per_sample) {
		printf("Malformed header\n");
		return 1;
	}

	printf("BlockAlign: %d\n", header.block_align);
	if (header.block_align != 2) {
		printf("Only 16 bit mono blocks supported currently\n");
		return 1;
	}

	printf("BitsPerSample: %d\n", header.bits_per_sample);
	if (header.bits_per_sample != BITS_PER_SAMPLE) {
		printf("Only %d bit audio is supported currently\n", BITS_PER_SAMPLE);
		return 1;
	}

	printf("Subchunk2ID: %#08x\n", header.subchunk2_id);
	// check subchunk2_id == "data" in ASCII backwards (RaspberryPI is LE)
	if (header.subchunk2_id != 0x61746164) {
		printf("malformed WAVE file\n");
		return 1;
	}

	printf("Subchunk2Size: %d\n", header.subchunk2_size);
	size_t expected_file_size = 12 + (8 + header.subchunk1_size) + (8 + header.subchunk2_size);
	// As long as the file is large enough to contain the data segment, its ok.
	if (expected_file_size > size) {
		printf("Subchunk2Size is incorrect\n");
		printf("Expected file size > %d\n", expected_file_size);
		return 1;
	} else if (expected_file_size < size) {
		printf("[WARNING] skipping %d bytes at end of file...\n", size - expected_file_size);
	}

	// Thanks to the above checks, we know the block_align is 16 bit * 1 channel = 2 bytes.
	// Both wave files and the raspberrypi is Little Endian so we can read 16 bit samples directly
	// We use subchunk2_size rather than file size to read the data,
	// since some files seems to contain garbage at the end.
	int16_t *buffer = (int16_t *)malloc(header.subchunk2_size);
	size_t length = header.subchunk2_size / header.block_align;
	if ((nread = fread(buffer, sizeof(int16_t), length, file)) != length) {
		printf("Error reading data segment\n");
		return 1;
	}
	fclose(file);
	
	// size_t i;
	// for (i = 0; i < length; i++) {
	//	printf("%d\n", buffer[i]);
	// }
	return 0;
}
