#pragma once

extern "C"
void yuv420p10torgba(unsigned short *rgba, const unsigned short *y, 
	const unsigned short *u, const unsigned short *v, const float *matrix, const float *white,int width, int height);
extern "C"
void yuv420p10tonv12p10resize(unsigned short *NV12, const unsigned short *y,
	const unsigned short *u, const unsigned short *v, const float *yuv2rgb, const float *white, float * TransferPirmary, const float *rgb2yuv, int srcwidth, int srcheight,
	int dstwidth, int dstheight, int transfer, int distranfer, int primary, int disprimary, float luama);
