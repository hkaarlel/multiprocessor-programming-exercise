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

#define BYTES_PER_PIXEL 4 // 3 = RGB (24-bit), 4 = RGBA (32-bit)

#define INPUT_IMG_WIDTH 2940
#define INPUT_IMG_HEIGHT 2016

#define REDUCED_IMG_WIDTH 735
#define REDUCED_IMG_HEIGHT 504

void encodeOneStep(const char* filename, const unsigned char* image, unsigned width, unsigned height)
{
	
}

int main(int argc, const char *argv[]) {
	
	const char *file_1 = argv[1];
	const char *file_2 = argv[2];
	
	unsigned int error;
	unsigned char *image;
	unsigned int width, height;
	
	error = lodepng_decode32_file(&image, &width, &height, file_1);
	if (error) {
		printf("error %u: %s\n", error, lodepng_error_text(error));
		exit(1);
	}
	
	if (width != INPUT_IMG_WIDTH || height != INPUT_IMG_HEIGHT) {
		printf("Expected input image with dimensions %d x %d. Instead got %d x %d", INPUT_IMG_WIDTH, INPUT_IMG_HEIGHT, width, height);
		exit(1);
	}

	// allocate memory for resized image
	unsigned int r_width = width / 4;
	unsigned int r_height = height / 4;
	unsigned int r_image_size = (r_width * r_height);
	
	unsigned int *p_resized_img = (unsigned int *) malloc(sizeof(unsigned int) * r_image_size);

	// put every 4th pixel to p_resized_img
	int byte_ctr = 0;
	for (unsigned int y = 0; y < height; y += 4) {
		for (unsigned int x = 0; x < width; x += 4) {
			unsigned char r = image[BYTES_PER_PIXEL*(y*width + x)];
			unsigned char g = image[BYTES_PER_PIXEL*(y*width + x) + 1];
			unsigned char b = image[BYTES_PER_PIXEL*(y*width + x) + 2];
			unsigned char a = image[BYTES_PER_PIXEL*(y*width + x) + 3];
			
			// inspired by http://stackoverflow.com/questions/6834343/ultra-quick-way-to-concatenate-byte-values
			// this saves the bytes in backwards byte-order i.e. ABGR. Don't know why, but it works 
			unsigned int pixel = (a << 24) | (b << 16) | (g << 8) | r;

			p_resized_img[byte_ctr] = pixel;
			byte_ctr++;
		}
	}
	free(image);

	
	unsigned char *p_greyscale_img = (unsigned char *) malloc(sizeof(unsigned char) * r_image_size);

	byte_ctr = 0;
	for (unsigned int y = 0; y < r_height; y++) {
		for (unsigned int x = 0; x < r_width; x++) {
			unsigned int pixel = p_resized_img[y*r_width + x];
			unsigned char r = pixel;
			unsigned char g = pixel >> 8;
			unsigned char b = pixel >> 16;

			unsigned char greyscale_value = 0.2126f * r + 0.7152f * g + 0.0722f * b;
			p_greyscale_img[byte_ctr] = greyscale_value;
			byte_ctr++;
		}
	}

	/*Encode the resized image*/
	const char *resized_file = "resized.png";
	error = lodepng_encode32_file(resized_file, p_resized_img, r_width, r_height);
	/*if there's an error, display it*/
	if (error) printf("error %u: %s\n", error, lodepng_error_text(error));

	/*Encode the greyscale image*/
	const char *greyscale_file = "greyscale.png";
	error = lodepng_encode_file(greyscale_file, p_greyscale_img, r_width, r_height, LCT_GREY, 8);
	if (error) printf("error %u: %s\n", error, lodepng_error_text(error));
	
	free(p_resized_img);
	free(p_greyscale_img);
	
	return 0;
	
}
