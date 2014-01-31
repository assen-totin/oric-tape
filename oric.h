/*
 *  Data files to WAV file converter for Orci Atmos and many other
 *  8-bit computers.
 *
 *  Written by Assen Totin <assen.totin@gmail.com>
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 *  USA.
 */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>

enum {
	QUANTIZATION_NONE = 0,
	QUANTIZATION_LINEAR
};

enum {
	CHANNELS_NONE = 0,
	CHANNELS_MONO,
	CHANNELS_STEREO
};

#define SAMPLE_RATE_CD 44100

#define BITS_PER_SAMPLE_16 16

#define MAX_GAIN 32000	// No more than 32768, but leave the DAC some margin of error

#define FREQUENCY_1 2400	// Hz 
#define PERIODS_1 8		// Periods for a logical 1

#define FREQUENCY_0 1200 	// Hz
#define PERIODS_0 4		// Periods for a logical 0

struct wav_header {
	char chunk_id[4];		// "RIFF" in ASCII form (0x52494646 big-endian form)
	unsigned int chunk_size;	// 4 + (8 + SubChunk1Size) + (8 + SubChunk2Size)
	char format[4];			// "WAVE" (0x57415645 big-endian form)

	char subchunk1_id[4];		// "fmt " (0x666d7420 big-endian form)
	unsigned int subchunk1_size;	// 16 for PCM.  This is the size of the rest of the Subchunk which follows this number
	unsigned short audio_format;	// PCM = 1 (i.e. Linear quantization). Other values indicate some for of compression
	unsigned short num_channels;	// Mono = 1, Stereo = 2, etc.
	unsigned int sample_rate;	// 8000, 44100, etc.
	unsigned int byte_rate;		// = SampleRate * NumChannels * BitsPerSample/8
	unsigned short block_align;	// = NumChannels * BitsPerSample/8: The number of bytes for one sample including all channels
	unsigned short bits_per_sample;	// 8 bits = 8, 16 bits = 16, etc.

	char subchunk2_id[4];		// "data" (0x64617461 big-endian form)
	unsigned int subchunk2_size;	// = NumSamples * NumChannels * BitsPerSample/8: number of bytes in the data (size of the read of the subchunk following this number)
};

struct wav_data {
	int num_samples;
	short *val;
};

struct r_data {
	short prev_sample;
	int counter;
	int threshold_0;
	int threshold_1;
	int counter_0;
	int counter_1;
	int bits_count;
	int *bits;
	int bits_pos;
	int bytes_count;
};

