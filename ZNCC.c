#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lodepng.h"

/*
	This file implements the Zero-mean Normalized Cross Correlation algorithm in C.
	Phase 1 of the course work for the Multiprocessor Programming -course.
	Authors:
		Heikki Kaarlela, student nr. <#TODO>
		Eero Paavola, student nr. 2195447

	link to doc: http://tinyurl.com/htyoa8u
	
	STEPS:

	1. read input images to memory
		a. input imgs are 32-bit RGBA .png images

	2. resize images to 1/16 of the original size (From 2940x2016 to 735x504)
		NOTE! this also means that ndisp has to be scaled (260 / 4 = 65 I think...?)
		a. naive method: take every 4th pixel, discard rest
		b. better: calculate mean of 4x4 blocks -> value of new pixel
		

	3. make images greyscale (8-bits/pixel)
		a. see if doing this manually is the same as using
		lodepng_decode_file(img, width, height,	fname, LodePNGColorType colortype, unsigned bitdepth),
		where colortype = 0, bitdepth = 8

	4. compute disparity images (2)
		a. first using img2 as "filter" to img1 and then vice versa

	5. apply postprocessing, producing one output img
		a. first crosscheck
		b. then occlusion filling

	6. check that the output file looks ok
		a. output img should be 8-bit greyscale image named 'depthmap.png'
		b. fiddle with block value in step 5 (15, 25...) and see what looks best

	7. benchmark the program using QueryPerformanceCounter() and VS profiler (compare)

*/

/*
HiFi:
	- handle input image of any dimensions
	- resize image to any fraction
*/
#define BLOCK_SIZE 9

#define BYTES_PER_PIXEL 3 // 3 = RGB (24-bit), 4 = RGBA (32-bit)

#define INPUT_IMG_WIDTH 2940
#define INPUT_IMG_HEIGHT 2016

#define REDUCED_IMG_WIDTH 735
#define REDUCED_IMG_HEIGHT 504

typedef struct RGB_pixel {
	unsigned char red, green, blue;
} RGB_pixel;

// useless struct? only for naming
typedef struct greyscale_pixel {
	unsigned char value;
} greyscale_pixel;


int main(int argc, const char *argv[]) {
	
	const char *file_1 = argv[1];
	const char *file_2 = argv[2];
	
	// size_t == basically a better version of 'unsigned int'.
	// Use it whenever dealing with numbers that cannot be < 0.
	unsigned int error;
	unsigned char *image;
	size_t width, height;
	
	// read file_1 to buffer
	error = lodepng_decode24_file(&image, &width, &height, file_1);
	if (error) {
		printf("error %u: %s\n", error, lodepng_error_text(error));
		exit(1);
	}
	
	if (width != INPUT_IMG_WIDTH || height != INPUT_IMG_HEIGHT) {
		printf("Expected input image with dimensions %d x %d. Instead got %d x %d", INPUT_IMG_WIDTH, INPUT_IMG_HEIGHT, width, height);
		exit(1);
	}

	// map byte buffer to array of structs (RGB_pixel) i.e. [FF, 02, 5A, FF, 05, 06] -> [struct(FF,02,5A), struct(FF,05,06)]
	// FIXME: needs to be done dynamically (malloc) to avoid stack overflow
	RGB_pixel reduced_img[REDUCED_IMG_WIDTH * REDUCED_IMG_HEIGHT];

	// get rid of every 4th pixel on both axis

	// convert RGB_pixel structs to greyscale pixels (do they even need a struct?)
	
	
	free(image);
	
	return 0;
	getchar();
}
