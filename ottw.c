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

#include "oric.h"

/**
 * Generate a sine wave
 *
 * @param int freq Frequency in Hz
 * @param int periods How many periods to generate 
 * @param int sampling The sampling frequency in Hz 
 * @param struct wav_data The struct to store the samples and their number
 */
void generate_sine(int freq, int periods, int sampling, struct wav_data *wave_data) {
        int i;
        int num_samples = periods * sampling / freq;
	double step_rad = (double) 2 * M_PI * freq / sampling;

	void *_tmp = realloc(wave_data->val, (wave_data->num_samples + num_samples) * sizeof(short));
	wave_data->val = (short *) _tmp;

        for (i=wave_data->num_samples; i < (wave_data->num_samples + num_samples); i++) {
                wave_data->val[i] = (short) (MAX_GAIN * sin(i * step_rad));
        }

	wave_data->num_samples += num_samples;
}


/**
 * Generate a logical 1
 */
void generate_1(struct wav_data *wave_data) {
	generate_sine(FREQUENCY_1, PERIODS_1, SAMPLE_RATE_CD, wave_data);
}


/**
 * Generate a logical 0
 */
void generate_0(struct wav_data *wave_data) {
        generate_sine(FREQUENCY_0, PERIODS_0, SAMPLE_RATE_CD, wave_data);
}


/**
 * Generate output sequence for a byte of data
 *
 * @param int data The input byte
 * @param *int number of samples generated for this byte
 * @returns pointer to allocated array of short
 */
void byte2wav(int data, struct wav_data *wave_data) {
	// The ROM writes one byte as 13 bits on tape: a starting 0, eight bits of data, a parity bit, and three 1 bits
	int i;
	int counter = 0;

	// Start bit
	generate_0(wave_data);

	// Data bits
	for (i=0; i<8; i++) {
		int bit = (int) (pow(2,i));
		if ((data & bit) == bit) {
			counter++;
			generate_1(wave_data);
		}
		else
			generate_0(wave_data);
	}

	// Parity bit
	if ((counter % 2) == 0)
		generate_1(wave_data);
	else
		generate_0(wave_data);

	// End bits
	generate_1(wave_data);
	generate_1(wave_data);
	generate_1(wave_data);
}


void print_usage(char *prog_name) {
	printf("USAGE: %s <input_file> <output_file>\n\n", prog_name);
}


int main(int argc, char**argv) {
	// Check args
	if (argc != 3) { 
		print_usage(argv[0]);
		return 1;
	}

	// Initialize header
	struct wav_header wave_header = {
		'R','I','F','F',
		0,			// Placeholder
		'W','A','V','E',

		'f','m','t', ' ',
		0,
		QUANTIZATION_LINEAR,
		CHANNELS_MONO,
		SAMPLE_RATE_CD,
		0,			// Placeholder
		0,			// Placeholder
		BITS_PER_SAMPLE_16,

		'd','a','t','a',
		0			// Placeholder
	};
        wave_header.byte_rate = SAMPLE_RATE_CD * CHANNELS_MONO * BITS_PER_SAMPLE_16 / 8;
        wave_header.block_align = CHANNELS_MONO * BITS_PER_SAMPLE_16 / 8;

	// Initialize data
	struct wav_data wave_data;
	wave_data.num_samples = 0;
	wave_data.val = malloc(sizeof(short));

	// Generate sound
	//generate_sine(1000, 5000, SAMPLE_RATE_CD, &wave_data);
	//generate_sine(500, 2500, SAMPLE_RATE_CD, &wave_data);
	FILE *fp_in = fopen(argv[1], "r");
	if (!fp_in) {
		printf("ERROR: Input file not found.\n");
		return 1;
	}
	int buffer;
	while ((buffer = fgetc(fp_in)) != EOF) {
		printf(".");
		byte2wav(buffer, &wave_data);
	}
	printf("\n");

	// Complete the header
	wave_header.subchunk1_size = 16;
	wave_header.subchunk2_size = wave_data.num_samples * CHANNELS_MONO * BITS_PER_SAMPLE_16 / 8;
	wave_header.chunk_size = 4 + (8 + wave_header.subchunk1_size) + (8 + wave_header.subchunk2_size);

	// Write output
	FILE *fp_out = fopen(argv[2], "wb");
	fwrite(&wave_header, sizeof(wave_header), 1, fp_out);
	fwrite(wave_data.val, BITS_PER_SAMPLE_16/8, wave_data.num_samples, fp_out);
	fclose(fp_out);

	// Free resources
	free(wave_data.val);

	printf("subchunk2_size: %i\n", wave_header.subchunk2_size);

	return 0;
}


