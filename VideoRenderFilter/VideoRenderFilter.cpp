#include "VideoRenderFilter.h"
#include "../Common/MediaTimer.h"
#include "../Common/utils.h"
#include "../Common/Helpers.h"
#include "ConversionMatrix.h"
#include "../Common/SSMainConfiguration.h"
struct VertexType
{
	XMFLOAT3 position;
	XMFLOAT2 texture;
};
struct MatrixBufferType
{
	XMMATRIX world;
	XMMATRIX view;
	XMMATRIX projection;
};
#define DEFAULT_BRIGHTNESS         200
#define DEFAULT_SRGB_BRIGHTNESS    200
#define MAX_HLG_BRIGHTNESS        1000
#define MAX_PQ_BRIGHTNESS        10000
dxgi_color_space color_spaces[]=
{
	{DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709,"DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709",COLOR_AXIS_RGB,AVCOL_PRI_BT709,AVCOL_TRC_BT709, AVCOL_SPC_BT709,true},
	{DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709,"DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709",COLOR_AXIS_RGB,AVCOL_PRI_BT709,AVCOL_TRC_LINEAR, AVCOL_SPC_BT709,true},
	{DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P709,"DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P709",COLOR_AXIS_RGB,AVCOL_PRI_BT709,AVCOL_TRC_BT709, AVCOL_SPC_BT709,false},
	{DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P2020,"DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P2020",COLOR_AXIS_RGB,AVCOL_PRI_BT2020,AVCOL_TRC_BT709, AVCOL_SPC_BT2020_NCL,false},
	{DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020,"DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020",COLOR_AXIS_RGB,AVCOL_PRI_BT2020,AVCOL_TRC_SMPTE2084, AVCOL_SPC_BT2020_NCL,true},
	{DXGI_COLOR_SPACE_RGB_STUDIO_G2084_NONE_P2020,"DXGI_COLOR_SPACE_RGB_STUDIO_G2084_NONE_P2020",COLOR_AXIS_RGB,AVCOL_PRI_BT2020,AVCOL_TRC_SMPTE2084, AVCOL_SPC_BT2020_NCL,false},
	{DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020,"DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020",COLOR_AXIS_RGB,AVCOL_PRI_BT2020,AVCOL_TRC_GAMMA22, AVCOL_SPC_BT2020_NCL,true},
	{DXGI_COLOR_SPACE_RGB_STUDIO_G24_NONE_P709,"DXGI_COLOR_SPACE_RGB_STUDIO_G24_NONE_P709",COLOR_AXIS_RGB,AVCOL_PRI_BT709,AVCOL_TRC_BT709, AVCOL_SPC_BT709,false},
	{DXGI_COLOR_SPACE_RGB_STUDIO_G24_NONE_P2020,"DXGI_COLOR_SPACE_RGB_STUDIO_G24_NONE_P2020",COLOR_AXIS_RGB,AVCOL_PRI_BT2020,AVCOL_TRC_BT709, AVCOL_SPC_BT2020_NCL,false},
};
struct ConvYCbCr
{
	float Kr, Kb;
};


const ConvYCbCr BT709YCbCr = { 0.2126, 0.0722 };
const ConvYCbCr BT601YCbCr = { 0.299, 0.114 };
const ConvYCbCr BT2020YCbCr = { 0.2627, 0.0593 };
const ConvYCbCr ST240YCbCr = { 0.212, 0.087 };

const Primaries PrimariesBT709 = { {{0.640, 0.330}, {0.300, 0.600}, {0.150, 0.060}},
  {0.3127, 0.3290} };
const Primaries PrimariesBT610_525 = { {{0.640, 0.340}, {0.310, 0.595}, {0.155, 0.070}},
  {0.3127, 0.3290} };
const Primaries PrimariesBT610_625 = { {{0.640, 0.330}, {0.290, 0.600}, {0.150, 0.060}},
  {0.3127, 0.3290} };
const Primaries PrimariesBT2020 = { {{0.708, 0.292}, {0.170, 0.797}, {0.131, 0.046}},
  {0.3127, 0.3290} };


VideoRenderFilter::~VideoRenderFilter()
{
	Shutdown();
}

float GetFormatLuminance(AVColorTransferCharacteristic srctranfunc)
{
	switch (srctranfunc)
	{
	case AVCOL_TRC_SMPTE2084:
		return MAX_PQ_BRIGHTNESS;
	case AVCOL_TRC_ARIB_STD_B67:
		return MAX_HLG_BRIGHTNESS;
	default:
		return DEFAULT_BRIGHTNESS;
	}
}

struct xy_primary {
	double x, y;
};

struct cie1931_primaries {
	int primary;
	struct xy_primary red, green, blue, white;
};

static const struct cie1931_primaries STANDARD_PRIMARIES[] = {
#define CIE_D65 {0.31271, 0.32902}
#define CIE_C   {0.31006, 0.31616}
	 {AVCOL_PRI_SMPTE170M,{0.630, 0.340},{0.310, 0.595}, {0.155, 0.070},CIE_D65},
	{AVCOL_PRI_BT470BG, {0.640, 0.330},{0.290, 0.600}, {0.150, 0.060}, CIE_D65},
	 {AVCOL_PRI_BT709, {0.640, 0.330}, {0.300, 0.600},{0.150, 0.060}, CIE_D65},
	 {AVCOL_PRI_BT2020, {0.708, 0.292}, {0.170, 0.797},{0.131, 0.046}, CIE_D65},
	{AVCOL_PRI_SMPTE431, {0.680, 0.320}, {0.265, 0.690},{0.150, 0.060}, CIE_D65},
#undef CIE_D65
#undef CIE_C
};

static void Float3x3Inverse(double in_out[3 * 3])
{
	double m00 = in_out[0 + 0 * 3], m01 = in_out[1 + 0 * 3], m02 = in_out[2 + 0 * 3],
		m10 = in_out[0 + 1 * 3], m11 = in_out[1 + 1 * 3], m12 = in_out[2 + 1 * 3],
		m20 = in_out[0 + 2 * 3], m21 = in_out[1 + 2 * 3], m22 = in_out[2 + 2 * 3];

	// calculate the adjoint
	in_out[0 + 0 * 3] = (m11 * m22 - m21 * m12);
	in_out[1 + 0 * 3] = -(m01 * m22 - m21 * m02);
	in_out[2 + 0 * 3] = (m01 * m12 - m11 * m02);
	in_out[0 + 1 * 3] = -(m10 * m22 - m20 * m12);
	in_out[1 + 1 * 3] = (m00 * m22 - m20 * m02);
	in_out[2 + 1 * 3] = -(m00 * m12 - m10 * m02);
	in_out[0 + 2 * 3] = (m10 * m21 - m20 * m11);
	in_out[1 + 2 * 3] = -(m00 * m21 - m20 * m01);
	in_out[2 + 2 * 3] = (m00 * m11 - m10 * m01);

	// calculate the determinant (as inverse == 1/det * adjoint,
	// adjoint * m == identity * det, so this calculates the det)
	double det = m00 * in_out[0 + 0 * 3] + m10 * in_out[1 + 0 * 3] + m20 * in_out[2 + 0 * 3];
	det = 1.0f / det;

	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++)
			in_out[j + i * 3] *= det;
	}
}

static void Float3x3Multiply(double m1[3 * 3], const double m2[3 * 3])
{
	double a00 = m1[0 + 0 * 3], a01 = m1[1 + 0 * 3], a02 = m1[2 + 0 * 3],
		a10 = m1[0 + 1 * 3], a11 = m1[1 + 1 * 3], a12 = m1[2 + 1 * 3],
		a20 = m1[0 + 2 * 3], a21 = m1[1 + 2 * 3], a22 = m1[2 + 2 * 3];

	for (int i = 0; i < 3; i++) {
		m1[i + 0 * 3] = a00 * m2[i + 0 * 3] + a01 * m2[i + 1 * 3] + a02 * m2[i + 2 * 3];
		m1[i + 1 * 3] = a10 * m2[i + 0 * 3] + a11 * m2[i + 1 * 3] + a12 * m2[i + 2 * 3];
		m1[i + 2 * 3] = a20 * m2[i + 0 * 3] + a21 * m2[i + 1 * 3] + a22 * m2[i + 2 * 3];
	}
}

static void Float3Multiply(const double in[3], const double mult[3 * 3], double out[3])
{
	for (size_t i = 0; i < 3; i++)
	{
		out[i] = mult[i + 0 * 3] * in[0] +
			mult[i + 1 * 3] * in[1] +
			mult[i + 2 * 3] * in[2];
	}
}

/* from http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html */
static void GetRGB2XYZMatrix(const struct cie1931_primaries *primaries,
	double out[3 * 3])
{
#define RED   0
#define GREEN 1
#define BLUE  2
	double X[3], Y[3], Z[3], S[3], W[3];
	double W_TO_S[3 * 3];

	X[RED] = primaries->red.x / primaries->red.y;
	X[GREEN] = 1;
	X[BLUE] = (1 - primaries->red.x - primaries->red.y) / primaries->red.y;

	Y[RED] = primaries->green.x / primaries->green.y;
	Y[GREEN] = 1;
	Y[BLUE] = (1 - primaries->green.x - primaries->green.y) / primaries->green.y;

	Z[RED] = primaries->blue.x / primaries->blue.y;
	Z[GREEN] = 1;
	Z[BLUE] = (1 - primaries->blue.x - primaries->blue.y) / primaries->blue.y;

	W_TO_S[0 + 0 * 3] = X[RED];
	W_TO_S[1 + 0 * 3] = X[GREEN];
	W_TO_S[2 + 0 * 3] = X[BLUE];
	W_TO_S[0 + 1 * 3] = Y[RED];
	W_TO_S[1 + 1 * 3] = Y[GREEN];
	W_TO_S[2 + 1 * 3] = Y[BLUE];
	W_TO_S[0 + 2 * 3] = Z[RED];
	W_TO_S[1 + 2 * 3] = Z[GREEN];
	W_TO_S[2 + 2 * 3] = Z[BLUE];

	Float3x3Inverse(W_TO_S);

	W[0] = primaries->white.x / primaries->white.y; /* Xw */
	W[1] = 1;                  /* Yw */
	W[2] = (1 - primaries->white.x - primaries->white.y) / primaries->white.y; /* Yw */

	Float3Multiply(W, W_TO_S, S);

	out[0 + 0 * 3] = S[RED] * X[RED];
	out[1 + 0 * 3] = S[GREEN] * Y[RED];
	out[2 + 0 * 3] = S[BLUE] * Z[RED];
	out[0 + 1 * 3] = S[RED] * X[GREEN];
	out[1 + 1 * 3] = S[GREEN] * Y[GREEN];
	out[2 + 1 * 3] = S[BLUE] * Z[GREEN];
	out[0 + 2 * 3] = S[RED] * X[BLUE];
	out[1 + 2 * 3] = S[GREEN] * Y[BLUE];
	out[2 + 2 * 3] = S[BLUE] * Z[BLUE];
#undef RED
#undef GREEN
#undef BLUE
}

/* from http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html */
static void GetXYZ2RGBMatrix(const struct cie1931_primaries *primaries,
	double out[3 * 3])
{
	GetRGB2XYZMatrix(primaries, out);
	Float3x3Inverse(out);
}
static void ChromaticAdaptation(const struct xy_primary *src_white,
	const struct xy_primary *dst_white,
	double in_out[3 * 3])
{
	if (fabs(src_white->x - dst_white->x) < 1e-6 &&
		fabs(src_white->y - dst_white->y) < 1e-6)
		return;

	/* TODO, see http://www.brucelindbloom.com/index.html?Eqn_ChromAdapt.html */
}


void GetPrimariesTransform(FLOAT Primaries[4 * 4], AVColorPrimaries src1, AVColorPrimaries dst1)
{
	int src = 0;
	int dst = 0;
	for (int i = 0; i < sizeof(STANDARD_PRIMARIES) / sizeof(STANDARD_PRIMARIES[0]); i++)
	{
		if (STANDARD_PRIMARIES[i].primary == src1)
		{
			src = i;
		}
		if (STANDARD_PRIMARIES[i].primary == dst1)
		{
			dst = i;
		}
	}
	const struct cie1931_primaries *p_src = &STANDARD_PRIMARIES[src];
	const struct cie1931_primaries *p_dst = &STANDARD_PRIMARIES[dst];
	double rgb2xyz[3 * 3], xyz2rgb[3 * 3];

	/* src[RGB] -> src[XYZ] */
	GetRGB2XYZMatrix(p_src, rgb2xyz);

	/* src[XYZ] -> dst[XYZ] */
	ChromaticAdaptation(&p_src->white, &p_dst->white, rgb2xyz);

	/* dst[XYZ] -> dst[RGB] */
	GetXYZ2RGBMatrix(p_dst, xyz2rgb);

	/* src[RGB] -> src[XYZ] -> dst[XYZ] -> dst[RGB] */
	Float3x3Multiply(xyz2rgb, rgb2xyz);

	for (size_t i = 0; i < 3; ++i)
	{
		for (size_t j = 0; j < 3; ++j)
			Primaries[j + i * 4] = xyz2rgb[j + i * 3];
		Primaries[3 + i * 4] = 0;
	}
	for (size_t j = 0; j < 4; ++j)
		Primaries[j + 3 * 4] = j == 3;
}

bool IsRGB(CFrameSharePtr &stFrame)
{
	bool bRet = true;
	if (stFrame->m_ePixType == eBGRA)
	{

	}
	else if (stFrame->m_ePixType == eRGBA)
	{

	}
	else if (stFrame->m_ePixType == eBGR)
	{

	}
	else
	{
		bRet = false;
	}
	return bRet;
	
}

void VideoRenderFilter::SetColPrimaries(AVColorPrimaries src, AVColorPrimaries dst, AVColorTransferCharacteristic srctranfunc, AVColorTransferCharacteristic dsttranfunc, CFrameSharePtr &stFrame, bool bvirtual)
{
	m_colPrimariesDst = dst;
	m_colPrimariesSrc = src;

	if (m_colPrimariesDst != m_colPrimariesSrc)
	{
		FLOAT Primaries[4 * 4] = { 0 };
		GetPrimariesTransform(Primaries, m_colPrimariesSrc, m_colPrimariesDst);
		for (int i = 0; i < 16; i++)
		{
			m_pixtransform.Primaries[i] = Primaries[i];
		}

		GetPrimariesTransform(Primaries, m_colPrimariesDst,m_colPrimariesSrc);
		for (int i = 0; i < 16; i++)
		{
			m_pixtransform.DisTransPrimaries[i] = Primaries[i];
		}

		
		
	}
	bool bRGB = IsRGB(stFrame);
	{
		FLOAT itu_black_level = 0.f;
		FLOAT itu_achromacy = 0.f;
		if (!bRGB)
		{
			switch (stFrame->m_nPixBits)
			{
			case 8:
				/* Rec. ITU-R BT.709-6 §4.6 */
				itu_black_level = 16.f / 255.f;
				itu_achromacy = 128.f / 255.f;
				break;
			case 10:
				/* Rec. ITU-R BT.709-6 §4.6 */
				itu_black_level = 64.f / 1023.f;
				itu_achromacy = 512.f / 1023.f;
				break;
			case 12:
				/* Rec. ITU-R BT.2020-2 Table 5 */
				itu_black_level = 256.f / 4095.f;
				itu_achromacy = 2048.f / 4095.f;
				break;
			default:
				/* unknown bitdepth, use approximation for infinite bit depth */
				itu_black_level = 16.f / 256.f;
				itu_achromacy = 128.f / 256.f;
				break;
			}
		}

		static const FLOAT IDENTITY_4X4[4 * 4] = {
			1.f, 0.f, 0.f, 0.f,
			0.f, 1.f, 0.f, 0.f,
			0.f, 0.f, 1.f, 0.f,
			0.f, 0.f, 0.f, 1.f,
		};

		/* matrices for studio range */
		/* see https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.601_conversion, in studio range */
		static const FLOAT COLORSPACE_BT601_YUV_TO_FULL_RGBA[4 * 4] = {
			1.164383561643836f,                 0.f,  1.596026785714286f, 0.f,
			1.164383561643836f, -0.391762290094914f, -0.812967647237771f, 0.f,
			1.164383561643836f,  2.017232142857142f,                 0.f, 0.f,
						   0.f,                 0.f,                 0.f, 1.f,
		};
		/* see https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.709_conversion, in studio range */
		static const FLOAT COLORSPACE_BT709_YUV_TO_FULL_RGBA[4 * 4] = {
			1.164383561643836f,                 0.f,  1.792741071428571f, 0.f,
			1.164383561643836f, -0.213248614273730f, -0.532909328559444f, 0.f,
			1.164383561643836f,  2.112401785714286f,                 0.f, 0.f,
						   0.f,                 0.f,                 0.f, 1.f,
		};
		/* see https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.2020_conversion, in studio range */
		static const FLOAT COLORSPACE_BT2020_YUV_TO_FULL_RGBA[4 * 4] = {
			1.164383561643836f,  0.000000000000f,  1.678674107143f, 0.f,
			1.164383561643836f, -0.127007098661f, -0.440987687946f, 0.f,
			1.164383561643836f,  2.141772321429f,  0.000000000000f, 0.f,
						   0.f,              0.f,              0.f, 1.f,
		};

		//PS_COLOR_TRANSFORM colorspace;

		memcpy(m_pixtransform.WhitePoint, IDENTITY_4X4, sizeof(m_pixtransform.WhitePoint));

		const FLOAT *ppColorspace;
		if (bRGB)
			ppColorspace = IDENTITY_4X4;
		else 
		{
			switch (stFrame->color_primaries)
			{
			case AVCOL_PRI_BT709:
				ppColorspace = COLORSPACE_BT709_YUV_TO_FULL_RGBA;
				break;
			case AVCOL_PRI_BT2020:
				ppColorspace = COLORSPACE_BT2020_YUV_TO_FULL_RGBA;
				break;
			case AVCOL_PRI_BT470BG:
				ppColorspace = COLORSPACE_BT601_YUV_TO_FULL_RGBA;
				break;
			default:
				if (stFrame->m_nHeight > 576)
					ppColorspace = COLORSPACE_BT709_YUV_TO_FULL_RGBA;
				else
					ppColorspace = COLORSPACE_BT601_YUV_TO_FULL_RGBA;
				break;
			}
			/* all matrices work in studio range and output in full range */
			m_pixtransform.WhitePoint[0 * 4 + 3] = -itu_black_level;
			m_pixtransform.WhitePoint[1 * 4 + 3] = -itu_achromacy;
			m_pixtransform.WhitePoint[2 * 4 + 3] = -itu_achromacy;
		}
		for (int i = 0; i < 16; i++)
		{
			m_pixtransform.Colorspace[i] = ppColorspace[i];
		}
	}

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT hr = m_deviceContext->Map((ID3D11Resource *)m_pixtranformBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(hr))
	{
		return;
	}
	PS_COLOR_TRANSFORM *dst_data = (PS_COLOR_TRANSFORM *)mappedResource.pData;
	*dst_data = m_pixtransform;
	m_deviceContext->Unmap((ID3D11Resource *)m_pixtranformBuffer, 0);
	m_pTransPrimariesInfo->SetConstantBuffer(m_pixtranformBuffer);

	float scale = m_displayInfo.luminance_peak / GetFormatLuminance(srctranfunc);
	scale = GetFormatLuminance(dsttranfunc) / GetFormatLuminance(srctranfunc);
	if (bvirtual)
	{
		scale = 200;
	}
	m_pLuminanceScale->SetFloat(scale);
}
VideoRenderFilter::VideoRenderFilter(long index, QWidget*  parent, std::string &strName, bool bdirect) : CSSFilter(strName)
{
	m_nIndex = index;
	m_bDirect = bdirect;
	m_alphaEnableBlendingState = NULL;
	m_alphaDisableBlendingState = NULL;
	m_swapChain = 0;
	m_swapChain4 = 0;
	m_device = 0;
	m_deviceContext = 0;
	m_renderTargetView = 0;
	m_textureText = NULL;
	m_texture2dText = NULL;
	m_renderTextureTargetView = NULL;
	m_indexBuffer = NULL;
	m_vertexBuffer = NULL;
	m_vertexBufferSource = NULL;
	m_layout = NULL;
	m_pEffect = NULL;

	for (int i = 0; i < MAXPLANE; i++)
	{
		m_pSourceTexture2d[i] = NULL;
		m_pSourceTexture[i] = NULL;
	}
	for (int i = 0; i < 3; i++)
	{
		 m_hSrcHandle[i]=0;
		 m_texture2dpip[i]=0;
		 m_texturepip[i]=0;
	}

	m_nLastWidth = 0;
	m_nLastHeight = 0;
	m_RenderPixelFormat = eUnknowPix;
	if (1)
	{
		m_pWidget = parent;
		m_nTextureWidth = m_pWidget->width();
		m_nTextureHeight = m_pWidget->height();
	
	}
	else
	{
		m_pWidget = new QWidget();
		m_nTextureWidth = 1920;
		m_nTextureHeight = 1080;
		m_pWidget->setFixedSize(m_nTextureWidth, m_nTextureHeight);
		m_pWidget->show();
	}

	m_bRendUpdate = true;
	int nWidth = m_pWidget->width();
	int nHeight = m_pWidget->height();
	HWND hwnd = (HWND)m_pWidget->winId();
	Initialize(nWidth, nHeight, hwnd);
	long long nNow = QkTimer::now();
	QString strEventName = QString::number(nNow);
	m_hEventHandle = CreateEvent(NULL, FALSE, FALSE, strEventName.toStdWString().c_str());
	m_bExit = false;
	m_hSyncThread = CreateThread(NULL, 0, SyncRead, this, 0, NULL);
}

int VideoRenderFilter::InputData(CFrameSharePtr &frame)
{
	if (frame->m_eFrameType == eVideoFrame)
	{
		std::lock_guard<std::mutex>  stLock(m_stFrameLockEx);
		m_pBRendFrame = frame;
		bool bSetEvent = SetEvent(m_hEventHandle);
	}
	return 0;
}

void VideoRenderFilter::RenderToWindow(bool bDirect)
{
	XMMATRIX worldMatrix, viewMatrix, projectionMatrix, orthoMatrix;
	// renderto  backbuffer
	if (m_bRendUpdate)
	{
		m_pWidget->setAttribute(Qt::WA_OpaquePaintEvent);
		m_pWidget->setUpdatesEnabled(false);
		m_bRendUpdate = false;
	}
	m_deviceContext->OMSetRenderTargets(1, &m_renderTargetView, NULL);
	m_deviceContext->RSSetViewports(1, &viewport);
	BeginScene(0.0f, 0.0f, 0.0f, 1.0f, m_renderTargetView, NULL);
	RenderBuffers(m_vertexBuffer);
	XMFLOAT4X4 viewwindows;
	viewwindows.m[0][0] = 2.0f / m_nTextureWidth;
	viewwindows.m[0][1] = 0.0f;
	viewwindows.m[0][2] = 0.0f;
	viewwindows.m[0][3] = 0.0f;
	viewwindows.m[1][0] = 0.0f;
	viewwindows.m[1][1] = -2.0f / m_nTextureHeight;
	viewwindows.m[1][2] = 0.0f;
	viewwindows.m[1][3] = 0.0f;
	viewwindows.m[2][0] = 0.0f;
	viewwindows.m[2][1] = 0.0f;
	viewwindows.m[2][2] = 1.0f;
	viewwindows.m[2][3] = 0.0f;
	viewwindows.m[3][0] = -1.0f;
	viewwindows.m[3][1] = 1.0f;
	viewwindows.m[3][2] = 0.0f;
	viewwindows.m[3][3] = 1.0f;
	viewMatrix = XMLoadFloat4x4(&viewwindows);
	m_pViewMatVar->SetMatrix(reinterpret_cast<const float*>(&viewMatrix));
	if (bDirect)
	{

	}
	else
	{

	}
	m_pTextSourceY->SetResource((ID3D11ShaderResourceView*)m_textureText);
	m_pType->SetInt(1);
	m_ptransfer->SetInt(m_displayInfo.dxgicolor.transfer);
	m_pdistransfer->SetInt(m_displayInfo.dxgicolor.transfer);
	m_pprimaries->SetInt(m_displayInfo.dxgicolor.primaries);
	m_pdisprimaries->SetInt(m_displayInfo.dxgicolor.primaries);
	m_pfullrange->SetInt(1);
	m_psrcrange->SetInt(1);
	m_deviceContext->IASetInputLayout(m_layout);
	D3DX11_TECHNIQUE_DESC techDescwindow;
	m_pTech->GetDesc(&techDescwindow);
	for (UINT i = 0; i < techDescwindow.Passes; ++i)
	{
		m_pTech->GetPassByIndex(i)->Apply(0, m_deviceContext);
		m_deviceContext->DrawIndexed(m_indexCount, 0, 0);
	}
	EndScene();
}

void VideoRenderFilter::UpdateBackBuffer()
{
	std::lock_guard<std::mutex> stLock(m_stD3DLock);
	ID3D11Texture2D* backBufferPtr = NULL;
	do
	{
	
		ID3D11Resource *res = NULL;
		D3D11_TEXTURE2D_DESC dsc = { 0 };
		m_renderTargetView->GetResource(&res);
		if (res)
		{
			ID3D11Texture2D * texure2d = (ID3D11Texture2D*)res;
			texure2d->GetDesc(&dsc);
			res->Release();
		}
		if (dsc.Width == m_nTextureWidth && dsc.Height == m_nTextureHeight && dsc.Format== m_dstBufferDXFormat)
		{
			break;
		}
		m_deviceContext->ClearState();

		if (m_renderTargetView)
		{
			m_renderTargetView->Release();
			m_renderTargetView = NULL;
		}
		HRESULT result = m_swapChain->ResizeBuffers(0, m_nTextureWidth, m_nTextureHeight, m_dstBufferDXFormat, 0);
		// Get the pointer to the back buffer.
		result = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backBufferPtr);
		if (FAILED(result))
		{
			break;
		}
		result = m_device->CreateRenderTargetView(backBufferPtr, NULL, &m_renderTargetView);
		if (FAILED(result))
		{
			break;
		}
		
		viewport.Width = (float)m_nTextureWidth;
		viewport.Height = (float)m_nTextureHeight;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;

	} while (0);
	if (backBufferPtr)
	{
		backBufferPtr->Release();
		backBufferPtr = 0;
	}
}

void VideoRenderFilter::ResizeBackBuffer(int nWidth, int nHeight)
{
	std::lock_guard<std::mutex> stLock(m_stD3DLock);
	m_nTextureWidth = nWidth;
	m_nTextureHeight = nHeight;
}

bool VideoRenderFilter::ReadData()
{
	bool bRet = false;
	CFrameSharePtr stFrame = NULL;
	{
		std::lock_guard<std::mutex>  stLock(m_stFrameLockEx);
		stFrame = m_pBRendFrame;
		m_pBRendFrame = NULL;
	}
	long long nNow = QkTimer::now();
	UpdateBackBuffer();
	if (stFrame != NULL)
	{
		bRet = true;
	}
	else
	{
		return bRet;
	}

	int nType = 0;


	TurnOnAlphaBlending();
	bool bRsp = ResetD3DResource(stFrame);
	if (false == bRsp)
	{
		return bRsp;
	}
	bRsp = ReadFrameDataToTexture(stFrame);
	if (false == bRsp)
	{
		return bRsp;
	}
	if (stFrame->m_ePixType == eBGRA)
	{
		nType = 1;
	}
	else if (eYVYU422 == stFrame->m_ePixType)
	{
		nType = 6;
	}
	else if (stFrame->m_ePixType == eUYVY422)
	{
		nType = 5;
	}
	else if (stFrame->m_ePixType == eYUYV422)
	{
		nType = 3;
	}
	else if (eBGR == stFrame->m_ePixType)
	{
		nType = 4;
	}
	else if (eRGBA == stFrame->m_ePixType)
	{
		nType = 7;
	}
	else if (eYUV420P10 == stFrame->m_ePixType)
	{
		nType = 8;
	}
	else
	{
		nType = 2;
	}

	int transfer = m_displayInfo.transfer;
	if (stFrame->color_trc == AVCOL_TRC_BT709)
	{
		transfer = AVCOL_TRC_BT709;
	}
	else if (stFrame->color_trc == AVCOL_TRC_SMPTEST2084) // HDR10
	{
		transfer = AVCOL_TRC_SMPTEST2084;
	}
	else if (stFrame->color_trc == AVCOL_TRC_ARIB_STD_B67) // HLG
	{
		transfer = AVCOL_TRC_ARIB_STD_B67;
	}
	else
	{
		transfer = stFrame->color_trc;
	}
	int distransfer = m_displayInfo.dxgicolor.transfer;
	int primaries = primaries = stFrame->color_primaries;
	
	SetSwapchainSetMetadata(stFrame);
	int disprimaries = m_displayInfo.dxgicolor.primaries;
	int fullrange = 1;
	int srcrange = 1;

	XMMATRIX worldMatrix, viewMatrix, projectionMatrix, orthoMatrix;

	// render to texture
	if (!m_bDirect)
	{
		m_deviceContext->OMSetRenderTargets(1, &m_renderTextureTargetView, NULL);
		m_deviceContext->RSSetViewports(1, &viewportframe);
		BeginScene(0.0f, 0.0f, 0.0f, 1.0f, m_renderTextureTargetView, NULL);
		RenderBuffers(m_vertexBufferSource);
		XMFLOAT4X4 view;
		view.m[0][0] = 2.0f / stFrame->m_nWidth;
		view.m[0][1] = 0.0f;
		view.m[0][2] = 0.0f;
		view.m[0][3] = 0.0f;
		view.m[1][0] = 0.0f;
		view.m[1][1] = -2.0f / stFrame->m_nHeight;
		view.m[1][2] = 0.0f;
		view.m[1][3] = 0.0f;
		view.m[2][0] = 0.0f;
		view.m[2][1] = 0.0f;
		view.m[2][2] = 1.0f;
		view.m[2][3] = 0.0f;
		view.m[3][0] = -1.0f;
		view.m[3][1] = 1.0f;
		view.m[3][2] = 0.0f;
		view.m[3][3] = 1.0f;
		viewMatrix = XMLoadFloat4x4(&view);
		m_pViewMatVar->SetMatrix(reinterpret_cast<const float*>(&viewMatrix));
		if (m_dstRenderDXFormat == DXGI_FORMAT_R16G16B16A16_FLOAT)
		{
			SetColPrimaries(stFrame->color_primaries, stFrame->color_primaries, stFrame->color_trc, stFrame->color_trc, stFrame);
			m_pdistransfer->SetInt(AVCOL_TRC_SMPTE2084);
			m_pdisprimaries->SetInt(AVCOL_PRI_BT2020);
		}
		else
		{
			SetColPrimaries(stFrame->color_primaries, stFrame->color_primaries, stFrame->color_trc, stFrame->color_trc, stFrame);
			m_pdistransfer->SetInt(AVCOL_TRC_BT709);
			m_pdisprimaries->SetInt(AVCOL_PRI_BT709);
		}
		m_pTextSourceY->SetResource((ID3D11ShaderResourceView*)m_pSourceTexture[0]);
		m_pTextSourceU->SetResource((ID3D11ShaderResourceView*)m_pSourceTexture[1]);
		m_pTextSourceV->SetResource((ID3D11ShaderResourceView*)m_pSourceTexture[2]);
		m_pTextSourceA->SetResource((ID3D11ShaderResourceView*)m_pSourceTexture[3]);
		m_pType->SetInt(nType);
		m_pSourceHeight->SetInt(stFrame->m_nHeight);
		m_pSourceWidth->SetInt(stFrame->m_nWidth);
		m_ptransfer->SetInt(transfer);
		m_pprimaries->SetInt(primaries);
		m_pfullrange->SetInt(fullrange);
		m_psrcrange->SetInt(srcrange);
		m_pDrawLine->SetInt(1);
		m_deviceContext->IASetInputLayout(m_layout);
		D3DX11_TECHNIQUE_DESC techDesc;
		m_pTech->GetDesc(&techDesc);
		for (UINT i = 0; i < techDesc.Passes; ++i)
		{
			m_pTech->GetPassByIndex(i)->Apply(0, m_deviceContext);
			m_deviceContext->DrawIndexed(m_indexCount, 0, 0);
		}
		m_deviceContext->CopyResource(m_texture2dShared,m_texture2dText);

	}
	ID3D11ShaderResourceView *const pSRV[3] = { NULL };
	m_deviceContext->PSSetShaderResources(0, 3, pSRV);
	//XMMATRIX worldMatrix, viewMatrix, projectionMatrix, orthoMatrix;
	// renderto  backbuffer
	if (m_bRendUpdate)
	{
		m_pWidget->setAttribute(Qt::WA_OpaquePaintEvent);
		m_pWidget->setUpdatesEnabled(false);
		m_bRendUpdate = false;
	}
	m_deviceContext->OMSetRenderTargets(1, &m_renderTargetView, NULL);
	m_deviceContext->RSSetViewports(1, &viewport);
	BeginScene(0.0f, 0.0f, 0.0f, 1.0f, m_renderTargetView, NULL);
	RenderBuffers(m_vertexBuffer);
	XMFLOAT4X4 viewwindows;
	viewwindows.m[0][0] = 2.0f / m_nTextureWidth;
	viewwindows.m[0][1] = 0.0f;
	viewwindows.m[0][2] = 0.0f;
	viewwindows.m[0][3] = 0.0f;
	viewwindows.m[1][0] = 0.0f;
	viewwindows.m[1][1] = -2.0f / m_nTextureHeight;
	viewwindows.m[1][2] = 0.0f;
	viewwindows.m[1][3] = 0.0f;
	viewwindows.m[2][0] = 0.0f;
	viewwindows.m[2][1] = 0.0f;
	viewwindows.m[2][2] = 1.0f;
	viewwindows.m[2][3] = 0.0f;
	viewwindows.m[3][0] = -1.0f;
	viewwindows.m[3][1] = 1.0f;
	viewwindows.m[3][2] = 0.0f;
	viewwindows.m[3][3] = 1.0f;
	viewMatrix = XMLoadFloat4x4(&viewwindows);
	m_pViewMatVar->SetMatrix(reinterpret_cast<const float*>(&viewMatrix));
	if (false == m_bDirect)
	{
		if (m_dstRenderDXFormat == DXGI_FORMAT_R16G16B16A16_FLOAT)
		{
			SetColPrimaries(stFrame->color_primaries, m_displayInfo.dxgicolor.primaries, stFrame->color_trc, m_displayInfo.dxgicolor.transfer, stFrame);
			m_ptransfer->SetInt(stFrame->color_trc);
			m_pprimaries->SetInt(stFrame->color_primaries);
		}
		else
		{
			SetColPrimaries(stFrame->color_primaries, m_displayInfo.dxgicolor.primaries, stFrame->color_trc, m_displayInfo.dxgicolor.transfer, stFrame);
			m_ptransfer->SetInt(stFrame->color_trc);
			m_pprimaries->SetInt(stFrame->color_primaries);
		}
		
		if (0)
		{
			m_pTextSourceY->SetResource((ID3D11ShaderResourceView*)m_pSourceTexture[0]);
			m_pTextSourceU->SetResource((ID3D11ShaderResourceView*)m_pSourceTexture[1]);
			m_pTextSourceV->SetResource((ID3D11ShaderResourceView*)m_pSourceTexture[2]);
			m_pTextSourceA->SetResource((ID3D11ShaderResourceView*)m_pSourceTexture[3]);
			m_pType->SetInt(nType);
			m_pDrawLine->SetInt(2);
		}
		else
		{
			m_pTextSourceY->SetResource((ID3D11ShaderResourceView*)m_textureText);
			m_pTextSourceU->SetResource(0);
			m_pTextSourceV->SetResource(0);
			m_pTextSourceA->SetResource(0);
			m_pType->SetInt(1);
			m_pDrawLine->SetInt(0);
		}
		
		
		m_pSourceHeight->SetInt(stFrame->m_nHeight);
		m_pSourceWidth->SetInt(stFrame->m_nWidth);
		m_pdistransfer->SetInt(distransfer);
		m_pdisprimaries->SetInt(disprimaries);
		m_pfullrange->SetInt(fullrange);
		m_psrcrange->SetInt(srcrange);
		
		m_deviceContext->IASetInputLayout(m_layout);
		D3DX11_TECHNIQUE_DESC techDescwindow;
		m_pTech->GetDesc(&techDescwindow);
		for (UINT i = 0; i < techDescwindow.Passes; ++i)
		{
			m_pTech->GetPassByIndex(i)->Apply(0, m_deviceContext);
			m_deviceContext->DrawIndexed(m_indexCount, 0, 0);
		}
	}
	
	
	EndScene();
	m_deviceContext->PSSetShaderResources(0, 1, pSRV);
	return bRet;
}

bool VideoRenderFilter::ReadDataPGM()
{
	bool bRet = false;
	long long nNow = QkTimer::now();
	UpdateBackBuffer();
	int nType = 0;
	TurnOnAlphaBlending();
	CFrameSharePtr stFrame = NewShareFrame();
	stFrame->m_nWidth = m_nTextureWidth;
	stFrame->m_nHeight = m_nTextureHeight;
	stFrame->m_nPixBits = 10;
	stFrame->colorspace = AVCOL_SPC_BT2020_NCL;
	stFrame->color_trc = AVCOL_TRC_SMPTE2084;
	stFrame->color_primaries = AVCOL_PRI_BT2020;
	stFrame->m_ePixType = eBGRA;
	ResetD3DResource(stFrame);
	 UpdateBuffers(m_vertexBuffer, stFrame->m_nWidth, stFrame->m_nHeight, m_nTextureWidth, m_nTextureHeight);
	
	SetSwapchainSetMetadata(stFrame);
	int fullrange = 1;
	int srcrange = 1;

	XMMATRIX worldMatrix, viewMatrix, projectionMatrix, orthoMatrix;

	// render to texture
	if (m_bDirect)
	{
		m_deviceContext->OMSetRenderTargets(1, &m_renderTextureTargetView, NULL);
		m_deviceContext->RSSetViewports(1, &viewportframe);
		BeginScene(0.0f, 0.0f, 0.0f, 1.0f, m_renderTextureTargetView, NULL);
		for (int i = 1; i <= 3; i++)
		{
			CChannelD3DTextureInfo info = SSMainConfiguration::instance().GetTextInfo(i);
			if (info.m_pHandle == 0 || info.m_nHeight <= 0 || info.m_nWidth <= 0)
			{
				continue;
			}
			 if (info.m_pHandle != m_hSrcHandle[i])
			{
				if (m_texture2dpip[i])
				{
					m_texture2dpip[i]->Release();
					m_texture2dpip[i] = NULL;
				}
				if (m_texturepip[i])
				{
					m_texturepip[i]->Release();
					m_texturepip[i] = NULL;
				}
				m_hSrcHandle[i] = info.m_pHandle;
				HRESULT hr = m_device->OpenSharedResource(m_hSrcHandle[i], __uuidof(ID3D11Texture2D), (void**)(&m_texture2dpip[i]));
				if (hr != S_OK)
				{
					continue;
				}
				D3D11_TEXTURE2D_DESC desc;
				m_texture2dpip[i]->GetDesc(&desc);
				D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
				ZeroMemory(&srv_desc, sizeof(srv_desc));
				srv_desc.Format = desc.Format;
				srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
				srv_desc.Texture2D.MipLevels = 1;
				srv_desc.Texture2D.MostDetailedMip = 0;
				m_device->CreateShaderResourceView(m_texture2dpip[i], &srv_desc, &m_texturepip[i]);
			}
			CD3D11_RECT dst = CD3D11_RECT((i - 1) * 200, 0, (i - 1) * 200 + 200, 0 + 100);
			dst = CD3D11_RECT(0, 0, stFrame->m_nWidth, stFrame->m_nHeight);
			bool result = UpdateBuffersEx(m_vertexBufferSource, info.m_nHeight, info.m_nHeight, dst);
			
			RenderBuffers(m_vertexBufferSource);
			XMFLOAT4X4 view;
			view.m[0][0] = 2.0f / stFrame->m_nWidth;
			view.m[0][1] = 0.0f;
			view.m[0][2] = 0.0f;
			view.m[0][3] = 0.0f;
			view.m[1][0] = 0.0f;
			view.m[1][1] = -2.0f / stFrame->m_nHeight;
			view.m[1][2] = 0.0f;
			view.m[1][3] = 0.0f;
			view.m[2][0] = 0.0f;
			view.m[2][1] = 0.0f;
			view.m[2][2] = 1.0f;
			view.m[2][3] = 0.0f;
			view.m[3][0] = -1.0f;
			view.m[3][1] = 1.0f;
			view.m[3][2] = 0.0f;
			view.m[3][3] = 1.0f;
			viewMatrix = XMLoadFloat4x4(&view);
			m_pViewMatVar->SetMatrix(reinterpret_cast<const float*>(&viewMatrix));
			if (m_dstRenderDXFormat == DXGI_FORMAT_R16G16B16A16_FLOAT)
			{
				SetColPrimaries(info.primaries, AVCOL_PRI_BT2020, info.transfer, AVCOL_TRC_SMPTE2084, stFrame, true);
				m_pdistransfer->SetInt(AVCOL_TRC_SMPTE2084);
				m_pdisprimaries->SetInt(AVCOL_PRI_BT2020);
			}
			else
			{
				SetColPrimaries(info.primaries, AVCOL_PRI_BT709, info.transfer, AVCOL_TRC_BT709, stFrame);
				m_pdistransfer->SetInt(AVCOL_TRC_BT709);
				m_pdisprimaries->SetInt(AVCOL_PRI_BT709);
			}
			m_pTextSourceY->SetResource((ID3D11ShaderResourceView*)m_texturepip[i]);
			
			m_pType->SetInt(1);
			m_pSourceHeight->SetInt(stFrame->m_nHeight);
			m_pSourceWidth->SetInt(stFrame->m_nWidth);
			m_ptransfer->SetInt(info.transfer);
			m_pprimaries->SetInt(info.primaries);
			m_pfullrange->SetInt(fullrange);
			m_psrcrange->SetInt(srcrange);
			m_pDrawLine->SetInt(2);
			m_deviceContext->IASetInputLayout(m_layout);
			D3DX11_TECHNIQUE_DESC techDesc;
			m_pTech->GetDesc(&techDesc);
			for (UINT i = 0; i < techDesc.Passes; ++i)
			{
				m_pTech->GetPassByIndex(i)->Apply(0, m_deviceContext);
				m_deviceContext->DrawIndexed(m_indexCount, 0, 0);
			}
		}
		

	}
	
	ID3D11ShaderResourceView *const pSRV[3] = { NULL };
	m_deviceContext->PSSetShaderResources(0, 3, pSRV);
	//XMMATRIX worldMatrix, viewMatrix, projectionMatrix, orthoMatrix;
	// renderto  backbuffer
	if (m_bRendUpdate)
	{
		m_pWidget->setAttribute(Qt::WA_OpaquePaintEvent);
		m_pWidget->setUpdatesEnabled(false);
		m_bRendUpdate = false;
	}
	m_deviceContext->OMSetRenderTargets(1, &m_renderTargetView, NULL);
	m_deviceContext->RSSetViewports(1, &viewport);
	BeginScene(0.0f, 0.0f, 0.0f, 1.0f, m_renderTargetView, NULL);
	RenderBuffers(m_vertexBuffer);
	XMFLOAT4X4 viewwindows;
	viewwindows.m[0][0] = 2.0f / m_nTextureWidth;
	viewwindows.m[0][1] = 0.0f;
	viewwindows.m[0][2] = 0.0f;
	viewwindows.m[0][3] = 0.0f;
	viewwindows.m[1][0] = 0.0f;
	viewwindows.m[1][1] = -2.0f / m_nTextureHeight;
	viewwindows.m[1][2] = 0.0f;
	viewwindows.m[1][3] = 0.0f;
	viewwindows.m[2][0] = 0.0f;
	viewwindows.m[2][1] = 0.0f;
	viewwindows.m[2][2] = 1.0f;
	viewwindows.m[2][3] = 0.0f;
	viewwindows.m[3][0] = -1.0f;
	viewwindows.m[3][1] = 1.0f;
	viewwindows.m[3][2] = 0.0f;
	viewwindows.m[3][3] = 1.0f;
	viewMatrix = XMLoadFloat4x4(&viewwindows);
	m_pViewMatVar->SetMatrix(reinterpret_cast<const float*>(&viewMatrix));

	{
		m_pTextSourceY->SetResource((ID3D11ShaderResourceView*)m_textureText);
		m_pTextSourceU->SetResource(0);
		m_pTextSourceV->SetResource(0);
		m_pTextSourceA->SetResource(0);
		m_pType->SetInt(1);
		if (m_dstRenderDXFormat == DXGI_FORMAT_R16G16B16A16_FLOAT)
		{
			m_ptransfer->SetInt(AVCOL_TRC_SMPTEST2084);
			m_pprimaries->SetInt(AVCOL_PRI_BT2020);
			SetColPrimaries(AVCOL_PRI_BT2020, m_displayInfo.dxgicolor.primaries, AVCOL_TRC_SMPTE2084, m_displayInfo.dxgicolor.transfer, stFrame);
		}
		else
		{
			m_ptransfer->SetInt(AVCOL_TRC_BT709);
			m_pprimaries->SetInt(AVCOL_PRI_BT709);
			SetColPrimaries(AVCOL_PRI_BT709, m_displayInfo.dxgicolor.primaries, AVCOL_TRC_BT709, m_displayInfo.dxgicolor.transfer, stFrame);
		}
		m_pdistransfer->SetInt(m_displayInfo.dxgicolor.transfer);
		m_pdisprimaries->SetInt(m_displayInfo.dxgicolor.primaries);
		m_pfullrange->SetInt(1);
		m_psrcrange->SetInt(1);
		m_pDrawLine->SetInt(2);
		m_deviceContext->IASetInputLayout(m_layout);
		D3DX11_TECHNIQUE_DESC techDescwindow;
		m_pTech->GetDesc(&techDescwindow);
		for (UINT i = 0; i < techDescwindow.Passes; ++i)
		{
			m_pTech->GetPassByIndex(i)->Apply(0, m_deviceContext);
			m_deviceContext->DrawIndexed(m_indexCount, 0, 0);
		}
	}


	EndScene();
	m_deviceContext->PSSetShaderResources(0, 1, pSRV);
	return bRet;
}

DWORD WINAPI VideoRenderFilter::SyncRead(LPVOID arg)
{

	VideoRenderFilter *pThis = (VideoRenderFilter *)arg;
	if (NULL == pThis)
	{
		return -1;
	}
	
	while (pThis->m_bExit == false)
	{

		DWORD dwRet = WaitForSingleObject(pThis->m_hEventHandle, 40);
		if (WAIT_OBJECT_0 == dwRet)
		{
			ResetEvent(pThis->m_hEventHandle);
		}
		else if (WAIT_TIMEOUT == dwRet)
		{
			
		}
		else if (WAIT_FAILED == dwRet)
		{
			
		}
		else
		{
		}
		long long nStartTime = QkTimer::nowns();
		bool bRet = true;
		if (pThis->m_bDirect)
		{
			bRet = pThis->ReadDataPGM();
		}
		else
		{
			bRet = pThis->ReadData();
		}
       
		if (false == bRet)
		{
		}
		else
		{
			long long nEndTime = QkTimer::nowns();
			double dbCost = nEndTime - nStartTime;
			
		}
	}
	return 0;
}

HRESULT VideoRenderFilter::create_swapchain_1_2(ID3D11Device *dev, IDXGIFactory2 *factory, bool flip, DXGI_FORMAT format, IDXGISwapChain **swapchain_out)
{
	IDXGISwapChain *swapchain = NULL;
	IDXGISwapChain1 *swapchain1 = NULL;
	HRESULT hr;
	do
	{
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
		ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
		// Set to a single back buffer.
		swapChainDesc.BufferCount = 1;
		// Set the width and height of the back buffer.
		swapChainDesc.Width = m_nTextureWidth;
		swapChainDesc.Height = m_nTextureHeight;
		// Set regular 32-bit surface for the back buffer.
		swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		// Set the usage of the back buffer.
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		// Set the handle for the window to render to.
		// Turn multisampling off.
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.SampleDesc.Quality = 0;
		// Discard the back buffer contents after presenting.
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		// Don't set the advanced flags.
		swapChainDesc.Flags = 0;
		if (flip)
		{
			swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
			swapChainDesc.BufferCount = 3;
		}
		else
		{
			swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
			swapChainDesc.BufferCount = 1;
		}

		hr = factory->CreateSwapChainForHwnd((IUnknown*)dev,
			(HWND)m_pWidget->winId(), &swapChainDesc, NULL, NULL, &swapchain1);
		if (FAILED(hr))
			break;
		hr = swapchain1->QueryInterface(IID_IDXGISwapChain,
			(void**)&swapchain);
		if (FAILED(hr))
			break;
		*swapchain_out = swapchain;
		swapchain = NULL;
	} while (0);

	if (swapchain1)
	{
		swapchain1->Release();
		swapchain1 = NULL;
	}
	if (swapchain)
	{
		swapchain->Release();
		swapchain = NULL;
	}
	return hr;
}

HRESULT VideoRenderFilter::create_swapchain_1_1(ID3D11Device *dev, IDXGIFactory1 *factory, bool flip, DXGI_FORMAT format, IDXGISwapChain **swapchain_out)
{
	DXGI_SWAP_CHAIN_DESC desc;
	memset(&desc, 0, sizeof(desc));
	desc.BufferDesc.Width = m_nTextureWidth;
	desc.BufferDesc.Height = m_nTextureHeight;
	desc.BufferDesc.Format = format;
	desc.SampleDesc.Count = 1;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.BufferCount = 1;
	desc.OutputWindow = (HWND)m_pWidget->winId();
	desc.Windowed = TRUE;
	desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	return factory->CreateSwapChain((IUnknown*)dev, &desc, swapchain_out);
}

bool VideoRenderFilter::d3d11_create_swapchain(ID3D11Device *dev, IDXGISwapChain **swapchain_out)
{
	IDXGIDevice1 *dxgi_dev = NULL;
	IDXGIAdapter1 *adapter = NULL;
	IDXGIFactory1 *factory = NULL;
	IDXGIFactory2 *factory2 = NULL;
	IDXGISwapChain *swapchain = NULL;
	bool success = false;
	HRESULT hr;
	do
	{
		hr = dev->QueryInterface(IID_IDXGIDevice1, (void**)&dxgi_dev);
		if (FAILED(hr))
		{
			break;
		}
		hr = dxgi_dev->GetParent(IID_IDXGIAdapter1, (void**)&adapter);
		if (FAILED(hr))
		{
			break;
		}
		hr = adapter->GetParent(IID_IDXGIFactory1, (void**)&factory);
		if (FAILED(hr))
		{
			break;
		}
		hr = factory->QueryInterface(IID_IDXGIFactory2, (void**)&factory2);
		if (FAILED(hr))
		{
			factory2 = NULL;
		}
		DXGI_FORMAT format = DXGI_FORMAT_R10G10B10A2_UNORM;//DXGI_FORMAT_R8G8B8A8_UNORM
		bool flip = factory2;
		bool bBreak = false;
		// Return here to retry creating the swapchain
		do {
			if (factory2)
			{
				// Create a DXGI 1.2+ (Windows 8+) swap chain if possible
				hr = create_swapchain_1_2(dev, factory2, flip, format, &swapchain);
			}
			else
			{
				// Fall back to DXGI 1.1 (Windows 7)
				hr = create_swapchain_1_1(dev, factory, flip, format, &swapchain);
			}
			if (SUCCEEDED(hr))
				break;

			if (flip)
			{
				flip = false;
				continue;
			}
			bBreak = true;
			break;
		} while (true);
		if (bBreak)
		{
			break;
		}

		if (factory2)
		{
			
		}
		else
		{
			
		}

		DXGI_SWAP_CHAIN_DESC scd = { 0 };
		swapchain->GetDesc(&scd);
		if (scd.SwapEffect == DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL)
		{
			
		}
		else
		{
			
		}

		*swapchain_out = swapchain;
		swapchain = NULL;
		success = true;
	} while (0);
	if (swapchain)
	{
		swapchain->Release();
		swapchain = NULL;
	}
	if (factory2)
	{
		factory2->Release();
		factory2 = NULL;
	}
	if (factory)
	{
		factory->Release();
		factory = NULL;
	}
	if (adapter)
	{
		adapter->Release();
		adapter = NULL;
	}
	if (dxgi_dev)
	{
		dxgi_dev->Release();
		dxgi_dev = NULL;
	}
	return success;
}

int VideoRenderFilter::GetFeatureLevels(int max_fl, int min_fl, const D3D_FEATURE_LEVEL **out)
{
	static const D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_3,
		D3D_FEATURE_LEVEL_9_2,
		D3D_FEATURE_LEVEL_9_1,
	};
	static const int levels_len = sizeof(levels) / sizeof(levels[0]);

	int start = 0;
	for (; start < levels_len; start++)
	{
		if (levels[start] <= max_fl)
			break;
	}
	int len = 0;
	for (; start + len < levels_len; len++)
	{
		if (levels[start + len] < min_fl)
			break;
	}
	*out = &levels[start];
	return len;
}

void VideoRenderFilter::TurnOffAlphaBlending()
{

	float blendFactor[4];
	// Setup the blend factor.
	blendFactor[0] = 0.0f;
	blendFactor[1] = 0.0f;
	blendFactor[2] = 0.0f;
	blendFactor[3] = 0.0f;

	// Turn off the alpha blending.
	m_deviceContext->OMSetBlendState(m_alphaDisableBlendingState, blendFactor, 0xffffffff);

	return;
}

void VideoRenderFilter::TurnOnAlphaBlending()
{

	float blendFactor[4];


	// Setup the blend factor.
	blendFactor[0] = 0.0f;
	blendFactor[1] = 0.0f;
	blendFactor[2] = 0.0f;
	blendFactor[3] = 0.0f;

	// Turn on the alpha blending.
	m_deviceContext->OMSetBlendState(m_alphaEnableBlendingState, blendFactor, 0xffffffff);

	return;
}

IDXGIAdapter *VideoRenderFilter::D3D11DeviceAdapter(ID3D11Device *d3ddev)
{
	IDXGIDevice *pDXGIDevice = NULL;
	HRESULT hr = d3ddev->QueryInterface(IID_IDXGIDevice, (void **)&pDXGIDevice);
	if (FAILED(hr))
	{
		return NULL;
	}

	IDXGIAdapter *p_adapter;
	hr = pDXGIDevice->GetAdapter(&p_adapter);
	pDXGIDevice->Release();
	pDXGIDevice = NULL;
	if (FAILED(hr))
	{
		return NULL;
	}
	return p_adapter;
}

#define FROM_AVRAT(default_factor, avrat) \
(uint64_t)(default_factor) * (avrat).num / (avrat).den

#define LAV_RED    0
#define LAV_GREEN  1
#define LAV_BLUE   2
#define ST2086_PRIM_FACTOR 50000
#define ST2086_LUMA_FACTOR 10000
bool isEqueHDR(DXGI_HDR_METADATA_HDR10 src, DXGI_HDR_METADATA_HDR10 dst)
{
	bool bRet = false;
	do
	{
		if (src.MaxContentLightLevel != dst.MaxContentLightLevel)
		{
			break;
		}
		if (src.MaxFrameAverageLightLevel != dst.MaxFrameAverageLightLevel)
		{
			break;
		}
		if (src.GreenPrimary[0] != dst.GreenPrimary[0])
		{
			break;
		}
		if (src.GreenPrimary[1] != dst.GreenPrimary[1])
		{
			break;
		}

		if (src.BluePrimary[0] != dst.BluePrimary[0])
		{
			break;
		}
		if (src.BluePrimary[1] != dst.BluePrimary[1])
		{
			break;
		}
		if (src.RedPrimary[0] != dst.RedPrimary[0])
		{
			break;
		}
		if (src.RedPrimary[1] != dst.RedPrimary[1])
		{
			break;
		}
		if (src.WhitePoint[0] != dst.WhitePoint[0])
		{
			break;
		}
		if (src.WhitePoint[1] != dst.WhitePoint[1])
		{
			break;
		}

		if (src.MinMasteringLuminance != dst.MinMasteringLuminance)
		{
			break;
		}
		if (src.MaxMasteringLuminance != dst.MaxMasteringLuminance)
		{
			break;
		}
		bRet = true;

	} while (0);
	return bRet;
}
void VideoRenderFilter::SetSwapchainSetMetadata(CFrameSharePtr &stFrame)
{

	if (stFrame->hasDisplayMetadata && m_displayInfo.sendmetadata && m_swapChain4)
	{
		DXGI_HDR_METADATA_HDR10  hdr10 = { 0 };
		if (stFrame->color_trc == AVCOL_TRC_SMPTE2084 && stFrame->color_primaries == AVCOL_PRI_BT2020)
		{
			if (stFrame->hasLightMetadata)
			{
				hdr10.MaxContentLightLevel = stFrame->lightMetadata.MaxCLL;
				hdr10.MaxFrameAverageLightLevel = stFrame->lightMetadata.MaxFALL;
			}
			if (stFrame->hasDisplayMetadata)
			{
				hdr10.GreenPrimary[0] = FROM_AVRAT(ST2086_PRIM_FACTOR, stFrame->displayMetadata.display_primaries[LAV_GREEN][0]);
				hdr10.GreenPrimary[1] = FROM_AVRAT(ST2086_PRIM_FACTOR, stFrame->displayMetadata.display_primaries[LAV_GREEN][1]);
				hdr10.BluePrimary[0] = FROM_AVRAT(ST2086_PRIM_FACTOR, stFrame->displayMetadata.display_primaries[LAV_BLUE][0]);
				hdr10.BluePrimary[1] = FROM_AVRAT(ST2086_PRIM_FACTOR, stFrame->displayMetadata.display_primaries[LAV_BLUE][1]);
				hdr10.RedPrimary[0] = FROM_AVRAT(ST2086_PRIM_FACTOR, stFrame->displayMetadata.display_primaries[LAV_RED][0]);
				hdr10.RedPrimary[1] = FROM_AVRAT(ST2086_PRIM_FACTOR, stFrame->displayMetadata.display_primaries[LAV_RED][1]);
				hdr10.WhitePoint[0] = FROM_AVRAT(ST2086_PRIM_FACTOR, stFrame->displayMetadata.white_point[0]);
				hdr10.WhitePoint[1] = FROM_AVRAT(ST2086_PRIM_FACTOR, stFrame->displayMetadata.white_point[1]);
				hdr10.MinMasteringLuminance = FROM_AVRAT(ST2086_LUMA_FACTOR, stFrame->displayMetadata.min_luminance);;
				hdr10.MaxMasteringLuminance = FROM_AVRAT(ST2086_LUMA_FACTOR, stFrame->displayMetadata.max_luminance);;
			}
			if (true || false == isEqueHDR(hdr10, m_hdr10))
			{
				m_swapChain4->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_HDR10, sizeof(&hdr10), &hdr10);
				m_hdr10 = hdr10;
			}
		}
		else if (stFrame->color_trc == AVCOL_TRC_ARIB_STD_B67 && stFrame->color_primaries == AVCOL_PRI_BT2020)// HLG
		{
			hdr10.RedPrimary[0] = 34000; // Display P3 primaries
			hdr10.RedPrimary[1] = 16000;
			hdr10.GreenPrimary[0] = 13250;
			hdr10.GreenPrimary[1] = 34500;
			hdr10.BluePrimary[0] = 7500;
			hdr10.BluePrimary[1] = 3000;
			hdr10.WhitePoint[0] = 15635;
			hdr10.WhitePoint[1] = 16450;
			hdr10.MaxMasteringLuminance = 1000 * 10000; // 1000 nits
			hdr10.MinMasteringLuminance = 100; // 0.01 nits
			if (true || false == isEqueHDR(hdr10, m_hdr10))
			{
				m_swapChain4->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_HDR10, sizeof(&hdr10), &hdr10);
				m_hdr10 = hdr10;
			}
		}
		
		

		
	}
}

bool canHandleConversion(const dxgi_color_space *src, const dxgi_color_space *dst)
{
	if (src == dst)
		return true;
	if (src->primaries == AVCOL_PRI_BT2020)
		return true; /* we can convert BT2020 to 2020 or 709 */
	if (dst->transfer == AVCOL_PRI_BT709)
		return true; /* we can handle anything to 709 */
	return false; /* let Windows do the rest */
}


void VideoRenderFilter::SelectSwapchainColorspace()
{
	HRESULT hr;
	int best = 0;
	int score, best_score = 0;
	UINT support;
	IDXGISwapChain4 *dxgiswapChain4 = NULL;
	hr = m_swapChain->QueryInterface(IID_IDXGISwapChain4, (void **)&dxgiswapChain4);
	if (FAILED(hr)) 
	{
		goto done;
	}
	best = -1;
	for (int i = 0; color_spaces[i].name; ++i)
	{
		hr = dxgiswapChain4->CheckColorSpaceSupport( color_spaces[i].dxgi, &support);
		if (SUCCEEDED(hr) && support) 
		{
			score = 0;
			if (color_spaces[i].primaries == m_displayInfo.primaries)
				score++;
			if (color_spaces[i].color == m_displayInfo.colorspace)
				score += 2; /* we don't want to translate color spaces */
			if (color_spaces[i].transfer == m_displayInfo.transfer)
				score++;
			if (color_spaces[i].b_full_range == m_displayInfo.full_range)
				score++;
			if (score > best_score || (score && best == -1))
			{
				best = i;
				best_score = score;
			}
		}
	}
	IDXGIOutput *dxgiOutput = NULL;
	if (SUCCEEDED(m_swapChain->GetContainingOutput( &dxgiOutput)))
	{
		IDXGIOutput6 *dxgiOutput6 = NULL;
		if (SUCCEEDED(dxgiOutput->QueryInterface((IID_IDXGIOutput6), (void **)&dxgiOutput6)))
		{
			DXGI_OUTPUT_DESC1 desc1;
			if (SUCCEEDED(dxgiOutput6->GetDesc1( &desc1)))
			{
				const dxgi_color_space *csp = NULL;
				for (int i = 0; color_spaces[i].name; ++i)
				{
					if (color_spaces[i].dxgi == desc1.ColorSpace)
					{
						if (!canHandleConversion(&color_spaces[best], &color_spaces[i]))
						{

						}
						else
						{
							best = i;
							csp = &color_spaces[i];
						}
						break;
					}
				}
			}
			dxgiOutput6->Release();
		}
		dxgiOutput->Release();
	}
	if (best == -1)
	{
		best = 0;
	}

	hr = dxgiswapChain4->SetColorSpace1( color_spaces[best].dxgi);
done:
	m_displayInfo.dxgicolor = color_spaces[best];
	m_displayInfo.sendmetadata = color_spaces[best].primaries == AVCOL_PRI_BT2020;
	if (m_displayInfo.dxgicolor.primaries == AVCOL_PRI_BT2020)
	{
		m_displayInfo.luminance_peak = MAX_PQ_BRIGHTNESS;
	}
	else
	{
		m_displayInfo.luminance_peak = DEFAULT_SRGB_BRIGHTNESS;
	}
	dxgiswapChain4->Release();
}

bool VideoRenderFilter::Initialize(int screenWidth, int screenHeight, HWND hwnd)
{
	m_displayInfo.bitdepth = 10;
	m_displayInfo.colorspace = AVCOL_SPC_BT2020_NCL;
	m_displayInfo.full_range = 1;
	m_displayInfo.primaries = AVCOL_PRI_BT2020;
	m_displayInfo.transfer = AVCOL_TRC_SMPTEST2084;
	std::lock_guard<std::mutex> stLock(m_stD3DLock);
	UINT createDeviceFlags = 0;
	m_dstRenderDXFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	m_dstBufferDXFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
#ifdef _DEBUG
	//createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
	//createDeviceFlags |= D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
	D3D_FEATURE_LEVEL min_level = D3D_FEATURE_LEVEL_9_1;
	D3D_FEATURE_LEVEL max_level = D3D_FEATURE_LEVEL_11_1;
	const D3D_FEATURE_LEVEL *levels;
	int levels_len = GetFeatureLevels(max_level, min_level, &levels);
	if (!levels_len)
	{
		return false;
	}

	D3D_FEATURE_LEVEL out_feature_level = D3D_FEATURE_LEVEL_9_1;
	D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_1;
	HRESULT result = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags,
		levels, levels_len, D3D11_SDK_VERSION,
		&m_device, &out_feature_level, NULL);
	if (SUCCEEDED(result))
	{
		
	}
	else
	{
		int kkk = 0;
	}
	
	DXGI_FORMAT format = m_dstBufferDXFormat;//DXGI_FORMAT_R8G8B8A8_UNORM
	DXGI_SWAP_CHAIN_DESC1 out;
	memset(&out, 0, sizeof(out));
	out.BufferCount = 3;
	out.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	out.SampleDesc.Count = 1;
	out.SampleDesc.Quality = 0;
	out.Width = screenWidth;
	out.Height = screenHeight;
	out.Format = format;
	out.Scaling = DXGI_SCALING_NONE;
	//out->Flags = 512; // DXGI_SWAP_CHAIN_FLAG_YUV_VIDEO;
	out.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	out.AlphaMode = DXGI_ALPHA_MODE_IGNORE;


	IDXGIAdapter *dxgiadapter = D3D11DeviceAdapter(m_device);
	if (dxgiadapter == NULL)
	{
		return false;
	}
	UINT i = 0;
	IDXGIOutput * pOutput;
	std::vector<IDXGIOutput*> vOutputs;
	while (dxgiadapter->EnumOutputs(i, &pOutput) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_OUTPUT_DESC outdesc;
		pOutput->GetDesc(&outdesc);

		vOutputs.push_back(pOutput);


		IDXGIOutput6 *dxgiOutput6 = NULL;
		if (SUCCEEDED(pOutput->QueryInterface((IID_IDXGIOutput6), (void **)&dxgiOutput6)))
		{
			DXGI_OUTPUT_DESC1 desc1;
			if (SUCCEEDED(dxgiOutput6->GetDesc1(&desc1)))
			{
				
			}
			dxgiOutput6->Release();
		}
		++i;
	}
	IDXGIDevice1 *pDXGIDevice = NULL;
	result = m_device->QueryInterface(IID_IDXGIDevice1, (void **)&pDXGIDevice);
	if (FAILED(result))
	{
		return false;
	}

	UINT aaa = 0;
	pDXGIDevice->GetMaximumFrameLatency(&aaa);
	pDXGIDevice->SetMaximumFrameLatency(2);
	pDXGIDevice->GetMaximumFrameLatency(&aaa);
	
	pDXGIDevice->Release();
	DXGI_ADAPTER_DESC desc1;
	result = dxgiadapter->GetDesc(&desc1);
	if (FAILED(result))
	{
	
		return false;
	}
	IDXGIFactory2 *dxgifactory;
	HRESULT hr = dxgiadapter->GetParent(IID_IDXGIFactory2, (void **)&dxgifactory);
	dxgiadapter->Release();
	if (FAILED(hr))
	{
		return false;
	}
	hr = dxgifactory->CreateSwapChainForHwnd((IUnknown*)m_device,
		(HWND)m_pWidget->winId(), &out, NULL, NULL, &m_swapChain);
	D3D_FEATURE_LEVEL selected_level = m_device->GetFeatureLevel();
	m_device->GetImmediateContext(&m_deviceContext);
	m_swapChain->QueryInterface(IID_IDXGISwapChain4, (void **)&m_swapChain4);
	ID3D10Multithread* pMultiThread = NULL;

	// Need to explitly set the multithreaded mode for this device
	hr = m_deviceContext->QueryInterface(__uuidof(ID3D10Multithread), (void**)&pMultiThread);
	if (FAILED(hr))
	{
		return false;
	}

	pMultiThread->SetMultithreadProtected(TRUE);

	pMultiThread->Release();
	pMultiThread = 0;
	SelectSwapchainColorspace();
	if (m_displayInfo.dxgicolor.primaries == AVCOL_PRI_BT2020)
	{
		m_dstBufferDXFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	}
	else
	{
		m_dstBufferDXFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	}
	ID3D11Texture2D* backBufferPtr = NULL;
	// Get the pointer to the back buffer.
	result = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backBufferPtr);
	if (FAILED(result))
	{
		return false;
	}
	result = m_device->CreateRenderTargetView(backBufferPtr, NULL, &m_renderTargetView);
	if (FAILED(result))
	{
		return false;
	}
	backBufferPtr->Release();
	backBufferPtr = 0;
	viewport.Width = (float)screenWidth;
	viewport.Height = (float)screenHeight;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	{
		do
		{
			std::string strPath = getExecutableDir();
			std::string strName = strPath + "\\FX\\colorconvert.fxo";
			FILE *fp = fopen(strName.c_str(), "rb");
			if (NULL == fp)
			{
				break;
			}
			fseek(fp, 0, SEEK_END);
			long nSize = ftell(fp);
			fseek(fp, 0, SEEK_SET);
			unsigned char * pData = new unsigned char[nSize];
			fread(pData, nSize, 1, fp);
			fclose(fp);
			HRESULT	hr = D3DX11CreateEffectFromMemory(pData, nSize, 0, m_device, &m_pEffect);
			if (pData)
			{
				delete[]pData;
			}
			if (hr != S_OK)
			{
				break;
			}
		} while (0);
		m_pTech = m_pEffect->GetTechniqueByName("BasicTech");
		m_pWorldMatVar = m_pEffect->GetVariableByName("worldMatrix")->AsMatrix();
		m_pViewMatVar = m_pEffect->GetVariableByName("viewMatrix")->AsMatrix();
		m_pProjMatVar = m_pEffect->GetVariableByName("projectionMatrix")->AsMatrix();
		m_pTextSourceY = m_pEffect->GetVariableByName("TextureSourceY")->AsShaderResource();
		m_pTextSourceU = m_pEffect->GetVariableByName("TextureSourceU")->AsShaderResource();
		m_pTextSourceV = m_pEffect->GetVariableByName("TextureSourceV")->AsShaderResource();
		m_pTextSourceA = m_pEffect->GetVariableByName("TextureSourceA")->AsShaderResource();
		m_pType = m_pEffect->GetVariableByName("PixType")->AsScalar();
		m_pSourceWidth = m_pEffect->GetVariableByName("sourcewidth")->AsScalar();
		m_pSourceHeight = m_pEffect->GetVariableByName("sourceheight")->AsScalar();
		m_pDrawLine = m_pEffect->GetVariableByName("DrawLine")->AsScalar();
		
		m_ptransfer = m_pEffect->GetVariableByName("transfer")->AsScalar();
		m_pdistransfer = m_pEffect->GetVariableByName("distransfer")->AsScalar();
		m_pprimaries = m_pEffect->GetVariableByName("primaries")->AsScalar();
		m_pdisprimaries = m_pEffect->GetVariableByName("disprimaries")->AsScalar();
		m_pfullrange = m_pEffect->GetVariableByName("fullrange")->AsScalar();
		m_psrcrange = m_pEffect->GetVariableByName("srcrange")->AsScalar();
		m_pLuminanceScale = m_pEffect->GetVariableByName("LuminanceScale")->AsScalar();
		m_pTransPrimariesInfo = m_pEffect->GetConstantBufferByName("PS_COLOR_TRANSFORM")->AsConstantBuffer();
		D3D11_INPUT_ELEMENT_DESC ied[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};
		UINT numElements = ARRAYSIZE(ied);
		D3DX11_PASS_DESC passDesc;
		m_pTech->GetPassByIndex(0)->GetDesc(&passDesc);
		int nRet = m_device->CreateInputLayout(ied, numElements, passDesc.pIAInputSignature, passDesc.IAInputSignatureSize, &m_layout);
	}

	InitializeBuffers(m_device);
	D3D11_BLEND_DESC desc = {};

	for (size_t i = 0; i < 8; i++)
		desc.RenderTarget[i].RenderTargetWriteMask =
		D3D11_COLOR_WRITE_ENABLE_ALL;

	ID3D11BlendState*alphaEnableBlendingState;
	hr = m_device->CreateBlendState(&desc, &alphaEnableBlendingState);
	if (FAILED(hr)) {
		return false;
	}

	D3D11_BLEND_DESC bd;
	memset(&bd, 0, sizeof(bd));
	for (int i = 0; i < 8; i++)
	{
		bd.RenderTarget[i].BlendEnable = TRUE;
		bd.RenderTarget[i].BlendOp = D3D11_BLEND_OP_ADD;
		bd.RenderTarget[i].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		bd.RenderTarget[i].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		bd.RenderTarget[i].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		bd.RenderTarget[i].SrcBlendAlpha = D3D11_BLEND_ONE;
		bd.RenderTarget[i].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		bd.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	}


	// 用描述符创建一个alpha blend状态
	result = m_device->CreateBlendState(&bd, &m_alphaEnableBlendingState);
	if (FAILED(result))
	{
		return false;
	}

	//修改描述符.
	bd.RenderTarget[0].BlendEnable = FALSE;
	//创建一个新的blend状态.
	result = m_device->CreateBlendState(&bd, &m_alphaDisableBlendingState);
	if (FAILED(result))
	{
		return false;
	}

	return true;
}

bool VideoRenderFilter::InitializeBuffers(ID3D11Device* device)
{
	{
		D3D11_BUFFER_DESC constantDesc;
		constantDesc.Usage = D3D11_USAGE_DYNAMIC;
		constantDesc.ByteWidth = sizeof(PS_COLOR_TRANSFORM);
		constantDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		constantDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		constantDesc.MiscFlags = 0;
		constantDesc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA constantInit;
		constantInit.pSysMem = &m_pixtransform;
		constantInit.SysMemPitch = 0;
		constantInit.SysMemSlicePitch = 0;
		HRESULT result = device->CreateBuffer(&constantDesc,&constantInit, &m_pixtranformBuffer);
		if (FAILED(result))
		{
			return false;
		}
	}
	m_vertexCount = 4;
	m_indexCount = 6;
	VertexType* vertices = new VertexType[m_vertexCount];
	VertexType* verticesex = new VertexType[m_vertexCount];
	if (!vertices || !verticesex)
	{
		return false;
	}
	unsigned long* indices = new unsigned long[m_indexCount];
	if (!indices)
	{
		return false;
	}
	memset(vertices, 0, (sizeof(VertexType) * m_vertexCount));
	memset(verticesex, 0, (sizeof(VertexType) * m_vertexCount));
	for (UINT i = 0; i < 1; ++i)
	{
		indices[i * 6 + 0] = i * 4 + 0;
		indices[i * 6 + 1] = i * 4 + 1;
		indices[i * 6 + 2] = i * 4 + 2;

		indices[i * 6 + 3] = i * 4 + 0;
		indices[i * 6 + 4] = i * 4 + 2;
		indices[i * 6 + 5] = i * 4 + 3;
	}
	D3D11_SUBRESOURCE_DATA vertexData, indexData, vertexDataex;
	D3D11_BUFFER_DESC vertexBufferDesc;
	vertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	vertexBufferDesc.ByteWidth = sizeof(VertexType) * m_vertexCount;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	vertexBufferDesc.MiscFlags = 0;
	vertexBufferDesc.StructureByteStride = 0;
	vertexData.pSysMem = vertices;
	vertexData.SysMemPitch = 0;
	vertexData.SysMemSlicePitch = 0;
	HRESULT result = device->CreateBuffer(&vertexBufferDesc, &vertexData, &m_vertexBuffer);
	if (FAILED(result))
	{
		return false;
	}
	{
		vertexDataex.pSysMem = verticesex;
		vertexDataex.SysMemPitch = 0;
		vertexDataex.SysMemSlicePitch = 0;
	}

	result = device->CreateBuffer(&vertexBufferDesc, &vertexData, &m_vertexBufferSource);
	if (FAILED(result))
	{
		return false;
	}
	D3D11_BUFFER_DESC indexBufferDesc;
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.ByteWidth = sizeof(unsigned long) * m_indexCount;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexBufferDesc.CPUAccessFlags = 0;
	indexBufferDesc.MiscFlags = 0;
	indexBufferDesc.StructureByteStride = 0;
	indexData.pSysMem = indices;
	indexData.SysMemPitch = 0;
	indexData.SysMemSlicePitch = 0;
	result = device->CreateBuffer(&indexBufferDesc, &indexData, &m_indexBuffer);
	if (FAILED(result))
	{
		return false;
	}
	delete[] vertices;
	delete[]verticesex;
	verticesex = 0;
	vertices = 0;
	delete[] indices;
	indices = 0;
	return true;
}

void VideoRenderFilter::Shutdown()
{
	
	if (m_swapChain)
	{
		m_swapChain->SetFullscreenState(false, NULL);
	}
	if (m_device)
	{
		m_device->Release();
		m_device = NULL;
	}
	if (m_swapChain)
	{
		m_swapChain->Release();
		m_swapChain = NULL;
	}
	if (m_deviceContext)
	{
		m_deviceContext->Release();
		m_deviceContext = NULL;
	}
	if (m_renderTargetView)
	{
		m_renderTargetView->Release();
		m_renderTargetView = NULL;
	}
	if (m_vertexBuffer)
	{
		m_vertexBuffer->Release();
		m_vertexBuffer = NULL;
	}
	if (m_vertexBufferSource)
	{
		m_vertexBufferSource->Release();
		m_vertexBufferSource = NULL;
	}
	if (m_indexBuffer)
	{
		m_indexBuffer->Release();
		m_indexBuffer = NULL;
	}
	if (m_renderTextureTargetView)
	{
		m_renderTextureTargetView->Release();
		m_renderTextureTargetView = NULL;
	}
	if (m_texture2dText)
	{
		m_texture2dText->Release();
		m_texture2dText = NULL;
	}
	if (m_textureText)
	{
		m_textureText->Release();
		m_textureText = NULL;
	}
	
	if (m_pEffect)
	{
		m_pEffect->Release();
		m_pEffect = NULL;
	}
	if (m_layout)
	{
		m_layout->Release();
		m_layout = NULL;
	}
	for (int i = 0; i < MAXPLANE; i++)
	{
		if (m_pSourceTexture2d[i])
		{
			m_pSourceTexture2d[i]->Release();
			m_pSourceTexture2d[i] = NULL;
		}
		if (m_pSourceTexture[i])
		{
			m_pSourceTexture[i]->Release();
			m_pSourceTexture[i] = NULL;
		}
	}
	

	if (m_alphaEnableBlendingState)
	{
		m_alphaEnableBlendingState->Release();
		m_alphaEnableBlendingState = NULL;
	}
	if (m_alphaDisableBlendingState)
	{
		m_alphaDisableBlendingState->Release();
		m_alphaDisableBlendingState = NULL;
	}
	
	return;
}

void VideoRenderFilter::BeginScene(float red, float green, float blue, float alpha, ID3D11RenderTargetView* renderTargetView, ID3D11DepthStencilView* depthStencilView)
{
	float color[4];
	color[0] = red;
	color[1] = green;
	color[2] = blue;
	color[3] = alpha;
	if (renderTargetView)
	{
		m_deviceContext->ClearRenderTargetView(renderTargetView, color);
	}

	if (depthStencilView)
	{
		m_deviceContext->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
	}

}

void VideoRenderFilter::EndScene()
{
	m_swapChain->Present(0, 0);
}

void VideoRenderFilter::RenderBuffers(ID3D11Buffer *verbuffer)
{
	unsigned int stride = sizeof(VertexType);
	unsigned int offset = 0;
	m_deviceContext->IASetVertexBuffers(0, 1, &verbuffer, &stride, &offset);
	m_deviceContext->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R32_UINT, 0);
	m_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

bool VideoRenderFilter::UpdateBuffers(ID3D11Buffer* verbuffer, int nLastWidth, int nLastHeight, int nBackWidth, int nBackHeight)
{
	double dbRatio = nBackWidth / double(nLastWidth);
	double dbRatio2 = nBackHeight / double(nLastHeight);
	if (dbRatio2 < dbRatio)
	{
		dbRatio = dbRatio2;
	}
	int nRealWidth = nLastWidth * dbRatio;
	int nRealHeight = nLastHeight * dbRatio;
	int nXStart = (nBackWidth - nRealWidth) / 2;
	int nYStart = (nBackHeight - nRealHeight) / 2;
	VertexType* vertices = new VertexType[m_vertexCount];
	if (!vertices)
	{
		return false;
	}
	const CD3D11_RECT& dest = CD3D11_RECT(nXStart, nYStart, nXStart + nRealWidth, nYStart + nRealHeight);
	float minu = 0;
	float maxu = 1;
	float minv = 0;
	float maxv = 1;
	vertices[1].position.x = dest.left;
	vertices[1].position.y = dest.top;
	vertices[1].position.z = 0.0f;
	vertices[1].texture.x = minu;
	vertices[1].texture.y = minv;
	vertices[0].position.x = dest.left;
	vertices[0].position.y = dest.bottom;
	vertices[0].position.z = 0.0f;
	vertices[0].texture.x = minu;
	vertices[0].texture.y = maxv;
	vertices[2].position.x = dest.right;
	vertices[2].position.y = dest.top;
	vertices[2].position.z = 0.0f;
	vertices[2].texture.x = maxu;
	vertices[2].texture.y = minv;
	vertices[3].position.x = dest.right;
	vertices[3].position.y = dest.bottom;
	vertices[3].position.z = 0.0f;
	vertices[3].texture.x = maxu;
	vertices[3].texture.y = maxv;

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT result = m_deviceContext->Map(verbuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(result))
	{
		delete[] vertices;
		vertices = 0;
		return false;
	}
	VertexType* verticesPtr = (VertexType*)mappedResource.pData;
	memcpy(verticesPtr, (void*)vertices, (sizeof(VertexType) * m_vertexCount));
	m_deviceContext->Unmap(verbuffer, 0);
	delete[] vertices;
	vertices = 0;
	return true;
}

bool VideoRenderFilter::UpdateBuffersEx(ID3D11Buffer* verbuffer, int nLastWidth, int nLastHeight, CD3D11_RECT dest)
{
	VertexType* vertices = new VertexType[m_vertexCount];
	if (!vertices)
	{
		return false;
	}
	
	float minu = 0;
	float maxu = 1;
	float minv = 0;
	float maxv = 1;
	vertices[1].position.x = dest.left;
	vertices[1].position.y = dest.top;
	vertices[1].position.z = 0.0f;
	vertices[1].texture.x = minu;
	vertices[1].texture.y = minv;
	vertices[0].position.x = dest.left;
	vertices[0].position.y = dest.bottom;
	vertices[0].position.z = 0.0f;
	vertices[0].texture.x = minu;
	vertices[0].texture.y = maxv;
	vertices[2].position.x = dest.right;
	vertices[2].position.y = dest.top;
	vertices[2].position.z = 0.0f;
	vertices[2].texture.x = maxu;
	vertices[2].texture.y = minv;
	vertices[3].position.x = dest.right;
	vertices[3].position.y = dest.bottom;
	vertices[3].position.z = 0.0f;
	vertices[3].texture.x = maxu;
	vertices[3].texture.y = maxv;

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT result = m_deviceContext->Map(verbuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(result))
	{
		delete[] vertices;
		vertices = 0;
		return false;
	}
	VertexType* verticesPtr = (VertexType*)mappedResource.pData;
	memcpy(verticesPtr, (void*)vertices, (sizeof(VertexType) * m_vertexCount));
	m_deviceContext->Unmap(verbuffer, 0);
	delete[] vertices;
	vertices = 0;
	return true;
}
std::vector< DXFormatInfo> VideoRenderFilter::GetDXFormat(CFrameSharePtr &stFrame)
{
	std::vector< DXFormatInfo> rsp;
	if (stFrame->m_ePixType == eBGRA)
	{
		DXFormatInfo info;
		info.format = DXGI_FORMAT_B8G8R8A8_UNORM;
		info.width = stFrame->m_nWidth;
		info.height = stFrame->m_nHeight;
		rsp.push_back(info);
	}
	else if (eRGBA == stFrame->m_ePixType)
	{
		DXFormatInfo info;
		info.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		info.width = stFrame->m_nWidth;
		info.height = stFrame->m_nHeight;
		rsp.push_back(info);
	}
	else if (eYVYU422 == stFrame->m_ePixType)
	{
		DXFormatInfo info;
		info.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		info.width = stFrame->m_nWidth / 2;
		info.height = stFrame->m_nHeight;
		rsp.push_back(info);
	}
	else if (stFrame->m_ePixType == eUYVY422)
	{
		DXFormatInfo info;
		info.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		info.width = stFrame->m_nWidth / 2;
		info.height = stFrame->m_nHeight;
		rsp.push_back(info);
	}
	else if (stFrame->m_ePixType == eYUYV422)
	{
		DXFormatInfo info;
		info.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		info.width = stFrame->m_nWidth / 2;
		info.height = stFrame->m_nHeight;
		rsp.push_back(info);
	}
	else if (eBGR == stFrame->m_ePixType)
	{
		DXFormatInfo info;
		info.format = DXGI_FORMAT_R8_UNORM;
		info.width = stFrame->m_nWidth;
		info.height = stFrame->m_nHeight;
		rsp.push_back(info);
		rsp.push_back(info);
		rsp.push_back(info);
	}
	else if (eYUV420P10 == stFrame->m_ePixType)
	{
		
		DXFormatInfo info;
		info.format = DXGI_FORMAT_R16_UNORM;
		info.width = stFrame->m_nWidth;
		info.height = stFrame->m_nHeight;
		rsp.push_back(info);
		info.width = stFrame->m_nWidth / 2;
		info.height = stFrame->m_nHeight / 2;
		rsp.push_back(info);
		info.width = stFrame->m_nWidth / 2;
		info.height = stFrame->m_nHeight / 2;
		rsp.push_back(info);
	}
	else
	{
		DXFormatInfo info;
		info.format = DXGI_FORMAT_R8_UNORM;
		info.width = stFrame->m_nWidth;
		info.height = stFrame->m_nHeight;
		rsp.push_back(info);
		info.width = stFrame->m_nWidth / 2;
		info.height = stFrame->m_nHeight/2;
		rsp.push_back(info);
		info.width = stFrame->m_nWidth/2;
		info.height = stFrame->m_nHeight/2;
		rsp.push_back(info);
	}
	return rsp;

}
bool VideoRenderFilter::ReadFrameDataToTexture(CFrameSharePtr &stFrame)
{
	bool bRet = false;
	do
	{
		if (m_nLastWidth != stFrame->m_nWidth || m_nLastHeight != stFrame->m_nHeight || stFrame->m_ePixType != m_RenderPixelFormat)
		{
			for (int i = 0; i < MAXPLANE; i++)
			{
				if (m_pSourceTexture2d[i])
				{
					m_pSourceTexture2d[i]->Release();
					m_pSourceTexture2d[i] = NULL;
				}
				if (m_pSourceTexture[i])
				{
					m_pSourceTexture[i]->Release();
					m_pSourceTexture[i] = NULL;
				}
			}
			std::vector< DXFormatInfo> rsp = GetDXFormat(stFrame);
			for (int i = 0; i < rsp.size(); i++)
			{
				D3D11_TEXTURE2D_DESC tex_desc;
				ZeroMemory(&tex_desc, sizeof(tex_desc));
				tex_desc.Width =rsp[i].width;
				tex_desc.Height = rsp[i].height;
				tex_desc.MipLevels = 1;
				tex_desc.ArraySize = 1;
				tex_desc.Format = rsp[i].format;
				tex_desc.SampleDesc.Count = 1;
				tex_desc.SampleDesc.Quality = 0;
				tex_desc.Usage = D3D11_USAGE_DYNAMIC;
				tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
				tex_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
				tex_desc.MiscFlags = 0;// D3D11_RESOURCE_MISC_SHARED;
				HRESULT hr = m_device->CreateTexture2D(&tex_desc, NULL, &m_pSourceTexture2d[i]);
				if (FAILED(hr))
				{
					return bRet;
				}
				D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
				ZeroMemory(&srv_desc, sizeof(srv_desc));
				srv_desc.Format = rsp[i].format;
				srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
				srv_desc.Texture2D.MipLevels = 1;
				srv_desc.Texture2D.MostDetailedMip = 0;
				m_device->CreateShaderResourceView(m_pSourceTexture2d[i], &srv_desc, &m_pSourceTexture[i]);
			}
			m_nLastHeight = stFrame->m_nHeight;
			m_nLastWidth = stFrame->m_nWidth;
			m_RenderPixelFormat = stFrame->m_ePixType;
			bool result = UpdateBuffers(m_vertexBufferSource, stFrame->m_nWidth, stFrame->m_nHeight, stFrame->m_nWidth, stFrame->m_nHeight);
			result = UpdateBuffers(m_vertexBuffer, stFrame->m_nWidth, stFrame->m_nHeight, m_nTextureWidth, m_nTextureHeight);
		}
		unsigned char *pFrameData = stFrame->GetDataPtr();
		if (stFrame->m_ePixType == eBGRA)
		{
			D3D11_MAPPED_SUBRESOURCE MappedResource;
			HRESULT	hr = m_deviceContext->Map(m_pSourceTexture2d[0], 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
			if (FAILED(hr))
				return bRet;
			int nStep = 0;
			int nStep2 = 0;
			int nRealStepWidth = stFrame->m_nWidth * 4;
			BYTE* pRow = (BYTE*)MappedResource.pData;
			if (MappedResource.RowPitch == nRealStepWidth)
			{
				memcpy(pRow, pFrameData, stFrame->m_nLen);
			}
			else
			{
				for (int j = 0; j < stFrame->m_nHeight; j++)
				{
					memcpy(pRow + j * MappedResource.RowPitch, pFrameData + nStep2, nRealStepWidth);
					nStep2 += nRealStepWidth;
				}
			}
			m_deviceContext->Unmap(m_pSourceTexture2d[0], 0);
		}
		else if (eRGBA == stFrame->m_ePixType)
		{
			D3D11_MAPPED_SUBRESOURCE MappedResource;
			HRESULT	hr = m_deviceContext->Map(m_pSourceTexture2d[0], 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
			if (FAILED(hr))
				return bRet;
			int nStep = 0;
			int nStep2 = 0;
			int nRealStepWidth = stFrame->m_nWidth * 4;
			BYTE* pRow = (BYTE*)MappedResource.pData;
			if (MappedResource.RowPitch == nRealStepWidth)
			{
				memcpy(pRow, pFrameData, stFrame->m_nLen);
			}
			else
			{
				for (int j = 0; j < stFrame->m_nHeight; j++)
				{
					memcpy(pRow + j * MappedResource.RowPitch, pFrameData + nStep2, nRealStepWidth);
					nStep2 += nRealStepWidth;
				}
			}
			m_deviceContext->Unmap(m_pSourceTexture2d[0], 0);
		}
		else if (eYVYU422 == stFrame->m_ePixType)
		{
			D3D11_MAPPED_SUBRESOURCE MappedResource;
			HRESULT	hr = m_deviceContext->Map(m_pSourceTexture2d[0], 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
			if (FAILED(hr))
				return bRet;
			byte * pRow = (BYTE *)MappedResource.pData;;
			int nStep2 = 0;
			int nRealStepWidth = stFrame->m_nWidth * 2;
			if (MappedResource.RowPitch == nRealStepWidth)
			{
				memcpy(pRow, pFrameData, stFrame->m_nLen);
			}
			else
			{
				for (int j = 0; j < stFrame->m_nHeight; j++)
				{
					memcpy(pRow + j * MappedResource.RowPitch, pFrameData + nStep2, nRealStepWidth);
					nStep2 += nRealStepWidth;
				}
			}
			m_deviceContext->Unmap(m_pSourceTexture2d[0], 0);
		}
		else if (stFrame->m_ePixType == eUYVY422)
		{
			D3D11_MAPPED_SUBRESOURCE MappedResource;
			HRESULT	hr = m_deviceContext->Map(m_pSourceTexture2d[0], 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
			if (FAILED(hr))
				return bRet;
			byte *pRow = (BYTE *)MappedResource.pData;;
			int nStep2 = 0;
			int nRealStepWidth = stFrame->m_nWidth * 2;
			if (MappedResource.RowPitch == nRealStepWidth)
			{
				memcpy(pRow, pFrameData, stFrame->m_nLen);
			}
			else
			{
				for (int j = 0; j < stFrame->m_nHeight; j++)
				{
					memcpy(pRow + j * MappedResource.RowPitch, pFrameData + nStep2, nRealStepWidth);
					nStep2 += nRealStepWidth;
				}
			}
			m_deviceContext->Unmap(m_pSourceTexture2d[0], 0);
		}
		else if (stFrame->m_ePixType == eYUYV422)
		{
			D3D11_MAPPED_SUBRESOURCE MappedResource;
			HRESULT	hr = m_deviceContext->Map(m_pSourceTexture2d[0], 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
			if (FAILED(hr))
				return bRet;
			byte * pRow = (BYTE *)MappedResource.pData;;
			int nStep2 = 0;
			int nRealStepWidth = stFrame->m_nWidth * 2;
			if (MappedResource.RowPitch == nRealStepWidth)
			{
				memcpy(pRow, pFrameData, stFrame->m_nLen);
			}
			else
			{
				for (int j = 0; j < stFrame->m_nHeight; j++)
				{
					memcpy(pRow + j * MappedResource.RowPitch, pFrameData + nStep2, nRealStepWidth);
					nStep2 += nRealStepWidth;
				}
			}
			m_deviceContext->Unmap(m_pSourceTexture2d[0], 0);
		}
		else if (stFrame->m_ePixType == eBGR)
		{
			D3D11_MAPPED_SUBRESOURCE MappedResource[3];
			BYTE* pRow[3];
			for (int i = 0; i < 3; i++)
			{
				HRESULT	hr = m_deviceContext->Map(m_pSourceTexture2d[i], 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource[i]);
				if (FAILED(hr))
					return bRet;
				pRow[i] = (BYTE*)MappedResource[i].pData;
			}
			
			{
				for (int i = 0; i < stFrame->m_nWidth; i++)
				{
					for (int j = 0; j < stFrame->m_nHeight; j++)
					{
						memcpy(pRow[0] + j * MappedResource[0].RowPitch + i, pFrameData + j * stFrame->m_nWidth *3 + i, 1);
						memcpy(pRow[1] + j * MappedResource[1].RowPitch + i, pFrameData + j * stFrame->m_nWidth * 3 + 1 + i, 1);
						memcpy(pRow[2] + j * MappedResource[2].RowPitch + i, pFrameData + j * stFrame->m_nWidth * 3 + + 2+ i, 1);
					}
				}
				
			}
			for (int i = 0; i < 3; i++)
			{
				m_deviceContext->Unmap(m_pSourceTexture2d[i], 0);
			}
		}
		else if (stFrame->m_ePixType == eYUV420P10)
		{
		D3D11_MAPPED_SUBRESOURCE MappedResource[3];
		BYTE* pRow[3];
		int nStrideY[3];
		for (int i = 0; i < 3; i++)
		{
			HRESULT	hr = m_deviceContext->Map(m_pSourceTexture2d[i], 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource[i]);
			if (FAILED(hr))
				return bRet;
			pRow[i] = (BYTE*)MappedResource[i].pData;
			nStrideY[i] = MappedResource[i].RowPitch;
		}
		unsigned char *pYsrc = NULL;
		unsigned char *pUsrc = NULL;
		unsigned char *pVsrc = NULL;
		int linesize[3] = { stFrame->m_nWidth*2, stFrame->m_nWidth , stFrame->m_nWidth};
		pYsrc = pFrameData;
		pUsrc = pFrameData + stFrame->m_nWidth * stFrame->m_nHeight  * 2;
		pVsrc = pFrameData + stFrame->m_nWidth * stFrame->m_nHeight * 5 / 2;

		for (int i = 0; i < stFrame->m_nHeight; i++)
		{
			memcpy(pRow[0] + i * nStrideY[0], pYsrc + i * linesize[0], stFrame->m_nWidth*2);
		}
		for (int i = 0; i < stFrame->m_nHeight / 2; i++)
		{
			memcpy(pRow[1] + nStrideY[1] * i, pUsrc + i * linesize[1], stFrame->m_nWidth);
			memcpy(pRow[2] + nStrideY[2] * i, pVsrc + i * linesize[2], stFrame->m_nWidth);
		}


		for (int i = 0; i < 3; i++)
		{
			m_deviceContext->Unmap(m_pSourceTexture2d[i], 0);
		}
		}
		else
		{
		

		D3D11_MAPPED_SUBRESOURCE MappedResource[3];
		BYTE* pRow[3];
		int nStrideY[3];
		for (int i = 0; i < 3; i++)
		{
			HRESULT	hr = m_deviceContext->Map(m_pSourceTexture2d[i], 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource[i]);
			if (FAILED(hr))
				return bRet;
			pRow[i] = (BYTE*)MappedResource[i].pData;
			nStrideY[i] = MappedResource[i].RowPitch;
		}
		unsigned char *pYsrc = NULL;
		unsigned char *pUsrc = NULL;
		unsigned char *pVsrc = NULL;
		int linesize[3] = { stFrame->m_nWidth, stFrame->m_nWidth / 2, stFrame->m_nWidth / 2 };
		pYsrc = pFrameData;
		pUsrc = pFrameData + stFrame->m_nWidth * stFrame->m_nHeight;
		pVsrc = pFrameData + stFrame->m_nWidth * stFrame->m_nHeight * 5 / 4;

		for (int i = 0; i < stFrame->m_nHeight; i++)
		{
			memcpy(pRow[0] + i * nStrideY[0], pYsrc + i * linesize[0], stFrame->m_nWidth);
		}
		for (int i = 0; i < stFrame->m_nHeight / 2; i++)
		{
				memcpy(pRow[1] + nStrideY[1]  * i , pUsrc + i * linesize[1], stFrame->m_nWidth/2);
				memcpy(pRow[2] + nStrideY[2]  * i , pVsrc + i * linesize[2], stFrame->m_nWidth/2);
		}

		
		for (int i = 0; i < 3; i++)
		{
			m_deviceContext->Unmap(m_pSourceTexture2d[i], 0);
		}
		}
		bRet = true;
		 UpdateBuffers(m_vertexBuffer, stFrame->m_nWidth, stFrame->m_nHeight, m_nTextureWidth, m_nTextureHeight);
	} while (0);
	return bRet;
}

bool VideoRenderFilter::ResetD3DResource(CFrameSharePtr &stFrame)
{
	bool bRet = true;
	if (m_nLastWidth != stFrame->m_nWidth || m_nLastHeight != stFrame->m_nHeight)
	{
		viewportframe.Width = (float)stFrame->m_nWidth;
		viewportframe.Height = (float)stFrame->m_nHeight;
		viewportframe.MinDepth = 0.0f;
		viewportframe.MaxDepth = 1.0f;
		viewportframe.TopLeftX = 0.0f;
		viewportframe.TopLeftY = 0.0f;
		if (m_texture2dText)
		{
			m_texture2dText->Release();
			m_texture2dText = NULL;
		}
		if (m_textureText)
		{
			m_textureText->Release();
			m_textureText = NULL;
		}
		if (m_renderTextureTargetView)
		{
			m_renderTextureTargetView->Release();
			m_renderTextureTargetView = NULL;
		}

		if (stFrame->m_nPixBits == 8)
		{
			m_dstRenderDXFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		}
		else
		{
			m_dstRenderDXFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
		}
		D3D11_TEXTURE2D_DESC tex_desc;
		ZeroMemory(&tex_desc, sizeof(tex_desc));
		tex_desc.Width = stFrame->m_nWidth;
		tex_desc.Height = stFrame->m_nHeight;
		tex_desc.MipLevels = 1;
		tex_desc.ArraySize = 1;
		tex_desc.Format = m_dstRenderDXFormat;
		tex_desc.SampleDesc.Count = 1;
		tex_desc.SampleDesc.Quality = 0;
		tex_desc.Usage = D3D11_USAGE_DEFAULT;;
		tex_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		tex_desc.CPUAccessFlags = 0;
		tex_desc.MiscFlags = 0;
		HRESULT hr = m_device->CreateTexture2D(&tex_desc, NULL, &m_texture2dText);
		if (FAILED(hr))
		{
			bRet = false;
			return bRet;
		}
		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
		ZeroMemory(&srv_desc, sizeof(srv_desc));
		srv_desc.Format = m_dstRenderDXFormat;
		srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MipLevels = 1;
		srv_desc.Texture2D.MostDetailedMip = 0;
		m_device->CreateShaderResourceView(m_texture2dText, &srv_desc, &m_textureText);
		

		D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
		renderTargetViewDesc.Format = tex_desc.Format;
		renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		renderTargetViewDesc.Texture2D.MipSlice = 0;
		hr = m_device->CreateRenderTargetView(m_texture2dText, &renderTargetViewDesc, &m_renderTextureTargetView);
		if (FAILED(hr))
		{
			bRet = false;
			return bRet;
		}
		if (m_texture2dShared)
		{
			m_texture2dShared->Release();
			m_texture2dShared = NULL;
		}
		/*D3D11_TEXTURE2D_DESC tex_desc;*/
		ZeroMemory(&tex_desc, sizeof(tex_desc));
		tex_desc.Width = stFrame->m_nWidth;
		tex_desc.Height = stFrame->m_nHeight;
		tex_desc.MipLevels = 1;
		tex_desc.ArraySize = 1;
		tex_desc.Format = m_dstRenderDXFormat;
		tex_desc.SampleDesc.Count = 1;
		tex_desc.SampleDesc.Quality = 0;
		tex_desc.Usage = D3D11_USAGE_DEFAULT;
		tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		tex_desc.CPUAccessFlags = 0;
		tex_desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
		hr = m_device->CreateTexture2D(&tex_desc, NULL, &m_texture2dShared);
		if (FAILED(hr))
		{
			return false;
		}
		HANDLE m_Handle = 0;
		IDXGIResource* pOtherResource(NULL);
		hr = m_texture2dShared->QueryInterface(__uuidof(IDXGIResource), (void**)&pOtherResource);
		if (FAILED(hr))
		{
			return false;
		}
		pOtherResource->GetSharedHandle(&m_Handle);
		pOtherResource->Release();
		CChannelD3DTextureInfo info1;
		info1.m_nHeight = stFrame->m_nWidth;
		info1.m_nWidth = stFrame->m_nHeight;
		info1.m_pHandle = m_Handle;
		info1.colorspace = stFrame->colorspace;
		info1.m_nBit = stFrame->m_nPixBits;
		info1.primaries = stFrame->color_primaries;
		info1.transfer = stFrame->color_trc;
		SSMainConfiguration::instance().SetTextInfo(m_nIndex, info1);
	}
	return bRet;
}
