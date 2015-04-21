//------------------------------------------------------------------------------
// <copyright file="DepthWithColor-D3D.h" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//------------------------------------------------------------------------------

#pragma once

#include "resource.h"
#include <iostream>
#include <iomanip>
#include <string>
#include <ctype.h>
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
#include <Kinect.h>
#include <Kinect.Face.h>

// Utilities Library
#include "Camera.h"
#include "DX11Utils.h"
//#include <FaceTrackLib.h>

//OpenCV Library
#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/gpu/gpu.hpp"

//-- Physx Library
#include <PxPhysicsAPI.h>
#include <PxExtensionsAPI.h>
#include <PxDefaultErrorCallback.h>
#include <PxDefaultAllocator.h>
#include <PxDefaultSimulationFilterShader.h>
#include <PxDefaultCpuDispatcher.h>
#include <PxShapeExt.h>
#include <PxMat33.h>
#include <PxSimpleFactory.h>
#include <vector>
#include <PxVisualDebuggerExt.h>

/// <summary>
/// Constant buffer for shader
/// </summary>
using namespace std;
using namespace cv;
using namespace cv::gpu;
using namespace DirectX;
using namespace DirectX::SimpleMath;
using namespace physx;

struct CBChangesEveryFrame
{
	DirectX::XMMATRIX View;
	DirectX::XMMATRIX Projection;
	DirectX::XMFLOAT4 XYScale;
	DirectX::XMFLOAT4 Rectangle;
};

struct Particle
{
	float x;
	float y;
	float Depth;
};

class CDepthWithColorD3D
{
	static const int        cDepthWidth = 512;
	static const int        cDepthHeight = 424;
	static const int        cColorWidth = 1920;
	static const int        cColorHeight = 1080;

public:

	CDepthWithColorD3D();

	~CDepthWithColorD3D();

	HRESULT                             CreateFirstConnected();
	HRESULT                             InitDevice();
	HRESULT                             InitWindow(HINSTANCE hInstance, int nCmdShow);
	HRESULT                             Render();
	HRESULT                             InitOpenCV();
	LRESULT                             HandleMessages(HWND  hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	PxVisualDebuggerConnection* theConnection;

	//--------------------------------------------------------------------------------------
	//Physx
	//--------------------------------------------------------------------------------------
	static PxPhysics*                gPhysicsSDK;
	static PxDefaultErrorCallback    gDefaultErrorCallback;
	static PxDefaultAllocator        gDefaultAllocatorCallback;
	static PxSimulationFilterShader  gDefaultFilterShader;
	static PxFoundation*             gFoundation;

	//PHYSX                 Function
	void                    InitializePhysX();
	void                    ShutdownPhysX();
	void                    StepPhysX();
	//PxRigidDynamic*         CreateSphere(const PxVec3& pos, const PxReal radius, const PxReal density);

protected:

	PxDistanceJoint*      gMouseJoint = NULL;
	PxReal                gMouseDepth = 0.0f;
	PxReal                myTimestep = 1.0/10.0;
	PxRigidDynamic*       gMouseSphere = NULL;
	PxRigidDynamic*       gSelectedActor = NULL;
	PxScene*              gScene = NULL;
	vector<PxRigidActor*> boxes;
	vector<PxRigidActor*> proxyParticleActor;

private:


	INT64                   m_nStartTime;
	INT64                   m_nLastCounter;
	double                  m_fFreq;
	INT64                   m_nNextStatusTime;
	DWORD                   m_nFramesSinceUpdate;

	Mat	LastGrayFrame;

	//                                  3d camera
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

	std::unique_ptr<BasicEffect>                                g_BatchEffect;
	std::unique_ptr<GeometricPrimitive>                         g_Box;
	std::unique_ptr<GeometricPrimitive>                         g_Sphere;
	std::unique_ptr<PrimitiveBatch<VertexPositionColorTexture>> g_Batch;

	std::vector<Particle>				 proxyParticle;

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

	//                                  Kinect
	IKinectSensor*	                    m_pKinectSensor;
	ICoordinateMapper*                  m_pCoordinateMapper;
	DepthSpacePoint*                    m_pDepthCoordinates;

	IMultiSourceFrameReader* m_pMultiSourceFrameReader;

	// for passing depth data as a texture
	ID3D11Texture2D*                    m_pDepthTexture2D;
	ID3D11ShaderResourceView*           m_pDepthTextureRV;

	// for passing color data as a texture
	ID3D11Texture2D*                    m_pColorTexture2D;
	ID3D11ShaderResourceView*           m_pColorTextureRV;

	ID3D11SamplerState*                 m_pColorSampler;

	// for                              mapping depth to color
	UINT16*                             m_depthD16;
	RGBQUAD*                            m_pColorRGBX;
	RGBQUAD*                            m_pOutputRGBX;
	RGBQUAD*                            m_pBackgroundRGBX;
	BYTE*					            m_pBodyIndex;

	Mat					   referenceFrame;
	Mat					   Last_u, Last_v;

	LONG*                               m_colorCoordinates;

	// to prevent drawing until we have data for both streams
	bool                                m_bDepthReceived;
	bool                                m_bColorReceived;

	bool                                m_bNearMode;

	// if the application is paused, for example in the minimized case
	bool                                m_bPaused;

	// Color reader
	IColorFrameReader*     m_pColorFrameReader;

	//                     Body reader
	IBodyFrameReader*      m_pBodyFrameReader;

	//                     Face sources
	IFaceFrameSource*	   m_pFaceFrameSources[BODY_COUNT];

	//                     Face readers
	IFaceFrameReader*	   m_pFaceFrameReaders[BODY_COUNT];

	IFaceFrameResult*      pFaceFrameResult;

	HRESULT                ProcessColor(int nBufferSize);

	void	               RenderParticle();

	void	               UpdateParticle(Mat& u, Mat& v);

	void                   getFlowField(const Mat& u, const Mat& v, Mat& flowField);

	void                   ProcessFrame(INT64 nTime,
		const UINT16* pDepthBuffer, int nDepthHeight, int nDepthWidth,
		const RGBQUAD* pColorBuffer, int nColorWidth, int nColorHeight,
		const BYTE* pBodyIndexBuffer, int nBodyIndexWidth, int nBodyIndexHeight, Mat& u, Mat& v);

	BOOLEAN ProcessFaces(IMultiSourceFrame* pMultiFrame);

	HRESULT UpdateBodyData(IBody** ppBodies, IMultiSourceFrame* pMultiFrame);

	void DrawGrid(PrimitiveBatch<VertexPositionColorTexture>& batch, FXMVECTOR xAxis, FXMVECTOR yAxis,
		FXMVECTOR origin, size_t xdivs, size_t ydivs, GXMVECTOR color);

	XMMATRIX PxtoXMMatrix(PxTransform input);
	void DrawBox(PxShape* pShape, PxRigidActor* actor);
	void DrawSphere(PxShape* pShape, PxRigidActor* actor);
	void DrawShape(PxShape* shape, PxRigidActor* actor);
	void DrawActor(PxRigidActor* actor);
	void RenderActors();
	PxRigidDynamic* CreateSphere(const PxVec3& pos, const PxReal radius, const PxReal density);

	

};

// Safe release for interfaces
template<class Interface>
inline void SafeRelease(Interface *& pInterfaceToRelease)
{
	if (pInterfaceToRelease != NULL)
	{
		pInterfaceToRelease->Release();
		pInterfaceToRelease = NULL;
	}
}