#include "VideoRenderFilter.h"
#include "../Common/MediaTimer.h"
#include "../Common/utils.h"

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

std::string getExecutableDir()
{
	char path[512] = "";

#ifdef _WIN32

	GetModuleFileNameA(NULL, path, sizeof(path));

#else

	ssize_t count = readlink("/proc/self/exe", path, sizeof(path));
	if (count <= 0)
	{
		return "";
	}

#endif
	std::string strpath = path;
	std::string::size_type pos = std::string(strpath).find_last_of("\\/");
	strpath = std::string(strpath).substr(0, pos);
	return strpath;
}

VideoRenderFilter::~VideoRenderFilter()
{
	Shutdown();
}

VideoRenderFilter::VideoRenderFilter(QWidget*  parent, std::string &strName) : CSSFilter(strName)
{
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
	

	m_nLastWidth = 0;
	m_nLastHeight = 0;
	m_RenderPixelFormat = eUnknowPix;
	m_pWidget = parent;
	m_nTextureWidth = m_pWidget->width();
	m_nTextureHeight = m_pWidget->height();
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

void VideoRenderFilter::RenderToWindow()
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
	//	m_pWorldMatVar->SetMatrix(reinterpret_cast<const float*>(&worldMatrix));
	m_pViewMatVar->SetMatrix(reinterpret_cast<const float*>(&viewMatrix));
	//	m_pProjMatVar->SetMatrix((reinterpret_cast<const float*>(&orthoMatrix)));
	m_pTextSourceY->SetResource((ID3D11ShaderResourceView*)m_textureText);
	m_pType->SetInt(1);
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
		if (dsc.Width == m_nTextureWidth && dsc.Height == m_nTextureHeight)
		{
			break;
		}
		m_deviceContext->ClearState();

		if (m_renderTargetView)
		{
			m_renderTargetView->Release();
			m_renderTargetView = NULL;
		}
		HRESULT result = m_swapChain->ResizeBuffers(0, m_nTextureWidth, m_nTextureHeight, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
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
		if (!m_bRendUpdate)
		{
			RenderToWindow();
		}
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
	XMMATRIX worldMatrix, viewMatrix, projectionMatrix, orthoMatrix;
	bool bRendToTexture = true;
	// render to texture
	if (bRendToTexture)
	{
		m_deviceContext->OMSetRenderTargets(1, &m_renderTextureTargetView, NULL);
		m_deviceContext->RSSetViewports(1, &viewportframe);
		BeginScene(0.0f, 0.0f, 0.0f, 1.0f, m_renderTextureTargetView, NULL);
		//	TurnOnAlphaBlending();
			// Put the vertex and index buffers on the graphics pipeline to prepare them for drawing.
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
		//	m_pWorldMatVar->SetMatrix(reinterpret_cast<const float*>(&worldMatrix));
		m_pViewMatVar->SetMatrix(reinterpret_cast<const float*>(&viewMatrix));
		//	m_pProjMatVar->SetMatrix((reinterpret_cast<const float*>(&orthoMatrix)));
		m_pTextSourceY->SetResource((ID3D11ShaderResourceView*)m_pSourceTexture[0]);
		m_pTextSourceU->SetResource((ID3D11ShaderResourceView*)m_pSourceTexture[1]);
		m_pTextSourceV->SetResource((ID3D11ShaderResourceView*)m_pSourceTexture[2]);
		m_pTextSourceA->SetResource((ID3D11ShaderResourceView*)m_pSourceTexture[3]);
		m_pType->SetInt(nType);
		m_pSourceHeight->SetInt(stFrame->m_nHeight);
		m_pSourceWidth->SetInt(stFrame->m_nWidth);
		m_deviceContext->IASetInputLayout(m_layout);
		D3DX11_TECHNIQUE_DESC techDesc;
		m_pTech->GetDesc(&techDesc);
		for (UINT i = 0; i < techDesc.Passes; ++i)
		{
			m_pTech->GetPassByIndex(i)->Apply(0, m_deviceContext);
			m_deviceContext->DrawIndexed(m_indexCount, 0, 0);
		}

	}
	ID3D11ShaderResourceView *const pSRV[1] = { NULL };
	m_deviceContext->PSSetShaderResources(0, 1, pSRV);
	RenderToWindow();
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

		DWORD dwRet = WaitForSingleObject(pThis->m_hEventHandle, 1000);
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
		bool bRet = pThis->ReadData();
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

bool VideoRenderFilter::Initialize(int screenWidth, int screenHeight, HWND hwnd)
{
	std::lock_guard<std::mutex> stLock(m_stD3DLock);
	UINT createDeviceFlags = 0;
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
	createDeviceFlags |= D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
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

	DXGI_FORMAT format = DXGI_FORMAT_R10G10B10A2_UNORM;//DXGI_FORMAT_R8G8B8A8_UNORM
	DXGI_SWAP_CHAIN_DESC1 out;
	memset(&out, 0, sizeof(out));
	out.BufferCount = 3;
	out.BufferUsage = DXGI_USAGE_BACK_BUFFER | DXGI_USAGE_RENDER_TARGET_OUTPUT;
	out.SampleDesc.Count = 1;
	out.SampleDesc.Quality = 0;
	out.Width = screenWidth;
	out.Height = screenHeight;
	out.Format = format;
	//out->Flags = 512; // DXGI_SWAP_CHAIN_FLAG_YUV_VIDEO;
	out.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;



	IDXGIAdapter *dxgiadapter = D3D11DeviceAdapter(m_device);
	if (dxgiadapter == NULL)
	{
		return false;
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

		D3D11_TEXTURE2D_DESC tex_desc;
		ZeroMemory(&tex_desc, sizeof(tex_desc));
		tex_desc.Width = stFrame->m_nWidth;
		tex_desc.Height = stFrame->m_nHeight;
		tex_desc.MipLevels = 1;
		tex_desc.ArraySize = 1;
		tex_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
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
		srv_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
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
	}
	return bRet;
}
