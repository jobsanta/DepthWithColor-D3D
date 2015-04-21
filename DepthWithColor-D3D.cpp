//------------------------------------------------------------------------------
// <copyright file="DepthWithColor-D3D.cpp" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//------------------------------------------------------------------------------

#include "DepthWithColor-D3D.h"

#ifdef SAMPLE_OPTIONS
#include "Options.h"
#else
PVOID _opt = NULL;
#endif

static const float c_FaceTextLayoutOffsetX = -0.1f;

// face property text layout offset in Y axis
static const float c_FaceTextLayoutOffsetY = -0.125f;

// define the face frame features required to be computed by this application
static const DWORD c_FaceFrameFeatures =
FaceFrameFeatures::FaceFrameFeatures_BoundingBoxInColorSpace
| FaceFrameFeatures::FaceFrameFeatures_PointsInColorSpace
| FaceFrameFeatures::FaceFrameFeatures_RotationOrientation
| FaceFrameFeatures::FaceFrameFeatures_Happy
| FaceFrameFeatures::FaceFrameFeatures_RightEyeClosed
| FaceFrameFeatures::FaceFrameFeatures_LeftEyeClosed
| FaceFrameFeatures::FaceFrameFeatures_MouthOpen
| FaceFrameFeatures::FaceFrameFeatures_MouthMoved
| FaceFrameFeatures::FaceFrameFeatures_LookingAway
| FaceFrameFeatures::FaceFrameFeatures_Glasses
| FaceFrameFeatures::FaceFrameFeatures_FaceEngagement;


PxPhysics*                CDepthWithColorD3D::gPhysicsSDK = NULL;
PxDefaultErrorCallback    CDepthWithColorD3D::gDefaultErrorCallback;
PxDefaultAllocator        CDepthWithColorD3D::gDefaultAllocatorCallback;
PxSimulationFilterShader  CDepthWithColorD3D::gDefaultFilterShader = PxDefaultSimulationFilterShader;
PxFoundation*             CDepthWithColorD3D::gFoundation = NULL;


// Global Variables
CDepthWithColorD3D g_Application;  // Application class

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

/// <summary>
/// Entry point for the application
/// </summary>
/// <param name="hInstance">handle to the application instance</param>
/// <param name="hPrevInstance">always 0</param>
/// <param name="lpCmdLine">command line arguments</param>
/// <param name="nCmdShow">whether to display minimized, maximized, or normally</param>
/// <returns>status</returns>
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    if ( FAILED( g_Application.InitWindow(hInstance, nCmdShow) ) )
    {
        return 0;
    }

    if ( FAILED( g_Application.InitDevice() ) )
    {
        return 0;
    }

	if (FAILED( g_Application.InitOpenCV()))
	{
		return 0;
	}

	g_Application.InitializePhysX();


    if ( FAILED( g_Application.CreateFirstConnected() ) )
    {
        MessageBox(NULL, L"No ready Kinect found!", L"Error", MB_ICONHAND | MB_OK);
        return 0;
    }

    // Main message loop
    MSG msg = {0};
    while (WM_QUIT != msg.message)
    {
        if (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        else
        {
            g_Application.Render();
        }
    }

    return (int)msg.wParam;
}

/// <summary>
/// Handles window messages, passes most to the class instance to handle
/// </summary>
/// <param name="hWnd">window message is for</param>
/// <param name="uMsg">message</param>
/// <param name="wParam">message data</param>
/// <param name="lParam">additional message data</param>
/// <returns>result of message processing</returns>
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;

    g_Application.HandleMessages(hWnd, message, wParam, lParam);

    switch( message )
    {
        case WM_PAINT:
            BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;      

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

/// <summary>
/// Constructor
/// </summary>
CDepthWithColorD3D::CDepthWithColorD3D()
{
	   // get resolution as DWORDS, but store as LONGs to avoid casts later
    DWORD width = 0;
    DWORD height = 0;

	m_nStartTime = 0;
	m_nLastCounter = 0;
	m_nFramesSinceUpdate = 0;
	m_fFreq = 0;

    m_depthWidth  = static_cast<LONG>(cDepthWidth);
    m_depthHeight = static_cast<LONG>(cDepthHeight);

    m_colorWidth  = static_cast<LONG>(cColorWidth);
    m_colorHeight = static_cast<LONG>(cColorHeight);

    m_colorToDepthDivisor = m_colorWidth/m_depthWidth;

    m_hInst = NULL;
    m_hWnd = NULL;
    m_featureLevel = D3D_FEATURE_LEVEL_11_0;
    m_pd3dDevice = NULL;
    m_pImmediateContext = NULL;
    m_pSwapChain = NULL;
    m_pRenderTargetView = NULL;
    m_pDepthStencil = NULL;
    m_pDepthStencilView = NULL;
    m_pVertexLayout = NULL;
    m_pVertexBuffer = NULL;
    m_pCBChangesEveryFrame = NULL;
    m_projection;

    m_pVertexShader = NULL;
    m_pPixelShader = NULL;
    m_pGeometryShader = NULL;

    m_xyScale = 0.0f;
    
    // Initial window resolution
    m_windowResX = 640;
    m_windowResY = 480;

    m_pDepthTexture2D = NULL;
    m_pDepthTextureRV = NULL;
    m_pColorTexture2D = NULL;
    m_pColorTextureRV = NULL;
    m_pColorSampler = NULL;

    m_bDepthReceived = false;
    m_bColorReceived = false;

    m_bPaused = false;
	m_pColorRGBX = NULL;


	m_pCoordinateMapper = NULL;
	m_pKinectSensor = NULL;
	m_pColorFrameReader = NULL;
	m_pBodyFrameReader = NULL;
	m_pDepthCoordinates = NULL;

	// create heap storage for composite image pixel data in RGBX format
	m_pOutputRGBX = new RGBQUAD[cColorWidth * cColorHeight];

	// create heap storage for background image pixel data in RGBX format
	m_pBackgroundRGBX = new RGBQUAD[cColorWidth * cColorHeight];

	// create heap storage for color pixel data in RGBX format
	m_pColorRGBX = new RGBQUAD[cColorWidth * cColorHeight];


	// create heap storage for the coorinate mapping from color to depth
	m_pDepthCoordinates = new DepthSpacePoint[cColorWidth * cColorHeight];

	const RGBQUAD c_green = { 255, 0, 0 };

	// Fill in with a background colour of green if we can't load the background image
	for (int i = 0; i < cColorWidth * cColorHeight; ++i)
	{
		m_pBackgroundRGBX[i] = c_green;
	}

	for (int i = 0; i < BODY_COUNT; i++)
	{
		m_pFaceFrameSources[i] = nullptr;
		m_pFaceFrameReaders[i] = nullptr;
	}

	m_pBodyIndex = NULL;
	m_depthD16 = NULL;

	referenceFrame = Mat::zeros(1920, 1080, CV_8UC1);

	proxyParticle.clear();

	//namedWindow("reference");

}

/// <summary>
/// Destructor
/// </summary>
CDepthWithColorD3D::~CDepthWithColorD3D()
{
    if (m_pKinectSensor)
    {
		m_pKinectSensor->Close();
        
    }
	SafeRelease(m_pKinectSensor);

	destroyAllWindows();

    if (m_pImmediateContext) 
    {
        m_pImmediateContext->ClearState();
    }

	// done with face sources and readers
	for (int i = 0; i < BODY_COUNT; i++)
	{
		SafeRelease(m_pFaceFrameSources[i]);
		SafeRelease(m_pFaceFrameReaders[i]);
	}

	// done with body frame reader
	SafeRelease(m_pBodyFrameReader);

	// done with color frame reader
	SafeRelease(m_pColorFrameReader);


    SAFE_RELEASE(m_pCBChangesEveryFrame);
    SAFE_RELEASE(m_pGeometryShader);
    SAFE_RELEASE(m_pPixelShader);
    SAFE_RELEASE(m_pVertexBuffer);
    SAFE_RELEASE(m_pVertexLayout);
    SAFE_RELEASE(m_pVertexShader);
    SAFE_RELEASE(m_pDepthStencil);
    SAFE_RELEASE(m_pDepthStencilView);
    SAFE_RELEASE(m_pDepthTexture2D);
    SAFE_RELEASE(m_pDepthTextureRV);
    SAFE_RELEASE(m_pColorTexture2D);
    SAFE_RELEASE(m_pColorTextureRV);
    SAFE_RELEASE(m_pColorSampler);
    SAFE_RELEASE(m_pRenderTargetView);
    SAFE_RELEASE(m_pSwapChain);
    SAFE_RELEASE(m_pImmediateContext);
    SAFE_RELEASE(m_pd3dDevice);

	//SAFE_RELEASE(m_pFaceTracker);
	//SAFE_RELEASE(m_pFTResult);

	if (m_pOutputRGBX)
	{
		delete[] m_pOutputRGBX;
		m_pOutputRGBX = NULL;
	}

	if (m_pBackgroundRGBX)
	{
		delete[] m_pBackgroundRGBX;
		m_pBackgroundRGBX = NULL;
	}

	if (m_pColorRGBX)
	{
		delete[] m_pColorRGBX;
		m_pColorRGBX = NULL;
	}

	if (m_pDepthCoordinates)
	{
		delete[] m_pDepthCoordinates;
		m_pDepthCoordinates = NULL;
	}
    delete[] m_colorCoordinates;
    delete[] m_depthD16;

	ShutdownPhysX();
}

/// <summary>
/// Register class and create window
/// </summary>
/// <returns>S_OK for success, or failure code</returns>
HRESULT CDepthWithColorD3D::InitWindow(HINSTANCE hInstance, int nCmdShow)
{
    // Register class
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIconW(hInstance, (LPCTSTR)IDI_APP);
    wcex.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = L"DepthWithColorD3DWindowClass";
    wcex.hIconSm = LoadIconW(wcex.hInstance, (LPCTSTR)IDI_APP);

    if ( !RegisterClassEx(&wcex) )
    {
        return E_FAIL;
    }

    // Create window
    m_hInst = hInstance;
    RECT rc = { 0, 0, m_windowResX, m_windowResY };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    m_hWnd = CreateWindow( L"DepthWithColorD3DWindowClass", L"Kinect View", WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hInstance,
                           NULL );
    if (NULL == m_hWnd)
    {
        return E_FAIL;
    }
	ShowWindow(m_hWnd, nCmdShow);


	//m_hWnd_user_view = CreateWindow(L"DepthWithColorD3DWindowClass", L"User View", WS_OVERLAPPEDWINDOW,
	//	CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hInstance,
	//	NULL);
	//if (NULL == m_hWnd_user_view)
	//{
	//	return E_FAIL;
	//}

	//ShowWindow(m_hWnd_user_view, nCmdShow);


    return S_OK;
}

/// <summary>
/// Handles window messages, used to process input
/// </summary>
/// <param name="hWnd">window message is for</param>
/// <param name="uMsg">message</param>
/// <param name="wParam">message data</param>
/// <param name="lParam">additional message data</param>
/// <returns>result of message processing</returns>
LRESULT CDepthWithColorD3D::HandleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    m_camera.HandleMessages(hWnd, uMsg, wParam, lParam);

    switch(uMsg)
    {
        // handle minimize
        case WM_SIZE:          
            if (SIZE_MINIMIZED == wParam)
            {
                m_bPaused = true;
            }
            break;

        // handle restore from minimized
        case WM_ACTIVATEAPP:
            if (wParam == TRUE)
            {
                m_bPaused = false;
            }
            break;

        case WM_KEYDOWN:
        {
            int nKey = static_cast<int>(wParam);

            break;
        }
    }

    return 0;
}

/// <summary>
/// Create the first connected Kinect found 
/// </summary>
/// <returns>indicates success or failure</returns>
HRESULT CDepthWithColorD3D::CreateFirstConnected()
{


	HRESULT hr;

	hr = GetDefaultKinectSensor(&m_pKinectSensor);
	if (FAILED(hr))
	{
		return hr;
	}

	if (m_pKinectSensor)
	{
		// Initialize the Kinect and get coordinate mapper and the frame reader

		if (SUCCEEDED(hr))
		{
			hr = m_pKinectSensor->get_CoordinateMapper(&m_pCoordinateMapper);
		}

		hr = m_pKinectSensor->Open();
		
		if (SUCCEEDED(hr))
		{
			hr = m_pKinectSensor->OpenMultiSourceFrameReader(
				FrameSourceTypes::FrameSourceTypes_Depth  | 
				FrameSourceTypes::FrameSourceTypes_Color | 
				FrameSourceTypes::FrameSourceTypes_BodyIndex | 
				FrameSourceTypes::FrameSourceTypes_Body,
				&m_pMultiSourceFrameReader);
		}

		if (SUCCEEDED(hr))
		{
			// create a face frame source + reader to track each body in the fov
			for (int i = 0; i < BODY_COUNT; i++)
			{
				if (SUCCEEDED(hr))
				{
					// create the face frame source by specifying the required face frame features
					hr = CreateFaceFrameSource(m_pKinectSensor, 0, c_FaceFrameFeatures, &m_pFaceFrameSources[i]);
				}
				if (SUCCEEDED(hr))
				{
					// open the corresponding reader
					hr = m_pFaceFrameSources[i]->OpenReader(&m_pFaceFrameReaders[i]);
				}
			}
		}
	}

	if (!m_pKinectSensor || FAILED(hr))
	{
		return E_FAIL;
	}

	return hr;


}


HRESULT CDepthWithColorD3D::InitOpenCV()
{
	DWORD colorWidth, colorHeight;
	colorHeight = cColorHeight;
	colorWidth = cColorWidth;

	Size size(colorWidth, colorHeight);


	namedWindow("flowfield");
	//namedWindow("reference");

	cv::gpu::printShortCudaDeviceInfo(cv::gpu::getDevice());
	return S_OK;
}

/// <summary>
/// Create Direct3D device and swap chain
/// </summary>
/// <returns>S_OK for success, or failure code</returns>
HRESULT CDepthWithColorD3D::InitDevice()
{
    HRESULT hr = S_OK;

    RECT rc;
    GetClientRect(m_hWnd, &rc);
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;

    UINT createDeviceFlags = 0;

    // Likely won't be very performant in reference
    D3D_DRIVER_TYPE driverTypes[] =
    {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };
    UINT numDriverTypes = ARRAYSIZE(driverTypes);

    // DX10 or 11 devices are suitable
    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    UINT numFeatureLevels = ARRAYSIZE(featureLevels);

    DXGI_SWAP_CHAIN_DESC sd = {0};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = m_hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;

    for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; ++driverTypeIndex)
    {
        hr = D3D11CreateDeviceAndSwapChain( NULL, driverTypes[driverTypeIndex], NULL, createDeviceFlags, featureLevels, numFeatureLevels,
                                            D3D11_SDK_VERSION, &sd, &m_pSwapChain, &m_pd3dDevice, &m_featureLevel, &m_pImmediateContext );
        if ( SUCCEEDED(hr) )
        {
            break;
        }
    }

    if ( FAILED(hr) )
    {
        MessageBox(NULL, L"Could not create a Direct3D 10 or 11 device.", L"Error", MB_ICONHAND | MB_OK);
        return hr;
    }

	// Obtain DXGI factory from device (since we used nullptr for pAdapter above)
	IDXGIFactory1* dxgiFactory = nullptr;
	{
		IDXGIDevice* dxgiDevice = nullptr;
		hr = m_pd3dDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
		if (SUCCEEDED(hr))
		{
			IDXGIAdapter* adapter = nullptr;
			hr = dxgiDevice->GetAdapter(&adapter);
			if (SUCCEEDED(hr))
			{
				hr = adapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgiFactory));
				adapter->Release();
			}
			dxgiDevice->Release();
		}
	}
	if (FAILED(hr))
		return hr;

	// DirectX 11.0 systems
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 1;
	sd.BufferDesc.Width = width;
	sd.BufferDesc.Height = height;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = m_hWnd_user_view;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;

	hr = dxgiFactory->CreateSwapChain(m_pd3dDevice, &sd, &m_pSwapChain_user);


    // Create a render target view
    ID3D11Texture2D* pBackBuffer = NULL;
    hr = m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    if ( FAILED(hr) ) { return hr; }

    hr = m_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &m_pRenderTargetView);
    pBackBuffer->Release();
    if ( FAILED(hr) ) { return hr; }


    // Create depth stencil texture
    D3D11_TEXTURE2D_DESC descDepth = {0};
    descDepth.Width = width;
    descDepth.Height = height;
    descDepth.MipLevels = 1;
    descDepth.ArraySize = 1;
    descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    descDepth.SampleDesc.Count = 1;
    descDepth.SampleDesc.Quality = 0;
    descDepth.Usage = D3D11_USAGE_DEFAULT;
    descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    descDepth.CPUAccessFlags = 0;
    descDepth.MiscFlags = 0;
    hr = m_pd3dDevice->CreateTexture2D(&descDepth, NULL, &m_pDepthStencil);
    if ( FAILED(hr) ) { return hr; }

    // Create the depth stencil view
    D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
    ZeroMemory( &descDSV, sizeof(descDSV) );
    descDSV.Format = descDepth.Format;
    descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    descDSV.Texture2D.MipSlice = 0;
    hr = m_pd3dDevice->CreateDepthStencilView(m_pDepthStencil, &descDSV, &m_pDepthStencilView);
    if ( FAILED(hr) ) { return hr; }

    m_pImmediateContext->OMSetRenderTargets(1, &m_pRenderTargetView, m_pDepthStencilView);

    // Create depth texture
    D3D11_TEXTURE2D_DESC depthTexDesc = {0};
    depthTexDesc.Width = m_depthWidth;
    depthTexDesc.Height = m_depthHeight;
    depthTexDesc.MipLevels = 1;
    depthTexDesc.ArraySize = 1;
    depthTexDesc.Format = DXGI_FORMAT_R16_SINT;
    depthTexDesc.SampleDesc.Count = 1;
    depthTexDesc.SampleDesc.Quality = 0;
    depthTexDesc.Usage = D3D11_USAGE_DYNAMIC;
    depthTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    depthTexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    depthTexDesc.MiscFlags = 0;

    hr = m_pd3dDevice->CreateTexture2D(&depthTexDesc, NULL, &m_pDepthTexture2D);
    if ( FAILED(hr) ) { return hr; }
    
    hr = m_pd3dDevice->CreateShaderResourceView(m_pDepthTexture2D, NULL, &m_pDepthTextureRV);
    if ( FAILED(hr) ) { return hr; }

    // Create color texture
    D3D11_TEXTURE2D_DESC colorTexDesc = {0};
    colorTexDesc.Width = m_colorWidth;
    colorTexDesc.Height = m_colorHeight;
    colorTexDesc.MipLevels = 1;
    colorTexDesc.ArraySize = 1;
    colorTexDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    colorTexDesc.SampleDesc.Count = 1;
    colorTexDesc.SampleDesc.Quality = 0;
    colorTexDesc.Usage = D3D11_USAGE_DYNAMIC;
    colorTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    colorTexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    colorTexDesc.MiscFlags = 0;

    hr = m_pd3dDevice->CreateTexture2D( &colorTexDesc, NULL, &m_pColorTexture2D );
    if ( FAILED(hr) ) { return hr; }

    hr = m_pd3dDevice->CreateShaderResourceView(m_pColorTexture2D, NULL, &m_pColorTextureRV);
    if ( FAILED(hr) ) { return hr; }

    // Setup the viewport
    D3D11_VIEWPORT vp;
    vp.Width = static_cast<FLOAT>(width);
    vp.Height = static_cast<FLOAT>(height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    m_pImmediateContext->RSSetViewports(1, &vp);

    // Initialize the projection matrix
    m_projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, width / static_cast<FLOAT>(height), 0.1f, 100.f);
    
    // Calculate correct XY scaling factor so that our vertices are correctly placed in the world
    // This helps us to unproject from the Kinect's depth camera back to a 3d world
    // Since the Horizontal and Vertical FOVs are proportional with the sensor's resolution along those axes
    // We only need to do this for horizontal
    // I.e. tan(horizontalFOV)/depthWidth == tan(verticalFOV)/depthHeight
    // Essentially we're computing the vector that light comes in on for a given pixel on the depth camera
    // We can then scale our x&y depth position by this and the depth to get how far along that vector we are
/*    const float DegreesToRadians = 3.14159265359f / 180.0f;
    m_xyScale = tanf(NUI_CAMERA_DEPTH_NOMINAL_HORIZONTAL_FOV * DegreesToRadians * 0.5f) / (m_depthWidth * 0.5f);  */  

    // Set rasterizer state to disable backface culling
    D3D11_RASTERIZER_DESC rasterDesc;
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_NONE;
    rasterDesc.FrontCounterClockwise = true;
    rasterDesc.DepthBias = false;
    rasterDesc.DepthBiasClamp = 0;
    rasterDesc.SlopeScaledDepthBias = 0;
    rasterDesc.DepthClipEnable = true;
    rasterDesc.ScissorEnable = false;
    rasterDesc.MultisampleEnable = false;
    rasterDesc.AntialiasedLineEnable = false;
    
    ID3D11RasterizerState* pState = NULL;

    hr = m_pd3dDevice->CreateRasterizerState(&rasterDesc, &pState);
    if ( FAILED(hr) ) { return hr; }

    m_pImmediateContext->RSSetState(pState);

    SAFE_RELEASE(pState);

	g_Batch.reset(new PrimitiveBatch<VertexPositionColorTexture>(m_pImmediateContext));

	g_BatchEffect.reset(new BasicEffect(m_pd3dDevice));
	g_BatchEffect->SetVertexColorEnabled(true);	
	g_BatchEffect->SetTextureEnabled(true);

	void const* shaderByteCode;
	size_t byteCodeLength;

	g_BatchEffect->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);


	hr = m_pd3dDevice->CreateInputLayout(VertexPositionColorTexture::InputElements,
		VertexPositionColorTexture::InputElementCount,
		shaderByteCode, byteCodeLength,
		&m_pVertexLayout);
	if (FAILED(hr))
		return hr;

	m_pImmediateContext->IASetInputLayout(m_pVertexLayout);


	g_Box = GeometricPrimitive::CreateCube(m_pImmediateContext, 2.0f, false);
	g_Sphere = GeometricPrimitive::CreateSphere(m_pImmediateContext, 1.0f);

	g_BatchEffect->SetProjection(m_projection);


    return S_OK;
}

/// <summary>
/// Process color data received from Kinect
/// </summary>
/// <returns>S_OK for success, or failure code</returns>
HRESULT CDepthWithColorD3D::ProcessColor(int nBufferSize)
{
	HRESULT hr;
	D3D11_MAPPED_SUBRESOURCE msT;
	hr = m_pImmediateContext->Map(m_pColorTexture2D, NULL, D3D11_MAP_WRITE_DISCARD, NULL, &msT);
	if (FAILED(hr)) { return hr; }

	memcpy(msT.pData, m_pColorRGBX, nBufferSize);
	m_pImmediateContext->Unmap(m_pColorTexture2D, NULL);
	
    return hr;
}

void CDepthWithColorD3D::ProcessFrame(INT64 nTime,
	const UINT16* pDepthBuffer, int nDepthWidth, int nDepthHeight,
	const RGBQUAD* pColorBuffer, int nColorWidth, int nColorHeight,
	const BYTE* pBodyIndexBuffer, int nBodyIndexWidth, int nBodyIndexHeight, Mat&u, Mat&v)
{

	//BOOLEAN firstParticle = false;
	//if (proxyParticle.empty())
	//{
	//	firstParticle = true;
	//}


	if (m_pCoordinateMapper && m_pDepthCoordinates && m_pOutputRGBX &&
		pDepthBuffer && (nDepthWidth == cDepthWidth) && (nDepthHeight == cDepthHeight) &&
		pColorBuffer && (nColorWidth == cColorWidth) && (nColorHeight == cColorHeight) &&
		pBodyIndexBuffer && (nBodyIndexWidth == cDepthWidth) && (nBodyIndexHeight == cDepthHeight))
	{
		HRESULT hr = m_pCoordinateMapper->MapColorFrameToDepthSpace(nDepthWidth * nDepthHeight, (UINT16*)pDepthBuffer, nColorWidth * nColorHeight, m_pDepthCoordinates);
		if (SUCCEEDED(hr))
		{



			if (!u.empty() && !v.empty())
			{
				vector<Particle>::iterator it = proxyParticle.begin();
				vector<PxRigidActor*>::iterator itact = proxyParticleActor.begin();

				for (int i = 0; i < boxes.size(); i++)
				{
					((PxRigidDynamic*)boxes[i])->wakeUp();
				}

				for (; it != proxyParticle.end();)
				{

						int x_small = (*it).x / 3.0f;
						int y_small = (*it).y / 3.0f;

						if (x_small >= 0 && x_small < 640 && y_small >= 0 && y_small < 360)
						{
							const float* ptr_u = u.ptr<float>(y_small);
							const float* ptr_v = v.ptr<float>(y_small);
							float dx = ptr_u[x_small];
							float dy = ptr_v[x_small];
							(*it).x += 3 * dx;
							(*it).y += 3 * dy;


							int x = (*it).x;
							int y = (*it).y;

							float x_r = -2.0f + 4.0f* (*it).x / (float)cColorWidth;
							float y_r = 2.0f * (-(*it).y / (float)cColorHeight) + 1.0f;

							x_r *= 10.0f;
							y_r *= 10.0f;

							int index = y*nColorWidth + x;
							if (x < 0 || x >= 1920 || y < 0 || y >= 1080)
							{
								it = proxyParticle.erase(it);
								(*itact)->release();
								itact = proxyParticleActor.erase(itact);
							}
							else
							{
								DepthSpacePoint p = m_pDepthCoordinates[index];

								//	 Values that are negative infinity means it is an invalid color to depth mapping so we
								//	 skip processing for this pixel
								if (p.X != -std::numeric_limits<float>::infinity() && p.Y != -std::numeric_limits<float>::infinity())
								{
									int depthX = static_cast<int>(p.X + 0.5f);
									int depthY = static_cast<int>(p.Y + 0.5f);

									if ((depthX >= 0 && depthX < nDepthWidth) && (depthY >= 0 && depthY < nDepthHeight))
									{
										//if the position is valid update the depth of the particle.
										BYTE player = pBodyIndexBuffer[depthX + (depthY * cDepthWidth)];
										UINT16 depth = pDepthBuffer[depthX + depthY*cDepthWidth];

										if (player != 0xff && depth > 500 && depth < 2000)
										{
											float depthM = (float)(depth - 500) / 50.0;
											(*it).Depth = depthM;

											PxTransform transform_update(PxVec3(x_r, y_r, depthM), PxQuat::createIdentity());
											((PxRigidDynamic*)(*itact))->setKinematicTarget(transform_update);
											++it;
											++itact;

											circle(referenceFrame, Point(x, y), 20, Scalar(255), -1);

										}
										else
										{
											it = proxyParticle.erase(it);
											(*itact)->release();
											itact = proxyParticleActor.erase(itact);
										}

									}
									else
									{
										it = proxyParticle.erase(it);
										(*itact)->release();
										itact = proxyParticleActor.erase(itact);
									}

								}
								else
								{
									it = proxyParticle.erase(it);
									(*itact)->release();
									itact = proxyParticleActor.erase(itact);
								}
							}
					}
				}

			}

			// loop over output pixels
			for (int colorIndex = 0; colorIndex < (nColorWidth*nColorHeight); ++colorIndex)
			{
				// default setting source to copy from the background pixel
				const RGBQUAD* pSrc = m_pBackgroundRGBX + colorIndex;

				DepthSpacePoint p = m_pDepthCoordinates[colorIndex];

				// Values that are negative infinity means it is an invalid color to depth mapping so we
				// skip processing for this pixel
				if (p.X != -std::numeric_limits<float>::infinity() && p.Y != -std::numeric_limits<float>::infinity())
				{
					int depthX = static_cast<int>(p.X + 0.5f);
					int depthY = static_cast<int>(p.Y + 0.5f);

					if ((depthX >= 0 && depthX < nDepthWidth) && (depthY >= 0 && depthY < nDepthHeight))
					{
						BYTE player = pBodyIndexBuffer[depthX + (depthY * cDepthWidth)];
						UINT16 depth = pDepthBuffer[depthX + depthY*cDepthWidth];

						// if we're tracking a player for the current pixel, draw from the color camera
						if (player != 0xff )
						{
							// set source for copy to the color pixels
							pSrc = m_pColorRGBX + colorIndex;
							int x = colorIndex % cColorWidth;
							int y = colorIndex / cColorWidth;
							if (x % 5 == 0 && y % 5 == 0)
							{
								if (referenceFrame.at<UCHAR>(y,x) == 0)
								{
									float depthM = (float)(depth-500)/50.0 ;
									Particle newParticle = { x, y, depthM };

		

									float x_r = -2.0f + 4.0f*x / (float)cColorWidth;
									float y_r = 2.0f * (-y / (float)cColorHeight) + 1.0f;

									x_r *= 10.0f;
									y_r *= 10.0f;

									PxOverlapBuffer hit;
									//PxGeometry overlapshape = PxSphereGeometry(0.5f);
									PxTransform shapePose = PxTransform(PxVec3(x_r, y_r, depthM));
									PxQueryFilterData fd;
									fd.flags |= PxQueryFlag::eANY_HIT;
																	
									bool status = gScene->overlap(PxSphereGeometry(0.5f), shapePose, hit, fd);
									if (!status)
									{
										proxyParticle.push_back(newParticle);
										PxRigidDynamic* newParticleActor = CreateSphere(PxVec3(x_r, y_r, depthM), 0.5f, 1.0f);
										newParticleActor->setRigidDynamicFlag(PxRigidDynamicFlag::eKINEMATIC, true);
										proxyParticleActor.push_back(newParticleActor);
									}
								}
							}
							
						}
					}
				}

				// write output
				m_pOutputRGBX[colorIndex] = *pSrc;
			}
		}
	}
}

BOOLEAN CDepthWithColorD3D::ProcessFaces(IMultiSourceFrame* pMultiFrame)
{
	HRESULT hr;
	IBody* ppBodies[BODY_COUNT] = { 0 };
	bool bHaveBodyData = SUCCEEDED(UpdateBodyData(ppBodies,pMultiFrame));
	bool bOneFaceTraked = false;
	// iterate through each face reader
	for (int iFace = 0; iFace < BODY_COUNT; ++iFace)
	{
		// retrieve the latest face frame from this reader
		IFaceFrame* pFaceFrame = nullptr;
		hr = m_pFaceFrameReaders[iFace]->AcquireLatestFrame(&pFaceFrame);

		BOOLEAN bFaceTracked = false;
		if (SUCCEEDED(hr) && nullptr != pFaceFrame)
		{
			// check if a valid face is tracked in this face frame
			hr = pFaceFrame->get_IsTrackingIdValid(&bFaceTracked);
			if (bFaceTracked)
			{
				bOneFaceTraked = true;
			}
			
		}

		if (SUCCEEDED(hr))
		{
			if (bFaceTracked)
			{
				hr = pFaceFrame->get_FaceFrameResult(&pFaceFrameResult);
			}
			else
			{
				// face tracking is not valid - attempt to fix the issue
				// a valid body is required to perform this step
				if (bHaveBodyData)
				{
					// check if the corresponding body is tracked 
					// if this is true then update the face frame source to track this body
					IBody* pBody = ppBodies[iFace];
					if (pBody != nullptr)
					{
						BOOLEAN bTracked = false;
						hr = pBody->get_IsTracked(&bTracked);

						UINT64 bodyTId;
						if (SUCCEEDED(hr) && bTracked)
						{
							// get the tracking ID of this body
							hr = pBody->get_TrackingId(&bodyTId);
							if (SUCCEEDED(hr))
							{
								// update the face frame source with the tracking ID
								m_pFaceFrameSources[iFace]->put_TrackingId(bodyTId);
							}
						}
					}
				}
			}
		}

		SafeRelease(pFaceFrame);
	}

	if (bHaveBodyData)
	{
		for (int i = 0; i < _countof(ppBodies); ++i)
		{
			SafeRelease(ppBodies[i]);
		}
	}
	return bOneFaceTraked;
}


PxRigidDynamic* CDepthWithColorD3D::CreateSphere(const PxVec3& pos, const PxReal radius, const PxReal density)
{
	PxTransform transform(pos, PxQuat::createIdentity());
	PxSphereGeometry geometry(radius);

	PxMaterial* mMaterial = gPhysicsSDK->createMaterial(0.5, 0.5, 0.5);

	PxRigidDynamic* actor = PxCreateDynamic(*gPhysicsSDK, transform, geometry, *mMaterial, density);
	
	if (!actor)
		cerr << "create actor failed" << endl;
	actor->setAngularDamping(0.75);
	actor->setLinearVelocity(PxVec3(0, 0, 0));
	gScene->addActor(*actor);
	return actor;
}

/// <summary>
/// Renders a frame
/// </summary>
/// <returns>S_OK for success, or failure code</returns>
HRESULT CDepthWithColorD3D::Render()
{

	float ClearColor[4] = { 0.0f, 0.0f, 0.5f, 1.0f };
	bool gotColor = false;
	bool gotFace = false;
    if (m_bPaused)
    {
        return S_OK;
    }

	if (m_hWnd)
	{
		double fps = 0.0;

		LARGE_INTEGER qpcNow = { 0 };
		if (m_fFreq)
		{
			if (QueryPerformanceCounter(&qpcNow))
			{
				if (m_nLastCounter)
				{
					m_nFramesSinceUpdate++;
					fps = m_fFreq * m_nFramesSinceUpdate / double(qpcNow.QuadPart - m_nLastCounter);
					if (fps > 10)
						myTimestep = 1.0 / fps;
					else
						myTimestep = 1.0 / 10.0;
				

				}
			}
		}
			m_nLastCounter = qpcNow.QuadPart;
			m_nFramesSinceUpdate = 0;
		
	}

	

#pragma region KinectRead
	if (!m_pMultiSourceFrameReader)
	{
		return E_FAIL;
	}
	IMultiSourceFrame* pMultiSourceFrame = NULL;
	IDepthFrame* pDepthFrame = NULL;
	IColorFrame* pColorFrame = NULL;
	IBodyIndexFrame* pBodyIndexFrame = NULL;

	HRESULT hr = m_pMultiSourceFrameReader->AcquireLatestFrame(&pMultiSourceFrame);

	if (SUCCEEDED(hr))
	{
		IDepthFrameReference* pDepthFrameReference = NULL;

		hr = pMultiSourceFrame->get_DepthFrameReference(&pDepthFrameReference);
		if (SUCCEEDED(hr))
		{
			hr = pDepthFrameReference->AcquireFrame(&pDepthFrame);
		}

		SafeRelease(pDepthFrameReference);
	}

	if (SUCCEEDED(hr))
	{
		IColorFrameReference* pColorFrameReference = NULL;

		hr = pMultiSourceFrame->get_ColorFrameReference(&pColorFrameReference);
		if (SUCCEEDED(hr))
		{
			hr = pColorFrameReference->AcquireFrame(&pColorFrame);
		}

		SafeRelease(pColorFrameReference);
	}

	if (SUCCEEDED(hr))
	{
		IBodyIndexFrameReference* pBodyIndexFrameReference = NULL;

		hr = pMultiSourceFrame->get_BodyIndexFrameReference(&pBodyIndexFrameReference);
		if (SUCCEEDED(hr))
		{
			hr = pBodyIndexFrameReference->AcquireFrame(&pBodyIndexFrame);
		}

		SafeRelease(pBodyIndexFrameReference);
	}

	if (SUCCEEDED(hr))
	{

		INT64 nDepthTime = 0;


		IFrameDescription* pDepthFrameDescription = NULL;
		int nDepthWidth = 0;
		int nDepthHeight = 0;
		UINT nDepthBufferSize = 0;
		UINT16 *pDepthBuffer = NULL;

		IFrameDescription* pColorFrameDescription = NULL;
		int nColorWidth = 0;
		int nColorHeight = 0;
		ColorImageFormat imageFormat = ColorImageFormat_None;
		UINT nColorBufferSize = 0;
		RGBQUAD *pColorBuffer = NULL;

		IFrameDescription* pBodyIndexFrameDescription = NULL;
		int nBodyIndexWidth = 0;
		int nBodyIndexHeight = 0;
		UINT nBodyIndexBufferSize = 0;
		BYTE *pBodyIndexBuffer = NULL;


		hr = pDepthFrame->get_RelativeTime(&nDepthTime);

		if (SUCCEEDED(hr))
		{
			hr = pDepthFrame->get_FrameDescription(&pDepthFrameDescription);
		}

		if (SUCCEEDED(hr))
		{
			hr = pDepthFrameDescription->get_Width(&nDepthWidth);
		}

		if (SUCCEEDED(hr))
		{
			hr = pDepthFrameDescription->get_Height(&nDepthHeight);
		}

		if (SUCCEEDED(hr))
		{

			hr = pDepthFrame->AccessUnderlyingBuffer(&nDepthBufferSize, &pDepthBuffer);
		}

		// get color frame data

		if (SUCCEEDED(hr))
		{
			hr = pColorFrame->get_FrameDescription(&pColorFrameDescription);
		}

		if (SUCCEEDED(hr))
		{
			hr = pColorFrameDescription->get_Width(&nColorWidth);
		}

		if (SUCCEEDED(hr))
		{
			hr = pColorFrameDescription->get_Height(&nColorHeight);
		}

		if (SUCCEEDED(hr))
		{
			hr = pColorFrame->get_RawColorImageFormat(&imageFormat);
		}

		if (SUCCEEDED(hr))
		{
			if (imageFormat == ColorImageFormat_Bgra)
			{
				hr = pColorFrame->AccessRawUnderlyingBuffer(&nColorBufferSize, reinterpret_cast<BYTE**>(&pColorBuffer));
			}
			else if (m_pColorRGBX)
			{
				pColorBuffer = m_pColorRGBX;
				nColorBufferSize = cColorWidth * cColorHeight * sizeof(RGBQUAD);
				hr = pColorFrame->CopyConvertedFrameDataToArray(nColorBufferSize, reinterpret_cast<BYTE*>(pColorBuffer), ColorImageFormat_Bgra);
			}
			else
			{
				hr = E_FAIL;
			}
		}

		if (SUCCEEDED(hr))
		{
			ProcessColor(nColorBufferSize);
			//gotFace = ProcessFaces(pMultiSourceFrame);
		}

		// get body index frame data

		if (SUCCEEDED(hr))
		{
			hr = pBodyIndexFrame->get_FrameDescription(&pBodyIndexFrameDescription);
		}

		if (SUCCEEDED(hr))
		{
			hr = pBodyIndexFrameDescription->get_Width(&nBodyIndexWidth);
		}

		if (SUCCEEDED(hr))
		{
			hr = pBodyIndexFrameDescription->get_Height(&nBodyIndexHeight);
		}

		if (SUCCEEDED(hr))
		{

			hr = pBodyIndexFrame->AccessUnderlyingBuffer(&nBodyIndexBufferSize, &pBodyIndexBuffer);

		}

		if (SUCCEEDED(hr))
		{
			ProcessFrame(nDepthTime, pDepthBuffer, nDepthWidth, nDepthHeight,
				pColorBuffer, nColorWidth, nColorHeight,
				pBodyIndexBuffer, nBodyIndexWidth, nBodyIndexHeight, Last_u, Last_v);
		}


		SafeRelease(pDepthFrameDescription);
		SafeRelease(pColorFrameDescription);
		SafeRelease(pBodyIndexFrameDescription);
	}

	SafeRelease(pDepthFrame);
	SafeRelease(pColorFrame);
	SafeRelease(pBodyIndexFrame);
	SafeRelease(pMultiSourceFrame);
#pragma endregion
	
	////Particle flow computations
	if (SUCCEEDED(hr))
	{
		referenceFrame = Mat::zeros(1080, 1920, CV_8UC1);
		Mat m_colorMat(cColorHeight, cColorWidth, CV_8UC4, reinterpret_cast<void*>(m_pOutputRGBX));
		Mat m_color_small, m_color_float, frame1Gray;
		resize(m_colorMat, m_color_small, Size(640, 360));
		m_color_small.convertTo(m_color_float, CV_32F, 1.0 / 255.0);

		cvtColor(m_color_float, frame1Gray, COLOR_BGR2GRAY);

		if (!LastGrayFrame.empty())
		{

			//imshow("Last Gray Frame", LastGrayFrame);
			//imshow("Next Gray Frame", frame1Gray);
			GpuMat d_frame0(LastGrayFrame);
			GpuMat d_frame1(frame1Gray);

			BroxOpticalFlow d_flow(0.197, 50.0, 0.75, 1, 14, 10);

			GpuMat d_fu, d_fv;

			d_flow(d_frame0, d_frame1, d_fu, d_fv);

			Mat flowFieldForward = m_color_small.clone();
			getFlowField(Mat(d_fu), Mat(d_fv), flowFieldForward);

			Last_u = Mat(d_fu).clone();
			Last_v = Mat(d_fv).clone();


			//UpdateParticle(Mat(d_fu), Mat(d_fv));
			//if (gotFace)
			//{

			//	RectI faceBox = { 0 };
			//	PointF facePoints[FacePointType::FacePointType_Count];
			//	_Vector4 faceRotation;
			//	DetectionResult faceProperties[FaceProperty::FaceProperty_Count];

			//	if (pFaceFrameResult != nullptr)
			//	{
			//		hr = pFaceFrameResult->get_FaceBoundingBoxInColorSpace(&faceBox);

			//		if (SUCCEEDED(hr))
			//		{
			//			hr = pFaceFrameResult->GetFacePointsInColorSpace(FacePointType::FacePointType_Count, facePoints);
			//		}

			//		if (SUCCEEDED(hr))
			//		{
			//			hr = pFaceFrameResult->get_FaceRotationQuaternion(&faceRotation);
			//		}

			//		if (SUCCEEDED(hr))
			//		{
			//			hr = pFaceFrameResult->GetFaceProperties(FaceProperty::FaceProperty_Count, faceProperties);
			//		}
			//		if (SUCCEEDED(hr))
			//		{
			//			rectangle(flowFieldForward, Point(faceBox.Left / 3, faceBox.Top / 3),
			//				Point(faceBox.Right / 3, faceBox.Bottom / 3),CV_RGB(255,0,0));
			//			for (int i = 0; i < FacePointType::FacePointType_Count; i++)
			//			{
			//				circle(flowFieldForward, Point(facePoints[i].X / 3, facePoints[i].Y / 3), 2, CV_RGB(255, 0, 0));
			//			}
			//		}
			//	}
			//}
			imshow("flowfield", flowFieldForward);


		}
		LastGrayFrame = frame1Gray.clone();
		//imshow("reference", referenceFrame);
	}
	if (gScene && SUCCEEDED(hr))
	{
		StepPhysX();
	}

	// Clear the back buffer
	m_pImmediateContext->OMSetRenderTargets(1, &m_pRenderTargetView, m_pDepthStencilView);
    m_pImmediateContext->ClearRenderTargetView(m_pRenderTargetView, ClearColor);

    // Clear the depth buffer to 1.0 (max depth)
    m_pImmediateContext->ClearDepthStencilView(m_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);


	D3D11_VIEWPORT vp;
	vp.Width = static_cast<FLOAT>(m_windowResX );
	vp.Height = static_cast<FLOAT>(m_windowResY);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	m_pImmediateContext->RSSetViewports(1, &vp);


    // Update the view matrix
    m_camera.Update();

	g_BatchEffect->SetView(m_camera.View);
	g_BatchEffect->SetProjection(m_projection);
	//g_BatchEffect->SetTexture(m_pColorTextureRV);
	g_BatchEffect->Apply(m_pImmediateContext);
	m_pImmediateContext->IASetInputLayout(m_pVertexLayout);

	/*g_Batch->Begin();
	VertexPositionColorTexture v1(Vector3(-1.6f, 0.9f, 0.0f), Colors::White, Vector2(0.0f, 0.0f));
	VertexPositionColorTexture v2(Vector3(1.6f, 0.9f, 0.0f), Colors::White, Vector2(1.0f, 0.0f));
	VertexPositionColorTexture v3(Vector3(1.6f, -0.9f, 0.0f), Colors::White, Vector2(1.0f, 1.0f));
	VertexPositionColorTexture v4(Vector3(-1.6f, -0.9f, 0.0f), Colors::White, Vector2(0.0f, 1.0f));
	g_Batch->DrawQuad(v1, v2, v3, v4);
	g_Batch->End();*/


	const XMVECTORF32 xaxis = { 20.0f, 0.f, 0.f };
	const XMVECTORF32 yaxis = { 0.f, 0.f, 20.f };
	DrawGrid(*g_Batch, xaxis, yaxis, g_XMZero, 20, 20, Colors::Gray);

	RenderActors();

	//if (!proxyParticle.empty())
	//RenderParticle();
    // Present our back buffer to our front buffer
    m_pSwapChain->Present(0, 0);

	return hr;

}

template <typename T> inline T clamp(T x, T a, T b)
{
	return ((x) > (a) ? ((x) < (b) ? (x) : (b)) : (a));
}

template <typename T> inline T mapValue(T x, T a, T b, T c, T d)
{
	x = clamp(x, a, b);
	return c + (d - c) * (x - a) / (b - a);
}

void CDepthWithColorD3D::getFlowField(const Mat& u, const Mat& v, Mat& flowField)
{
	float maxDisplacement = -1.0f;

	for (int i = 0; i < u.rows; i+=10)
	{
		const float* ptr_u = u.ptr<float>(i);
		const float* ptr_v = v.ptr<float>(i);

		for (int j = 0; j < u.cols; j+=10)
		{
			float d = sqrt(ptr_u[j] * ptr_u[j] + ptr_v[j] * ptr_v[j]); //max(fabsf(ptr_u[j]), fabsf(ptr_v[j]));

			if (d > maxDisplacement)
				maxDisplacement = d;
		}
	}

	//flowField = Mat::zeros(u.size(), CV_8UC3);


	for (int i = 0; i < flowField.rows; i+=10)
	{
		const float* ptr_u = u.ptr<float>(i);
		const float* ptr_v = v.ptr<float>(i);

		//Vec4b* row = flowField.ptr<Vec4b>(i);

		for (int j = 0; j < flowField.cols; j+=10)
		{
			//row[j][0] = 0;
			//row[j][1] = static_cast<unsigned char> (mapValue(-ptr_v[j], -maxDisplacement, maxDisplacement, 0.0f, 255.0f));
			//row[j][2] = static_cast<unsigned char> (mapValue(ptr_u[j], -maxDisplacement, maxDisplacement, 0.0f, 255.0f));
			//row[j][3] = 255;


			Point p = Point(j, i);
			float l = sqrt(ptr_u[j] * ptr_u[j] + ptr_v[j] * ptr_v[j]);
			float l_max = maxDisplacement;

			float dx = ptr_u[j];
			float dy = ptr_v[j];
			if (l > 0 && flowField.at<Vec4b>(i,j) != Vec4b(255,0,0))
			{
				double spinSize = 5.0 * l / l_max;  // Factor to normalise the size of the spin depeding on the length of the arrow

				Point p2 = Point(p.x + (int)(dx), p.y + (int)(dy));
				line(flowField, p, p2, CV_RGB(0,255,0), 1);

				double angle;               // Draws the spin of the arrow
				angle = atan2((double)p.y - p2.y, (double)p.x - p2.x);

				p.x = (int)(p2.x + spinSize * cos(angle + 3.1416 / 4));
				p.y = (int)(p2.y + spinSize * sin(angle + 3.1416 / 4));
				line(flowField, p, p2, CV_RGB(0, 255, 0), 1);

				p.x = (int)(p2.x + spinSize * cos(angle - 3.1416 / 4));
				p.y = (int)(p2.y + spinSize * sin(angle - 3.1416 / 4));
				line(flowField, p, p2, CV_RGB(0, 255, 0), 1);

			}
		}
	}
		
}

void CDepthWithColorD3D::UpdateParticle(Mat&u, Mat&v)
{
	if (proxyParticle.empty())
	{
		return;
	}

	for (int i = 0; i < boxes.size(); i++)
	{
		((PxRigidDynamic*)boxes[i])->wakeUp();
	}

	for (int i = 0; i < proxyParticle.size(); i++)
	{
		int x = proxyParticle[i].x/3.0f;
		int y = proxyParticle[i].y/3.0f;

		if (x >=  0 && x < 640 && y >= 0 && y < 360)
		{
			const float* ptr_u = u.ptr<float>(y);
			const float* ptr_v = v.ptr<float>(y);
			float dx = ptr_u[x];
			float dy = ptr_v[x];
			proxyParticle[i].x += 3*dx;
			proxyParticle[i].y += 3*dy;


			float x_r = -2.0f + 4.0f*proxyParticle[i].x / (float)cColorWidth;
			float y_r = 2.0f * (-proxyParticle[i].y / (float)cColorHeight) + 1.0f;

			x_r *= 10.0f;
			y_r *= 10.0f;

			PxTransform transform = proxyParticleActor[i]->getGlobalPose();
			PxVec3 pos = transform.p;
			PxTransform transform_update(PxVec3(x_r, y_r, pos.z), PxQuat::createIdentity());
			((PxRigidDynamic*) proxyParticleActor[i])->setKinematicTarget(transform_update);

			//PxOverlapBuffer hit;
			//PxGeometry overlapshape = PxSphereGeometry(0.5f);
			//PxTransform shapePose = PxTransform(PxVec3(x_r, y_r, pos.z));
			//PxQueryFilterData fd;
			//fd.flags |= PxQueryFlag::eANY_HIT;
			//
			//
			//gScene->overlap(overlapshape, shapePose, hit,fd);



		}

		circle(referenceFrame, Point(proxyParticle[i].x, proxyParticle[i].y),20, Scalar(255),-1 );
		

	}
	//imshow("reference", referenceFrame);
}

HRESULT CDepthWithColorD3D::UpdateBodyData(IBody** ppBodies, IMultiSourceFrame* pMultiFrame)
{
	HRESULT hr = E_FAIL;

	if (pMultiFrame != nullptr)
	{
		IBodyFrame* pBodyFrame = nullptr;
		IBodyFrameReference* pBodyFrameReference = nullptr;
		hr = pMultiFrame->get_BodyFrameReference(&pBodyFrameReference);
		if (SUCCEEDED(hr))
		{
			hr = pBodyFrameReference->AcquireFrame(&pBodyFrame);
			if (SUCCEEDED(hr))
			{
				hr = pBodyFrame->GetAndRefreshBodyData(BODY_COUNT, ppBodies);
			}
		}

		SafeRelease(pBodyFrame);
	}

	return hr;
}

void CDepthWithColorD3D::InitializePhysX() 
{

	gFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, gDefaultAllocatorCallback, gDefaultErrorCallback);

	gPhysicsSDK = PxCreatePhysics(PX_PHYSICS_VERSION, *gFoundation, PxTolerancesScale());
	if (gPhysicsSDK == NULL) {
		cerr << "Error creating PhysX3 device." << endl;
		cerr << "Exiting..." << endl;
		exit(1);
	}

	if (!PxInitExtensions(*gPhysicsSDK))
		cerr << "PxInitExtensions failed!" << endl;

	if (gPhysicsSDK->getPvdConnectionManager() == NULL)
		return;
	const char* pvd_host_ip = "127.0.0.1";
	int port = 5425;
	unsigned int timeout = 100;
	
	
	//--- Debugger
	PxVisualDebuggerConnectionFlags connectionFlags = PxVisualDebuggerExt::getAllConnectionFlags();
	theConnection = PxVisualDebuggerExt::createConnection(gPhysicsSDK->getPvdConnectionManager(),
		pvd_host_ip, port, timeout, connectionFlags);


	//Create the scene
	PxSceneDesc sceneDesc(gPhysicsSDK->getTolerancesScale());
	sceneDesc.gravity = PxVec3(0.0f, -9.8f, 0.0f);
	sceneDesc.flags |= PxSceneFlag::eENABLE_CCD;

	if (!sceneDesc.cpuDispatcher) {
		PxDefaultCpuDispatcher* mCpuDispatcher = PxDefaultCpuDispatcherCreate(1);
		if (!mCpuDispatcher)
			cerr << "PxDefaultCpuDispatcherCreate failed!" << endl;
		sceneDesc.cpuDispatcher = mCpuDispatcher;
	}
	if (!sceneDesc.filterShader)
		sceneDesc.filterShader = gDefaultFilterShader;


	gScene = gPhysicsSDK->createScene(sceneDesc);
	if (!gScene)
		cerr << "createScene failed!" << endl;

	gScene->setVisualizationParameter(PxVisualizationParameter::eSCALE, 1.0);
	gScene->setVisualizationParameter(PxVisualizationParameter::eCOLLISION_SHAPES, 1.0f);


	PxMaterial* mMaterial = gPhysicsSDK->createMaterial(0.5, 0.5, 0.5);

	//Create actors 
	//1) Create ground plane
	PxReal d = 0.0f;
	PxTransform pose = PxTransform(PxVec3(0.0f, 0, 0.0f), PxQuat(PxHalfPi, PxVec3(0.0f, 0.0f, 1.0f)));

	PxRigidStatic* plane = gPhysicsSDK->createRigidStatic(pose);
	if (!plane)
		cerr << "create plane failed!" << endl;

	PxShape* shape = plane->createShape(PxPlaneGeometry(), *mMaterial);
	if (!shape)
		cerr << "create shape failed!" << endl;
	gScene->addActor(*plane);


	//2)           Create cube	 
	PxReal         density = 1.0f;
	PxTransform    transform(PxVec3(0.0f, 10.0f, 0.0f), PxQuat::createIdentity());
	PxVec3         dimensions(1.0f, 1.0f, 1.0f);
	PxBoxGeometry  geometry(dimensions);

	for (int i = 0; i < 10; i++)
	{
		transform.p = PxVec3(0.0f, 5.0f + 5 * i, 3.0f);
		PxRigidDynamic *actor = PxCreateDynamic(*gPhysicsSDK, transform, geometry, *mMaterial, density);

		actor->setAngularDamping(0.75);
		actor->setLinearVelocity(PxVec3(0, 0, 0));
		if (!actor)
			cerr << "create actor failed!" << endl;
		gScene->addActor(*actor);
		boxes.push_back(actor);
	}


}

void CDepthWithColorD3D::DrawGrid(PrimitiveBatch<VertexPositionColorTexture>& batch, FXMVECTOR xAxis, FXMVECTOR yAxis, FXMVECTOR origin, size_t xdivs, size_t ydivs, GXMVECTOR color)
{
	g_BatchEffect->Apply(m_pImmediateContext);

	m_pImmediateContext->IASetInputLayout(m_pVertexLayout);

	g_Batch->Begin();

	xdivs = std::max<size_t>(1, xdivs);
	ydivs = std::max<size_t>(1, ydivs);

	for (size_t i = 0; i <= xdivs; ++i)
	{
		float fPercent = float(i) / float(xdivs);
		fPercent = (fPercent * 2.0f) - 1.0f;
		XMVECTOR vScale = XMVectorScale(xAxis, fPercent);
		vScale = XMVectorAdd(vScale, origin);

		VertexPositionColorTexture v1(XMVectorSubtract(vScale, yAxis), color,Vector2(0,0));
		VertexPositionColorTexture v2(XMVectorAdd(vScale, yAxis), color, Vector2(0, 0));
		batch.DrawLine(v1, v2);
	}

	for (size_t i = 0; i <= ydivs; i++)
	{
		FLOAT fPercent = float(i) / float(ydivs);
		fPercent = (fPercent * 2.0f) - 1.0f;
		XMVECTOR vScale = XMVectorScale(yAxis, fPercent);
		vScale = XMVectorAdd(vScale, origin);

		VertexPositionColorTexture v1(XMVectorSubtract(vScale, xAxis), color, Vector2(0, 0));
		VertexPositionColorTexture v2(XMVectorAdd(vScale, xAxis), color, Vector2(0, 0));
		batch.DrawLine(v1, v2);
	}

	g_Batch->End();
}

void CDepthWithColorD3D::StepPhysX()
{
	gScene->simulate(myTimestep);

	while (!gScene->fetchResults())
	{

	}
}

void CDepthWithColorD3D::ShutdownPhysX() {

	if (gScene != NULL)
	{
		for (int i = 0; i < boxes.size(); i++)
			gScene->removeActor(*boxes[i]);

		gScene->release();

		for (int i = 0; i < boxes.size(); i++)
			boxes[i]->release();
	}
	if (gPhysicsSDK != NULL)
		gPhysicsSDK->release();

	//if (theConnection!=NULL)
	//	theConnection->release();
}

XMMATRIX CDepthWithColorD3D::PxtoXMMatrix(PxTransform input)
{
	PxMat33 quat = PxMat33(input.q);
	XMFLOAT4X4 start;
	XMMATRIX mat;
	start._11 = quat.column0[0];
	start._12 = quat.column0[1];
	start._13 = quat.column0[2];
	start._14 = 0;


	start._21 = quat.column1[0];
	start._22 = quat.column1[1];
	start._23 = quat.column1[2];
	start._24 = 0;

	start._31 = quat.column2[0];
	start._32 = quat.column2[1];
	start._33 = quat.column2[2];
	start._34 = 0;

	start._41 = input.p.x;
	start._42 = input.p.y;
	start._43 = input.p.z;
	start._44 = 1;

	mat = XMLoadFloat4x4(&start);
	return mat;
}

void CDepthWithColorD3D::DrawBox(PxShape* pShape, PxRigidActor* actor)
{
	PxTransform pT = PxShapeExt::getGlobalPose(*pShape, *actor);
	PxBoxGeometry bg;
	pShape->getBoxGeometry(bg);
	XMMATRIX mat = PxtoXMMatrix(pT);
	g_Box->Draw(mat, m_camera.View, m_projection, Colors::Gray);
}

void CDepthWithColorD3D::DrawSphere(PxShape* pShape, PxRigidActor* actor)
{
	PxTransform pT = PxShapeExt::getGlobalPose(*pShape, *actor);
	PxSphereGeometry bg;
	pShape->getSphereGeometry(bg);
	XMMATRIX mat = PxtoXMMatrix(pT);
	g_Sphere->Draw(mat, m_camera.View, m_projection, Colors::Red);
}

void CDepthWithColorD3D::DrawShape(PxShape* shape, PxRigidActor* actor)
{
	PxGeometryType::Enum type = shape->getGeometryType();
	switch (type)
	{
	case PxGeometryType::eBOX:
		DrawBox(shape, actor);
		break;
	case PxGeometryType::eSPHERE:
		DrawSphere(shape, actor);
		break;
	}
}

void CDepthWithColorD3D::DrawActor(PxRigidActor* actor)
{
	PxU32 nShapes = actor->getNbShapes();
	PxShape** shapes = new PxShape*[nShapes];

	actor->getShapes(shapes, nShapes);
	while (nShapes--)
	{
		DrawShape(shapes[nShapes], actor);
	}
	delete[] shapes;
}

void CDepthWithColorD3D::RenderActors()
{

	for (int i = 0; i < boxes.size(); i++)
		DrawActor(boxes[i]);

	for (int i = 0; i < proxyParticleActor.size(); i++)
		DrawActor(proxyParticleActor[i]);
}
