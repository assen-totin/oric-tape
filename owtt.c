/*
 *  WAV file co nverter for Oric Atmos and many other
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

#include "oric.h"

int get_frequency(struct r_data *runtime_data) {
	if ((runtime_data->counter >= (runtime_data->threshold_0 - 3)) && (runtime_data->counter <= (runtime_data->threshold_0 + 3)))
		return 0;
	else if ((runtime_data->counter >= (runtime_data->threshold_1 - 3)) && (runtime_data->counter <= (runtime_data->threshold_1 + 3)))
		return 1;
	// When switching from 2400 Hz to 1200 Hz and back, Oric produces a half-period of both... 
	// so if we detect a frequency in the midlle, just check what are we loking for - a 1 or a 0.
	else if ((runtime_data->counter >=(runtime_data->threshold_0 + runtime_data->threshold_1)/2 - 3) && (runtime_data->counter <=(runtime_data->threshold_0 + runtime_data->threshold_1)/2 + 3)) {
		if (runtime_data->counter_1 > runtime_data->counter_0)
			return 1;
		else
			return 0;
	}
	else
		printf("Unknown frequency detected, counter is: %u, counter_0 is %u, counter_1 is %u\n", runtime_data->counter, runtime_data->counter_0, runtime_data->counter_1);
	return -1;
}


/**
 * Generate output sequence for a byte of data
 *
 * @param int data The input byte
 * @param *int number of samples generated for this byte
 * @returns pointer to allocated array of short
 */
void process_sample(short sample, struct r_data *runtime_data) {
	int freq = 0;
	// The ROM writes one byte as 13 bits on tape: a starting 0, eight bits of data, a parity bit, and three 1 bits

	// Check if the sample is positive and if the previous was negative; 
	// if so, calc the frequency of the last period (from the counter) and reset the counter

	if ((runtime_data->prev_sample > 0) && (sample <= 0)) {
		freq = get_frequency(runtime_data);
		runtime_data->counter = 0;

		// Add the period to the counters
		if (freq == 1)
			runtime_data->counter_1++;
		else if (freq == 0)
			runtime_data->counter_0++;

		// Check if we've had enough for a bit
		if (runtime_data->counter_1 == PERIODS_1) {
			runtime_data->counter_0 = 0;
			runtime_data->counter_1 = 0;
			// Give a 1
			runtime_data->bits[runtime_data->bits_pos] = 1;
			runtime_data->bits_pos++;
		}
		else if (runtime_data->counter_0 == PERIODS_0) {
			runtime_data->counter_0 = 0;
			runtime_data->counter_1 = 0;
			// Give a 0
			runtime_data->bits[runtime_data->bits_pos] = 0;
			runtime_data->bits_pos++;
		}
	}

	runtime_data->prev_sample = sample;
	runtime_data->counter++;
}


void print_usage(char *prog_name) {
	printf("USAGE: %s <input_file> <output_file>\n\n", prog_name);
}


unsigned int get_uint(FILE *fp) {
	unsigned int res;
	fread(&res, 4, 1, fp);
	return res;
}


unsigned short get_ushort(FILE *fp) {
	unsigned short res;
	fread(&res, 2, 1, fp);
	return res;
}

short get_short(FILE *fp) {
	short res;
	fread(&res, 2, 1, fp);
	return res;
}

int main(int argc, char**argv) {
	struct wav_header wave_header = {
		'R','I','F','F',
		0,			// Placeholder
		'W','A','V','E',

		'f','m','t', ' ',
		0,
		0, 			// Placeholder: expected QUANTIZATION_LINEAR,
		0, 			// Placeholder: expected CHANNELS_MONO,
		0, 			// Placeholder: expected SAMPLE_RATE_CD,
		0,			// Placeholder
		0,			// Placeholder
		BITS_PER_SAMPLE_16,

		'd','a','t','a',
		0			// Placeholder
	};

	struct r_data runtime_data = {
		0, 
		0,
		0,
		0,
		0,
		0,
		0, 
		0,
		0, 
		0
	};

	// Check args
	if (argc != 3) { 
		print_usage(argv[0]);
		return 1;
	}

	// Open input file
	FILE *fp_in = fopen(argv[1], "r");
	if (!fp_in) {
		printf("ERROR: Input file not found.\n");
		return 1;
	}

	// Check RIFF Header
	char s[1024];
	fgets(&s[0], 5, fp_in);
	if (strncmp(&s[0], &wave_header.chunk_id[0], 4)) {
		printf("Input file is not a RIFF file: expected '%s', got '%s'\n", &wave_header.chunk_id[0], &s[0]);
		exit(1);
	}

	// Get chunk size
	wave_header.chunk_size =  get_uint(fp_in);
	printf("Header chunk_size: %i\n", wave_header.chunk_size);
	
	// Get WAV header
	fgets(&s[0], 5, fp_in);
	if (strncmp(&s[0], &wave_header.format[0], 4)) {
		printf("Input file is not a WAVE file: expected '%s', got '%s'\n", &wave_header.format[0], &s[0]);
		exit(1);
	}	

	// Get fmt header
	fgets(&s[0], 5, fp_in);
	if (strncmp(&s[0], &wave_header.subchunk1_id[0], 4)) {
		printf("Format chunk not found in header: expected '%s', found '%s'\n", &wave_header.subchunk1_id[0], &s[0]);
		exit(1);
	}	
	
	// Subchunk 1 size
	wave_header.subchunk1_size =  get_uint(fp_in);
	printf("Header subchunk1_size: %u\n", wave_header.subchunk1_size);

	// Audio format
	wave_header.audio_format =  get_ushort(fp_in);
	if (wave_header.audio_format != QUANTIZATION_LINEAR) {
		printf("Unknown quantization format: expected '%u', got '%u'\n", QUANTIZATION_LINEAR, wave_header.audio_format);
		//exit(1);
	}

	// Number of channels
	wave_header.num_channels =  get_ushort(fp_in);
	if (wave_header.num_channels != CHANNELS_MONO) {
		printf("Unknown number of channels: expected '%u', got '%u'\n", CHANNELS_MONO, wave_header.num_channels);
		//exit(1);
	}

	// Sample rate
	wave_header.sample_rate =  get_ushort(fp_in);
	if (wave_header.sample_rate != SAMPLE_RATE_CD) {
		printf("Unknown sample rate: expected '%u', got '%u'\n", SAMPLE_RATE_CD, wave_header.sample_rate);
		//exit(1);
	}

/*
	// Byte rate
	wave_header.byte_rate =  get_int(fp_in);
	printf("Byte rate: %u\n", wave_header.sample_rate);

	// Block align
	wave_header.block_align =  get_short(fp_in);
	printf("Block align: %u\n", wave_header.block_align);

	// Bits per sample
	wave_header.bits_per_sample =  get_short(fp_in);
	printf("Bits per sample: %u\n", wave_header.bits_per_sample);

	unsigned short unf = get_short(fp_in);
	printf("WTF: %u\n", unf);
*/

	// Skip rest of chunk 1, as fields may differ - be we don't actually need them
	fgets(&s[0], wave_header.subchunk1_size - 5, fp_in);

	// Get data header
	fgets(&s[0], 5, fp_in);
	if (strncmp(&s[0], &wave_header.subchunk2_id[0], 4)) {
		printf("Data chunk not found in header: expected '%s', found '%s'\n", &wave_header.subchunk2_id[0], &s[0]);
		exit(1);
	}	

	// Subchunk 2 size
	wave_header.subchunk2_size =  get_uint(fp_in);
	printf("Header subchunk2_size: %u\n", wave_header.subchunk2_size);

	runtime_data.threshold_0 = wave_header.sample_rate / FREQUENCY_0;
	runtime_data.threshold_1 = wave_header.sample_rate / FREQUENCY_1;

	// Total bits of data: No of samples / sample_rate * 300
	// We register the same number of bytes in order to store each bit as an int
	runtime_data.bits_count = wave_header.subchunk2_size/2 / (float)wave_header.sample_rate * 300;
	runtime_data.bits = (int *) malloc(runtime_data.bits_count);
	memset(runtime_data.bits, '\0', runtime_data.bits_count);

	// Total bytes for the final data
	runtime_data.bytes_count = runtime_data.bits_count / 13;
	char bytes[runtime_data.bytes_count];
	int bytes_pos = 0;

	printf("Threshold for 0: %u\n", runtime_data.threshold_0);
	printf("Threshold for 1: %u\n", runtime_data.threshold_1);
	printf("Expected bits: %u\n", runtime_data.bits_count);
	printf("Expected bytes: %u\n", runtime_data.bytes_count);

	// Read the rest of the file, process byte by byte
	int i;
	short sample;
	for (i=0; i<(wave_header.subchunk2_size/2); i++) {
		sample = get_short(fp_in);
		process_sample(sample, &runtime_data);
	}

	printf("Processing complete - bits: %u\n", runtime_data.bits_pos);

	// Reprocess the bits array, try to assemble bytes
	int j, data_byte;
	for (i=0; i<runtime_data.bits_pos; i++) {
		if ((runtime_data.bits[i] == 0) &&
		(runtime_data.bits[i+10] == 1) &&
		(runtime_data.bits[i+11] == 1) &&
		(runtime_data.bits[i+12] == 1)) {
			// We have a byte!
			bytes[bytes_pos] = 0;
			for (j=0; j<8; j++)
				bytes[bytes_pos] += runtime_data.bits[i+1+j] * pow(2, j);
			bytes_pos++;
			i+=12;
		}

	}

	// Write output
	FILE *fp_out = fopen(argv[2], "wb");
	fwrite(bytes, 1, bytes_pos, fp_out);
	fclose(fp_out);

	return 0;
}


