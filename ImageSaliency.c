
//	AUTHOR:
//		SRIDHAR SOMASANI
//		sridhar.somasani@gmail.com


#include <stdio.h>
#include <math.h>
#include"cv.h"

// calculate saliency of image
void salient_map(IplImage* image, int patch_size, int k_dissimilarity, char* name);

// calculates saliency for the patch comparing with other patches
float calculate_patch_saliency(IplImage* image, int patch_size, int k_dissimilarity, int current_row, int current_col, IplImage* avg_color, IplImage* avg_pos);

// calculates dissimilarity value for given rgb and position
float calculate_dissimilarity(IplImage* image, int row, int col, int current_row, int current_col, IplImage* avg_color, IplImage* avg_pos);

// normalize position coordinates of the image.
void calc_normalize_pos(IplImage* image, int row, int col, float* normal_pos);

// calculate average patch rgb and normalizes RGB values
void calc_avg_normalize_rgb(IplImage* image, int patch_size, int row, int col, float* avg_color);

// get pixel data from 8bit image
uchar get_pixel(IplImage* image, int rows, int cols, int channel);

// comparision function for qsort function
int float_comp(const void* elem1, const void* elem2);

// get float data from 32bit float image
float get_float(IplImage* image, int rows, int cols, int channel);

int main(int argc, char* argv[]){
	
	// input validation
	if(argc != 4 || atoi(argv[2]) <= 0 || atoi(argv[3]) <= 0){
		printf("Usage: %s input-image patch_size #dissimilarity\n", argv[0]);
		exit(0);
	}
	IplImage* input = cvLoadImage(argv[1],1);
	if(input == NULL){
		printf("Unable to load image %s\n", argv[1]);
		return 0;
	}
	salient_map(input, atoi(argv[2]), atoi(argv[3]), argv[1]);
	cvWaitKey();
	// free the memory
	cvReleaseImage(&input);
	return 0;
}


void salient_map(IplImage* image, int patch_size, int k_dissimilarity, char* name){
	
	char full_name[100] = "salient_";
	strcat(full_name, name);
	int rows = image->height;
	int cols = image->width;
	int row, col, channel;
	int offset = patch_size == 1 ? 1 : patch_size/2;
	float salient = 0;
	int final=0;
	offset++;
	float tmp_pos[2];
	float tmp_color[3];

	// final output image
	IplImage* output = cvCreateImage(cvSize(cols,rows),image->depth, 1);

	// calculate and store normalized average patch rgb values for each pixel
	IplImage* avg_color = cvCreateImage(cvSize(cols,rows),IPL_DEPTH_32F, image->nChannels);

	// calculate and store normalized position coordinates
	IplImage* avg_pos = cvCreateImage(cvSize(cols,rows),IPL_DEPTH_32F, 2);

	// calculate saliency and store to scale it by maximum saliency
	IplImage* salient_image = cvCreateImage(cvSize(cols,rows),IPL_DEPTH_32F, 1);

	float* color_data = (float*) avg_color->imageData;
	float* pos_data = (float*) avg_pos->imageData;
	int color_step = avg_color->widthStep/sizeof(float);
	int pos_step = avg_pos->widthStep/sizeof(float);

	double max_salient = 0;
	printf("Resolutionss : %d, %d\n", rows, cols);

	// calculate normalized average patch rgb values for each pixel
	for(row = offset; row <= rows-offset; row++){
		for(col = offset; col <= cols-offset; col++){
			calc_avg_normalize_rgb(image,patch_size,row,col,tmp_color);
			calc_normalize_pos(image, row, col, tmp_pos);
			pos_data[row * pos_step + col*2 + 0] = tmp_pos[0];
			pos_data[row * pos_step + col*2 + 1] = tmp_pos[1];
			color_data[row * color_step + col*avg_color->nChannels + 0] = tmp_color[0];
			color_data[row * color_step + col*avg_color->nChannels + 1] = tmp_color[1];
			color_data[row * color_step + col*avg_color->nChannels + 2] = tmp_color[2];
		}
		
	}

	float* salient_data = (float*) salient_image->imageData;
	int salient_step = salient_image->widthStep/sizeof(float);

	for(row = offset; row <= rows-offset; ){
		for(col = offset; col <= cols-offset; ){
			salient = 0;
			salient = calculate_patch_saliency(image, patch_size, k_dissimilarity, row, col, avg_color, avg_pos);
			salient = 1.0-(exp(-salient));
			// salient = 1.0-(1/exp(salient));
			if(salient > max_salient){
				max_salient = salient;
			}
			salient_data[row*salient_step + col*1 + 0] = salient;
			col++;
		}
		row++;
	}
	printf("Max Saliency: %f\n", max_salient);
	max_salient = (1.0/max_salient);

	// normalize by maximum saliency
	cvConvertScale(salient_image, salient_image, max_salient, 0);

	// scale to rgb values 8 bit pixel
	cvConvertScale(salient_image, output, 255,0);

	// save and release memory
	cvSaveImage(full_name, output);	
	cvReleaseImage(&salient_image);	
	cvReleaseImage(&output);
	cvReleaseImage(&avg_color);
	cvReleaseImage(&avg_pos);
}

// calculate saliency map
float calculate_patch_saliency(IplImage* image, int patch_size, int k_dissimilarity, int current_row, int current_col, IplImage* avg_color, IplImage* avg_pos){
	int rows = image->height;
	int cols = image->width;
	int row, col;
	int offset = patch_size == 1 ? 1 : patch_size/2;
	offset++;
	float saliency=0;
	int counter = rows-offset-offset;
	counter = counter * (cols-offset-offset);
	float dissim_array[counter];
	counter = 0;
	for(row = offset; row <= rows-offset; ){
		for(col = offset; col <= cols-offset; ){
			dissim_array[counter] = calculate_dissimilarity(image, row, col, current_row, current_col, avg_color, avg_pos);
			col += offset;
			counter++;
		}
		row += offset;
	}

	// sort dissimilarity array
	qsort(dissim_array, counter, sizeof(float), float_comp);
	if(k_dissimilarity > counter){
		k_dissimilarity = counter;
	}
	for(row = 0; row < k_dissimilarity; row++){
		saliency += dissim_array[row];
	}
	//calculate dissimilarity by taking average
	saliency = saliency/(float)k_dissimilarity;
	return saliency;
}

// calculate dissimilarity of two patches
float calculate_dissimilarity(IplImage* image, int row, int col, int current_row, int current_col, IplImage* avg_color, IplImage* avg_pos){
	float tmp_color = 0;
	float tmp_pos = 0;
	int i;
	double d_pos = 0;
	double d_color = 0;

	for (i = 0; i < 3; i++)
	{
		tmp_color = fabsf(get_float(avg_color,current_row,current_col,i)-get_float(avg_color,row,col,i));
		d_color = d_color + (tmp_color*tmp_color);
		if(i < 2){
			tmp_pos = fabsf(get_float(avg_pos,current_row,current_col,i)-get_float(avg_pos,row,col,i));
			d_pos = d_pos + (tmp_pos*tmp_pos);
		}
	}
	d_pos *= 3;
	d_pos++;
	return d_color/d_pos;
}

// normalize position of patch
void calc_normalize_pos(IplImage* image, int row, int col, float* normal_pos){
	float normal;
	normal = image->width;
	if(image->height > image->width){
		normal = image->height;
	}
	normal_pos[0] = row/normal;
	normal_pos[1] = col/normal;
}

// calculate average and normalize RGB of a patch
void calc_avg_normalize_rgb(IplImage* image, int patch_size, int row, int col, float* avg_color){
	avg_color[0] = avg_color[1] = avg_color[2] = 0;
	int offset = patch_size == 1 ? 1 : patch_size/2;
	int row_i, col_j;
	float normal = 255.0*patch_size*patch_size;
	for(row_i = row-offset; row_i <= row+offset; row_i++){
		for(col_j = col-offset; col_j <= col+offset; col_j++){
			avg_color[0] += get_pixel(image, row_i, col_j, 0);
			avg_color[1] += get_pixel(image, row_i, col_j, 1);
			avg_color[2] += get_pixel(image, row_i, col_j, 2);
		}
	}
	
	avg_color[0] = avg_color[0]/ normal;
	avg_color[1] = avg_color[1]/ normal;
	avg_color[2] = avg_color[2]/ normal;
}


float get_float(IplImage* image, int rows, int cols, int channel){
	
	float* image_data = (float*) image->imageData;
	int step = image->widthStep/sizeof(float);
	int index = rows * step + cols*image->nChannels + channel;
	return image_data[index];
}

// get pixel value
uchar get_pixel(IplImage* image, int rows, int cols, int channel){
	int index = rows*image->widthStep + cols*image->nChannels + channel;
	if(channel > image->nChannels){
		return 255;
	}
	if(index > (image->height * image->widthStep)){
		printf("OutOfBoundIndex :%d, row: %d, col: %d, channel:%d\n",index, rows, cols, channel );
		return 255;
	}
	return image->imageData[index];
}

int float_comp(const void* elem1, const void* elem2){
    if(*(const float*)elem1 < *(const float*)elem2)
        return -1;
    return *(const float*)elem1 > *(const float*)elem2;
}
