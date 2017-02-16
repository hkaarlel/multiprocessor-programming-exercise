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
#define BLOCK_SIZE 9 // Has to be uneven i.e. 9, 15, 25

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
	unsigned ref_img_pixel_number;
	vector<unsigned char> disparity_map;
	// For each pixel in source image...
	for (int y = 0; y < (int)src_img.height; y++) {
		for (int x = 0; x < (int)src_img.width; x++) {
			if (x - block_radius < 0 || x + block_radius >= (int)src_img.width ||
				y - block_radius < 0 || y + block_radius >= (int)src_img.height) {
				//std::cout << "At column " << x << ", row " << y <<", writing 0 to disparity map." << std::endl;
				disparity_map.push_back(0);
				continue;
			}
			// get pixel number (to get window mean value from map)
			pixel_number = y*src_img.width + x;
			// get mean of pixel's window 
			float src_window_mean = src_img_window_avgs[pixel_number];
			// get actual values of window (vector.length = 81)
			vector<unsigned char> src_window_pixels = get_window_around_point(src_img, x, y, block_radius);

			float zncc_numerator_sum = 0;
			float zncc_denominator_sum_L = 0;
			float zncc_denominator_sum_R = 0;
			float zncc = 0;
			float best_zncc = 0;
			unsigned char best_disparity_value = 0;
			// for each disparity value...
			for (int disparity = min_disp; disparity <= max_disp; disparity++) {
				int offset = x - disparity; // offset = value of x in reference image
				if (offset - block_radius < 0 || offset + block_radius >= (int)src_img.width) {
					break; // Make sure that disparity values dont move the window outside of image.
				}
				ref_img_pixel_number = y*ref_img.width + offset;
				float ref_window_mean = ref_img_window_avgs[ref_img_pixel_number];
				vector<unsigned char> ref_window_pixels = get_window_around_point(ref_img, offset, y, block_radius);
				// for pixel in window...
				for (unsigned i = 0; i < src_window_pixels.size(); i++) {

					float src_window_diff = src_window_pixels[i] - src_window_mean;

					float ref_window_diff = ref_window_pixels[i] - ref_window_mean;

					zncc_numerator_sum += src_window_diff * ref_window_diff;
					zncc_denominator_sum_L += pow(src_window_diff, 2);
					zncc_denominator_sum_R += pow(ref_window_diff, 2);
				}

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

vector<unsigned char> cross_check(vector<unsigned char> &left_image, vector<unsigned char> &right_image, int threshold) {
	//Käy läpi kaksi listaa, vertailee keskenään, jos yli thresholdin niin vaihtaa tilalle nollan
	vector<unsigned char> cross_checked_image = left_image;
	for (int x = 0; x < (int)left_image.size(); x++) {
		if (abs(left_image[x] - right_image[x]) > threshold)
			cross_checked_image[x] = 0;
	}
	return cross_checked_image;
}

vector<unsigned char> occlusion_filling(vector<unsigned char> &image) {
	//Käy läpi listan, jos löytyy nolla niin laittaa tilalle sitä edeltävän luvun. Jos eka on nolla niin käy läpi kunnes löytyy eka ei-nolla ja laittaa sen ekaksi. Hyvin yksiulotteinen
	vector<unsigned char> occl_filled_image = image;
	for (int x = 0; x < (int)image.size(); x++) {

		if (image[x] = 0) {
			//voi tehä paremmankin algoritmin, tämä ei esim. ota huomioon sitä et onko rivin ihan eka pikseli, jollon sitä ei kannata ottaa edellisestä pikselistä
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

		}
			
	}
	return occl_filled_image;
}

vector<unsigned char> normalise_disparity_map(vector<unsigned char> &disp_map, int max_disp) {
	int max_value = 255;
	vector<unsigned char> normalised = vector<unsigned char>();
	for (vector<unsigned char>::iterator it = disp_map.begin(); it != disp_map.end(); ++it) {
		unsigned char val = *it;
		unsigned char normalized_val = (val * max_value) / max_disp;
		normalised.push_back(normalized_val);
	}
	return normalised;
}

int main(int argc, const char *argv[]) {
	
	const char* filename_1 = argc > 1 ? argv[1] : "im0.png";
	const char* filename_2 = argc > 2 ? argv[2] : "im1.png";

	unsigned int L_img_width, L_img_height;
	vector<unsigned char> left_img = vector<unsigned char>();
	decodeFile(filename_1, left_img, L_img_width, L_img_height);
	
	if (L_img_width != INPUT_IMG_WIDTH || L_img_height != INPUT_IMG_HEIGHT) {
		std::cout << "Expected input image with dimensions " << INPUT_IMG_WIDTH << " x " << INPUT_IMG_HEIGHT << 
					 ". Instead got " << L_img_width << " x " << L_img_height << std::endl;
		exit(1);
	}

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

	// allocate memory for resized images
	vector<unsigned char> left_img_scaled = vector<unsigned char>();
	vector<unsigned char> right_img_scaled = vector<unsigned char>();

	// create resized images from original image
	std::cout << "Resizing images..." << std::endl;
	reduce_img_size(left_img, height, width, left_img_scaled);
	reduce_img_size(right_img, height, width, right_img_scaled);

	// allocate memory for greyscale image
	GreyscaleImage Left_img = { scaled_width, scaled_height };
	GreyscaleImage Right_img = { scaled_width, scaled_height };

	// create greyscale images from resized images
	std::cout << "Creating greyscale images..." << std::endl;
	convert_to_greyscale(left_img_scaled, Left_img);
	convert_to_greyscale(right_img_scaled, Right_img);

	if (Left_img.pixels.size() != scaled_width*scaled_height 
		|| Right_img.pixels.size() != scaled_width*scaled_height) {
		std::cout << "Greyscale image has wrong dimensions! Aborting..." << std::endl;
	}

	int block_radius = (BLOCK_SIZE - 1) / 2;
	std::cout << "Mapping window averages..." << std::endl;
	unordered_map<unsigned, float> left_img_window_avgs = calc_window_averages(Left_img, block_radius);
	unordered_map<unsigned, float> right_img_window_avgs = calc_window_averages(Right_img, block_radius);
	std::cout << "Len left avgs: "<< left_img_window_avgs.size() << std::endl;
	std::cout << "Len right avgs: "<< right_img_window_avgs.size() << std::endl;
	std::cout << "Both should be: 360592" << std::endl;

	int max_disp = MAX_DISP / 4;
	std::cout << "Calculating disparity maps..." << std::endl;
	vector<unsigned char> L2R_disparity_map_values = calc_disparity_map(Left_img, left_img_window_avgs, Right_img, right_img_window_avgs, 0, max_disp, block_radius);
	vector<unsigned char> R2L_disparity_map_values = calc_disparity_map(Right_img, right_img_window_avgs, Left_img, left_img_window_avgs, -max_disp, 0, block_radius);
	
	vector<unsigned char> L2R_disparity_map_normalised = normalise_disparity_map(L2R_disparity_map_values, max_disp);
	vector<unsigned char> R2L_disparity_map_normalised = normalise_disparity_map(R2L_disparity_map_values, max_disp);
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
	const char *greyscale_file_2 = "greyscale2.png";
	encode_to_greyscale_file(greyscale_file_1, L2R_disparity_map_normalised, scaled_width, scaled_height);
	encode_to_greyscale_file(greyscale_file_2, R2L_disparity_map_normalised, scaled_width, scaled_height);
	
	int a = 0;
	return 0;
	
}
