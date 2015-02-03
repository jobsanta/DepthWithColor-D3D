//------------------------------------------------------------------------------
// <copyright file="DepthWithColor-D3D.h" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//------------------------------------------------------------------------------

#pragma once

#include "resource.h"
#include <iostream>
#include <windowsx.h>
#include <algorithm>

//-- DirecX Library
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <DirectXColors.h>

//---DirectxTK Library
#include <Effects.h>
#include <PrimitiveBatch.h>
#include <GeometricPrimitive.h>
#include <VertexTypes.h>
#include <CommonStates.h>
#include <DirectXHelpers.h>
#include <SimpleMath.h>

// Kinect Library
#include "NuiApi.h"

// Utilities Library
#include "Camera.h"
#include "DX11Utils.h"
#include <FaceTrackLib.h>

/// <summary>
/// Constant buffer for shader
/// </summary>
using namespace std;
using namespace DirectX;
using namespace DirectX::SimpleMath;

struct CBChangesEveryFrame
{
	DirectX::XMMATRIX View;
	DirectX::XMMATRIX Projection;
	DirectX::XMFLOAT4 XYScale;
	DirectX::XMFLOAT4 Rectangle;
};

class CDepthWithColorD3D
{
	static const int                    cBytesPerPixel = 4;

	static const NUI_IMAGE_RESOLUTION   cDepthResolution = NUI_IMAGE_RESOLUTION_640x480;
	static const NUI_IMAGE_RESOLUTION   cColorResolution = NUI_IMAGE_RESOLUTION_640x480;
	

public:
	/// <summary>
	/// Constructor
	/// </summary>
	CDepthWithColorD3D();

	/// <summary>
	/// Denstructor
	/// </summary>
	~CDepthWithColorD3D();

	/// <summary>
	/// Register class and create window
	/// </summary>
	/// <returns>S_OK for success, or failure code</returns>
	HRESULT                             InitWindow(HINSTANCE hInstance, int nCmdShow);

	/// <summary>
	/// Create Direct3D device and swap chain
	/// </summary>
	/// <returns>S_OK for success, or failure code</returns>
	HRESULT                             InitDevice();

	/// <summary>
	/// Create the first connected Kinect found 
	/// </summary>
	/// <returns>S_OK on success, otherwise failure code</returns>
	HRESULT                             CreateFirstConnected();

	/// <summary>
	/// Renders a frame
	/// </summary>
	/// <returns>S_OK for success, or failure code</returns>
	HRESULT                             Render();

	/// <summary>
	/// Handles window messages, used to process input
	/// </summary>
	/// <param name="hWnd">window message is for</param>
	/// <param name="uMsg">message</param>
	/// <param name="wParam">message data</param>
	/// <param name="lParam">additional message data</param>
	/// <returns>result of message processing</returns>
	LRESULT HandleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
	// 3d camera
	CCamera                             m_camera;

	HINSTANCE                           m_hInst;
	HWND                                m_hWnd;
	HWND								m_hWnd_user_view;
	D3D_FEATURE_LEVEL                   m_featureLevel;
	ID3D11Device*                       m_pd3dDevice;
	ID3D11DeviceContext*                m_pImmediateContext;
	IDXGISwapChain*                     m_pSwapChain;
	IDXGISwapChain*						m_pSwapChain_user;
	
	ID3D11RenderTargetView*             m_pRenderTargetView;
	ID3D11RenderTargetView*				m_pRenderTargetView_user;
	

	std::unique_ptr<BasicEffect>                         g_BatchEffect;
	std::unique_ptr<GeometricPrimitive>                  g_Box;
	std::unique_ptr<GeometricPrimitive>                  g_Sphere;
	std::unique_ptr<PrimitiveBatch<VertexPositionColorTexture>> g_Batch;


	ID3D11Texture2D*                    m_pDepthStencil;
	ID3D11DepthStencilView*             m_pDepthStencilView;
	ID3D11InputLayout*                  m_pVertexLayout;
	ID3D11Buffer*                       m_pVertexBuffer;
	ID3D11Buffer*                       m_pCBChangesEveryFrame;


	DirectX::XMMATRIX                   m_projection;

	ID3D11VertexShader*                 m_pVertexShader;
	ID3D11PixelShader*                  m_pPixelShader;
	ID3D11GeometryShader*               m_pGeometryShader;

	LONG                                m_depthWidth;
	LONG                                m_depthHeight;

	LONG                                m_colorWidth;
	LONG                                m_colorHeight;

	LONG                                m_colorToDepthDivisor;

	float                               m_xyScale;

	// Initial window resolution
	int                                 m_windowResX;
	int                                 m_windowResY;

	// Kinect
	INuiSensor*                         m_pNuiSensor;
	HANDLE                              m_hNextDepthFrameEvent;
	HANDLE                              m_pDepthStreamHandle;
	HANDLE                              m_hNextColorFrameEvent;
	HANDLE                              m_pColorStreamHandle;
	HANDLE								m_pSkeletonStreamhandle;
	HANDLE								m_hNextSkeletonEvent;


	// for passing depth data as a texture
	ID3D11Texture2D*                    m_pDepthTexture2D;
	ID3D11ShaderResourceView*           m_pDepthTextureRV;

	// for passing color data as a texture
	ID3D11Texture2D*                    m_pColorTexture2D;
	ID3D11ShaderResourceView*           m_pColorTextureRV;


	ID3D11SamplerState*                 m_pColorSampler;

	// for mapping depth to color
	USHORT*                             m_depthD16;
	BYTE*                               m_colorRGBX;
	LONG*                               m_colorCoordinates;

	// to prevent drawing until we have data for both streams
	bool                                m_bDepthReceived;
	bool                                m_bColorReceived;

	bool                                m_bNearMode;

	// if the application is paused, for example in the minimized case
	bool                                m_bPaused;

	//Face Tracker 
	IFTFaceTracker*						m_pFaceTracker;
	IFTResult*							m_pFTResult;
	float								m_XCenterFace;
	float								m_YCenterFace;
	bool								m_LastTrackSucceeded;
	IFTImage*							m_colorImage;
	IFTImage*							m_depthImage;

	/// <summary>
	/// Toggles between near and default mode
	/// Does nothing on a non-Kinect for Windows device
	/// </summary>
	/// <returns>S_OK for success, or failure code</returns>
	HRESULT                             ToggleNearMode();

	/// <summary>
	/// Process depth data received from Kinect
	/// </summary>
	/// <returns>S_OK for success, or failure code</returns>
	HRESULT                             ProcessDepth();

	/// <summary>
	/// Process color data received from Kinect
	/// </summary>
	/// <returns>S_OK for success, or failure code</returns>
	HRESULT                             ProcessColor();

	/// <summary>
	/// Adjust color to the same space as depth
	/// </summary>
	/// <returns>S_OK on success, otherwise failure code</returns>
	HRESULT                             MapColorToDepth();

	/// <summary>
	/// Compile and set layout for shaders
	/// </summary>
	/// <returns>S_OK for success, or failure code</returns>
	HRESULT                             LoadShaders();

	void	                            RenderParticle();
	
	void								SetCenterOfImage(IFTResult*);

	bool								CheckCameraInput();

	HRESULT								ProcessSkeleton();

	FT_VECTOR3D                         m_NeckPoint[NUI_SKELETON_COUNT];
	FT_VECTOR3D                         m_HeadPoint[NUI_SKELETON_COUNT];
	bool                                m_SkeletonTracked[NUI_SKELETON_COUNT];
	HRESULT                             GetClosestHint(FT_VECTOR3D* pHint3D);
	FT_VECTOR3D	                        m_hint3D[2];
	float                               faceTranslation[3];

	float                               ftRect[4];
};