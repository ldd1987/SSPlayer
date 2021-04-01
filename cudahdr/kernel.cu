
#include "cuda_runtime.h"
#include "device_launch_parameters.h"
#include "common/inc/helper_math.h"
#include "cudahdr.h"
#include <stdio.h>
#include <vector>
const int blockperthread = 512;
cudaError_t addWithCuda(int *c, const int *a, const int *b, unsigned int size);

__global__ void addKernel(int *c, const int *a, const int *b)
{
	int i = threadIdx.x +  blockIdx.x * blockperthread;
    c[i] = a[i] + b[i];
}



__host__ __device__
float4 mul(const float4 *m, const float4 &v)
{
	float4 r;
	r.x = dot(v, m[0]);
	r.y = dot(v, m[1]);
	r.z = dot(v, m[2]);
	r.w = 1.0f;
	return r;
}

__host__ __device__
float4 mul(const float4 &v, const float4 *m)
{
	float4 r;
	r.x = dot(v, m[0]);
	r.y = dot(v, m[1]);
	r.z = dot(v, m[2]);
	r.w = 1.0f;
	return r;
}

__host__ __device__  float4 hable(float4 x)
{
	const float A = 0.15, B = 0.50, C = 0.10, D = 0.20, E = 0.02, F = 0.30;
	return ((x * (A*x + (C*B)) + (D*E)) / (x * (A*x + B) + (D*F))) - E / F;
}

__host__ __device__ float4 HDRToneMapping(float4 rgb, float LuminanceScale)
{
	float4 HABLE_DIV = hable(make_float4(11.2,11.2,11.2,11.2));
	float4 rgba = hable(rgb* LuminanceScale) / HABLE_DIV;
	return rgba;
}

__host__ __device__ float4 transformPrimaries(float4 rgb, int primaries,int disprimaries, float4*TransPrimaries)
{
	if (primaries != disprimaries)
	{
		return fmaxf(mul(rgb, TransPrimaries),make_float4(0,0,0 ,0));
	}
	else
	{
		return rgb;
	}
}

__host__ __device__ float4 toneMapping(float4 rgb, int  transfer,int distransfer, float LuminanceScale)
{
	if (distransfer == transfer)
	{
		return rgb;
	}
	if (distransfer == 1 || distransfer == 4)
	{
		if (transfer == 16 || transfer == 18)
		{
			return HDRToneMapping(rgb, LuminanceScale);
		}
		else
		{
			return rgb * LuminanceScale;
		}
	}
	else
	{
		return rgb * LuminanceScale;
	}

}



// 电光转换函数  这里转换为自然光 
__host__ __device__ float inverse_HLG(float x)
{
	const float B67_a = 0.17883277;
	const float B67_b = 0.28466892;
	const float B67_c = 0.55991073;
	const float B67_inv_r2 = 4.0;
	if (x <= 0.5)
		x = x * x * B67_inv_r2;
	else
		x = exp((x - B67_c) / B67_a) + B67_b;
	return x;
}

// 自然光转换为电信号
__host__ __device__ float LineToHLG(float Lc)
{
	const double a = 0.17883277;
	const double b = 0.28466892;
	const double c = 0.55991073;
	return (0.0 > Lc) ? 0.0 :
		(Lc <= 1.0 / 12.0 ? sqrt(3.0 * Lc) : a * log(12.0 * Lc - b) + c);
}


__host__ __device__  float4 ST2084TOLinear(float4 rgb)
{
	const float ST2084_m1 = 2610.0 / (4096.0 * 4);
	const float ST2084_m2 = (2523.0 / 4096.0) * 128.0;
	const float ST2084_c1 = 3424.0 / 4096.0;
	const float ST2084_c2 = (2413.0 / 4096.0) * 32.0;
	const float ST2084_c3 = (2392.0 / 4096.0) * 32.0;
	rgb = fpowf(fmaxf(rgb, make_float4(0,0,0, 0)), 1.0 / ST2084_m2);
	rgb = fmaxf(rgb - ST2084_c1, make_float4(0,0,0,0)) / (ST2084_c2 - ST2084_c3 * rgb);
	rgb = fpowf(rgb, 1.0 / ST2084_m1);
	return rgb * 10000;
}


// 这里转换为自然 光 
__host__ __device__  float4 HLGTOSenceLinear(float4 rgb)
{
	rgb.x= inverse_HLG(rgb.x);
	rgb.y = inverse_HLG(rgb.y);
	rgb.z = inverse_HLG(rgb.z);
	return rgb;
}

// 这里转换为display 光 
__host__ __device__  float4 HLGTOLinear(float4 rgb)
{
	const float alpha_gain = 2000;
	rgb.x = inverse_HLG(rgb.x);
	rgb.y = inverse_HLG(rgb.y);
	rgb.z = inverse_HLG(rgb.z);
	// 下面是光光转换函数 转换为display 光
	float3 ootf_2020 = make_float3(0.2627, 0.6780, 0.0593);
	float ootf_ys = alpha_gain * dot(ootf_2020, make_float3(rgb.x, rgb.y, rgb.z));
	return rgb * powf(ootf_ys, 1.200 - 1.0);
}
__host__ __device__  float4 BT709TOLinear(float4 rgb)
{
	return fpowf(rgb, 1.0 / 0.45);
}

__host__ __device__  float4 BT470M_SRGB_TOLinear(float4 rgb)
{
	return fpowf(rgb, 2.2);
}
__host__ __device__  float4 BT470BGTOLinear(float4 rgb)
{
	return fpowf(rgb, 2.8);
}

__host__ __device__  float4 LineTOSRGB(float4 rgb)
{
	return fpowf(rgb, 1.0 / 2.2);
}
__host__ __device__  float4 LineTOST2084(float4 rgb)
{
	const float ST2084_m1 = 2610.0 / (4096.0 * 4);
	const float ST2084_m2 = (2523.0 / 4096.0) * 128.0;
	const float ST2084_c1 = 3424.0 / 4096.0;
	const float ST2084_c2 = (2413.0 / 4096.0) * 32.0;
	const float ST2084_c3 = (2392.0 / 4096.0) * 32.0;
	rgb = fpowf(rgb / 10000, ST2084_m1);
	rgb = (ST2084_c1 + ST2084_c2 * rgb) / (1 + ST2084_c3 * rgb);
	rgb = fpowf(rgb, ST2084_m2);
	return rgb;
}


__host__ __device__  float4 PQToHLG(float4 rgb, float LuminanceScale)
{

	float r = rgb.x / LuminanceScale / 10000.0 * 10;
	float g = rgb.y / LuminanceScale / 10000.0 * 10;
	float b = rgb.z / LuminanceScale / 10000.0 * 10;
	 r = r > 1.0 ? 1.0 : r;
	 g = g > 1.0 ? 1.0 : g;
	 b = b > 1.0 ? 1.0 : b;
	float a = rgb.w;
	// 上面得到的是display光 
	// 逆光光转换函数  将display 光转换为自然光 
	if (0)
	{
		float3 ootf_2020 = make_float3(0.2627, 0.6780, 0.0593);
		float ootf_ys = dot(ootf_2020, make_float3(rgb.x, rgb.y,rgb.z));
		ootf_ys = powf(ootf_ys, (1.0 - 1.2) / 1.200);
		float hlgr = r * ootf_ys;
		float hlgg = g * ootf_ys;
		float hlgb = b * ootf_ys;
		float hlga = a;
		hlgr = LineToHLG(hlgr);
		hlgg = LineToHLG(hlgg);
		hlgb = LineToHLG(hlgb);
		return  make_float4(hlgr, hlgg, hlgb, hlga);
	}
	else
	{
		r = LineToHLG(r);
		g = LineToHLG(g);
		b = LineToHLG(b);
		return  make_float4(r, g, b, a);
	}


}

__host__ __device__  float4 SDRToHLG(float4 rgb, float LuminanceScale)
{
	float a = rgb.w;
	rgb.x = 0.265 *rgb.x * 2/ LuminanceScale;
	rgb.y = 0.265 *rgb.y * 2 / LuminanceScale;
	rgb.z = 0.265 *rgb.z * 2 / LuminanceScale;
	float ootf_ys = 1;
	float r = rgb.x > 1.0 ? 1.0 : rgb.x;
	float g = rgb.y > 1.0 ? 1.0 : rgb.y;
	float b = rgb.z > 1.0 ? 1.0 : rgb.z;
	float hlgr = r * ootf_ys;
	float hlgg = g * ootf_ys;
	float hlgb = b * ootf_ys;
	hlgr = LineToHLG(hlgr);
	hlgg = LineToHLG(hlgg);
	hlgb = LineToHLG(hlgb);
	float hlga = a;
	return  make_float4(hlgr, hlgg, hlgb, hlga);
}

__host__ __device__  float4 LinerToSTDB67(float4 hlg, int transfer, int distransfer, float LuminanceScale)
{
	if (transfer == distransfer)
	{
		return hlg;
	}
	else
	{
		if (transfer == 16)
		{
			return PQToHLG(hlg, LuminanceScale);
		}
		else
		{
			return SDRToHLG(hlg, LuminanceScale);
		}
	}


}

__host__ __device__  float4 linearToDisplay(float4 rgb, int transfer, int distransfer, float LuminanceScale)
{
	if (distransfer == transfer)
	{
		return  rgb;
	}
	else if (distransfer == 16)
	{
		return LineTOST2084(rgb);
	}
	else if (distransfer == 18)
	{
		return LinerToSTDB67(rgb, transfer, distransfer, LuminanceScale);
	}
	else if (distransfer == 1)
	{
		return fpowf(rgb,1.0/2.2);
	}
	else if (distransfer == 4)
	{
		return fpowf(rgb, 1.0 / 2.2);
	}
	else
	{
		return rgb;
	}
}

__host__ __device__  float4 sourcetolinekernel(float4 rgb,  int transfer, int distransfer)
{
	if (transfer == distransfer)
	{
		return rgb;
	}
	else if (transfer == 8)  //line 
	{
		return rgb;
	}
	else if (transfer == 16) // pq
	{
		return ST2084TOLinear(rgb);
	}
	else if (transfer == 18) // hlg
	{
		return HLGTOLinear(rgb);
	}
	else if (transfer == 1) // bt709
	{
		return BT709TOLinear(rgb);
	}
	else if (transfer == 4)
	{
		return BT470M_SRGB_TOLinear(rgb);
	}
	else if (transfer == 5)
	{
		return BT470BGTOLinear(rgb);
	}
	else
	{
		return rgb;
	}
}

__host__ __device__  float4 Render2RGBA(float4 rgb, int transfer, int distransfer, int primary, int disprimary, float LuminanceScale, float4 *TransPrimaries)
{
	float a = rgb.w;
	rgb.w = 0;
	rgb = sourcetolinekernel(rgb, transfer, distransfer);
	rgb = transformPrimaries(rgb, primary, disprimary, TransPrimaries);
	rgb = toneMapping(rgb, transfer, distransfer, LuminanceScale);
	rgb = linearToDisplay(rgb, transfer, distransfer, LuminanceScale);
	rgb.w = a;
	return rgb;
}
__host__ __device__  float4 RGBA2yuv(float4 rgb, float4 *rgb2yuv)
{
	float4 yuva = mul(rgb, rgb2yuv);
	float y = clamp(yuva.x, 0.0, 1.0);
	float u = clamp(yuva.y, 0.0, 1.0);
	float v = clamp(yuva.z, 0.0, 1.0);
	return make_float4(y, u, v, 1.0);
}

__host__ __device__  float4 yuv2rgb(float4 yuv, float4 *yuv2rgb)
{
	float4 yuva = mul(yuv, yuv2rgb);
	float r = clamp(yuva.x, 0.0, 1.0);
	float g = clamp(yuva.y, 0.0, 1.0);
	float b = clamp(yuva.z, 0.0, 1.0);
	return make_float4(r, g, b, 1.0);
}

__global__ void yuv420p10torgbakernel(unsigned short *dst, const unsigned short *y, const unsigned short *u, const unsigned short *v, const float4 *matrix,const float4 *white, int width,int height)
{
	int index = threadIdx.x + blockIdx.x * blockperthread;
	int xpos = index % width;
	int ypos = index / width ;
	xpos = xpos / 2;
	ypos = ypos / 2;
	int uvindex = xpos + ypos * width / 2;
	float y0 = y[index]/1023.0;
	float u0 = u[uvindex] / 1023.0;
	float v0 = v[uvindex] / 1023.0;
	 float4 rgba = mul(make_float4(y0,u0,v0,1), white);
	 rgba = fmaxf(mul(rgba, matrix), make_float4(0,0,0,0));
	 float r0 = clamp(rgba.x, 0.0, 1.0);;
	 float g0 = clamp(rgba.y, 0.0, 1.0);;
	 float b0 = clamp(rgba.z, 0.0, 1.0);
	 dst[index * 4 + 0] = (unsigned short)(r0 *1023);
	 dst[index * 4 + 1] = (unsigned short)(g0 * 1023);
	 dst[index * 4 + 2] = (unsigned short)(b0 * 1023);
	 dst[index * 4 + 3] = 1 * 1023;

}

__global__ void yuvnv12p10torgbakernel(unsigned short *rgba, const unsigned short *y, const unsigned short *u, const unsigned short *v, const float *matrix, int width, int height)
{
	int index = threadIdx.x + blockIdx.x * blockperthread;
	int xpos = index % width;
	int ypos = index / (width *height);
	xpos = xpos / 2;
	ypos = ypos / 2;
	int uvindex = xpos + ypos * height / 2;
	float y0 = y[index] / 1023.0;
	float u0 = u[uvindex] / 1023.0;
	float v0 = v[uvindex] / 1023.0;
	float r0 = matrix[0] * y0 + matrix[1] * u0 + matrix[2] * v0 + matrix[3];
	float g0 = matrix[4] * y0 + matrix[5] * u0 + matrix[6] * v0 + matrix[7];
	float b0 = matrix[8] * y0 + matrix[9] * u0 + matrix[10] * v0 + matrix[11];
	r0 = clamp(r0, 0.0, 1.0);
	g0 = clamp(g0, 0.0, 1.0);
	b0 = clamp(b0, 0.0, 1.0);
	rgba[index * 4 + 0] = r0 * 1023;
	rgba[index * 4 + 1] = g0 * 1023;
	rgba[index * 4 + 2] = v0 * 1023;

}

__global__ void yuv444p10torgbkernel(unsigned short *rgba, const unsigned short *y, const unsigned short *u, const unsigned short *v, const float *matrix, int width, int height)
{
	int index = threadIdx.x + blockIdx.x * blockperthread;
	int uvindex = index;
	float y0 = y[index] / 1023.0;
	float u0 = u[uvindex] / 1023.0;
	float v0 = v[uvindex] / 1023.0;
	float r0 = matrix[0] * y0 + matrix[1] * u0 + matrix[2] * v0 + matrix[3];
	float g0 = matrix[4] * y0 + matrix[5] * u0 + matrix[6] * v0 + matrix[7];
	float b0 = matrix[8] * y0 + matrix[9] * u0 + matrix[10] * v0 + matrix[11];
	r0 = clamp(r0, 0.0, 1.0);
	g0 = clamp(g0, 0.0, 1.0);
	b0 = clamp(b0, 0.0, 1.0);
	rgba[index * 4 + 0] = r0 * 1023;
	rgba[index * 4 + 1] = g0 * 1023;
	rgba[index * 4 + 2] = v0 * 1023;
}
__global__ void yuv422p10torgbakernel(unsigned short *rgba, const unsigned short *y, const unsigned short *u, const unsigned short *v, const float *matrix, int width, int height)
{
	int index = threadIdx.x + blockIdx.x * blockperthread;
	int xpos = index % width;
	int ypos = index / width;
	xpos = xpos / 2;
	int uvindex = xpos + ypos * height;
	float y0 = y[index] / 1023.0;
	float u0 = u[uvindex] / 1023.0;
	float v0 = v[uvindex] / 1023.0;
	float r0 = matrix[0] * y0 + matrix[1] * u0 + matrix[2] * v0 + matrix[3];
	float g0 = matrix[4] * y0 + matrix[5] * u0 + matrix[6] * v0 + matrix[7];
	float b0 = matrix[8] * y0 + matrix[9] * u0 + matrix[10] * v0 + matrix[11];
	r0 = clamp(r0, 0.0, 1.0);
	g0 = clamp(g0, 0.0, 1.0);
	b0 = clamp(b0, 0.0, 1.0);
	rgba[index * 4 + 0] = r0 * 1023;
	rgba[index * 4 + 1] = g0 * 1023;
	rgba[index * 4 + 2] = v0 * 1023;
}


__global__ void rgbtoyuv420p10(int *c, const int *a, const float *b)
{
	int i = threadIdx.x + blockIdx.x * blockperthread;
	c[i] = a[i] + b[i];
}
__global__ void rgbtoyuv444p10(int *c, const int *a, const float *b)
{
	int i = threadIdx.x + blockIdx.x * blockperthread;
	c[i] = a[i] + b[i];
}

__global__ void rgba10resize(unsigned short *dst, const unsigned short *src, int srcwidth,int srcheight,int dstwidth,int dstheight,int xstart,int ystart, float scale)
{
	int i = threadIdx.x + blockIdx.x * blockperthread;
	int xpos = i % dstwidth;
	int ypos = i / dstwidth;

	float xposorg = xpos * scale;
	float yposorg = ypos * scale;
	int xx = xposorg;
	int yy = yposorg;
	float r = src[yy * srcwidth * 4 + xx *4 + 0]/1023;
	float g = src[yy * srcwidth * 4 + xx * 4 + 1] / 1023;
	float b = src[yy * srcwidth * 4 + xx * 4 + 2] / 1023;
	float a = src[yy * srcwidth * 4 + xx * 4 + 3] / 1023;
	dst[ystart * dstwidth * 4 + xstart *4 + i * 4 + 0] = r * 1023;
	dst[ystart * dstwidth * 4 + xstart * 4 + i * 4 + 1] = g*1023;
	dst[ystart * dstwidth * 4 + xstart * 4 + i * 4 + 2] = b*1023;
	dst[ystart * dstwidth * 4 + xstart * 4 + i * 4 + 3] = a*1023;
}

__global__ void rgba10resize2YUV42010(unsigned short *dsty, unsigned short *dstuv, const unsigned short *src, int srcwidth, int srcheight, 
	int dstwidth, int dstheight, int xstart, int ystart, float scale, int transfer, int distransfer, int primary, int disprimary,
	float LuminanceScale, float4 *TransPrimaries,float4 *rgb2yuv)
{
	int i = threadIdx.x + blockIdx.x * blockperthread;
	if (i >= dstwidth * dstheight)
	{
		return;
	}
	int xpos = i % dstwidth;
	int ypos = i / dstwidth;
	float xposorg = xpos * scale;
	float yposorg = ypos * scale;
	int xx = xposorg;
	int yy = yposorg;
	if (xx >= srcwidth || yy >= srcheight)
	{
		return;
	}
	float r = src[yy * srcwidth * 4 + xx * 4 + 0] / 1023.0;
	float g = src[yy * srcwidth * 4 + xx * 4 + 1] / 1023.0;
	float b = src[yy * srcwidth * 4 + xx * 4 + 2] / 1023.0;
	float4 rgba = make_float4(r, g, b, 1);
	rgba = Render2RGBA(rgba, transfer, distransfer, primary, disprimary, LuminanceScale, TransPrimaries);
	float4 yuva = RGBA2yuv(rgba, rgb2yuv);
	dsty[i] = clamp(yuva.x, 0.0, 1.0) * 1023;
	if (1)
	{
		//float a = src[yy * srcwidth * 4 + xx * 4 + 3] / 1023.0;
		
		if (xpos % 2 == 0 && ypos % 2 == 0)
		{
			dstuv[ypos * dstwidth / 2 + xpos + 0] = clamp(yuva.y, 0.0, 1.0) * 1023;
			dstuv[ypos * dstwidth / 2 + xpos + 1] = clamp(yuva.z, 0.0, 1.0) * 1023;
		}
	}
	
}


void yuv420p10tonv12p10resize(unsigned short *NV12, const unsigned short *y,
	const unsigned short *u, const unsigned short *v, const float *yuv2rgb, const float *white, float * TransferPirmary, const float *rgb2yuv, int srcwidth, int srcheight,
	int dstwidth, int dstheight, int transfer, int distranfer, int primary, int disprimary, float luama)
{
	int nGpu = 0;
	cudaError_t res = cudaGetDeviceCount(&nGpu);
	if (res != cudaSuccess)
	{
		fprintf(stderr, "addWithCuda failed!");
		return;
	}
	std::vector< cudaDeviceProp> vProp;
	for (int i = 0; i < nGpu; i++)
	{
		cudaDeviceProp deviceprop;
		res = cudaGetDeviceProperties(&deviceprop, i);
		if (res == cudaSuccess)
		{
			vProp.push_back(deviceprop);
		}
	}
	cudaError_t cudaStatus;
	unsigned short * ydevice = 0;
	unsigned short *udevice = 0;
	unsigned short *vdevice = 0;
	unsigned short *rgbdevice = 0;
	unsigned short *nv1210device = 0;
	float4 *yuv2rgbmatrixdevice = 0;
	float4 *whitedevice = 0;
	float4 *rgb2yuvmatrixdevice = 0;
	float4 *transferprimarymatrixdevice = 0;
	// Choose which GPU to run on, change this on a multi-GPU system.
	cudaStatus = cudaSetDevice(0);
	if (cudaStatus != cudaSuccess)
	{
		fprintf(stderr, "cudaSetDevice failed!  Do you have a CUDA-capable GPU installed?");
		goto Error;
	}
	// Allocate GPU buffers for three vectors (two input, one output)    .
	cudaStatus = cudaMalloc((void**)&yuv2rgbmatrixdevice, 4 * sizeof(float4));
	if (cudaStatus != cudaSuccess)
	{
		fprintf(stderr, "cudaMalloc failed!");
		goto Error;
	}
	cudaStatus = cudaMalloc((void**)&transferprimarymatrixdevice, 4 * sizeof(float4));
	if (cudaStatus != cudaSuccess)
	{
		fprintf(stderr, "cudaMalloc failed!");
		goto Error;
	}
	cudaStatus = cudaMalloc((void**)&rgb2yuvmatrixdevice, 4 * sizeof(float4));
	if (cudaStatus != cudaSuccess)
	{
		fprintf(stderr, "cudaMalloc failed!");
		goto Error;
	}
	cudaStatus = cudaMalloc((void**)&whitedevice, 4 * sizeof(float4));
	if (cudaStatus != cudaSuccess)
	{
		fprintf(stderr, "cudaMalloc failed!");
		goto Error;
	}


	// Allocate GPU buffers for three vectors (two input, one output)    .
	cudaStatus = cudaMalloc((void**)&rgbdevice, srcwidth *srcheight * 4 * sizeof(unsigned short));
	if (cudaStatus != cudaSuccess)
	{
		fprintf(stderr, "cudaMalloc failed!");
		goto Error;
	}
	cudaStatus = cudaMalloc((void**)&nv1210device, dstwidth *dstheight * 3/2 * sizeof(unsigned short));
	if (cudaStatus != cudaSuccess)
	{
		fprintf(stderr, "cudaMalloc failed!");
		goto Error;
	}
	


	cudaStatus = cudaMalloc((void**)&ydevice, srcwidth *srcheight * sizeof(unsigned short));
	if (cudaStatus != cudaSuccess)
	{
		fprintf(stderr, "cudaMalloc failed!");
		goto Error;
	}

	cudaStatus = cudaMalloc((void**)&udevice, srcwidth *srcheight / 4 * sizeof(unsigned short));
	if (cudaStatus != cudaSuccess)
	{
		fprintf(stderr, "cudaMalloc failed!");
		goto Error;
	}
	cudaStatus = cudaMalloc((void**)&vdevice, srcwidth *srcheight / 4 * sizeof(unsigned short));
	if (cudaStatus != cudaSuccess)
	{
		fprintf(stderr, "cudaMalloc failed!");
		goto Error;
	}
	// Copy input vectors from host memory to GPU buffers.
	cudaStatus = cudaMemcpy(yuv2rgbmatrixdevice, yuv2rgb, 16 * sizeof(float), cudaMemcpyHostToDevice);
	if (cudaStatus != cudaSuccess)
	{
		fprintf(stderr, "cudaMemcpy failed!");
		goto Error;
	}
	cudaStatus = cudaMemcpy(rgb2yuvmatrixdevice, rgb2yuv, 16 * sizeof(float), cudaMemcpyHostToDevice);
	if (cudaStatus != cudaSuccess)
	{
		fprintf(stderr, "cudaMemcpy failed!");
		goto Error;
	}
	cudaStatus = cudaMemcpy(transferprimarymatrixdevice, TransferPirmary, 16 * sizeof(float), cudaMemcpyHostToDevice);
	if (cudaStatus != cudaSuccess)
	{
		fprintf(stderr, "cudaMemcpy failed!");
		goto Error;
	}
	cudaStatus = cudaMemcpy(whitedevice, white, 16 * sizeof(float), cudaMemcpyHostToDevice);
	if (cudaStatus != cudaSuccess)
	{
		fprintf(stderr, "cudaMemcpy failed!");
		goto Error;
	}
	cudaStatus = cudaMemcpy(ydevice, y, srcwidth *srcheight * sizeof(unsigned short), cudaMemcpyHostToDevice);
	if (cudaStatus != cudaSuccess)
	{
		fprintf(stderr, "cudaMemcpy failed!");
		goto Error;
	}

	cudaStatus = cudaMemcpy(udevice, u, srcwidth *srcheight / 4 * sizeof(unsigned short), cudaMemcpyHostToDevice);
	if (cudaStatus != cudaSuccess)
	{
		fprintf(stderr, "cudaMemcpy failed!");
		goto Error;
	}
	cudaStatus = cudaMemcpy(vdevice, v, srcwidth *srcheight / 4 * sizeof(unsigned short), cudaMemcpyHostToDevice);
	if (cudaStatus != cudaSuccess)
	{
		fprintf(stderr, "cudaMemcpy failed!");
		goto Error;
	}
	int nsize = srcwidth * srcheight;
	unsigned int grid = (nsize + blockperthread - 1) / blockperthread;
	dim3 ngrid = { grid };
	unsigned int perthread = blockperthread;
	dim3 nthread = { perthread };
	// Launch a kernel on the GPU with one thread for each element.
	yuv420p10torgbakernel << <grid, perthread >> > (rgbdevice, ydevice, udevice, vdevice, yuv2rgbmatrixdevice, whitedevice, srcwidth, srcheight);

	// Check for any errors launching the kernel
	cudaStatus = cudaGetLastError();
	if (cudaStatus != cudaSuccess) {
		fprintf(stderr, "addKernel launch failed: %s\n", cudaGetErrorString(cudaStatus));
		goto Error;
	}

	// cudaDeviceSynchronize waits for the kernel to finish, and returns
	// any errors encountered during the launch.
	cudaStatus = cudaDeviceSynchronize();
	if (cudaStatus != cudaSuccess) {
		fprintf(stderr, "cudaDeviceSynchronize returned error code %d after launching addKernel!\n", cudaStatus);
		goto Error;
	}
	 nsize = dstwidth * dstheight;
	 grid = (nsize + blockperthread - 1) / blockperthread;
	 ngrid = { grid };
	 unsigned short *dsty = nv1210device;
	 unsigned short *dstuv = nv1210device + dstwidth * dstheight;
	 float scale = 1.0 * srcwidth / dstwidth;
	rgba10resize2YUV42010 << <grid, perthread >> > (dsty, dstuv,rgbdevice, srcwidth,srcheight,dstwidth,dstheight,0,0,scale,transfer,distranfer,
		primary, disprimary,luama,transferprimarymatrixdevice, rgb2yuvmatrixdevice);

	cudaStatus = cudaGetLastError();
	if (cudaStatus != cudaSuccess) {
		fprintf(stderr, "addKernel launch failed: %s\n", cudaGetErrorString(cudaStatus));
		goto Error;
	}

	// cudaDeviceSynchronize waits for the kernel to finish, and returns
	// any errors encountered during the launch.
	cudaStatus = cudaDeviceSynchronize();
	if (cudaStatus != cudaSuccess) {
		fprintf(stderr, "cudaDeviceSynchronize returned error code %d after launching addKernel!\n", cudaStatus);
		goto Error;
	}

	// Copy output vector from GPU buffer to host memory.
	cudaStatus = cudaMemcpy(NV12, nv1210device, dstheight *dstwidth * 3 / 2* sizeof(unsigned short), cudaMemcpyDeviceToHost);
	if (cudaStatus != cudaSuccess) {
		fprintf(stderr, "cudaMemcpy failed!");
		goto Error;
	}

Error:
	cudaFree(rgbdevice);
	cudaFree(nv1210device);
	cudaFree(ydevice);
	cudaFree(udevice);
	cudaFree(vdevice);
	cudaFree(yuv2rgbmatrixdevice);
	cudaFree(rgb2yuvmatrixdevice);
	cudaFree(whitedevice);
	cudaFree(transferprimarymatrixdevice);
	cudaStatus = cudaDeviceReset();
	if (cudaStatus != cudaSuccess)
	{
		fprintf(stderr, "cudaDeviceReset failed!");
		return;
	}
}
void yuv420p10torgba(unsigned short *rgba, const unsigned short *y,
	const unsigned short *u, const unsigned short *v, const float *matrix, const float *white,int width, int height)
{
	int nGpu = 0;
	cudaError_t res = cudaGetDeviceCount(&nGpu);
	if (res != cudaSuccess)
	{
		fprintf(stderr, "addWithCuda failed!");
		return ;
	}
	std::vector< cudaDeviceProp> vProp;
	for (int i = 0; i < nGpu; i++)
	{
		cudaDeviceProp deviceprop;
		res = cudaGetDeviceProperties(&deviceprop, i);
		if (res == cudaSuccess)
		{
			vProp.push_back(deviceprop);
		}
	}
	cudaError_t cudaStatus;
	unsigned short * ydevice = 0;
	unsigned short *udevice = 0;
	unsigned short *vdevice = 0;
	unsigned short *rgbdevice = 0;
	float4 *matrixdevice = 0;
	float4 *whitedevice = 0;
	// Choose which GPU to run on, change this on a multi-GPU system.
	cudaStatus = cudaSetDevice(0);
	if (cudaStatus != cudaSuccess)
	{
		fprintf(stderr, "cudaSetDevice failed!  Do you have a CUDA-capable GPU installed?");
		goto Error;
	}
	// Allocate GPU buffers for three vectors (two input, one output)    .
	cudaStatus = cudaMalloc((void**)&matrixdevice,4 * sizeof(float4));
	if (cudaStatus != cudaSuccess)
	{
		fprintf(stderr, "cudaMalloc failed!");
		goto Error;
	}
	cudaStatus = cudaMalloc((void**)&whitedevice, 4 * sizeof(float4));
	if (cudaStatus != cudaSuccess)
	{
		fprintf(stderr, "cudaMalloc failed!");
		goto Error;
	}
	

	// Allocate GPU buffers for three vectors (two input, one output)    .
	cudaStatus = cudaMalloc((void**)&rgbdevice, width *height *4* sizeof(unsigned short));
	if (cudaStatus != cudaSuccess)
	{
		fprintf(stderr, "cudaMalloc failed!");
		goto Error;
	}

	cudaStatus = cudaMalloc((void**)&ydevice, width *height * sizeof(unsigned short));
	if (cudaStatus != cudaSuccess)
	{
		fprintf(stderr, "cudaMalloc failed!");
		goto Error;
	}

	cudaStatus = cudaMalloc((void**)&udevice, width *height/4 * sizeof(unsigned short));
	if (cudaStatus != cudaSuccess)
	{
		fprintf(stderr, "cudaMalloc failed!");
		goto Error;
	}
	cudaStatus = cudaMalloc((void**)&vdevice, width *height / 4 * sizeof(unsigned short));
	if (cudaStatus != cudaSuccess)
	{
		fprintf(stderr, "cudaMalloc failed!");
		goto Error;
	}
	// Copy input vectors from host memory to GPU buffers.
	cudaStatus = cudaMemcpy(matrixdevice, matrix, 16 * sizeof(float), cudaMemcpyHostToDevice);
	if (cudaStatus != cudaSuccess)
	{
		fprintf(stderr, "cudaMemcpy failed!");
		goto Error;
	}
	cudaStatus = cudaMemcpy(whitedevice, white, 16 * sizeof(float), cudaMemcpyHostToDevice);
	if (cudaStatus != cudaSuccess)
	{
		fprintf(stderr, "cudaMemcpy failed!");
		goto Error;
	}
	cudaStatus = cudaMemcpy(ydevice, y, width *height * sizeof(unsigned short), cudaMemcpyHostToDevice);
	if (cudaStatus != cudaSuccess)
	{
		fprintf(stderr, "cudaMemcpy failed!");
		goto Error;
	}

	cudaStatus = cudaMemcpy(udevice, u, width *height / 4 * sizeof(unsigned short), cudaMemcpyHostToDevice);
	if (cudaStatus != cudaSuccess)
	{
		fprintf(stderr, "cudaMemcpy failed!");
		goto Error;
	}
	cudaStatus = cudaMemcpy(vdevice, v, width *height / 4 * sizeof(unsigned short), cudaMemcpyHostToDevice);
	if (cudaStatus != cudaSuccess)
	{
		fprintf(stderr, "cudaMemcpy failed!");
		goto Error;
	}
	int nsize = width * height;
	unsigned int grid = (nsize + blockperthread - 1) / blockperthread;
	dim3 ngrid = { grid};
	unsigned int perthread = blockperthread;
	dim3 nthread = { perthread };
	// Launch a kernel on the GPU with one thread for each element.
	yuv420p10torgbakernel << <grid, perthread >> > (rgbdevice,ydevice,udevice,vdevice, matrixdevice,whitedevice, width, height);

	// Check for any errors launching the kernel
	cudaStatus = cudaGetLastError();
	if (cudaStatus != cudaSuccess) {
		fprintf(stderr, "addKernel launch failed: %s\n", cudaGetErrorString(cudaStatus));
		goto Error;
	}

	// cudaDeviceSynchronize waits for the kernel to finish, and returns
	// any errors encountered during the launch.
	cudaStatus = cudaDeviceSynchronize();
	if (cudaStatus != cudaSuccess) {
		fprintf(stderr, "cudaDeviceSynchronize returned error code %d after launching addKernel!\n", cudaStatus);
		goto Error;
	}

	// Copy output vector from GPU buffer to host memory.
	cudaStatus = cudaMemcpy(rgba, rgbdevice, width *height * 4 * sizeof(unsigned short), cudaMemcpyDeviceToHost);
	if (cudaStatus != cudaSuccess) {
		fprintf(stderr, "cudaMemcpy failed!");
		goto Error;
	}

Error:
	cudaFree(rgbdevice);
	cudaFree(ydevice);
	cudaFree(udevice);
	cudaFree(vdevice);
	cudaFree(matrixdevice);
	cudaFree(whitedevice);
	cudaStatus = cudaDeviceReset();
	if (cudaStatus != cudaSuccess)
	{
		fprintf(stderr, "cudaDeviceReset failed!");
		return ;
	}

}
#ifdef EXE
const long arraySize = 3840 * 2160;
int main()
{
	int nGpu = 0;
	cudaError_t res = cudaGetDeviceCount(&nGpu);
	if (res != cudaSuccess)
	{
		fprintf(stderr, "addWithCuda failed!");
		return 1;
	}
	std::vector< cudaDeviceProp> vProp;
	for (int i = 0 ;i < nGpu; i++)
	{
		cudaDeviceProp deviceprop;
		res = cudaGetDeviceProperties(&deviceprop, i);
		if (res == cudaSuccess)
		{
			vProp.push_back(deviceprop);
		}
	}
	
     int *a = new int[arraySize];
	 int *b = new int[arraySize];
	 int *c = new int[arraySize];
	 for (int i = 0; i < arraySize; i++)
	 {
		 a[i] = 0;
		 b[i] = 1;
		 c[i] = 0;
	 }
  

    // Add vectors in parallel.
    cudaError_t cudaStatus = addWithCuda(c, a, b, arraySize);
    if (cudaStatus != cudaSuccess) {
        fprintf(stderr, "addWithCuda failed!");
        return 1;
    }
	for (int i = 0; i < arraySize; i++)
	{
		if (c[i] != a[i] + b[i])
		{
			fprintf(stderr, "addWithCuda error----id is:%d!", i);
		}
	}
   

    // cudaDeviceReset must be called before exiting in order for profiling and
    // tracing tools such as Nsight and Visual Profiler to show complete traces.
    cudaStatus = cudaDeviceReset();
    if (cudaStatus != cudaSuccess)
	{
        fprintf(stderr, "cudaDeviceReset failed!");
        return 1;
    }

    return 0;
}

// Helper function for using CUDA to add vectors in parallel.
cudaError_t addWithCuda(int *c, const int *a, const int *b, unsigned int size)
{
    int *dev_a = 0;
    int *dev_b = 0;
    int *dev_c = 0;
    cudaError_t cudaStatus;

    // Choose which GPU to run on, change this on a multi-GPU system.
    cudaStatus = cudaSetDevice(0);
    if (cudaStatus != cudaSuccess)
	{
        fprintf(stderr, "cudaSetDevice failed!  Do you have a CUDA-capable GPU installed?");
        goto Error;
    }

    // Allocate GPU buffers for three vectors (two input, one output)    .
    cudaStatus = cudaMalloc((void**)&dev_c, size * sizeof(int));
    if (cudaStatus != cudaSuccess) 
	{
        fprintf(stderr, "cudaMalloc failed!");
        goto Error;
    }

    cudaStatus = cudaMalloc((void**)&dev_a, size * sizeof(int));
    if (cudaStatus != cudaSuccess) 
	{
        fprintf(stderr, "cudaMalloc failed!");
        goto Error;
    }

    cudaStatus = cudaMalloc((void**)&dev_b, size * sizeof(int));
    if (cudaStatus != cudaSuccess)
	{
        fprintf(stderr, "cudaMalloc failed!");
        goto Error;
    }

    // Copy input vectors from host memory to GPU buffers.
    cudaStatus = cudaMemcpy(dev_a, a, size * sizeof(int), cudaMemcpyHostToDevice);
    if (cudaStatus != cudaSuccess) 
	{
        fprintf(stderr, "cudaMemcpy failed!");
        goto Error;
    }

    cudaStatus = cudaMemcpy(dev_b, b, size * sizeof(int), cudaMemcpyHostToDevice);
    if (cudaStatus != cudaSuccess) 
	{
        fprintf(stderr, "cudaMemcpy failed!");
        goto Error;
    }
	dim3 ngrid = { (arraySize + blockperthread-1) / blockperthread , 1,1};
	int perthread = blockperthread;
	dim3 nthread = { 512,1,1 };
    // Launch a kernel on the GPU with one thread for each element.
    addKernel<< <ngrid, nthread >> >(dev_c, dev_a, dev_b);

    // Check for any errors launching the kernel
    cudaStatus = cudaGetLastError();
    if (cudaStatus != cudaSuccess) {
        fprintf(stderr, "addKernel launch failed: %s\n", cudaGetErrorString(cudaStatus));
        goto Error;
    }
    
    // cudaDeviceSynchronize waits for the kernel to finish, and returns
    // any errors encountered during the launch.
    cudaStatus = cudaDeviceSynchronize();
    if (cudaStatus != cudaSuccess) {
        fprintf(stderr, "cudaDeviceSynchronize returned error code %d after launching addKernel!\n", cudaStatus);
        goto Error;
    }

    // Copy output vector from GPU buffer to host memory.
    cudaStatus = cudaMemcpy(c, dev_c, size * sizeof(int), cudaMemcpyDeviceToHost);
    if (cudaStatus != cudaSuccess) {
        fprintf(stderr, "cudaMemcpy failed!");
        goto Error;
    }

Error:
    cudaFree(dev_c);
    cudaFree(dev_a);
    cudaFree(dev_b);
    
    return cudaStatus;
}
#endif