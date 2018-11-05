//***************************************************************************************
// d3dApp.h by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

#pragma once

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#ifdef PIXSUPPORT
#include <dxgi1_4.h>
#else
#define USE_DXGI_1_6
#include <dxgi1_6.h>
#endif

#include "d3dUtil.h"
#include "GameTimer.h"
#include "B:/Project/DirectXPractice/FrameResource.h"

// Link necessary d3d12 libraries.
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dwrite")
#pragma comment(lib, "D3D11.lib")
#pragma comment(lib, "D2D1.lib")

#pragma warning (disable : 4100 )

class D3DApp
{
protected:

    D3DApp(HINSTANCE hInstance);
    D3DApp(const D3DApp& rhs) = delete;
    D3DApp& operator=(const D3DApp& rhs) = delete;
    virtual ~D3DApp();

public:

    static D3DApp* GetApp();
    
	HINSTANCE AppInst()const;
	HWND      MainWnd()const;
	float     AspectRatio()const;

    bool Get4xMsaaState()const;
    void Set4xMsaaState(bool value);

	int Run();
 
    virtual bool Initialize();
    virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
    virtual void CreateRtvAndDsvDescriptorHeaps();
	virtual void OnResize(); 
	virtual void Update(const GameTimer& gt)=0;
    virtual void Draw(const GameTimer& gt)=0;

	// Convenience overrides for handling mouse input.
	virtual void OnKeyDown(WPARAM btnState) { }
	virtual void OnMouseDown(WPARAM btnState, int x, int y){ }
	virtual void OnMouseUp(WPARAM btnState, int x, int y)  { }
	virtual void OnMouseMove(WPARAM btnState, int x, int y){ }

protected:

	bool InitMainWindow();
	bool InitDirect3D();
	void CreateCommandObjects();
    void CreateSwapChain();
	void ToggleFullscreenWindow();

	void FlushCommandQueue();

	ID3D12Resource* CurrentBackBuffer()const;
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView()const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView()const;

	void CalculateFrameStats();

    void LogAdapters();
    void LogAdapterOutputs(IDXGIAdapter* adapter);
    void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);
	void D3DApp::GetGPUAdapter(UINT adapterIndex, IDXGIAdapter1** ppAdapter);

protected:

    static D3DApp* mApp;

	RECT m_windowRect;
    HINSTANCE mhAppInst = nullptr; // application instance handle
    HWND      mhMainWnd = nullptr; // main window handle
	bool      mAppPaused = false;  // is the application paused?
	bool      mMinimized = false;  // is the application minimized?
	bool      mMaximized = false;  // is the application maximized?
	bool      mResizing = false;   // are the resize bars being dragged?
    bool      mFullscreenState = false;// fullscreen enabled

	// Set true to use 4X MSAA (?.1.8).  The default is false.
    bool      m4xMsaaState = false;    // 4X MSAA enabled
    UINT      m4xMsaaQuality = 0;      // quality level of 4X MSAA

	// Used to keep track of the “delta-time?and game time (?.4).
	GameTimer mTimer;
	
    Microsoft::WRL::ComPtr<IDXGIFactory6> mdxgiFactory;
    Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain;
    Microsoft::WRL::ComPtr<ID3D12Device> md3dDevice;

    Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
    UINT64 mCurrentFence = 0;
	
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;

	static const int SwapChainBufferCount = 2;
	int mCurrBackBuffer = 0;
    Microsoft::WRL::ComPtr<ID3D12Resource> mSwapChainBuffer[SwapChainBufferCount];
    Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDsvHeap;

    D3D12_VIEWPORT mScreenViewport; 
    D3D12_RECT mScissorRect;

	UINT mRtvDescriptorSize = 0;
	UINT mDsvDescriptorSize = 0;
	UINT mCbvSrvUavDescriptorSize = 0;

	// Derived class should set these in derived constructor to customize starting values.
	std::wstring mMainWndCaption = L"d3d App";
	D3D_DRIVER_TYPE md3dDriverType = D3D_DRIVER_TYPE_HARDWARE;
    DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	int mClientWidth = 1280;
	int mClientHeight = 800;

	int mBeforeFullscreenClientWidth = 1280;
	int mBeforeFullscreenClientHeight = 800;

	Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_d3d11DeviceContext;
	Microsoft::WRL::ComPtr<ID3D11On12Device> m_d3d11On12Device;
	Microsoft::WRL::ComPtr<IDWriteFactory> m_dwriteFactory;
	Microsoft::WRL::ComPtr<ID2D1Factory3> m_d2dFactory;
	Microsoft::WRL::ComPtr<ID2D1Device2> m_d2dDevice;
	Microsoft::WRL::ComPtr<ID2D1DeviceContext2> m_d2dDeviceContext;
	std::vector<Microsoft::WRL::ComPtr<ID3D11Resource>> m_wrappedRenderTargets;
	std::vector<Microsoft::WRL::ComPtr<ID2D1Bitmap1>> m_d2dRenderTargets;

	Microsoft::WRL::ComPtr<IDWriteFontCollection> m_dwFontColl;
	std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID2D1SolidColorBrush>> m_Brush;
	std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<IDWriteTextFormat>> m_textFormat;

	Microsoft::WRL::ComPtr<IDWriteTextLayout> m_textLayout;

	std::vector<DXGI_MODE_DESC> m_modeList;

	bool m_fullscreenMode = false;
	bool uiFirst = true;

	struct DxgiAdapterInfo
	{
		DXGI_ADAPTER_DESC1 desc;
		bool supportsDx12FL11;
	};
	DXGI_GPU_PREFERENCE m_activeGpuPreference = DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE;
	std::vector<DxgiAdapterInfo> m_gpuAdapterDescs;
	UINT m_activeAdapter;
	LUID m_activeAdapterLuid;

	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	std::array<bool, 256> keyState;
};

