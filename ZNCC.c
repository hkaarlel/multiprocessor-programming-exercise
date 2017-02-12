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

void encode_to_32_file(const char* filename, const unsigned char* image, unsigned width, unsigned height) {
	unsigned error = lodepng_encode32_file(filename, image, width, height);
	if (error) printf("error %u: %s\n", error, lodepng_error_text(error));
}

void encode_to_greyscale_file(const char* filename, const unsigned char* image, unsigned width, unsigned height) {
	unsigned error = lodepng_encode_file(filename, image, width, height, LCT_GREY, 8);
	if (error) printf("error %u: %s\n", error, lodepng_error_text(error));
}

void reduce_img_size(unsigned char *source_img, unsigned source_height, unsigned source_width, unsigned *resized_img) {
	// put every 4th pixel from source_img[] to resized_img[]
	int byte_ctr = 0;
	for (unsigned int y = 0; y < source_height; y += 4) {
		for (unsigned int x = 0; x < source_width; x += 4) {
			unsigned char r = source_img[BYTES_PER_PIXEL*(y*source_width + x)];
			unsigned char g = source_img[BYTES_PER_PIXEL*(y*source_width + x) + 1];
			unsigned char b = source_img[BYTES_PER_PIXEL*(y*source_width + x) + 2];
			unsigned char a = source_img[BYTES_PER_PIXEL*(y*source_width + x) + 3];

			// inspired by http://stackoverflow.com/questions/6834343/ultra-quick-way-to-concatenate-byte-values
			// this saves the bytes in backwards byte-order i.e. ABGR. Don't know why, but it works 
			unsigned int pixel = (a << 24) | (b << 16) | (g << 8) | r;

			resized_img[byte_ctr] = pixel;
			byte_ctr++;
		}
	}
}

void convert_to_greyscale(unsigned *input_img, unsigned input_height, unsigned input_width, unsigned char *output_img) {
	int byte_ctr = 0;
	for (unsigned int y = 0; y < input_height; y++) {
		for (unsigned int x = 0; x < input_width; x++) {
			unsigned int pixel = input_img[y*input_width + x];
			unsigned char r = pixel;
			unsigned char g = pixel >> 8;
			unsigned char b = pixel >> 16;

			unsigned char greyscale_value = 0.2126f * r + 0.7152f * g + 0.0722f * b;
			output_img[byte_ctr] = greyscale_value;
			byte_ctr++;
		}
	}
}

int main(int argc, const char *argv[]) {
	
	const char *file_1 = argv[1];
	const char *file_2 = argv[2];
	
	unsigned int error;

	unsigned int img1_width, img1_height;
	unsigned char *image_1;
	
	error = lodepng_decode32_file(&image_1, &img1_width, &img1_height, file_1);
	if (error) {
		printf("error %u: %s\n", error, lodepng_error_text(error));
		exit(1);
	}

	if (img1_width != INPUT_IMG_WIDTH || img1_height != INPUT_IMG_HEIGHT) {
		printf("Expected input image with dimensions %d x %d. Instead got %d x %d", INPUT_IMG_WIDTH, INPUT_IMG_HEIGHT, img1_width, img1_height);
		exit(1);
	}
	unsigned int img2_width, img2_height;
	unsigned char *image_2;
	error = lodepng_decode32_file(&image_2, &img2_width, &img2_height, file_2);
	if (error) {
		printf("error %u: %s\n", error, lodepng_error_text(error));
		exit(1);
	}
	
	if (img1_width != img2_width || img1_height != img2_height) {
		printf("Images have to be the same size! Exiting...");
		exit(1);
	}

	unsigned int width = img1_width;
	unsigned int height = img1_height;
	
	unsigned int r_width = width / 4;
	unsigned int r_height = height / 4;
	unsigned int r_image_size = (r_width * r_height);

	// allocate memory for resized images
	unsigned int *p_resized_img_1 = (unsigned int *) malloc(sizeof(unsigned int) * r_image_size);
	unsigned int *p_resized_img_2 = (unsigned int *) malloc(sizeof(unsigned int) * r_image_size);

	// create resized images from original image
	reduce_img_size(image_1, height, width, p_resized_img_1);
	reduce_img_size(image_2, height, width, p_resized_img_2);
	// original images no longer needed
	free(image_1);
	free(image_2);

	// allocate memory for greyscale image
	unsigned char *p_greyscale_img_1 = (unsigned char *) malloc(sizeof(unsigned char) * r_image_size);
	unsigned char *p_greyscale_img_2 = (unsigned char *) malloc(sizeof(unsigned char) * r_image_size);


	// create greyscale images from resized images
	convert_to_greyscale(p_resized_img_1, r_height, r_width, p_greyscale_img_1);
	convert_to_greyscale(p_resized_img_2, r_height, r_width, p_greyscale_img_2);
	
	// FOR DEBUGGING
	// Encode the resized images
	const char *resized_filename_1 = "resized1.png";
	const char *resized_filename_2 = "resized2.png";
	encode_to_32_file(resized_filename_1, p_resized_img_1, r_width, r_height);
	encode_to_32_file(resized_filename_2, p_resized_img_2, r_width, r_height);
	
	// FOR DEBUGGING
	// Encode the greyscale images
	const char *greyscale_file_1 = "greyscale1.png";
	const char *greyscale_file_2 = "greyscale2.png";
	encode_to_greyscale_file(greyscale_file_1, p_greyscale_img_1, r_width, r_height);
	encode_to_greyscale_file(greyscale_file_2, p_greyscale_img_2, r_width, r_height);
	
	free(p_resized_img_1);
	free(p_resized_img_2);

	free(p_greyscale_img_1);
	free(p_greyscale_img_2);
	
	return 0;
	
}
