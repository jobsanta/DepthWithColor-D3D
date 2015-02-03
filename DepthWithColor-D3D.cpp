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

    NuiImageResolutionToSize(cDepthResolution, width, height);
    m_depthWidth  = static_cast<LONG>(width);
    m_depthHeight = static_cast<LONG>(height);

    NuiImageResolutionToSize(cColorResolution, width, height);
    m_colorWidth  = static_cast<LONG>(width);
    m_colorHeight = static_cast<LONG>(height);

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

    m_hNextDepthFrameEvent = INVALID_HANDLE_VALUE;
    m_pDepthStreamHandle = INVALID_HANDLE_VALUE;
    m_hNextColorFrameEvent = INVALID_HANDLE_VALUE;
    m_pColorStreamHandle = INVALID_HANDLE_VALUE;
	m_hNextSkeletonEvent = INVALID_HANDLE_VALUE;
	m_pSkeletonStreamhandle = INVALID_HANDLE_VALUE;



    m_depthD16 = new USHORT[m_depthWidth*m_depthHeight];
    m_colorCoordinates = new LONG[m_depthWidth*m_depthHeight*2];
    m_colorRGBX = new BYTE[m_colorWidth*m_colorHeight*cBytesPerPixel];

    m_bNearMode = false;

    m_bPaused = false;

	m_pFaceTracker = NULL;
	m_pFTResult = NULL;
	m_LastTrackSucceeded = false;
	m_XCenterFace = 0;
	m_YCenterFace = 0;

	ftRect[0] = ftRect[1] = ftRect[2] = ftRect[3] = 0.0f;
	faceTranslation[0] = faceTranslation[1] = faceTranslation[2] = -1.0f;

}

/// <summary>
/// Destructor
/// </summary>
CDepthWithColorD3D::~CDepthWithColorD3D()
{
    if (NULL != m_pNuiSensor)
    {
        m_pNuiSensor->NuiShutdown();
        m_pNuiSensor->Release();
    }

    if (m_pImmediateContext) 
    {
        m_pImmediateContext->ClearState();
    }
    
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

	SAFE_RELEASE(m_pFaceTracker);
	SAFE_RELEASE(m_pFTResult);

    CloseHandle(m_hNextDepthFrameEvent);
    CloseHandle(m_hNextColorFrameEvent);
	CloseHandle(m_hNextSkeletonEvent);

    // done with pixel data
    delete[] m_colorRGBX;
    delete[] m_colorCoordinates;
    delete[] m_depthD16;
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


	m_hWnd_user_view = CreateWindow(L"DepthWithColorD3DWindowClass", L"User View", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hInstance,
		NULL);
	if (NULL == m_hWnd_user_view)
	{
		return E_FAIL;
	}

	ShowWindow(m_hWnd_user_view, nCmdShow);


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

            if (nKey == 'N')
            {
                ToggleNearMode();
            }
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
    INuiSensor * pNuiSensor = NULL;
    HRESULT hr;

    int iSensorCount = 0;
    hr = NuiGetSensorCount(&iSensorCount);
    if (FAILED(hr) ) { return hr; }

    // Look at each Kinect sensor
    for (int i = 0; i < iSensorCount; ++i)
    {
        // Create the sensor so we can check status, if we can't create it, move on to the next
        hr = NuiCreateSensorByIndex(i, &pNuiSensor);
        if (FAILED(hr))
        {
            continue;
        }

        // Get the status of the sensor, and if connected, then we can initialize it
        hr = pNuiSensor->NuiStatus();
        if (S_OK == hr)
        {
            m_pNuiSensor = pNuiSensor;
            break;
        }

        // This sensor wasn't OK, so release it since we're not using it
        pNuiSensor->Release();
    }

    if (NULL == m_pNuiSensor)
    {
        return E_FAIL;
    }

    // Initialize the Kinect and specify that we'll be using depth and color
    hr = m_pNuiSensor->NuiInitialize(NUI_INITIALIZE_FLAG_USES_COLOR | NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX | NUI_INITIALIZE_FLAG_USES_SKELETON); 
    if (FAILED(hr) ) { return hr; }

    // Create an event that will be signaled when depth data is available
    m_hNextDepthFrameEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    // Open a depth image stream to receive depth frames
    hr = m_pNuiSensor->NuiImageStreamOpen(
        NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX,
        cDepthResolution,
        0,
        2,
        m_hNextDepthFrameEvent,
        &m_pDepthStreamHandle);
    if (FAILED(hr) ) { return hr; }

    // Create an event that will be signaled when color data is available
    m_hNextColorFrameEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    // Open a color image stream to receive color frames
    hr = m_pNuiSensor->NuiImageStreamOpen(
        NUI_IMAGE_TYPE_COLOR,
        cColorResolution,
        0,
        2,
        m_hNextColorFrameEvent,
        &m_pColorStreamHandle );
    if (FAILED(hr) ) { return hr; }

	m_hNextSkeletonEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
	DWORD dwSkeletonFlags = NUI_SKELETON_TRACKING_FLAG_ENABLE_IN_NEAR_RANGE	| NUI_SKELETON_TRACKING_FLAG_ENABLE_SEATED_SUPPORT;
	hr = m_pNuiSensor->NuiSkeletonTrackingEnable(m_hNextSkeletonEvent, dwSkeletonFlags);
	if (FAILED(hr)) { return hr; }

    // Start with near mode on
    ToggleNearMode();

	FT_CAMERA_CONFIG videoConfig;
	FT_CAMERA_CONFIG depthConfig;
	FT_CAMERA_CONFIG* pDepthConfig = NULL;


	videoConfig.FocalLength = NUI_CAMERA_COLOR_NOMINAL_FOCAL_LENGTH_IN_PIXELS;
	videoConfig.Height = m_colorHeight;
	videoConfig.Width = m_colorWidth;

	depthConfig.FocalLength = NUI_CAMERA_DEPTH_NOMINAL_FOCAL_LENGTH_IN_PIXELS *2.0f;
	depthConfig.Width = m_depthWidth;
	depthConfig.Height = m_depthHeight; 
	pDepthConfig = &depthConfig;

	m_pFaceTracker = FTCreateFaceTracker(_opt);
	if (!m_pFaceTracker)
	{
		MessageBoxW(m_hWnd, L"Could not create the face tracker.\n", L"Face Tracker Initialization Error\n", MB_OK);
		return 2;
	}
	

	hr = m_pFaceTracker->Initialize(&videoConfig, pDepthConfig, NULL, NULL);
	if (FAILED(hr))
	{
		MessageBoxW(m_hWnd, L"Could initialized the face tracker.\n", L"Face Tracker Initialization Error\n", MB_OK);
		return 2;
	}

	hr = m_pFaceTracker->CreateFTResult(&m_pFTResult);
	if (FAILED(hr) || !m_pFTResult)
	{
		MessageBoxW(m_hWnd, L"Could not initialize the face tracker result.\n", L"Face Tracker Initialization Error\n", MB_OK);
		return 4;
	}

	m_colorImage = FTCreateImage();
	if (!m_colorImage || FAILED(hr = m_colorImage->Attach(m_colorWidth,m_colorHeight, m_colorRGBX, FTIMAGEFORMAT_UINT8_B8G8R8X8,m_colorWidth*4)))
	{
		return 5;
	}

	if (pDepthConfig)
	{
		m_depthImage = FTCreateImage();
		if (!m_depthImage || FAILED(hr = m_depthImage->Attach(m_depthWidth,m_colorHeight,m_depthD16, FTIMAGEFORMAT_UINT16_D13P3, m_depthWidth)))
		{
			return 6;
		}
	}


	SetCenterOfImage(NULL);
	m_LastTrackSucceeded = false;

	m_hint3D[0] = m_hint3D[1] = FT_VECTOR3D(0, 0, 0);

    return hr;
}

void CDepthWithColorD3D::SetCenterOfImage(IFTResult* pResult)
{
	float centerX = ((float)m_colorWidth) / 2.0f;
	float centerY = ((float)m_colorHeight) / 2.0f;
	if (pResult)
	{
		if (SUCCEEDED(pResult->GetStatus()))
		{
			RECT faceRect;
			pResult->GetFaceRect(&faceRect);
			centerX = (faceRect.left + faceRect.right) / 2.0f;
			centerY = (faceRect.top + faceRect.bottom) / 2.0f;
		}
		m_XCenterFace += 0.02f*(centerX - m_XCenterFace);
		m_YCenterFace += 0.02f*(centerY - m_YCenterFace);
	}
	else
	{
		m_XCenterFace = centerX;
		m_YCenterFace = centerY;
	}
}

/// <summary>
/// Toggles between near and default mode
/// Does nothing on a non-Kinect for Windows device
/// </summary>
/// <returns>S_OK for success, or failure code</returns>
HRESULT CDepthWithColorD3D::ToggleNearMode()
{
    HRESULT hr = E_FAIL;

    if ( m_pNuiSensor )
    {
        hr = m_pNuiSensor->NuiImageStreamSetImageFrameFlags(m_pDepthStreamHandle, m_bNearMode ? 0 : NUI_IMAGE_STREAM_FLAG_ENABLE_NEAR_MODE);

        if ( SUCCEEDED(hr) )
        {
            m_bNearMode = !m_bNearMode;
        }
    }

    return hr;
}

/// <summary>
/// Compile and set layout for shaders
/// </summary>
/// <returns>S_OK for success, or failure code</returns>
HRESULT CDepthWithColorD3D::LoadShaders()
{
    // Compile the geometry shader
    ID3D10Blob* pBlob = NULL;
	HRESULT hr;
    //HRESULT hr = CompileShaderFromFile(L"DepthWithColor-D3D.fx", "GS", "gs_4_0", &pBlob);
    //if ( FAILED(hr) ) { return hr; };

    //// Create the geometry shader
    //hr = m_pd3dDevice->CreateGeometryShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &m_pGeometryShader);
    //SAFE_RELEASE(pBlob);
    //if ( FAILED(hr) ) { return hr; }

    // Compile the pixel shader
    hr = CompileShaderFromFile(L"DepthWithColor-D3D.fx", "PS", "ps_4_0", &pBlob);
    if ( FAILED(hr) ) {
		return hr; }

    // Create the pixel shader
    hr = m_pd3dDevice->CreatePixelShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &m_pPixelShader);
    SAFE_RELEASE(pBlob);
    if ( FAILED(hr) ) { 
		return hr; }

    // Compile the vertex shader
    hr = CompileShaderFromFile(L"DepthWithColor-D3D.fx", "VS", "vs_4_0", &pBlob);
    if ( FAILED(hr) ) { 
		return hr; }

    // Create the vertex shader
    hr = m_pd3dDevice->CreateVertexShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &m_pVertexShader);
    if ( SUCCEEDED(hr) )
    {
        // Define the vertex input layout
        D3D11_INPUT_ELEMENT_DESC layout[] = { { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 } };

        // Create the vertex input layout
        hr = m_pd3dDevice->CreateInputLayout(layout, ARRAYSIZE(layout), pBlob->GetBufferPointer(), pBlob->GetBufferSize(), &m_pVertexLayout);
    }

    SAFE_RELEASE(pBlob);
    if ( FAILED(hr) ) {
		return hr; }

    // Set the input vertex layout
    // In this case we don't actually use it for anything
    // All the work is done in the geometry shader, but we need something here
    // We only need to set this once since we have only one vertex format
    m_pImmediateContext->IASetInputLayout(m_pVertexLayout);

    return hr;
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

	hr = m_pSwapChain_user->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
	if (FAILED(hr)) { return hr; }

	hr = m_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &m_pRenderTargetView_user);
	pBackBuffer->Release();
	if (FAILED(hr)) { return hr; }


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

    //hr = LoadShaders();

    //if ( FAILED(hr) )
    //{
    //    MessageBox(NULL, L"Could not load shaders.", L"Error", MB_ICONHAND | MB_OK);
    //    return hr;
    //}



    //// Create the vertex buffer
    //D3D11_BUFFER_DESC bd = {0};
    //bd.Usage = D3D11_USAGE_DEFAULT;
    //bd.ByteWidth = sizeof(short);
    //bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    //bd.CPUAccessFlags = 0;

    //hr = m_pd3dDevice->CreateBuffer(&bd, NULL, &m_pVertexBuffer);
    //if ( FAILED(hr) ) { return hr; }

    //// Set vertex buffer
    //UINT stride = 0;
    //UINT offset = 0;
    //m_pImmediateContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);

    //// Set primitive topology
    //m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
    //
    //// Create the constant buffers
    //bd.Usage = D3D11_USAGE_DEFAULT;
    //bd.ByteWidth = sizeof(CBChangesEveryFrame);
    //bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    //bd.CPUAccessFlags = 0;
    //hr = m_pd3dDevice->CreateBuffer(&bd, NULL, &m_pCBChangesEveryFrame);
    //if ( FAILED(hr) ) { return hr; }

    //// Create the sample state
    //D3D11_SAMPLER_DESC sampDesc;
    //ZeroMemory( &sampDesc, sizeof(sampDesc) );
    //sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    //sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    //sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    //sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    //sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    //sampDesc.MinLOD = 0;
    //sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    //hr = m_pd3dDevice->CreateSamplerState( &sampDesc, &m_pColorSampler );
    //if ( FAILED(hr) ) { return hr; }

    // Initialize the projection matrix
    m_projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, width / static_cast<FLOAT>(height), 0.1f, 100.f);
    
    // Calculate correct XY scaling factor so that our vertices are correctly placed in the world
    // This helps us to unproject from the Kinect's depth camera back to a 3d world
    // Since the Horizontal and Vertical FOVs are proportional with the sensor's resolution along those axes
    // We only need to do this for horizontal
    // I.e. tan(horizontalFOV)/depthWidth == tan(verticalFOV)/depthHeight
    // Essentially we're computing the vector that light comes in on for a given pixel on the depth camera
    // We can then scale our x&y depth position by this and the depth to get how far along that vector we are
    const float DegreesToRadians = 3.14159265359f / 180.0f;
    m_xyScale = tanf(NUI_CAMERA_DEPTH_NOMINAL_HORIZONTAL_FOV * DegreesToRadians * 0.5f) / (m_depthWidth * 0.5f);    

    //// Set rasterizer state to disable backface culling
    //D3D11_RASTERIZER_DESC rasterDesc;
    //rasterDesc.FillMode = D3D11_FILL_SOLID;
    //rasterDesc.CullMode = D3D11_CULL_NONE;
    //rasterDesc.FrontCounterClockwise = true;
    //rasterDesc.DepthBias = false;
    //rasterDesc.DepthBiasClamp = 0;
    //rasterDesc.SlopeScaledDepthBias = 0;
    //rasterDesc.DepthClipEnable = true;
    //rasterDesc.ScissorEnable = false;
    //rasterDesc.MultisampleEnable = false;
    //rasterDesc.AntialiasedLineEnable = false;
    //
    //ID3D11RasterizerState* pState = NULL;

    //hr = m_pd3dDevice->CreateRasterizerState(&rasterDesc, &pState);
    //if ( FAILED(hr) ) { return hr; }

    //m_pImmediateContext->RSSetState(pState);

    //SAFE_RELEASE(pState);

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


	g_Box = GeometricPrimitive::CreateCube(m_pImmediateContext, 1.0f, false);
	g_Sphere = GeometricPrimitive::CreateSphere(m_pImmediateContext, 0.03f);

	g_BatchEffect->SetProjection(m_projection);


    return S_OK;
}

/// <summary>
/// Process depth data received from Kinect
/// </summary>
/// <returns>S_OK for success, or failure code</returns>
HRESULT CDepthWithColorD3D::ProcessDepth()
{
    NUI_IMAGE_FRAME imageFrame;

    HRESULT hr = m_pNuiSensor->NuiImageStreamGetNextFrame(m_pDepthStreamHandle, 0, &imageFrame);
    if ( FAILED(hr) ) { return hr; }
   
    NUI_LOCKED_RECT LockedRect;
    hr = imageFrame.pFrameTexture->LockRect(0, &LockedRect, NULL, 0);
    if ( FAILED(hr) ) { return hr; }

    memcpy(m_depthD16, LockedRect.pBits, LockedRect.size);
    m_bDepthReceived = true;

    hr = imageFrame.pFrameTexture->UnlockRect(0);
    if ( FAILED(hr) ) { return hr; };

    hr = m_pNuiSensor->NuiImageStreamReleaseFrame(m_pDepthStreamHandle, &imageFrame);

    // copy to our d3d 11 depth texture
    D3D11_MAPPED_SUBRESOURCE msT;
    hr = m_pImmediateContext->Map(m_pDepthTexture2D, NULL, D3D11_MAP_WRITE_DISCARD, NULL, &msT);
    if ( FAILED(hr) ) { return hr; }

    memcpy(msT.pData, m_depthD16, LockedRect.size);    
    m_pImmediateContext->Unmap(m_pDepthTexture2D, NULL);

    return hr;
}

/// <summary>
/// Process color data received from Kinect
/// </summary>
/// <returns>S_OK for success, or failure code</returns>
HRESULT CDepthWithColorD3D::ProcessColor()
{
    NUI_IMAGE_FRAME imageFrame;

    HRESULT hr = m_pNuiSensor->NuiImageStreamGetNextFrame(m_pColorStreamHandle, 0, &imageFrame);
    if ( FAILED(hr) ) { return hr; }
  
    NUI_LOCKED_RECT LockedRect;
    hr = imageFrame.pFrameTexture->LockRect(0, &LockedRect, NULL, 0);
    if ( FAILED(hr) ) { return hr; }

    memcpy(m_colorRGBX, LockedRect.pBits, LockedRect.size);
    m_bColorReceived = true;

    hr = imageFrame.pFrameTexture->UnlockRect(0);
    if ( FAILED(hr) ) { return hr; };

    hr = m_pNuiSensor->NuiImageStreamReleaseFrame(m_pColorStreamHandle, &imageFrame);
    
    return hr;
}

HRESULT CDepthWithColorD3D::ProcessSkeleton()
{
	NUI_SKELETON_FRAME SkeletonFrame = { 0 };

	HRESULT hr = NuiSkeletonGetNextFrame(0, &SkeletonFrame);
	NUI_TRANSFORM_SMOOTH_PARAMETERS somewhatLatentParams =
	{ 0.5f, 0.1f, 0.5f, 0.1f, 0.1f };
	if (FAILED(hr))
	{
		return hr;
	}

	m_pNuiSensor->NuiTransformSmooth(&SkeletonFrame, &somewhatLatentParams);

	for (int i = 0; i < NUI_SKELETON_COUNT; i++)
	{
		if (SkeletonFrame.SkeletonData[i].eTrackingState == NUI_SKELETON_TRACKED &&
			NUI_SKELETON_POSITION_TRACKED == SkeletonFrame.SkeletonData[i].eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_HEAD] &&
			NUI_SKELETON_POSITION_TRACKED == SkeletonFrame.SkeletonData[i].eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_SHOULDER_CENTER])
		{
			m_SkeletonTracked[i] = true;
			m_HeadPoint[i].x = SkeletonFrame.SkeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_HEAD].x;
			m_HeadPoint[i].y = SkeletonFrame.SkeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_HEAD].y;
			m_HeadPoint[i].z = SkeletonFrame.SkeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_HEAD].z;
			m_NeckPoint[i].x = SkeletonFrame.SkeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].x;
			m_NeckPoint[i].y = SkeletonFrame.SkeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].y;
			m_NeckPoint[i].z = SkeletonFrame.SkeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].z;
		}
		else
		{
			m_HeadPoint[i] = m_NeckPoint[i] = FT_VECTOR3D(0, 0, 0);
			m_SkeletonTracked[i] = false;
		}
	}
}

HRESULT CDepthWithColorD3D::GetClosestHint(FT_VECTOR3D* pHint3D)
{
	int selectedSkeleton = -1;
	float smallestDistance = 0;

	if (!pHint3D)
	{
		return(E_POINTER);
	}

	if (pHint3D[1].x == 0 && pHint3D[1].y == 0 && pHint3D[1].z == 0)
	{
		// Get the skeleton closest to the camera
		for (int i = 0; i < NUI_SKELETON_COUNT; i++)
		{
			if (m_SkeletonTracked[i] && (smallestDistance == 0 || m_HeadPoint[i].z < smallestDistance))
			{
				smallestDistance = m_HeadPoint[i].z;
				selectedSkeleton = i;
			}
		}
	}
	else
	{   // Get the skeleton closest to the previous position
		for (int i = 0; i < NUI_SKELETON_COUNT; i++)
		{
			if (m_SkeletonTracked[i])
			{
				float d = abs(m_HeadPoint[i].x - pHint3D[1].x) +
					abs(m_HeadPoint[i].y - pHint3D[1].y) +
					abs(m_HeadPoint[i].z - pHint3D[1].z);
				if (smallestDistance == 0 || d < smallestDistance)
				{
					smallestDistance = d;
					selectedSkeleton = i;
				}
			}
		}
	}
	if (selectedSkeleton == -1)
	{
		return E_FAIL;
	}

	pHint3D[0] = m_NeckPoint[selectedSkeleton];
	pHint3D[1] = m_HeadPoint[selectedSkeleton];

	return S_OK;
}


/// <summary>
/// Process color data received from Kinect
/// </summary>
/// <returns>S_OK for success, or failure code</returns>
HRESULT CDepthWithColorD3D::MapColorToDepth()
{
    HRESULT hr;

    // Get of x, y coordinates for color in depth space
    // This will allow us to later compensate for the differences in location, angle, etc between the depth and color cameras
    m_pNuiSensor->NuiImageGetColorPixelCoordinateFrameFromDepthPixelFrameAtResolution(
        cColorResolution,
        cDepthResolution,
        m_depthWidth*m_depthHeight,
        m_depthD16,
        m_depthWidth*m_depthHeight*2,
        m_colorCoordinates
        );

    // copy to our d3d 11 color texture
    D3D11_MAPPED_SUBRESOURCE msT;
    hr = m_pImmediateContext->Map(m_pColorTexture2D, NULL, D3D11_MAP_WRITE_DISCARD, NULL, &msT);
    if ( FAILED(hr) ) { return hr; }
    
    // loop over each row and column of the color
    for (LONG y = 0; y < m_colorHeight; ++y)
    {
        LONG* pDest = (LONG*)((BYTE*)msT.pData + msT.RowPitch * y);
        for (LONG x = 0; x < m_colorWidth; ++x)
        {
            // calculate index into depth array
            int depthIndex = x/m_colorToDepthDivisor + y/m_colorToDepthDivisor * m_depthWidth;

            // retrieve the depth to color mapping for the current depth pixel
            LONG colorInDepthX = m_colorCoordinates[depthIndex * 2];
            LONG colorInDepthY = m_colorCoordinates[depthIndex * 2 + 1];

            // make sure the depth pixel maps to a valid point in color space
            if ( colorInDepthX >= 0 && colorInDepthX < m_colorWidth && colorInDepthY >= 0 && colorInDepthY < m_colorHeight )
            {
                // calculate index into color array
                LONG colorIndex = colorInDepthX + colorInDepthY * m_colorWidth;

                // set source for copy to the color pixel
                LONG* pSrc = (LONG *)m_colorRGBX + colorIndex;
                *pDest = *pSrc;
            }
            else
            {
                *pDest = 0;
            }

            pDest++;
        }
    }

    m_pImmediateContext->Unmap(m_pColorTexture2D, NULL);

    return hr;
}

void CDepthWithColorD3D::RenderParticle()
{
	for (int x = 0; x < m_depthWidth; x += 15)
	{
		for (int y = 0; y < m_depthHeight; y+=15)
		{
			int depthIndex = x + y * m_depthWidth;
			if ((m_depthD16[depthIndex] & 7) > 0)
			{
				float depth = (float)m_depthD16[depthIndex] / 8000.0f;
				if (depth > 0.8)
				{
					XMMATRIX mat = XMMatrixTranslation( -1 + 2* (float)x / (float)m_depthWidth,
						2*(-(float)y / (float)m_depthHeight)+1,
						depth);
					g_Sphere->Draw(mat, m_camera.View, m_projection, Colors::Red);
				}
			}
			

		}
	}
}

/// <summary>
/// Renders a frame
/// </summary>
/// <returns>S_OK for success, or failure code</returns>
HRESULT CDepthWithColorD3D::Render()
{
    if (m_bPaused)
    {
        return S_OK;
    }

    bool needToMapColorToDepth = false;
	bool gotHint = false;

    if ( WAIT_OBJECT_0 == WaitForSingleObject(m_hNextDepthFrameEvent, 0) )
    {
        // if we have received any valid new depth data we may need to draw
        if ( SUCCEEDED(ProcessDepth()) )
        {
            needToMapColorToDepth = true;
        }
    }

    if ( WAIT_OBJECT_0 == WaitForSingleObject(m_hNextColorFrameEvent, 0) )
    {
        // if we have received any valid new color data we may need to draw
        if ( SUCCEEDED(ProcessColor()) )
        {
            needToMapColorToDepth = true;
        }
    }
	if (WAIT_OBJECT_0 == WaitForSingleObject(m_hNextSkeletonEvent, 0))
	{
		if (SUCCEEDED(ProcessSkeleton()))
		{
			gotHint = true;
		}
	}


    // If we have not yet received any data for either color or depth since we started up, we shouldn't draw
    if (!m_bDepthReceived || !m_bColorReceived)
    {
        needToMapColorToDepth = false;
    }
	float ClearColor[4] = { 0.0f, 0.0f, 0.5f, 1.0f };
	float notColor[4] = { 0.0f, 0.5f, 0.0f, 1.0f };

    if (needToMapColorToDepth & gotHint)
    {
        MapColorToDepth();
		CheckCameraInput();
    }


	

    // Clear the back buffer
	m_pImmediateContext->OMSetRenderTargets(1, &m_pRenderTargetView, m_pDepthStencilView);
	if (m_bDepthReceived)
    m_pImmediateContext->ClearRenderTargetView(m_pRenderTargetView, ClearColor);
	else if (!m_bDepthReceived)
		m_pImmediateContext->ClearRenderTargetView(m_pRenderTargetView, notColor);

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
    
 //   // Update variables that change once per frame
 //   CBChangesEveryFrame cb;
 //   cb.View = XMMatrixTranspose(m_camera.View);
 //   cb.Projection = XMMatrixTranspose(m_projection);
 //   cb.XYScale = XMFLOAT4(m_xyScale, -m_xyScale, 0.f, 0.f); 
	//cb.Rectangle = XMFLOAT4(ftRect[0], ftRect[1], ftRect[2], ftRect[3]);
 //   m_pImmediateContext->UpdateSubresource(m_pCBChangesEveryFrame, 0, NULL, &cb, 0, 0);

 //   // Set up shaders
 //   m_pImmediateContext->VSSetShader(m_pVertexShader, NULL, 0);

 //   //m_pImmediateContext->GSSetShader(m_pGeometryShader, NULL, 0);
 //   //m_pImmediateContext->GSSetConstantBuffers(0, 1, &m_pCBChangesEveryFrame);
 //   //m_pImmediateContext->GSSetShaderResources(0, 1, &m_pDepthTextureRV);
 //   //m_pImmediateContext->GSSetShaderResources(1, 1, &m_pColorTextureRV);
 //   //m_pImmediateContext->GSSetSamplers(0, 1, &m_pColorSampler);

 //   m_pImmediateContext->PSSetShader(m_pPixelShader, NULL, 0);

	g_BatchEffect->SetView(m_camera.View);
	g_BatchEffect->SetProjection(m_projection);
	g_BatchEffect->SetTexture(m_pColorTextureRV);
	g_BatchEffect->Apply(m_pImmediateContext);
	m_pImmediateContext->IASetInputLayout(m_pVertexLayout);

	//g_Batch->Begin();
	//VertexPositionColorTexture v1(Vector3(-1.0f, 1.0f, 0.f), Colors::White, Vector2(0.0f, 0.0f));
	//VertexPositionColorTexture v2(Vector3(1.0f, 1.0f, 0.f), Colors::White, Vector2(1.0f, 0.0f));
	//VertexPositionColorTexture v3(Vector3(1.0f, -1.0f, 0.f), Colors::White, Vector2(1.0f, 1.0f));
	//VertexPositionColorTexture v4(Vector3(-1.0f, -1.0f, 0.0f), Colors::White, Vector2(0.0f, 1.0f));
	//g_Batch->DrawQuad(v1, v2, v3, v4);
	//g_Batch->End();

	if (m_bDepthReceived)
	RenderParticle();
    // Present our back buffer to our front buffer
    m_pSwapChain->Present(0, 0);



	//m_pImmediateContext->OMSetRenderTargets(1, &m_pRenderTargetView_user, m_pDepthStencilView);
	//m_pImmediateContext->ClearRenderTargetView(m_pRenderTargetView_user, ClearColor);

	//// Clear the depth buffer to 1.0 (max depth)
	//m_pImmediateContext->ClearDepthStencilView(m_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

	//vp.Width = static_cast<FLOAT>(m_windowResX/2);
	//vp.Height = static_cast<FLOAT>(m_windowResY);
	//vp.MinDepth = 0.0f;
	//vp.MaxDepth = 1.0f;
	//vp.TopLeftX = 0;
	//vp.TopLeftY = 0;
	//m_pImmediateContext->RSSetViewports(1, &vp);


	//XMVECTOR m_eye = XMVectorSet(faceTranslation[0]-0.1,faceTranslation[1],faceTranslation[2]-0.1, 0.0f);
	//XMVECTOR m_at = XMVectorSet(0.f, 0.f, -1.5f, 0.f);
 //   XMVECTOR m_up = XMVectorSet(0.f, 1.f, 0.f, 0.f);
	//XMMATRIX user_view = XMMatrixLookAtLH(m_eye, m_at, m_up);

	//// Update variables that change once per frame
	////CBChangesEveryFrame cb;
	//cb.View = XMMatrixTranspose(user_view);
	////cb.Projection = XMMatrixTranspose(m_projection);
	////cb.XYScale = XMFLOAT4(m_xyScale, -m_xyScale, 0.f, 0.f);
	////cb.Rectangle = XMFLOAT4(ftRect[0], ftRect[1], ftRect[2], ftRect[3]);
	//m_pImmediateContext->UpdateSubresource(m_pCBChangesEveryFrame, 0, NULL, &cb, 0, 0);

	//// Set up shaders
	//m_pImmediateContext->VSSetShader(m_pVertexShader, NULL, 0);

	//m_pImmediateContext->GSSetShader(m_pGeometryShader, NULL, 0);
	//m_pImmediateContext->GSSetConstantBuffers(0, 1, &m_pCBChangesEveryFrame);
	//m_pImmediateContext->GSSetShaderResources(0, 1, &m_pDepthTextureRV);
	//m_pImmediateContext->GSSetShaderResources(1, 1, &m_pColorTextureRV);
	//m_pImmediateContext->GSSetSamplers(0, 1, &m_pColorSampler);

	//m_pImmediateContext->PSSetShader(m_pPixelShader, NULL, 0);

	//// Draw the scene
	//m_pImmediateContext->Draw(m_depthWidth * m_depthHeight, 0);


	//vp.Width = static_cast<FLOAT>(m_windowResX / 2);
	//vp.Height = static_cast<FLOAT>(m_windowResY);
	//vp.MinDepth = 0.0f;
	//vp.MaxDepth = 1.0f;
	//vp.TopLeftX = m_windowResX / 2;
	//vp.TopLeftY = 0;
	//m_pImmediateContext->RSSetViewports(1, &vp);

	//m_eye = XMVectorSet(faceTranslation[0]+0.1, faceTranslation[1], faceTranslation[2] - 0.1, 0.0f);
	//m_at = XMVectorSet(0.f, 0.f, -1.5f, 0.f);
	//m_up = XMVectorSet(0.f, 1.f, 0.f, 0.f);
	//user_view = XMMatrixLookAtLH(m_eye, m_at, m_up);

	//// Update variables that change once per frame
	////CBChangesEveryFrame cb;
	//cb.View = XMMatrixTranspose(user_view);
	////cb.Projection = XMMatrixTranspose(m_projection);
	////cb.XYScale = XMFLOAT4(m_xyScale, -m_xyScale, 0.f, 0.f);
	////cb.Rectangle = XMFLOAT4(ftRect[0], ftRect[1], ftRect[2], ftRect[3]);
	//m_pImmediateContext->UpdateSubresource(m_pCBChangesEveryFrame, 0, NULL, &cb, 0, 0);

	//// Set up shaders
	//m_pImmediateContext->VSSetShader(m_pVertexShader, NULL, 0);

	//m_pImmediateContext->GSSetShader(m_pGeometryShader, NULL, 0);
	//m_pImmediateContext->GSSetConstantBuffers(0, 1, &m_pCBChangesEveryFrame);
	//m_pImmediateContext->GSSetShaderResources(0, 1, &m_pDepthTextureRV);
	//m_pImmediateContext->GSSetShaderResources(1, 1, &m_pColorTextureRV);
	//m_pImmediateContext->GSSetSamplers(0, 1, &m_pColorSampler);

	//m_pImmediateContext->PSSetShader(m_pPixelShader, NULL, 0);

	//// Draw the scene
	//m_pImmediateContext->Draw(m_depthWidth * m_depthHeight, 0);


	//// Present our back buffer to our front buffer
	//return m_pSwapChain_user->Present(0, 0);
}

// Get a video image and process it.
bool CDepthWithColorD3D::CheckCameraInput()
{
	HRESULT hrFT = E_FAIL;

	if (m_bColorReceived && m_bDepthReceived)
		// Do face tracking
	{
		FT_SENSOR_DATA sensorData;
		sensorData.pDepthFrame = m_depthImage;
		sensorData.pVideoFrame = m_colorImage;
		sensorData.ViewOffset = { 0, 0 };
		sensorData.ZoomFactor = 1.0f;

		FT_VECTOR3D* hint = NULL;
		if (SUCCEEDED(GetClosestHint(m_hint3D)))
		{
			hint = m_hint3D;
		}
		if (m_LastTrackSucceeded)
		{
			hrFT = m_pFaceTracker->ContinueTracking(&sensorData, hint, m_pFTResult);
		}
		else
		{
			hrFT = m_pFaceTracker->StartTracking(&sensorData, NULL, hint, m_pFTResult);
		}
	}
	

	m_LastTrackSucceeded = SUCCEEDED(hrFT) && SUCCEEDED(m_pFTResult->GetStatus());
	if (m_LastTrackSucceeded)
	{
		RECT Test;
		FLOAT pScale, pRotation[3];
			m_pFTResult->Get3DPose(&pScale,pRotation,faceTranslation);
		m_pFTResult->GetFaceRect(&Test);
		ftRect[0] = (float)(Test.left / 640.0f);
		ftRect[1] = (float)(Test.right / 640.0f);
		ftRect[2] = (float)(Test.top / 480.0f);
		ftRect[3] = (float)(Test.bottom/ 480.0f);
	}
	else
	{
		m_pFTResult->Reset();
	}
	SetCenterOfImage(m_pFTResult);
	return m_LastTrackSucceeded;
}