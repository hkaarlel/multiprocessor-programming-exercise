#include <iostream>
#include <vector>
#include <unordered_map>
#include <cmath>

#include "lodepng.h"

/*
	This file implements the Zero-mean Normalized Cross Correlation algorithm in C.
	Phase 1 of the course work for the Multiprocessor Programming -course.
	Authors:
		Heikki Kaarlela, student nr. 2316624
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
#define BLOCK_SIZE 15 // Has to be uneven i.e. 9, 15, 25

#define BYTES_PER_PIXEL 4 // 3 = RGB (24-bit), 4 = RGBA (32-bit)

#define INPUT_IMG_WIDTH 2940
#define INPUT_IMG_HEIGHT 2016

#define REDUCED_IMG_WIDTH 735
#define REDUCED_IMG_HEIGHT 504

#define MAX_DISP 260

using std::vector;
using std::unordered_map;

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

vector<unsigned char> get_window_around_point(GreyscaleImage &image, unsigned center_x, unsigned center_y, int block_radius) {

	vector<unsigned char> window_pixels;

	for (unsigned y = center_y - block_radius; y <= center_y + block_radius; y++) {
		for (unsigned x = center_x - block_radius; x <= center_x + block_radius; x++) {
			window_pixels.push_back(image.get_pixel(x, y));
		}
	}
	return window_pixels;
}

unordered_map<unsigned, float> calc_window_averages(GreyscaleImage &image, int block_radius) {

	unsigned pixel_number;
	unordered_map<unsigned, float> avg_map;
	for (int y = 0; y < (int)image.height; y++) {
		if (y - block_radius < 0 || y + block_radius >= (int)image.height) {
			continue;
		}
		for (int x = 0; x < (int)image.width; x++) {
			if (x - block_radius < 0 || x + block_radius >= (int)image.width) {
				continue;
			}

			vector<unsigned char> window_pixels = get_window_around_point(image, x, y, block_radius);

			unsigned sum = 0;
			for (unsigned i = 0; i < window_pixels.size(); i++) {
				sum += window_pixels[i];
			}
			float mean = (float)sum / (float)(BLOCK_SIZE * BLOCK_SIZE);
			pixel_number = y*image.width + x;
			avg_map.insert({pixel_number, mean});
		}
	}
	return avg_map;
}

vector<unsigned char> calc_disparity_map(GreyscaleImage &src_img, unordered_map<unsigned, float> &src_img_window_avgs, 
										GreyscaleImage &ref_img, unordered_map<unsigned, float> &ref_img_window_avgs, 
										int min_disp, int max_disp, int block_radius)
{
	unsigned pixel_number;
	unsigned ref_img_pixel_index;
	vector<unsigned char> disparity_map;
	// For each pixel in source image...
	for (int y = 0; y < (int)src_img.height; y++) {
		for (int x = 0; x < (int)src_img.width; x++) {
			// Only consider pixels that can be windowed
			if (x - block_radius < 0 || x + block_radius >= (int)src_img.width ||
				y - block_radius < 0 || y + block_radius >= (int)src_img.height) {
				//std::cout << "At column " << x << ", row " << y <<", writing 0 to disparity map." << std::endl;
				disparity_map.push_back(0);
				continue;
			}
			// Get pixel index to get window mean value from map
			pixel_number = y*src_img.width + x;
			// Get mean of pixel's window 
			float src_window_mean = src_img_window_avgs[pixel_number];
			// Get pixel values of window (length = blocksize**2 = 81)
			vector<unsigned char> src_window_pixels = get_window_around_point(src_img, x, y, block_radius);

			float zncc_numerator_sum = 0;
			float zncc_denominator_sum_L = 0;
			float zncc_denominator_sum_R = 0;
			float zncc = 0;
			float best_zncc = 0;
			unsigned char best_disparity_value = 0;
			// For each disparity value...
			for (int disparity = min_disp; disparity <= max_disp; disparity++) {
				int offset = x - disparity; // offset = value of x in reference image
				if (offset - block_radius < 0 || offset + block_radius >= (int)src_img.width) {
					break; // Make sure that disparity values dont move the window outside of image.
				}
				ref_img_pixel_index = y*ref_img.width + offset;
				float ref_window_mean = ref_img_window_avgs[ref_img_pixel_index];
				vector<unsigned char> ref_window_pixels = get_window_around_point(ref_img, offset, y, block_radius);
				// For pixel in window...
				for (unsigned i = 0; i < src_window_pixels.size(); i++) {
					
					float src_window_diff = src_window_pixels[i] - src_window_mean;

					float ref_window_diff = ref_window_pixels[i] - ref_window_mean;

					zncc_numerator_sum += src_window_diff * ref_window_diff;
					zncc_denominator_sum_L += pow(src_window_diff, 2);
					zncc_denominator_sum_R += pow(ref_window_diff, 2);
				}
				// Calculate ZNCC, store highest disparity value
				zncc = zncc_numerator_sum / (sqrt(zncc_denominator_sum_L) * sqrt(zncc_denominator_sum_R));
				if (zncc > best_zncc) {
					best_zncc = zncc;
					best_disparity_value = abs(disparity);
				}
			}
			//std::cout << "Disparity for (" << x << "," << y << ") is " << (int)best_disparity_value << std::endl;
			disparity_map.push_back(best_disparity_value);
		}
	}
	return disparity_map;
}

GreyscaleImage cross_check(GreyscaleImage &left_image, GreyscaleImage &right_image, int threshold) {
	//Käy läpi kaksi listaa, vertailee keskenään, jos yli thresholdin niin vaihtaa tilalle nollan
	GreyscaleImage cross_checked_image;
	cross_checked_image.height = left_image.height;
	cross_checked_image.width = left_image.width;
	cross_checked_image.pixels = left_image.pixels;
	for (int x = 0; x < (int)left_image.pixels.size(); x++) {
		if (abs(left_image.pixels[x] - right_image.pixels[x]) > threshold)
			cross_checked_image.pixels[x] = 0;
	}
	return cross_checked_image;
}

GreyscaleImage occlusion_filling(GreyscaleImage &image) {
	//Käy läpi listan, jos löytyy nolla niin laittaa tilalle sitä edeltävän luvun. Jos eka on nolla niin käy läpi kunnes löytyy eka ei-nolla ja laittaa sen ekaksi. Hyvin yksiulotteinen
	GreyscaleImage occl_filled_image;
	occl_filled_image.height = image.height;
	occl_filled_image.width = image.width;
	occl_filled_image.pixels = vector<unsigned char>();

	unsigned char first_nonzero;
	for (int i = 0; ; i++) {
		if (image.pixels[i] != 0) {
			first_nonzero = image.pixels[i];
			break;
		}
	}
	unsigned char near_nonzero = first_nonzero;

	for (int y = 0; y < (int)image.height; y++) {
		for (int x = 0; x < (int)image.width; x++) {
			unsigned char pixel_val = image.get_pixel(x, y);
			if (pixel_val == 0) {
				occl_filled_image.pixels.push_back(near_nonzero);
			}
			else {
				near_nonzero = pixel_val;
				occl_filled_image.pixels.push_back(pixel_val);
			}
		}
	}
	return occl_filled_image;
	/*
	voi tehä paremmankin algoritmin, tämä ei esim. ota huomioon sitä et onko rivin ihan eka pikseli, jollon sitä ei kannata ottaa edellisestä pikselistä
			if (x > 0) {
				occl_filled_image[x] = occl_filled_image[x-1];
			}
			if (x = 0) {
				int i = 0;
				while (true) {
					i++;
					if (occl_filled_image[i] != 0) {
						occl_filled_image[x] = occl_filled_image[i];
						break;
					}
				}
			}
	*/
}

GreyscaleImage normalise_disparity_map(GreyscaleImage &disp_map, int max_disp) {
	int max_value = 255;
	GreyscaleImage normalised;
	normalised.height = disp_map.height;
	normalised.width  =disp_map.width;
	normalised.pixels = vector<unsigned char>();
	for (vector<unsigned char>::iterator it = disp_map.pixels.begin(); it != disp_map.pixels.end(); ++it) {
		unsigned char val = *it;
		unsigned char normalized_val = (val * max_value) / max_disp;
		normalised.pixels.push_back(normalized_val);
	}
	return normalised;
}

int main(int argc, const char *argv[]) {
	
	const char* filename_1 = argc > 1 ? argv[1] : "im0.png";
	const char* filename_2 = argc > 2 ? argv[2] : "im1.png";

	// Read im0 to memory
	unsigned int L_img_width, L_img_height;
	vector<unsigned char> left_img = vector<unsigned char>();
	decodeFile(filename_1, left_img, L_img_width, L_img_height);
	
	if (L_img_width != INPUT_IMG_WIDTH || L_img_height != INPUT_IMG_HEIGHT) {
		std::cout << "Expected input image with dimensions " << INPUT_IMG_WIDTH << " x " << INPUT_IMG_HEIGHT << 
					 ". Instead got " << L_img_width << " x " << L_img_height << std::endl;
		exit(1);
	}

	// Read im1 to memory
	unsigned int R_img_width, R_img_height;
	vector<unsigned char> right_img = vector<unsigned char>();
	decodeFile(filename_2, right_img, R_img_width, R_img_height);
	
	if (L_img_width != R_img_width || L_img_height != R_img_height) {
		std::cout << "Images have to be the same size! Exiting..." << std::endl;
		exit(1);
	}

	unsigned int width = L_img_width;
	unsigned int height = L_img_height;
	
	unsigned int scaled_width = width / 4;
	unsigned int scaled_height = height / 4;

	// Allocate memory for resized images
	vector<unsigned char> left_img_scaled = vector<unsigned char>();
	vector<unsigned char> right_img_scaled = vector<unsigned char>();

	// Create resized images from original image
	std::cout << "Resizing images..." << std::endl;
	reduce_img_size(left_img, height, width, left_img_scaled);
	std::cout << "Image 1 done... ";
	reduce_img_size(right_img, height, width, right_img_scaled);
	std::cout << "Image 2 done" << std::endl;

	// Allocate memory for greyscale image
	GreyscaleImage Left_img = { scaled_width, scaled_height };
	GreyscaleImage Right_img = { scaled_width, scaled_height };

	// Create greyscale images from resized images
	std::cout << "Creating greyscale images..." << std::endl;
	convert_to_greyscale(left_img_scaled, Left_img);
	std::cout << "Image 1 done... ";
	convert_to_greyscale(right_img_scaled, Right_img);
	std::cout << "Image 2 done" << std::endl;

	// Check that greyscale images still have the correct dimensions
	if (Left_img.pixels.size() != scaled_width*scaled_height 
		|| Right_img.pixels.size() != scaled_width*scaled_height) {
		std::cout << "Greyscale image has wrong dimensions! Aborting..." << std::endl;
	}

	// Create unordered_map (pixelIndex, windowMean) of window means for both images
	int block_radius = (BLOCK_SIZE - 1) / 2;
	std::cout << "Mapping window averages..." << std::endl;
	unordered_map<unsigned, float> left_img_window_avgs = calc_window_averages(Left_img, block_radius);
	std::cout << "Image 1 done... ";
	unordered_map<unsigned, float> right_img_window_avgs = calc_window_averages(Right_img, block_radius);
	std::cout << "Image 2 done" << std::endl;

	// Calculate disparity maps using ZNCC
	int max_disp = MAX_DISP / 4;
	std::cout << "Calculating disparity maps..." << std::endl;
	vector<unsigned char> L2R_disparity_map_values = calc_disparity_map(Left_img, left_img_window_avgs, Right_img, right_img_window_avgs, 0, max_disp, block_radius);
	std::cout << "Image 1 done... ";
	vector<unsigned char> R2L_disparity_map_values = calc_disparity_map(Right_img, right_img_window_avgs, Left_img, left_img_window_avgs, -max_disp, 0, block_radius);
	std::cout << "Image 2 done" << std::endl;

	GreyscaleImage L_disparity_map = { scaled_width, scaled_height, L2R_disparity_map_values };
	GreyscaleImage R_disparity_map = { scaled_width, scaled_height, R2L_disparity_map_values };

	// Post-process images
	std::cout << "Postprocessing..." << std::endl;
	GreyscaleImage x_checked = cross_check(L_disparity_map, R_disparity_map, 8);
	std::cout << "cross check done... ";
	GreyscaleImage occ_filled = occlusion_filling(x_checked);
	std::cout << "occlusion filling done" << std::endl;

	std::cout << "Normalizing pixel values..." << std::endl;
	GreyscaleImage normalized = normalise_disparity_map(occ_filled, max_disp);
	
	//vector<unsigned char> R2L_disparity_map_normalised = normalise_disparity_map(R2L_disparity_map_values, max_disp);
	/*
	// FOR DEBUGGING
	// Encode the resized images
	const char *resized_filename_1 = "resized1.png";
	const char *resized_filename_2 = "resized2.png";
	encode_to_32_file(resized_filename_1, left_img_scaled, scaled_width, scaled_height);
	encode_to_32_file(resized_filename_2, right_img_scaled, scaled_width, scaled_height);
	*/
	// FOR DEBUGGING
	// Encode the greyscale images
	std::cout << "Writing output images to disk..." << std::endl;
	const char *greyscale_file_1 = "greyscale1.png";
	encode_to_greyscale_file(greyscale_file_1, normalized.pixels, scaled_width, scaled_height);

	//const char *greyscale_file_2 = "greyscale2.png";
	//encode_to_greyscale_file(greyscale_file_2, R_disparity_map.pixels, scaled_width, scaled_height);
	std::cout << "All done!" << std::endl;
	int a = 0;
	return 0;
	
}
