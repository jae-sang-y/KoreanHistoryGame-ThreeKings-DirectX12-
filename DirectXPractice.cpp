//***************************************************************************************
// MyApp.cpp by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

#include "Common/d3dApp.h"
#include "Common/MathHelper.h"
#include "Common/UploadBuffer.h"
#include "Common/GeometryGenerator.h"
#include "Waves.h"


using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;



const int gNumFrameResources = 3;


// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
struct RenderItem
{
	RenderItem() = default;

	bool Visible = true;

	BoundingBox Bounds;

    // World matrix of the shape that describes the object's local space
    // relative to the world space, which defines the position, orientation,
    // and scale of the object in the world.
    XMFLOAT4X4 World = MathHelper::Identity4x4();

	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = 0xFFFFFFFF;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

    // Primitive topology.
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // DrawIndexedInstanced parameters.
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
	std::string Name = "None";
};

enum class RenderLayer : int
{
	Opaque = 0,
	Transparent,
	AlphaTested,
	Province,
	Count
};

class MyApp : public D3DApp
{
public:
    MyApp(HINSTANCE hInstance);
    MyApp(const MyApp& rhs) = delete;
    MyApp& operator=(const MyApp& rhs) = delete;
    ~MyApp();

    virtual bool Initialize()override;

private:
    virtual void OnResize()override;
    virtual void Update(const GameTimer& gt)override;
    virtual void Draw(const GameTimer& gt)override;

	void DrawUI();


	virtual void OnKeyDown(WPARAM btnState)override;
    virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

    void OnKeyboardInput(const GameTimer& gt);
	void UpdateCamera(const GameTimer& gt);
	void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateWaves(const GameTimer& gt); 

	void LoadTextures();
    void BuildRootSignature();
	void BuildDescriptorHeaps();
    void BuildShadersAndInputLayout();
    void BuildLandGeometry();
    void BuildWavesGeometry();
	void BuildBoxGeometry();
    void BuildPSOs();
    void BuildFrameResources();
    void BuildMaterials();
    void BuildRenderItems();
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);
	void Pick(WPARAM btnState, int sx, int sy);
	void LoadSizeDependentResources();
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();
	void UILayerInitialize(ID3D12Device* pDevice, ID3D12CommandQueue* pCommandQueue);
	void UILayerResize(ComPtr<ID3D12Resource>* ppRenderTargets, UINT width, UINT height);
	void GameInit();
	void GameUpdate();
	void GameStep();
	void GameSave();
	void GameLoad();
	void ProvinceMousedown(WPARAM btnState, ProvinceId id);
private:

    UINT mCbvSrvDescriptorSize = 0;

    ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout_prv;
 
    RenderItem* mWavesRitem = nullptr;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// Render items divided by PSO.
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];
	std::vector<VertexForProvince> mLandVertices;

	std::unique_ptr<Waves> mWaves;

    PassConstants mMainPassCB;

	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

    float mTheta = 1.5f*XM_PI;
    float mPhi = XM_PIDIV2 - 0.1f;
    float mRadius = 50.0f;

	size_t map_w = 0;
	size_t map_h = 0;

	float mX = 0;
	
    POINT mLastMousePos;

	struct {
		std::wstring DebugText = L"선택 되지 않음";
		NationId nationPick = 0;
	} mUser;


	struct Province {
		std::string name;
		Color32 color;
		std::uint64_t p_num = 1;
		float px, py, pz;
		float ty;


		bool is_rebel = false;
		NationId owner = 0;
		NationId ruler = 0;

		Province()
		{
		}
		Province (std::string _name, Color32 _color, float _px, float _py, float _pz):name(_name), color(_color), px(_px), py(_py), pz(_pz), ty(_py)
		{		
		}
	};
	std::map<ProvinceId, Province> prov_stack;
	using WCHAR3 = WCHAR[3];
	struct Nation
	{	
		XMFLOAT4 MainColor = { 0.f, 0.f, 0.f, 0.f };
		std::wstring MainName = L"오류";


	};

	XMVECTOR mEyetarget = XMVectorSet(0.0f, 15.0f, 0.0f, 0.0f);

	std::unordered_map<NationId, std::unique_ptr<Nation>>  nations;
	std::unordered_map<std::wstring, std::wstring> captions;

	float mEyeMoveX = 0.f;
	float mEyeMoveZ = 0.f;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    try
    {
        MyApp theApp(hInstance);
        if(!theApp.Initialize())
            return 0;

        return theApp.Run();
    }
    catch(DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}

MyApp::MyApp(HINSTANCE hInstance)
    : D3DApp(hInstance)
{
}

MyApp::~MyApp()
{
    if(md3dDevice != nullptr)
        FlushCommandQueue();
}

bool MyApp::Initialize()
{
    if(!D3DApp::Initialize())
        return false;

    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    // Get the increment size of a descriptor in this heap type.  This is hardware specific, 
	// so we have to query this information.
    mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

 
	LoadTextures();
    BuildRootSignature();
	BuildDescriptorHeaps();
    BuildShadersAndInputLayout();
    BuildLandGeometry();
    BuildWavesGeometry();
	BuildBoxGeometry();	
	BuildMaterials();
    BuildRenderItems();
    BuildFrameResources();
    BuildPSOs();

    // Execute the initialization commands.
    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Wait until initialization is complete.
    FlushCommandQueue();

	GameInit();

    return true;
}
 
void MyApp::OnResize()
{
    D3DApp::OnResize();

    // The window resized, so update the aspect ratio and recompute the projection matrix.
    XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&mProj, P);
}


void MyApp::Update(const GameTimer& gt)
{
    OnKeyboardInput(gt);
	UpdateCamera(gt);

    // Cycle through the circular frame resource array.
    mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
    mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

    // Has the GPU finished processing the commands of the current frame resource?
    // If not, wait until the GPU has completed commands up to this fence point.
    if(mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

	AnimateMaterials(gt);
	UpdateObjectCBs(gt);
	UpdateMaterialCBs(gt);
	UpdateMainPassCB(gt);
    UpdateWaves(gt);
	LoadSizeDependentResources();
	GameUpdate();
}

void MyApp::DrawUI()
{
	auto&& g = m_d2dDeviceContext;

	g->DrawText(mUser.DebugText.c_str(), (UINT32)mUser.DebugText.length(), m_textFormat.Get(), D2D1::RectF(30.0f, 30.0f, 300.f, 100.f), m_textBrush.Get());






	{

		XMMATRIX V = XMLoadFloat4x4(&mView);
		XMMATRIX invView = XMMatrixInverse(new XMVECTOR(XMMatrixDeterminant(V)), V);


		XMMATRIX W = XMLoadFloat4x4(&MathHelper::Identity4x4());

		XMMATRIX invWorld = XMMatrixInverse(new XMVECTOR(XMMatrixDeterminant(W)), W);

		XMMATRIX toLocal = XMMatrixMultiply(invView, invWorld);

		XMVECTOR rayOrigin = XMVector3TransformCoord(rayOrigin, toLocal);
		XMVECTOR rayDir = XMVector3TransformNormal(rayDir, toLocal);
		rayDir = XMVector3Normalize(rayDir);

		float vx = rayDir.m128_f32[0];
		float vy = rayDir.m128_f32[1];

		XMFLOAT4X4 P = mProj;
		float sx = (-vx * P(0, 0) + 1.0f) * (mClientWidth / 2.0f);
		float sy = -(-vy * P(1, 1) - 1.0f) * (mClientHeight / 2.0f);

		captions[L"SX"] = std::to_wstring(sx);
		captions[L"SY"] = std::to_wstring(sy);

		g->DrawRectangle({ sx - 64.f, sy - 64.f, sx + 64.f, sy + 64.f }, m_textBrush.Get());
	}



	//if (captions.find(L"선택한 프로빈스") != captions.end())
	{
		//float x = std::stof(captions[L"선택한 프로빈스.x"]), y = std::stof(captions[L"선택한 프로빈스.z"]), z = 0.f;
		


		{			
			//g->DrawRectangle({ pos.x - 64.f, pos.y - 64.f, pos.x + 64.f, pos.y + 64.f }, m_textBrush.Get());
		}


	}

	

}
void MyApp::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	ThrowIfFailed(cmdListAlloc->Reset());

	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	mCommandList->ResourceBarrier(1, new CD3DX12_RESOURCE_BARRIER(CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET)));

	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), (float*)&mMainPassCB.FogColor, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	mCommandList->OMSetRenderTargets(1, new D3D12_CPU_DESCRIPTOR_HANDLE(CurrentBackBufferView()), true, new D3D12_CPU_DESCRIPTOR_HANDLE(DepthStencilView()));

	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

	mCommandList->SetPipelineState(mPSOs["alphaTested"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::AlphaTested]);

	mCommandList->SetPipelineState(mPSOs["province"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Province]);

	mCommandList->SetPipelineState(mPSOs["transparent"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Transparent]);

	ThrowIfFailed(mCommandList->Close());

	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);


	{
		int m_frameIndex = mCurrBackBuffer;
		ID3D11Resource* ppResources[] = { m_wrappedRenderTargets[m_frameIndex].Get() };

		m_d2dDeviceContext->SetTarget(m_d2dRenderTargets[m_frameIndex].Get());

		m_d3d11On12Device->AcquireWrappedResources(ppResources, _countof(ppResources));
		m_d2dDeviceContext->BeginDraw();

		DrawUI();

		m_d2dDeviceContext->EndDraw();
		m_d3d11On12Device->ReleaseWrappedResources(ppResources, _countof(ppResources));

		m_d3d11DeviceContext->Flush();

	}
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	mCurrFrameResource->Fence = ++mCurrentFence;

	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void MyApp::GameSave()
{
	captions[L"파일 입출력"] = L"저장시작";
	std::ofstream file("Saves/Nation");
	file << "Hello World!" << std::endl;
	file.close();
	captions[L"파일 입출력"] = L"저장종료";
}
void MyApp::GameLoad()
{
	captions[L"파일 입출력"] = L"적재시작";
	captions[L"파일 입출력"] = L"적재종료";

}

void MyApp::GameInit()
{
	srand((unsigned int)time(nullptr));

	NationId nation_count = 0;
	{
		std::unique_ptr<Nation> 신라 = std::make_unique<Nation>();
		신라->MainColor = XMFLOAT4(0.5f, 0.5f, 0.1f, 1.f);
		신라->MainName = L"신라";
		nations[++nation_count] = std::move(신라);

		std::unique_ptr<Nation> 백제 = std::make_unique<Nation>();
		백제->MainColor = XMFLOAT4(0.1f, 0.5f, 0.45f, 1.f);
		백제->MainName = L"백제";
		nations[++nation_count] = std::move(백제);

		std::unique_ptr<Nation> 고구려 = std::make_unique<Nation>();
		고구려->MainColor = XMFLOAT4(0.4f, 0.0f, 0.0f, 1.f);
		고구려->MainName = L"고구려";
		nations[++nation_count] = std::move(고구려);
	}


}
void MyApp::GameUpdate()
{
	for (const auto& O : prov_stack)
	{
		mMainPassCB.gSubProv[O.first] = XMFLOAT4(0.f, 0.f, 0.f, 0.f);
		mMainPassCB.gProv[O.first] = { 0.f,0.f,0.f,0.f };
		
		if (const auto& P = nations.find(O.second.owner); P != nations.end())
		{
			mMainPassCB.gProv[O.first] = P->second->MainColor;
			mMainPassCB.gSubProv[O.first] = P->second->MainColor;
		}
		if (const auto& P = nations.find(O.second.ruler); P != nations.end())
		{
			mMainPassCB.gSubProv[O.first] = P->second->MainColor;
		}
	}

	mMainPassCB.gProv[0] = { 0.f, 0.f, 0.f, 0.f };
	mMainPassCB.gSubProv[0] = mMainPassCB.gProv[0];	

	if (captions.size() > 0)
	{
		mUser.DebugText = L"";
		for (const auto& O : captions)
		{
			mUser.DebugText += O.first + L" : " + O.second + L"\n";
		}
	}

	mEyetarget.m128_f32[0] += mEyeMoveX;
	mEyetarget.m128_f32[2] += mEyeMoveZ;
	mEyeMoveX -= mEyeMoveX;
	mEyeMoveZ -= mEyeMoveZ;

	captions[L"Theta"] = std::to_wstring(mTheta);
	captions[L"Phi"] = std::to_wstring(mPhi);
	captions[L"Radius"] = std::to_wstring(mRadius);


	captions[L"CamPosX"] = std::to_wstring(mEyetarget.m128_f32[0]);
	captions[L"CamPosY"] = std::to_wstring(mEyetarget.m128_f32[1]);
	captions[L"CamPosZ"] = std::to_wstring(mEyetarget.m128_f32[2]);	
}

void MyApp::ProvinceMousedown(WPARAM btnState, ProvinceId id)
{
	auto prov = prov_stack.find(id);
	if (prov == prov_stack.end())
		return;
	captions[L"선택한 프로빈스"] = std::to_wstring(id);
	captions[L"선택한 프로빈스.x"] = std::to_wstring(2.f * prov->second.px / prov->second.p_num);
	captions[L"선택한 프로빈스.z"] = std::to_wstring(2.f * prov->second.pz / prov->second.p_num);

	if (btnState & MK_LBUTTON)
	{
		if (prov->second.owner == 0)
		{
			prov->second.ruler = mUser.nationPick;
			prov->second.owner = mUser.nationPick;
		}
		else if (prov->second.ruler != mUser.nationPick)
		{
			prov->second.ruler = mUser.nationPick;
		}
		else
		{
			prov->second.owner = mUser.nationPick;
		}
		return;
	}
	if (btnState & MK_RBUTTON)
	{
		prov->second.ruler = mUser.nationPick;

		return;
	}
	if (btnState & MK_MBUTTON)
	{

		if (const auto& O = nations.find(prov->second.owner); O != nations.end())
		{
			mUser.nationPick = prov->second.owner;
			captions[L"선택한 국가"] = O->second->MainName;
		}
		else
		{
			mUser.nationPick = 0;
			captions[L"선택한 국가"] = L"미개인";
		}

		return;
	}
}

void MyApp::OnKeyDown(WPARAM btnState)
{
	float deltaTime = mTimer.DeltaTime();
	//bool pressAlt = !keyState.at(VK_MENU);
	bool pressShift = GetAsyncKeyState(VK_LSHIFT) != 0;

	//bool pressCtrl = !keyState.at(VK_CONTROL);

	if (keyState.at('A'))
	{
		mEyeMoveX -= pressShift ? 0.48f : 0.24f * mRadius * deltaTime;
	}
	if (keyState.at('D'))
	{
		mEyeMoveX += pressShift ? 0.48f : 0.24f * mRadius * deltaTime;
	}
	if (keyState.at('W'))
	{
		mEyeMoveZ += pressShift ? 0.48f : 0.24f * mRadius * deltaTime;
	}
	if (keyState.at('S'))
	{
		mEyeMoveZ -= pressShift ? 0.48f : 0.24f * mRadius * deltaTime;
	}

	switch (btnState)
	{
	case VK_SPACE:
	{
		const auto& key = std::next(std::begin(nations), rand() % nations.size());
		captions[L"선택한 국가"] = key->second->MainName;
		mUser.nationPick = key->first;
		return;
	}
	case VK_INSERT:
	{
		GameSave();
		return;
	}
	case VK_DELETE:
	{
		GameLoad();
		return;
	}
	}
}

void MyApp::LoadSizeDependentResources()
{
	for (UINT i = 0; i < SwapChainBufferCount; i++)
	{
		ThrowIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mSwapChainBuffer[i])));
	}

	//m_scene->LoadSizeDependentResources(md3dDevice.Get(), mSwapChainBuffer, mClientWidth, mClientHeight);

	if (true)//m_enableUI)
	{
		{
			m_wrappedRenderTargets.resize(SwapChainBufferCount);
			m_d2dRenderTargets.resize(SwapChainBufferCount);
			//m_textBlocks.resize(1);
			UILayerInitialize(md3dDevice.Get(), mCommandQueue.Get());
		}
		UILayerResize(mSwapChainBuffer, mClientWidth, mClientHeight);
	}
}
void MyApp::UILayerResize(ComPtr<ID3D12Resource>* ppRenderTargets, UINT width, UINT height)
{
	//auto m_width = static_cast<float>(width);
	auto m_height = static_cast<float>(height);

	// Query the desktop's dpi settings, which will be used to create
	// D2D's render targets.
	D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(
		D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
		D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED));

	// Create a wrapped 11On12 resource of this back buffer. Since we are 
	// rendering all D3D12 content first and then all D2D content, we specify 
	// the In resource state as RENDER_TARGET - because D3D12 will have last 
	// used it in this state - and the Out resource state as PRESENT. When 
	// ReleaseWrappedResources() is called on the 11On12 device, the resource 
	// will be transitioned to the PRESENT state.
	for (UINT i = 0; i < m_wrappedRenderTargets.size(); i++)
	{
		D3D11_RESOURCE_FLAGS d3d11Flags = { D3D11_BIND_RENDER_TARGET };
		ThrowIfFailed(m_d3d11On12Device->CreateWrappedResource(
			ppRenderTargets[i].Get(),
			&d3d11Flags,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT,
			IID_PPV_ARGS(&m_wrappedRenderTargets[i])));

		// Create a render target for D2D to draw directly to this back buffer.
		ComPtr<IDXGISurface> surface;
		ThrowIfFailed(m_wrappedRenderTargets[i].As(&surface));
		ThrowIfFailed(m_d2dDeviceContext->CreateBitmapFromDxgiSurface(
			surface.Get(),
			&bitmapProperties,
			&m_d2dRenderTargets[i]));
	}

	ThrowIfFailed(m_d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_d2dDeviceContext));
	m_d2dDeviceContext->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
	ThrowIfFailed(m_d2dDeviceContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &m_textBrush));

	// Create DWrite text format objects.
	const float fontSize = m_height / 30.0f;
	const float smallFontSize = m_height / 40.0f;

	ThrowIfFailed(m_dwriteFactory->CreateTextFormat(
		L"Arial",
		nullptr,
		DWRITE_FONT_WEIGHT_NORMAL,
		DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_NORMAL,
		fontSize,
		L"en-us",
		&m_textFormat));

	ThrowIfFailed(m_textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER));
	ThrowIfFailed(m_textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR));
	ThrowIfFailed(m_dwriteFactory->CreateTextFormat(
		L"Arial",
		nullptr,
		DWRITE_FONT_WEIGHT_NORMAL,
		DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_NORMAL,
		smallFontSize,
		L"en-us",
		&m_textFormat));
}
void MyApp::UILayerInitialize(ID3D12Device* pDevice, ID3D12CommandQueue* pCommandQueue)
{
	UINT d3d11DeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
	D2D1_FACTORY_OPTIONS d2dFactoryOptions = {};

#if defined(_DEBUG) || defined(DBG)
	// Enable the D2D debug layer.
	d2dFactoryOptions.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;

	// Enable the D3D11 debug layer.
	d3d11DeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	// Create an 11 device wrapped around the 12 device and share
	// 12's command queue.
	ComPtr<ID3D11Device> d3d11Device;
	ID3D12CommandQueue* ppCommandQueues[] = { pCommandQueue };
	ThrowIfFailed(D3D11On12CreateDevice(
		pDevice,
		d3d11DeviceFlags,
		nullptr,
		0,
		reinterpret_cast<IUnknown**>(ppCommandQueues),
		_countof(ppCommandQueues),
		0,
		&d3d11Device,
		&m_d3d11DeviceContext,
		nullptr
	));

	// Query the 11On12 device from the 11 device.
	ThrowIfFailed(d3d11Device.As(&m_d3d11On12Device));

#if defined(_DEBUG) || defined(DBG)
	// Filter a debug error coming from the 11on12 layer.
	ComPtr<ID3D12InfoQueue> infoQueue;
	if (SUCCEEDED(pDevice->QueryInterface(IID_PPV_ARGS(&infoQueue))))
	{
		// Suppress messages based on their severity level.
		D3D12_MESSAGE_SEVERITY severities[] =
		{
			D3D12_MESSAGE_SEVERITY_INFO,
		};

		// Suppress individual messages by their ID.
		D3D12_MESSAGE_ID denyIds[] =
		{
			// This occurs when there are uninitialized descriptors in a descriptor table, even when a
			// shader does not access the missing descriptors.
			D3D12_MESSAGE_ID_INVALID_DESCRIPTOR_HANDLE,
		};

		D3D12_INFO_QUEUE_FILTER filter = {};
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;

		ThrowIfFailed(infoQueue->PushStorageFilter(&filter));
	}
#endif

	// Create D2D/DWrite components.
	{
		ComPtr<IDXGIDevice> dxgiDevice;
		ThrowIfFailed(m_d3d11On12Device.As(&dxgiDevice));

		ThrowIfFailed(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory3), &d2dFactoryOptions, &m_d2dFactory));
		ThrowIfFailed(m_d2dFactory->CreateDevice(dxgiDevice.Get(), &m_d2dDevice));
		ThrowIfFailed(m_d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_d2dDeviceContext));

		m_d2dDeviceContext->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
		ThrowIfFailed(m_d2dDeviceContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &m_textBrush));

		ThrowIfFailed(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), &m_dwriteFactory));
	}
}


void MyApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    mLastMousePos.x = x;
    mLastMousePos.y = y;

	//if (btnState & MK_LBUTTON)
	{
		Pick(btnState, x, y);
	}

    SetCapture(mhMainWnd);

}

void MyApp::Pick(WPARAM btnState, int sx, int sy)
{
	#pragma region Pick


	XMFLOAT4X4 P = mProj;

	float vx = (+2.0f*sx / mClientWidth - 1.0f) / P(0, 0);
	float vy = (-2.0f*sy / mClientHeight + 1.0f) / P(1, 1);

	XMVECTOR rayOrigin = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	XMVECTOR rayDir = XMVectorSet(vx, vy, 1.0f, 0.0f);

	XMMATRIX V = XMLoadFloat4x4(&mView);
	XMMATRIX invView = XMMatrixInverse(new XMVECTOR(XMMatrixDeterminant(V)), V);

	std::uint16_t lastPick = 0;
	VertexForProvince* lastPickObj = nullptr;
	for (auto& ri : mRitemLayer[(int)RenderLayer::Province])
	{
		auto geo = ri->Geo;

		if (!ri->Visible)
			continue;

		XMMATRIX W = XMLoadFloat4x4(&ri->World);
		XMMATRIX invWorld = XMMatrixInverse(new XMVECTOR(XMMatrixDeterminant(W)), W);

		XMMATRIX toLocal = XMMatrixMultiply(invView, invWorld);

		rayOrigin = XMVector3TransformCoord(rayOrigin, toLocal);
		rayDir = XMVector3TransformNormal(rayDir, toLocal);
		rayDir = XMVector3Normalize(rayDir);

		float tmin = 0.0f;
		if (ri->Bounds.Intersects(rayOrigin, rayDir, tmin))
		{
			auto vertices = (VertexForProvince*)geo->VertexBufferCPU->GetBufferPointer();
			auto indices = (std::uint16_t*)geo->IndexBufferCPU->GetBufferPointer();
			UINT triCount = ri->IndexCount / 3;

			tmin = MathHelper::Infinity;
			for (UINT i = 0; i < triCount; ++i)
			{
				
				std::uint16_t i0 = indices[i * 3 + 0];
				std::uint16_t i1 = indices[i * 3 + 1];
				std::uint16_t i2 = indices[i * 3 + 2];
				

				XMVECTOR v0 = XMLoadFloat3(&vertices[i0].Pos);
				XMVECTOR v1 = XMLoadFloat3(&vertices[i1].Pos);
				XMVECTOR v2 = XMLoadFloat3(&vertices[i2].Pos);

				float t = 0.0f;
				if (TriangleTests::Intersects(rayOrigin, rayDir, v0, v1, v2, t))
				{
					if (t < tmin)
					{
						tmin = t;
						lastPickObj = vertices;
						lastPick = indices[i * 3 + 0];
					}
				}
			}
			#pragma endregion
		}
	}
	if (lastPick != 0)
	{
		ProvinceMousedown(btnState, lastPickObj[lastPick].Prov);
	}
}
void MyApp::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void MyApp::OnMouseMove(WPARAM btnState, int x, int y)
{
    if((btnState & MK_LBUTTON) != 0)
    {
		//mPhi = MathHelper::Clamp(mPhi, 0.000001f, MathHelper::Pi - 0.1f);
        // Make each pixel correspond to a quarter of a degree.
        float dx = XMConvertToRadians(0.25f*static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(0.25f*static_cast<float>(y - mLastMousePos.y));

        // Update angles based on input to orbit camera around box.
        mTheta += dx;
        mPhi += dy;

		if (mRadius * cosf(mPhi) < 10.f)
		{
			mPhi = acosf(10.f / mRadius);
		}
		else if (mPhi < 0.000001f)
		{
			mPhi = 0.000001f;
		}

        // Restrict the angle mPhi.
    }
    else if((btnState & MK_RBUTTON) != 0)
    {
        // Make each pixel correspond to 0.2 unit in the scene.
        float dx = 0.2f*static_cast<float>(x - mLastMousePos.x);
        float dy = 0.2f*static_cast<float>(y - mLastMousePos.y);

        // Update the camera radius based on input.
        mRadius += dx - dy;

        // Restrict the radius.
        mRadius = MathHelper::Clamp(mRadius, 10.0f, 240.0f);
    }

    mLastMousePos.x = x;
    mLastMousePos.y = y;
}
 
void MyApp::OnKeyboardInput(const GameTimer& gt)
{
}


void MyApp::UpdateCamera(const GameTimer& gt)
{
	// Convert Spherical to Cartesian coordinates.
	mEyePos.x = mRadius*sinf(mPhi + 0.0001f)*cosf(mTheta);
	mEyePos.z = mRadius*sinf(mPhi + 0.0001f)*sinf(mTheta);
	mEyePos.y = mRadius*cosf(mPhi + 0.0001f) - 5.0f;
	
	// Build the view matrix.
	XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f) + mEyetarget;
	
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	
	XMMATRIX view = XMMatrixLookAtLH(pos, mEyetarget, up);
	XMStoreFloat4x4(&mView, view);
}

void MyApp::AnimateMaterials(const GameTimer& gt)
{
	// Scroll the water material texture coordinates.
	auto waterMat = mMaterials["water"].get();

	float& tu = waterMat->MatTransform(3, 0);
	float& tv = waterMat->MatTransform(3, 1);

	tu += 0.1f * gt.DeltaTime();
	tv += 0.02f * gt.DeltaTime();

	if(tu >= 1.0f)
		tu -= 1.0f;

	if(tv >= 1.0f)
		tv -= 1.0f;

	waterMat->MatTransform(3, 0) = tu;
	waterMat->MatTransform(3, 1) = tv;

	// Material has changed, so need to update cbuffer.
	waterMat->NumFramesDirty = gNumFrameResources;
}

//register(b0)
void MyApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for(auto& e : mAllRitems)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if(e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}
}

//register(b2)
void MyApp::UpdateMaterialCBs(const GameTimer& gt)
{
	auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
	for(auto& e : mMaterials)
	{
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if(mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstants matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0 = mat->FresnelR0;
			matConstants.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));

			currMaterialCB->CopyData(mat->MatCBIndex, matConstants);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

//register(b1)
void MyApp::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(new XMVECTOR(XMMatrixDeterminant(view)), view);
	XMMATRIX invProj = XMMatrixInverse(new XMVECTOR(XMMatrixDeterminant(proj)), proj);
	XMMATRIX invViewProj = XMMatrixInverse(new XMVECTOR(XMMatrixDeterminant(viewProj)), viewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mMainPassCB.EyePosW = mEyePos;
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();
	mMainPassCB.AmbientLight = {0.f,0.f,0.f,0.f};//{ 0.25f, 0.25f, 0.35f, 1.0f };

	float time = 0.f;// fmodf(mTimer.TotalTime() / 3.f + 0.f, 20.f) - 10.f;

	mMainPassCB.Lights[0].Direction = { time , -0.57735f, 0.57735f };
	
	
	mMainPassCB.Lights[0].Strength = { MathHelper::Clamp(2.f - powf(time / 4, 2), 0.f, 1.f), MathHelper::Clamp(1.5f - powf(time / 4, 2), 0.f, 1.f), MathHelper::Clamp(1.f - powf(time / 4, 2), 0.f, 1.f) };

	mMainPassCB.Lights[1].Direction = { 0.f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[1].Strength = { 0.05f, 0.1f, 0.9f };
	//mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	//mMainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void MyApp::UpdateWaves(const GameTimer& gt)
{
	mWaves->Update(gt.TotalTime());

	auto currWavesVB = mCurrFrameResource->WavesVB.get();
	for(int i = 0; i < mWaves->VertexCount(); ++i)
	{
		Vertex v;

		v.Pos = mWaves->Position(i);
		v.Normal = mWaves->Normal(i);
		
		v.TexC.x = 0.5f + v.Pos.x / mWaves->Width();
		v.TexC.y = 0.5f - v.Pos.z / mWaves->Depth();

		currWavesVB->CopyData(i, v);
	}

	mWavesRitem->Geo->VertexBufferGPU = currWavesVB->Resource();
}

void MyApp::LoadTextures()
{
	auto grassTex = std::make_unique<Texture>();
	grassTex->Name = "grassTex";
	grassTex->Filename = L"Textures/grass.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), grassTex->Filename.c_str(),
		grassTex->Resource, grassTex->UploadHeap));

	auto waterTex = std::make_unique<Texture>();
	waterTex->Name = "waterTex";
	waterTex->Filename = L"Textures/water1.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), waterTex->Filename.c_str(),
		waterTex->Resource, waterTex->UploadHeap));

	auto fenceTex = std::make_unique<Texture>();
	fenceTex->Name = "fenceTex";
	fenceTex->Filename = L"Textures/stone.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), fenceTex->Filename.c_str(),
		fenceTex->Resource, fenceTex->UploadHeap));

	mTextures[grassTex->Name] = std::move(grassTex);
	mTextures[waterTex->Name] = std::move(waterTex);
	mTextures[fenceTex->Name] = std::move(fenceTex);
}

void MyApp::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
    slotRootParameter[1].InitAsConstantBufferView(0);
    slotRootParameter[2].InitAsConstantBufferView(1);
    slotRootParameter[3].InitAsConstantBufferView(2);

	auto staticSamplers = GetStaticSamplers();

    // A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

    if(errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void MyApp::BuildDescriptorHeaps()
{
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 3;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	auto grassTex = mTextures["grassTex"]->Resource;
	auto waterTex = mTextures["waterTex"]->Resource;
	auto fenceTex = mTextures["fenceTex"]->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = grassTex->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 0xFFFFFFFF;
	md3dDevice->CreateShaderResourceView(grassTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = waterTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(waterTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = fenceTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(fenceTex.Get(), &srvDesc, hDescriptor);
}

void MyApp::BuildShadersAndInputLayout()
{
	const D3D_SHADER_MACRO defines[] =
	{
		NULL, NULL
	};

	const D3D_SHADER_MACRO waves_defines[] =
	{
		"WAVE", "1",
		NULL, NULL
	};

	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	mShaders["provincePS"] = d3dUtil::CompileShader(L"Shaders\\Province.hlsl", nullptr, "PS", "ps_5_0");
	mShaders["provinceGS"] = d3dUtil::CompileShader(L"Shaders\\GeometryShader.hlsl", nullptr, "GS", "gs_5_0");
	mShaders["provinceVS"] = d3dUtil::CompileShader(L"Shaders\\Province.hlsl", nullptr, "VS", "vs_5_0");

	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["waveVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", waves_defines, "VS", "vs_5_0");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "PS", "ps_5_0");
	mShaders["alphaTestedPS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", alphaTestDefines, "PS", "ps_5_0");
	
    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
	mInputLayout_prv =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "PROVCOLOR", 0, DXGI_FORMAT_R32G32B32_UINT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

template<typename T>
Color32 rgb2dex(const T& r, const T& g, const T& b)
{
	return ((r * 256) + g) * 256 + b;
}

void dex2rgb(float& r, float& g, float& b, const Color32& dex)
{
	r = ((dex & 16711680) / 65536) / 255.f;
	g = ((dex & 65208) / 256) / 255.f;
	b = (dex & 255) / 255.f;
}
void dex2rgb(unsigned char& r, unsigned char& g, unsigned char& b, const Color32& dex)
{
	r = static_cast<UCHAR>((dex & 16711680) / 65536);
	g = static_cast<UCHAR>((dex & 65208) / 256);
	b = static_cast<UCHAR>((dex & 255));
}
void dex2rgb(unsigned int& r, unsigned int& g, unsigned int& b, const Color32& dex)
{
	r = (dex & 16711680) / 65536;
	g = (dex & 65208) / 256;
	b = (dex & 255);
}

void MyApp::BuildLandGeometry()
{
	std::ifstream file("Map/map.bmp");
	std::ifstream prov_list("Map/prov.txt");
	std::ifstream prov_file("Map/prov.bmp");
	assert(file && prov_file && prov_list);
	{
		std::map<Color32, std::pair<ProvinceId, std::string>> prov_key;
		
		{
			std::string name, index, r, g, b;
			for (std::string line; std::getline(prov_list, line); )
			{
				for (size_t i = 0; i < line.size(); ++i)
				{
					if (line.at(i) == ' ')
					{
						line.erase(i--, 1);
						continue;
					}
				}
				name = line.substr(0, line.find('/'));
				line.erase(0, line.find('/') + 1);
				index = line.substr(0, line.find('='));
				line.erase(0, line.find('=') + 1);
				r = line.substr(0, line.find(','));
				line.erase(0, line.find(',') + 1);
				g = line.substr(0, line.find(','));
				line.erase(0, line.find(',') + 1);
				b = line;

				//OutputDebugStringA((index + "::(" + r + ", " + g + ", " + b + ")\n").c_str());

				prov_key.insert(std::make_pair(rgb2dex(std::stoi(r), std::stoi(g), std::stoi(b)), std::make_pair(std::stoi(index), name)));
			}
		}

		file.seekg(0, std::ios::end);
		prov_file.seekg(0, std::ios::end);
		std::streampos length = file.tellg();
		file.seekg(0, std::ios::beg);
		prov_file.seekg(0, std::ios::beg);

		OutputDebugStringA(("File Length : " + std::to_string(length) + "\n").c_str());
			
		std::vector<unsigned char> prov_buf(length);
		std::vector<unsigned char> buf(length);
		//char* buf = (char*)std::malloc(sizeof (char) * length);
		file.read((char*)&buf[0], length);
		prov_file.read((char*)&prov_buf[0], length);
		file.close();
		prov_file.close();


		//B-G-R

		size_t w = (((((int)buf[21] * 256) + buf[20]) * 256) + buf[19]) * 256 + buf[18];
		size_t h = (((((int)buf[25] * 256) + buf[24]) * 256) + buf[23]) * 256 + buf[22];

		map_w = w;
		map_h = h;

		OutputDebugStringA(("Map Size(w, h) = (" + std::to_string(w) + ", " + std::to_string(h) + ")\n").c_str());
		mLandVertices.resize(w * h);
		std::unordered_map<Color32, std::pair<XMUINT3, size_t>> unregisted_color;

		XMFLOAT3 vMinf3(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
		XMFLOAT3 vMaxf3(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);

		XMVECTOR vMin = XMLoadFloat3(&vMinf3);
		XMVECTOR vMax = XMLoadFloat3(&vMaxf3);
		
		Color32 ind;
		unsigned char r, g, b;
		float R0, G0, B0, R1, G1, B1, syc;
		size_t addr;
		Color32 dex;
		mWaves = std::make_unique<Waves>(map_h, map_w, 1.0f, 0.03f, 4.0f, 0.2f);
		for (size_t y = h - 1;; --y) {
			for (size_t x = 0; x < w; ++x) {
				//OutputDebugStringA((std::to_string(x) + ", " + std::to_string(y) + "\n").c_str());
				addr = 54 + (x + (y * w)) * 3;
				r = (int)buf[addr + 0];
				g = (int)buf[addr + 1];
				b = (int)buf[addr + 2];
				mLandVertices[x + y * w].Pos = { (x - (w - 1) / 2.f), (float)(r + g + b) / 128.0f - 1.5f, (y - (h - 1) / 2.f) };

				if (mLandVertices[x + y * w].Pos.y > 1.5f)
				{
					mLandVertices[x + y * w].Pos.y += powf(MathHelper::RandF(), 6) / 3.f;
				}

				mWaves->mPrevSolution[x + (h - 1 - y) * w].earth = mLandVertices[x + y * w].Pos.y;

				mLandVertices[x + y * w].TexC = { 1.f / (w - 1) * x, 1.f / (h - 1) * y};
				mLandVertices[x + y * w].Prov = 0;

				XMVECTOR Pos = XMLoadFloat3(&mLandVertices[x + y * w].Pos);
				
				vMin = XMVectorMin(vMin, Pos);
				vMax = XMVectorMax(vMax, Pos);
				 
				dex = rgb2dex(prov_buf.at(addr + 2), prov_buf.at(addr + 1), prov_buf.at(addr));
				
				//Registed Province Color
				if (auto search = prov_key.find(dex); search != prov_key.end())
				{
					mLandVertices[x + y * w].Prov = search->second.first;
					if (auto search_stack = prov_stack.find(search->second.first); search_stack == prov_stack.end())
					{
						prov_stack.insert(std::make_pair(search->second.first, Province(search->second.second,dex, mLandVertices[x + y * w].Pos.x, mLandVertices[x + y * w].Pos.y, mLandVertices[x + y * w].Pos.z)));
					}
					else
					{
						search_stack->second.px += mLandVertices[x + y * w].Pos.x;
						search_stack->second.py += mLandVertices[x + y * w].Pos.y;
						if (search_stack->second.ty < mLandVertices[x + y * w].Pos.y)
						{
							search_stack->second.ty = mLandVertices[x + y * w].Pos.y;
						}
						search_stack->second.pz += mLandVertices[x + y * w].Pos.z;
						++search_stack->second.p_num;
					}
				}
				else if (dex * (dex - 8421504) != 0)
				{

					if (auto O = unregisted_color.find(dex); O == unregisted_color.end())
					{
						XMUINT3 u3;
						syc = MathHelper::Infinity;
						dex2rgb(R0, G0, B0, dex);
						
						for (const auto& P : prov_key)
						{
							dex2rgb(R1, G1, B1, P.first);
							if (float get = powf(R0 - R1, 2.f) + powf(G0 - G1, 2.f) + powf(B0 - B1, 2.f); get < syc)
							{
								dex2rgb(u3.x, u3.y, u3.z, P.first);
								syc = get;
								ind = P.first;
							}
						}
						unregisted_color.insert(std::make_pair(dex,std::make_pair(XMUINT3(std::move(u3)),1)));
					}
					else
					{
						O->second.second++;
					}
				}


				//float dx = 0.f, dy = 0.f;

				XMFLOAT3 n = { 0.f, 1.f, 0.f };
				if (x > 0)
				{
					n.x = mLandVertices[x + y * w - 1].Pos.y - mLandVertices[x + y * w].Pos.y;
				}
				XMVECTOR unitNormal = XMVector3Normalize(XMLoadFloat3(&n));
				DirectX::XMStoreFloat3(&mLandVertices[x + y * w].Normal, unitNormal);
			}
			if (y == 0)
			{
				break;
			}
		}

		for (const auto& O : unregisted_color)
		{
			dex2rgb(r, g, b, O.first);
			OutputDebugStringA(("Unregisted Color (" + std::to_string(r) + ", " + std::to_string(g) + ", " + std::to_string(b) + ") x " + std::to_string(O.second.second) + " : It looks like (" + std::to_string(O.second.first.x) + ", " + std::to_string(O.second.first.y) + ", " + std::to_string(O.second.first.z) + ")\n").c_str());
		}

		concurrency::parallel_for((size_t)1, h - 1, [&](size_t i)
		{
			for (size_t j = 1; j < w - 1; ++j)
			{
				float l = mLandVertices[i*w + j - 1].Pos.y;
				float r = mLandVertices[i*w + j + 1].Pos.y;
				float t = mLandVertices[(i - 1)*w + j].Pos.y;
				float b = mLandVertices[(i + 1)*w + j].Pos.y;
				mLandVertices[j + i * w].Normal.x = -r + l;
				mLandVertices[j + i * w].Normal.y = 2.0f*1.0f;
				mLandVertices[j + i * w].Normal.z = b - t;

				XMVECTOR n = XMVector3Normalize(XMLoadFloat3(&mLandVertices[j + i * w].Normal));
				DirectX::XMStoreFloat3(&mLandVertices[j + i * w].Normal, n);

				//mLandVertices[i*w + j]. = XMFLOAT3(2.0f*mSpatialStep, r - l, 0.0f);
				//XMVECTOR T = XMVector3Normalize(XMLoadFloat3(&mTangentX[i*mNumCols + j]));
				//XMStoreFloat3(&mTangentX[i*mNumCols + j], T);
			}
		});




		std::vector<std::uint16_t> indices;
		
		for (std::uint16_t x = 0; x < w - 2; ++x)
		{
			for (std::uint16_t y = 0; y < h - 2; ++y)
			{
				indices.push_back(static_cast<std::uint16_t>(x + 1 + (y + 1) * w)); // 3
				indices.push_back(static_cast<std::uint16_t>(x + 1 + y * w)); // 1
				indices.push_back(static_cast<std::uint16_t>(x + y * w)); // 0

				indices.push_back(static_cast<std::uint16_t>(x + (y + 1) * w)); // 2
				indices.push_back(static_cast<std::uint16_t>(x + 1 + (y + 1) * w)); // 3
				indices.push_back(static_cast<std::uint16_t>(x + y * w)); // 0

			}
		}
		OutputDebugStringA(("Vertext Size, Indices Size = " + std::to_string(mLandVertices.size()) + " : " + std::to_string(indices.size()) + "\n").c_str());
		const UINT vbByteSize = (UINT)mLandVertices.size() * sizeof(VertexForProvince);

		const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

		auto geo = std::make_unique<MeshGeometry>();
		geo->Name = "landGeo";

		ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
		CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), mLandVertices.data(), vbByteSize);

		ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
		CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

		geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
			mCommandList.Get(), mLandVertices.data(), vbByteSize, geo->VertexBufferUploader);

		geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
			mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

		geo->VertexByteStride = sizeof(VertexForProvince);
		geo->VertexBufferByteSize = vbByteSize;
		geo->IndexFormat = DXGI_FORMAT_R16_UINT;
		geo->IndexBufferByteSize = ibByteSize;
		
		BoundingBox bounds;
		XMStoreFloat3(&bounds.Center, 0.5f*(vMin + vMax));
		XMStoreFloat3(&bounds.Extents, 0.5f*(vMax - vMin));

		SubmeshGeometry submesh;
		submesh.IndexCount = (UINT)indices.size();
		submesh.StartIndexLocation = 0;
		submesh.BaseVertexLocation = 0;
		submesh.Bounds = bounds;

		geo->DrawArgs["grid"] = submesh;

		mGeometries["landGeo"] = std::move(geo);
	}
}

void MyApp::BuildWavesGeometry()
{
    std::vector<std::uint16_t> indices(3 * mWaves->TriangleCount()); // 3 indices per face
	assert(mWaves->VertexCount() < 0x0000ffff);

    // Iterate over each quad.
    int m = mWaves->RowCount();
    int n = mWaves->ColumnCount();
    int k = 0;
    for(int i = 0; i < m - 1; ++i)
    {
        for(int j = 0; j < n - 1; ++j)
        {
            indices.at(k) = static_cast<std::uint16_t>(i*n + j);
            indices.at(k + 1) = static_cast<std::uint16_t>(i*n + j + 1);
            indices.at(k + 2) = static_cast<std::uint16_t>((i + 1)*n + j);

            indices.at(k + 3) = static_cast<std::uint16_t>((i + 1)*n + j);
            indices.at(k + 4) = static_cast<std::uint16_t>(i*n + j + 1);
            indices.at(k + 5) = static_cast<std::uint16_t>((i + 1)*n + j + 1);

            k += 6; // next quad
        }
    }

	UINT vbByteSize = mWaves->VertexCount()*sizeof(Vertex);
	UINT ibByteSize = (UINT)indices.size()*sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "waterGeo";

	// Set dynamically.
	geo->VertexBufferCPU = nullptr;
	geo->VertexBufferGPU = nullptr;

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;
	submesh.Bounds = BoundingBox();

	geo->DrawArgs["grid"] = submesh;

	mGeometries["waterGeo"] = std::move(geo);
}

void MyApp::BuildBoxGeometry()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);



	XMFLOAT3 vMinf3(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
	XMFLOAT3 vMaxf3(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);

	XMVECTOR vMin = XMLoadFloat3(&vMinf3);
	XMVECTOR vMax = XMLoadFloat3(&vMaxf3);


	std::vector<Vertex> vertices(box.Vertices.size());
	for (size_t i = 0; i < box.Vertices.size(); ++i)
	{
		auto& p = box.Vertices[i].Position;
		vertices[i].Pos = p;
		vertices[i].Normal = box.Vertices[i].Normal;
		vertices[i].TexC = box.Vertices[i].TexC;

		XMVECTOR P = XMLoadFloat3(&vertices[i].Pos);
		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

	std::vector<std::uint16_t> indices = box.GetIndices16();
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "boxGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	BoundingBox bounds;
	XMStoreFloat3(&bounds.Center, 0.5f*(vMin + vMax));
	XMStoreFloat3(&bounds.Extents, 0.5f*(vMax - vMin));

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;
	submesh.Bounds = bounds;

	geo->DrawArgs["box"] = submesh;

	mGeometries["boxGeo"] = std::move(geo);
}

void MyApp::BuildPSOs()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;
    ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS = 
	{ 
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()), 
		mShaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS = 
	{ 
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};
	
	auto Raster = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	Raster.MultisampleEnable = true;
	opaquePsoDesc.RasterizerState = Raster;
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));

	//
	// PSO for transparent objects
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentPsoDesc = opaquePsoDesc;

	D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
	transparencyBlendDesc.BlendEnable = true;
	transparencyBlendDesc.LogicOpEnable = false;
	transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	transparencyBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	transparentPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["waveVS"]->GetBufferPointer()),
		mShaders["waveVS"]->GetBufferSize()
	};
	transparentPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&transparentPsoDesc, IID_PPV_ARGS(&mPSOs["transparent"])));

	//
	// PSO for alpha tested objects
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC alphaTestedPsoDesc = opaquePsoDesc;
	alphaTestedPsoDesc.PS = 
	{ 
		reinterpret_cast<BYTE*>(mShaders["alphaTestedPS"]->GetBufferPointer()),
		mShaders["alphaTestedPS"]->GetBufferSize()
	};
	alphaTestedPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&alphaTestedPsoDesc, IID_PPV_ARGS(&mPSOs["alphaTested"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC ProvincePsoDesc = opaquePsoDesc;
	ProvincePsoDesc.InputLayout = { mInputLayout_prv.data(), (UINT)mInputLayout_prv.size() };
	ProvincePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["provincePS"]->GetBufferPointer()),
		mShaders["provincePS"]->GetBufferSize()
	};
	ProvincePsoDesc.GS = {
		reinterpret_cast<BYTE*>(mShaders["provinceGS"]->GetBufferPointer()),
		mShaders["provinceGS"]->GetBufferSize()
	};
	ProvincePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["provinceVS"]->GetBufferPointer()),
		mShaders["provinceVS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&ProvincePsoDesc, IID_PPV_ARGS(&mPSOs["province"])));


}

void MyApp::BuildFrameResources()
{
    for(int i = 0; i < gNumFrameResources; ++i)
    {
        mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
            1, (UINT)mAllRitems.size(), (UINT)mMaterials.size(), mWaves->VertexCount()));
    }
}

void MyApp::BuildMaterials()
{
	auto grass = std::make_unique<Material>();
	grass->Name = "grass";
	grass->MatCBIndex = 0;
	grass->DiffuseSrvHeapIndex = 0;
	grass->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	grass->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	grass->Roughness = 0.75f;

	// This is not a good water material definition, but we do not have all the rendering
	// tools we need (transparency, environment reflection), so we fake it for now.
	auto water = std::make_unique<Material>();
	water->Name = "water";
	water->MatCBIndex = 1;
	water->DiffuseSrvHeapIndex = 1;
	water->DiffuseAlbedo = XMFLOAT4(0.3f, 0.5f, 1.0f, 0.45f);
	water->FresnelR0 = XMFLOAT3(0.3f, 0.3f, 0.3f);
	water->Roughness = 0.0f;

	auto wirefence = std::make_unique<Material>();
	wirefence->Name = "wirefence";
	wirefence->MatCBIndex = 2;
	wirefence->DiffuseSrvHeapIndex = 2;
	wirefence->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	wirefence->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	wirefence->Roughness = 0.25f;

	mMaterials["grass"] = std::move(grass);
	mMaterials["water"] = std::move(water);
	mMaterials["wirefence"] = std::move(wirefence);
}

void MyApp::BuildRenderItems()
{
	UINT all_CBIndex = 0;

    auto wavesRitem = std::make_unique<RenderItem>();
    wavesRitem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&wavesRitem->TexTransform, XMMatrixScaling(5.0f, 5.0f, 1.0f));
	wavesRitem->ObjCBIndex = all_CBIndex++;
	wavesRitem->Mat = mMaterials["water"].get();
	wavesRitem->Geo = mGeometries["waterGeo"].get();
	wavesRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	wavesRitem->Bounds = wavesRitem->Geo->DrawArgs["grid"].Bounds;
	wavesRitem->IndexCount = wavesRitem->Geo->DrawArgs["grid"].IndexCount;
	wavesRitem->StartIndexLocation = wavesRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	wavesRitem->BaseVertexLocation = wavesRitem->Geo->DrawArgs["grid"].BaseVertexLocation;

    mWavesRitem = wavesRitem.get();

	mRitemLayer[(int)RenderLayer::Transparent].push_back(wavesRitem.get());

    auto gridRitem = std::make_unique<RenderItem>();
    gridRitem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&gridRitem->TexTransform, XMMatrixScaling(5.0f, 5.0f, 1.0f));
	gridRitem->ObjCBIndex = all_CBIndex++;
	gridRitem->Mat = mMaterials["grass"].get();
	gridRitem->Geo = mGeometries["landGeo"].get();
	gridRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->Bounds = gridRitem->Geo->DrawArgs["grid"].Bounds;
    gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
    gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
    gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
	gridRitem->Name = "Province";

	mRitemLayer[(int)RenderLayer::Province].push_back(gridRitem.get());

	
	auto boxRitem = RenderItem();

	

	boxRitem.ObjCBIndex = 2;
	boxRitem.Mat = mMaterials["wirefence"].get();
	boxRitem.Geo = mGeometries["boxGeo"].get();
	boxRitem.PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem.Bounds = boxRitem.Geo->DrawArgs["box"].Bounds;
	boxRitem.IndexCount = boxRitem.Geo->DrawArgs["box"].IndexCount;
	boxRitem.StartIndexLocation = boxRitem.Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem.BaseVertexLocation = boxRitem.Geo->DrawArgs["box"].BaseVertexLocation;

	float border_wdith = 10.f;
	float border_height = 2.f;

	/*{
		auto newboxRitem = boxRitem;
		auto newboxRitem_u = std::make_unique<RenderItem>(newboxRitem);

		newboxRitem_u->ObjCBIndex = all_CBIndex++;
		XMStoreFloat4x4(&newboxRitem_u->World, 
			XMMatrixScaling(border_wdith, border_wdith * border_height, 2.0f * map_h) +
			XMMatrixTranslation(1.0f * map_w + border_wdith / 2.f, border_wdith / 2.f, 0.0f)
		);

		mRitemLayer[(int)RenderLayer::Opaque].push_back(newboxRitem_u.get());
		mAllRitems.push_back(std::move(newboxRitem_u));
	}
	{
		auto newboxRitem = boxRitem;
		auto newboxRitem_u = std::make_unique<RenderItem>(newboxRitem);

		newboxRitem_u->ObjCBIndex = all_CBIndex++;
		XMStoreFloat4x4(&newboxRitem_u->World,
			XMMatrixScaling(border_wdith, border_wdith * border_height, 2.0f * map_h) +
			XMMatrixTranslation(- 1.0f * map_w - border_wdith / 2.f, border_wdith / 2.f, 0.0f)
		);

		mRitemLayer[(int)RenderLayer::Opaque].push_back(newboxRitem_u.get());
		mAllRitems.push_back(std::move(newboxRitem_u));
	}
	{
		auto newboxRitem = boxRitem;
		auto newboxRitem_u = std::make_unique<RenderItem>(newboxRitem);

		newboxRitem_u->ObjCBIndex = all_CBIndex++;
		XMStoreFloat4x4(&newboxRitem_u->World,
			XMMatrixScaling(2.0f * map_w + border_wdith * 2.f, border_wdith * border_height, border_wdith) +
			XMMatrixTranslation(0.0f, border_wdith / 2.f, -1.0f * map_h - border_wdith / 2.f)
		);

		mRitemLayer[(int)RenderLayer::Opaque].push_back(newboxRitem_u.get());
		mAllRitems.push_back(std::move(newboxRitem_u));
	}
	{
		auto newboxRitem = boxRitem;
		auto newboxRitem_u = std::make_unique<RenderItem>(newboxRitem);

		newboxRitem_u->ObjCBIndex = all_CBIndex++;
		XMStoreFloat4x4(&newboxRitem_u->World,
			XMMatrixScaling(2.0f * map_w + border_wdith * 2.f, border_wdith * border_height, border_wdith) +
			XMMatrixTranslation(0.0f, border_wdith / 2.f, 1.0f * map_h - border_wdith / 2.f)
		);

		mRitemLayer[(int)RenderLayer::Opaque].push_back(newboxRitem_u.get());
		mAllRitems.push_back(std::move(newboxRitem_u));
	}*/
	{
		auto newboxRitem = boxRitem;
		auto newboxRitem_u = std::make_unique<RenderItem>(newboxRitem);

		newboxRitem_u->ObjCBIndex = all_CBIndex++;
		XMStoreFloat4x4(&newboxRitem_u->World,
			XMMatrixTranslation(10.0f, 0.0f, 0.0f)
		);

		mRitemLayer[(int)RenderLayer::Opaque].push_back(newboxRitem_u.get());
		mAllRitems.push_back(std::move(newboxRitem_u));
	}
	/*for (size_t a = 0; a < map_w; ++a)
	{
		for (size_t b = 0; b < map_h; ++b)
		{
			const auto& P = mLandVertices.at(a + b * map_w);
			OutputDebugStringA(("(" + std::to_string(a) + ", " + std::to_string(b) + ") => (" + std::to_string(P.Pos.x + (map_w - 1.f) / 2.f) + ", " + std::to_string(P.Pos.z + (map_h - 1.f) / 2.f) + ")\n").c_str());
		}
	}*/

	//for (const auto& O : prov_stack)
	//{
	//	auto newboxRitem = boxRitem;
	//	auto newboxRitem_u = std::make_unique<RenderItem>(newboxRitem);

	//	
	//	int x = (int)(1.f * O.second.px / O.second.p_num + (map_w - 1.f) / 2.f);
	//	int y = (int)(1.f * O.second.pz / O.second.p_num + (map_h - 1.f) / 2.f);


	//	//int x = std::roundf(2.f * O.second.px / O.second.p_num + (map_w - 1) / 2.f);
	//	//int y = std::roundf(2.f * O.second.pz / O.second.p_num + (map_h - 1) / 2.f);
	//	
	//	newboxRitem_u->ObjCBIndex = all_CBIndex++;

	//	if (x >= 0 && x < map_w && y >= 0 && y < map_h)
	//	{
	//		//OutputDebugStringA((std::to_string(mLandVertices.at(x + map_w * y).Pos.x - 2.f * O.second.px / O.second.p_num) + " ; " + std::to_string(mLandVertices.at(x + map_w * y).Pos.z - 2.f * O.second.pz / O.second.p_num) + "\n").c_str());
	//		XMStoreFloat4x4(&newboxRitem_u->World,
	//			XMMatrixTranslation(
	//				2.f * O.second.px / O.second.p_num,
	//				2.f * mLandVertices.at(x + map_w * y).Pos.y - 2.f,
	//				2.f * O.second.pz / O.second.p_num)
	//			+
	//			XMMatrixScaling(5.0f, 5.0f, 5.0f)
	//		);
	//	}

	//	mRitemLayer[(int)RenderLayer::Opaque].push_back(newboxRitem_u.get());
	//	mAllRitems.push_back(std::move(newboxRitem_u));
	//}
		
	/*auto pickedRitem = std::make_unique<RenderItem>();
	pickedRitem->World = MathHelper::Identity4x4();
	pickedRitem->TexTransform = MathHelper::Identity4x4();
	pickedRitem->ObjCBIndex = 1;
	pickedRitem->Mat = mMaterials["wirefence"].get();
	pickedRitem->Geo = mGeometries["boxGeo"].get();
	pickedRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	pickedRitem->Visible = false;

	pickedRitem->IndexCount = 0;
	pickedRitem->StartIndexLocation = 0;
	pickedRitem->BaseVertexLocation = 0;
	mPickedRitem = pickedRitem.get();
	mRitemLayer[(int)RenderLayer::Opaque].push_back(pickedRitem.get());*/


	//mAllRitems.push_back(std::move(pickedRitem));
    mAllRitems.push_back(std::move(wavesRitem));
    mAllRitems.push_back(std::move(gridRitem));


}

void MyApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto matCB = mCurrFrameResource->MaterialCB->Resource();

    // For each render item...
    for(size_t i = 0; i < ritems.size(); ++i)
    {
        auto ri = ritems[i];
		
		if (!ri->Visible)
			continue;

		
        cmdList->IASetVertexBuffers(0, 1, new D3D12_VERTEX_BUFFER_VIEW(ri->Geo->VertexBufferView()));
        cmdList->IASetIndexBuffer(new D3D12_INDEX_BUFFER_VIEW(ri->Geo->IndexBufferView()));
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		tex.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);

        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex*objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex*matCBByteSize;

		cmdList->SetGraphicsRootDescriptorTable(0, tex);
        cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
        cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);

        cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> MyApp::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return { 
		pointWrap, pointClamp,
		linearWrap, linearClamp, 
		anisotropicWrap, anisotropicClamp };
}


