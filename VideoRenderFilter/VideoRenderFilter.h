#pragma once

#include "videorenderfilter_global.h"
#include "../Common/Filter.h"
#include <QWidget>
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#include "d3dx11effect.h"
#include <d3d11.h>
#include <DirectXMath.h>
using namespace DirectX;
class  VideoRenderFilter : public CSSFilter
{
public:
	VideoRenderFilter(QWidget*  parent, std::string &strName);
	virtual ~VideoRenderFilter();
	virtual int InputData(CFrameSharePtr &frame);
	static DWORD WINAPI  SyncRead(LPVOID arg);
	void ResizeBackBuffer(int nWidth, int nHeight);
	void UpdateBackBuffer();
	void RenderToWindow();
	bool ReadData();
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

private:
	QWidget * m_pWidget;
	IDXGISwapChain* m_swapChain;
	ID3D11Device1* m_device1;
	ID3D11DeviceContext1* m_deviceContext1;
	ID3D11Device* m_device;
	ID3D11DeviceContext* m_deviceContext;


	ID3D11RenderTargetView* m_renderTargetView;

	D3D11_VIEWPORT viewport;
	D3D11_VIEWPORT viewportframe;

	ID3D11Buffer *m_vertexBuffer, *m_indexBuffer;
	ID3D11Buffer *m_vertexBufferSource;
	int m_vertexCount, m_indexCount;
	// ‰÷»æµΩŒ∆¿Ì
	ID3D11RenderTargetView* m_renderTextureTargetView;
	ID3D11Texture2D *m_texture2dText;
	ID3D11ShaderResourceView* m_textureText;

	ID3D11Texture2D *m_pSourceTexture2d;
	ID3D11ShaderResourceView* m_pSourceTexture;


protected:
	ID3DX11Effect	*m_pEffect;
	ID3DX11EffectTechnique *m_pTech;
	ID3DX11EffectMatrixVariable *m_pWorldMatVar;
	ID3DX11EffectMatrixVariable *m_pViewMatVar;
	ID3DX11EffectMatrixVariable *m_pProjMatVar;

	ID3DX11EffectScalarVariable     *m_pType;
	ID3DX11EffectScalarVariable     *m_pSourceWidth;
	ID3DX11EffectScalarVariable     *m_pSourceHeight;
	ID3DX11EffectShaderResourceVariable	*m_pTextSource;
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
};
