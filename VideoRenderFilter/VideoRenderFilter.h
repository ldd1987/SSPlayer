#pragma once

#include "videorenderfilter_global.h"
#include "../Common/Filter.h"
#include <QWidget>
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#include "d3dx11effect.h"
#include <d3d11.h>
#include <DirectXMath.h>
# include <dxgi1_6.h>
#include <vector>
using namespace DirectX;
const int MAXPLANE = 4;
struct DXFormatInfo
{
	DXGI_FORMAT format;
	int width;
	int height;
};

typedef enum video_color_axis {
	COLOR_AXIS_RGB,
	COLOR_AXIS_YCBCR,
} video_color_axis;

typedef struct {
	DXGI_COLOR_SPACE_TYPE   dxgi;
	const char              *name;
	video_color_axis        axis;
	AVColorPrimaries primaries;
	AVColorTransferCharacteristic   transfer;
	AVColorSpace     color;
	bool                    b_full_range;
} dxgi_color_space;

struct Primaries
{
	float primaries[3][2];
	float whitepoint[2];
};
typedef struct {
	FLOAT WhitePoint[4 * 4];
	FLOAT Colorspace[4 * 4];
	FLOAT Primaries[4 * 4];
//	FLOAT DisTransPrimaries[4 * 4];
} PS_COLOR_TRANSFORM;
 struct video_render_cfg_t
{
	unsigned width;                        /** rendering video width in pixel */
	unsigned height;                      /** rendering video height in pixel */
	unsigned bitdepth;      /** rendering video bit depth in bits per channel */
	bool full_range;          /** video is full range or studio/limited range */
	AVColorSpace colorspace;              /** video color space */
	AVColorPrimaries primaries;       /** video color primaries */
	AVColorTransferCharacteristic transfer;        /** video transfer function */
	dxgi_color_space dxgicolor;
	int luminance_peak;
	bool sendmetadata;
} ;

class  VideoRenderFilter : public CSSFilter
{
public:
	VideoRenderFilter(long index,QWidget*  parent, std::string &strName, bool bpgm= true);
	virtual ~VideoRenderFilter();
	virtual int InputData(CFrameSharePtr &frame);
	static DWORD WINAPI  SyncRead(LPVOID arg);
	void ResizeBackBuffer(int nWidth, int nHeight);
	void UpdateBackBuffer();
	void RenderToWindow(bool bDirect);
	bool ReadData();
	bool ReadDataPGM();
	bool UpdateBuffersEx(ID3D11Buffer* verbuffer, int nLastWidth, int nLastHeight, CD3D11_RECT dest);
	void SetSwapchainSetMetadata(CFrameSharePtr &stFrame);
	void SelectSwapchainColorspace();
	bool ReadFrameDataToTexture(CFrameSharePtr &stFrame);
	bool ResetD3DResource(CFrameSharePtr &stFrame);
	IDXGIAdapter *D3D11DeviceAdapter(ID3D11Device *d3ddev);
	void TurnOnAlphaBlending();
	void TurnOffAlphaBlending();
	int GetFeatureLevels(int max_fl, int min_fl, const D3D_FEATURE_LEVEL **out);
	HRESULT create_swapchain_1_2(ID3D11Device *dev, IDXGIFactory2 *factory, bool flip, DXGI_FORMAT format, IDXGISwapChain **swapchain_out);
	HRESULT create_swapchain_1_1(ID3D11Device *dev, IDXGIFactory1 *factory, bool flip, DXGI_FORMAT format, IDXGISwapChain **swapchain_out);
	bool d3d11_create_swapchain(ID3D11Device *dev, IDXGISwapChain **swapchain_out);
	bool Initialize(int, int, HWND);
	void Shutdown();
	void BeginScene(float, float, float, float, ID3D11RenderTargetView* renderTargetView, ID3D11DepthStencilView* depthStencilView);
	void EndScene();
	bool InitializeBuffers(ID3D11Device*);
	bool UpdateBuffers(ID3D11Buffer*, int, int, int, int);
	void RenderBuffers(ID3D11Buffer*);
	std::vector< DXFormatInfo> GetDXFormat(CFrameSharePtr &stFrame);
	void SetColPrimaries(AVColorPrimaries dst, AVColorPrimaries src , AVColorTransferCharacteristic srctranfunc, AVColorTransferCharacteristic dsttranfunc, CFrameSharePtr &stFrame, bool b = false);
private:
	QWidget * m_pWidget;
	ID3D11Device* m_device;
	ID3D11DeviceContext* m_deviceContext;
	IDXGISwapChain1        *m_swapChain;   /* DXGI 1.2 swap chain */
	IDXGISwapChain4        *m_swapChain4;  /* DXGI 1.5 for HDR metadata */
	DXGI_HDR_METADATA_HDR10 m_hdr10;

	ID3D11RenderTargetView* m_renderTargetView;

	D3D11_VIEWPORT viewport;
	D3D11_VIEWPORT viewportframe;

	ID3D11Buffer *m_vertexBuffer, *m_indexBuffer;
	ID3D11Buffer *m_vertexBufferSource;
	int m_vertexCount, m_indexCount;
	// ‰÷»æµΩŒ∆¿Ì
	ID3D11RenderTargetView* m_renderTextureTargetView;
	ID3D11Texture2D *m_texture2dText;
	ID3D11Texture2D *m_texture2dShared=0;
	ID3D11ShaderResourceView* m_textureText;

	ID3D11Texture2D *m_pSourceTexture2d[MAXPLANE];
	ID3D11ShaderResourceView* m_pSourceTexture[MAXPLANE];

	ID3D11Buffer *m_pixtranformBuffer=0;
	PS_COLOR_TRANSFORM m_pixtransform;
protected:
	ID3DX11Effect	*m_pEffect;
	ID3DX11EffectTechnique *m_pTech;
	ID3DX11EffectMatrixVariable *m_pWorldMatVar;
	ID3DX11EffectMatrixVariable *m_pViewMatVar;
	ID3DX11EffectMatrixVariable *m_pProjMatVar;

	ID3DX11EffectScalarVariable     *m_pType;
	ID3DX11EffectScalarVariable     *m_pSourceWidth;
	ID3DX11EffectScalarVariable     *m_pSourceHeight;
	ID3DX11EffectShaderResourceVariable	*m_pTextSourceY;
	ID3DX11EffectShaderResourceVariable	*m_pTextSourceU;
	ID3DX11EffectShaderResourceVariable	*m_pTextSourceV;
	ID3DX11EffectShaderResourceVariable	*m_pTextSourceA;
	ID3DX11EffectScalarVariable     *m_ptransfer;
	ID3DX11EffectScalarVariable     *m_pdistransfer;
	ID3DX11EffectScalarVariable     *m_pprimaries;
	ID3DX11EffectConstantBuffer              *m_pTransPrimariesInfo = 0;
	ID3DX11EffectScalarVariable     *m_pdisprimaries;
	ID3DX11EffectScalarVariable     *m_pfullrange;
	ID3DX11EffectScalarVariable     *m_psrcrange;
	ID3DX11EffectScalarVariable     *m_pLuminanceScale;
	ID3DX11EffectScalarVariable     *m_pDrawLine;
	ID3D11InputLayout* m_layout;

	ID3D11BlendState* m_alphaEnableBlendingState;
	ID3D11BlendState* m_alphaDisableBlendingState;

private:
	HANDLE m_hEventHandle;
	int						m_nTextureWidth;
	int						m_nTextureHeight;
	std::mutex	m_stFrameLockEx;
	CFrameSharePtr m_pBRendFrame;
	HANDLE  m_hSyncThread;
	bool    m_bExit;

	int                     m_nLastWidth;
	int                     m_nLastHeight;
	PixType m_RenderPixelFormat;
	bool m_bRendUpdate;
	std::mutex	m_stD3DLock;
	video_render_cfg_t m_displayInfo;
	AVColorPrimaries m_colPrimariesDst;
	AVColorPrimaries m_colPrimariesSrc;
	Primaries m_pMatPrim;
	DXGI_FORMAT  m_dstBufferDXFormat;
	DXGI_FORMAT  m_dstRenderDXFormat;
	bool m_bDirect;
	long m_nIndex;
	HANDLE m_hSrcHandle[3];
	ID3D11Texture2D *m_texture2dpip[3];
	ID3D11ShaderResourceView *m_texturepip[3];
};
