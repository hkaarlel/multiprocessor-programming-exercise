#include <iostream>
#include <vector>

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

using std::cout;
using std::vector;

struct GreyscaleImage {

	unsigned width, height;
	vector<unsigned char> pixels;

	unsigned char get_pixel(unsigned x, unsigned y) {
		if (x >= width || y >= height) {
			return 0;
		}
		return pixels[y*width + x];
	}
};

void decodeFile(const char* filename, vector<uint8_t> &image, unsigned &width, unsigned &height) {

	//decode
	unsigned error = lodepng::decode(image, width, height, filename);

	//if there's an error, display it
	if (error) std::cout << "decoder error " << error << ": " << lodepng_error_text(error) << std::endl;
}

void encode_to_32_file(const char* filename, vector<unsigned char> &image, unsigned width, unsigned height) {
	unsigned error = lodepng::encode(filename, image, width, height);
	if (error) std::cout << "encoder error " << error << ": " << lodepng_error_text(error) << std::endl;
}

void encode_to_greyscale_file(const char* filename, vector<unsigned char> &image, unsigned width, unsigned height) {
	unsigned error = lodepng::encode(filename, image, width, height, LCT_GREY, 8);
	if (error) printf("error %u: %s\n", error, lodepng_error_text(error));
}

void reduce_img_size(vector<unsigned char> &source_img, unsigned source_height, unsigned source_width, vector<unsigned char> &resized_img) {
	// put every 4th pixel from source_img[] to resized_img[]
	for (unsigned int y = 0; y < source_height; y += 4) {
		for (unsigned int x = 0; x < source_width; x += 4) {
			unsigned char r = source_img[BYTES_PER_PIXEL*(y*source_width + x)];
			unsigned char g = source_img[BYTES_PER_PIXEL*(y*source_width + x) + 1];
			unsigned char b = source_img[BYTES_PER_PIXEL*(y*source_width + x) + 2];
			unsigned char a = source_img[BYTES_PER_PIXEL*(y*source_width + x) + 3];
			resized_img.push_back(r);
			resized_img.push_back(g);
			resized_img.push_back(b);
			resized_img.push_back(a);
		}
	}
}

void convert_to_greyscale(vector<unsigned char> &input_img, GreyscaleImage &output_img) {
	for (unsigned byte = 0; byte < input_img.size(); byte += 4) {
		unsigned char r = input_img[byte];
		unsigned char g = input_img[byte + 1];
		unsigned char b = input_img[byte + 2];

		unsigned char greyscale_value = 0.2126f * r + 0.7152f * g + 0.0722f * b;
		output_img.pixels.push_back(greyscale_value);
	}
}

int main(int argc, const char *argv[]) {
	
	const char* filename_1 = argc > 1 ? argv[1] : "im0.png";
	const char* filename_2 = argc > 2 ? argv[2] : "im1.png";

	unsigned int L_img_width, L_img_height;
	vector<unsigned char> left_img = vector<unsigned char>();
	decodeFile(filename_1, left_img, L_img_width, L_img_height);
	
	if (L_img_width != INPUT_IMG_WIDTH || L_img_height != INPUT_IMG_HEIGHT) {
		printf("Expected input image with dimensions %d x %d. Instead got %d x %d", INPUT_IMG_WIDTH, INPUT_IMG_HEIGHT, L_img_width, L_img_height);
		exit(1);
	}

	unsigned int R_img_width, R_img_height;
	vector<unsigned char> right_img = vector<unsigned char>();

	decodeFile(filename_2, right_img, R_img_width, R_img_height);
	
	if (L_img_width != R_img_width || L_img_height != R_img_height) {
		printf("Images have to be the same size! Exiting...");
		exit(1);
	}

	unsigned int width = L_img_width;
	unsigned int height = L_img_height;
	
	unsigned int scaled_width = width / 4;
	unsigned int scaled_height = height / 4;

	// allocate memory for resized images
	vector<unsigned char> left_img_scaled = vector<unsigned char>();
	vector<unsigned char> right_img_scaled = vector<unsigned char>();

	// create resized images from original image
	reduce_img_size(left_img, height, width, left_img_scaled);
	reduce_img_size(right_img, height, width, right_img_scaled);

	// allocate memory for greyscale image
	GreyscaleImage Left_img, Right_img;

	// create greyscale images from resized images
	convert_to_greyscale(left_img_scaled, Left_img);
	convert_to_greyscale(right_img_scaled, Right_img);
	
	// FOR DEBUGGING
	// Encode the resized images
	const char *resized_filename_1 = "resized1.png";
	const char *resized_filename_2 = "resized2.png";
	encode_to_32_file(resized_filename_1, left_img_scaled, scaled_width, scaled_height);
	encode_to_32_file(resized_filename_2, right_img_scaled, scaled_width, scaled_height);
	
	// FOR DEBUGGING
	// Encode the greyscale images
	const char *greyscale_file_1 = "greyscale1.png";
	const char *greyscale_file_2 = "greyscale2.png";
	encode_to_greyscale_file(greyscale_file_1, Left_img.pixels, scaled_width, scaled_height);
	encode_to_greyscale_file(greyscale_file_2, Right_img.pixels, scaled_width, scaled_height);
	
	return 0;
	
}
