#include <stdio.h>
#include <string.h>

#include "lodepng.h"

/*
	This file implements the Zero-mean Normalized Cross Correlation algorithm in C.
	Phase 1 of the course work for the Multiprocessor Programming -course.
	Authors:
		Heikki Kaarlela, student nr. <#TODO>
		Eero Paavola, student nr. 2195447

	INSTRUCTIONS (link to doc: http://tinyurl.com/htyoa8u)
	1.
		Read/decode the images using LodePNG.
		(You need to download and add the appropriate .c or .cpp files and the header file to you project).
		Use the lodepng_decode32_file-function to decode the images into 32-bit RGBA raw images
		and lodepng_encode_file-function to encode the output image(s).
		Notice that you need to normalize the results to gray scale 0..255 as the output image should be greyscale.

	2.
		Resize the images to 1/16 of the original size (From 2940x2016 to 735x504).
		For this work it is enough to simply take pixels from every fourth row and column.
		You are free to use more advanced approach if you wish. 

	3.
		Transform the images to grey scale images (8-bits per pixel). E.g. Y=0.2126R+0.7152G+0.0722B.

	4.
		Implement the ZNCC algorithm with the grey scale images, start with 9x9 block size.
		Since the image size has been downscaled, you also need to scale the ndisp-value found in the calib.txt.
		The ndisp value is referred to as MAX_DISP in the pseudo-code.
		Use the resized grayscale images as input. You are free to choose how you process the image borders. 

	5.
		Compute the disparity images for both input images in order to get two disparity maps for post-processing.
		After post-processing one final disparity map is sufficient in this work. 

	6.
		Check that your outputs resembles the one in the assignment doc (Output the result image to depthmap.png).
		Notice that you need to normalize the pixel values from 0-ndisp to 0-255.
		However, once you have checked that the image looks as it should be move the normalization after the post-processing.

	7.
		Implement the post-processing including cross-check and occlusion filling.
		Instructions for post-processing are after the pseudo code on the last page of the doc.
		Check again that the disparity map looks right

	8.
		Measure the total execution time of your implementation including the scaling and grey scaling
		using QueryPerformanceCounter-function in Windows and gettimeofday-function in Linux
		and report them in your final report.
*/

#define BLOCK_SIZE 9

int main(int argc, const char *argv[]) {
	
	char *file_1 = argv[1];
	char *file_2 = argv[2];

	// printf("arguments: %s, %s\n", file_1, file_2);
	
	return 0;

}