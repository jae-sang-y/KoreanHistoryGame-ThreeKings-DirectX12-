//***************************************************************************************
// MyApp.cpp by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

#include "Common/d3dApp.h"
#include "Common/MathHelper.h"
#include "Common/UploadBuffer.h"
#include "Common/GeometryGenerator.h"
#include "Waves.h"
#include "YTML.h"
#include "Yscript.h"

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
	r = static_cast<unsigned char>((dex & 16711680) / 65536);
	g = static_cast<unsigned char>((dex & 65208) / 256);
	b = static_cast<unsigned char>((dex & 255));
}
void dex2rgb(unsigned int& r, unsigned int& g, unsigned int& b, const Color32& dex)
{
	r = (dex & 16711680) / 65536;
	g = (dex & 65208) / 256;
	b = (dex & 255);
}

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
	void BuildImage();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);
	void Pick(WPARAM btnState, int sx, int sy);
	void LoadSizeDependentResources();
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();
	const XMFLOAT3 MyApp::Convert3Dto2D(XMVECTOR pos);
	void UILayerInitialize();
	void UILayerResize();
	void GameInit();
	void GameUpdate();
	void GameStep();
	void GameSave();
	void GameLoad();
	void GameClose();
	void ProvinceMousedown(WPARAM btnState, ProvinceId id);
	void CreateBrush();

	void Query(const std::wstring& query);
	void Execute(const std::wstring& func_name, const std::uint64_t& uuid);

private:

	UINT mCbvSrvDescriptorSize = 0;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	std::wofstream log_main;

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
		std::wstring name;
		Color32 color;
		std::uint64_t p_num = 1;
		XMFLOAT3 pixel;
		XMFLOAT3 on3Dpos;


		bool is_rebel = false;
		NationId owner = 0;
		NationId ruler = 0;

		Province()
		{
		}
		Province(std::wstring _name, Color32 _color, XMFLOAT3 _pixel) :name(_name), color(_color), pixel(_pixel)
		{

		}
	};

	using WCHAR3 = WCHAR[3];
	struct Nation
	{
		XMFLOAT4 MainColor = { 0.f, 0.f, 0.f, 0.f };
		std::wstring MainName = L"오류";
	};
	struct Leader
	{
		ProvinceId location;
		Leader(const ProvinceId& loc) : location(loc) {};
	};
	struct Data
	{
		std::map<ProvinceId, std::unique_ptr<Province>> province;
		std::unordered_map<NationId, std::unique_ptr<Nation>>  nations;
		std::list<std::unique_ptr<Leader>> leaders;

	};

	std::unique_ptr<Data> m_gamedata = std::make_unique<Data>();

	XMVECTOR mEyetarget = XMVectorSet(0.0f, 15.0f, 0.0f, 0.0f);

	std::unordered_map<std::wstring, std::wstring> captions;
	std::unordered_map<std::wstring, HBITMAP> mBitmap;

	std::shared_ptr<YTML::DrawItemList> m_DrawItems = std::make_shared<YTML::DrawItemList>();
	//YScript::YScript m_script;

	float mEyeMoveX = 0.f;
	float mEyeMoveZ = 0.f;

	bool mUI_isInitial = false;

	struct Query
	{
		bool enable = false;

		bool isString = false;
		bool isLineComment = false;
		bool isComment = false;

		size_t pos;
		std::vector<std::wstring> word;
		std::wstring index;

		ProvinceId tag_prov;
		decltype(Data::province)::iterator tag_prov_it;

	} mQuery;

	D2D1_POINT_2F point;
	D2D1_RECT_F rect;
	D2D1_SIZE_F size;
	D2D1_SIZE_F scale;

	std::uint64_t focus = 0;
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
		if (!theApp.Initialize())
			return 0;

		return theApp.Run();
	}
	catch (DxException& e)
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
	if (md3dDevice != nullptr)
		FlushCommandQueue();
}

bool MyApp::Initialize()
{
	std::locale::global(std::locale(""));

	srand((unsigned int)time(nullptr));
	log_main.open("Log/main.log");

	if (!D3DApp::Initialize())
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

	mUI_isInitial = false;
	D3DApp::OnResize();

	// The window resized, so update the aspect ratio and recompute the projection matrix.
	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, P);
}

void MyApp::BuildImage()
{
	auto loadBitmap = [this](std::wstring filename, std::wstring imagename) {

		ThrowIfFailed(CoCreateInstance(
			CLSID_WICImagingFactory,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(m_d2d->pIWICFactory.GetAddressOf())
		));

		ComPtr<IWICBitmapDecoder> pDecoder = nullptr;
		ComPtr<IWICBitmapFrameDecode> pFrame = nullptr;

		ThrowIfFailed(m_d2d->pIWICFactory->CreateDecoderFromFilename(
			filename.c_str(),                      // Image to be decoded
			NULL,                            // Do not prefer a particular vendor
			GENERIC_READ,                    // Desired read access to the file
			WICDecodeMetadataCacheOnDemand,  // Cache metadata when needed
			pDecoder.GetAddressOf()                        // Pointer to the decoder
		));

		// Retrieve the first frame of the image from the decoder

		ThrowIfFailed(pDecoder->GetFrame(0, pFrame.GetAddressOf()));

		pFrame->GetSize(&m_d2d->pD2DBitmap[imagename].width, &m_d2d->pD2DBitmap[imagename].height);

		m_d2d->pConvertedSourceBitmap.Reset();
		ThrowIfFailed(m_d2d->pIWICFactory->CreateFormatConverter(m_d2d->pConvertedSourceBitmap.GetAddressOf()));

		ThrowIfFailed(m_d2d->pConvertedSourceBitmap->Initialize(
			pFrame.Get(),                          // Input bitmap to convert
			GUID_WICPixelFormat32bppPBGRA,   // Destination pixel format
			WICBitmapDitherTypeNone,         // Specified dither pattern
			nullptr,                         // Specify a particular palette 
			0.f,                             // Alpha threshold
			WICBitmapPaletteTypeCustom       // Palette translation type
		));

		ThrowIfFailed(m_d2d->d2dDeviceContext->CreateBitmapFromWicBitmap(m_d2d->pConvertedSourceBitmap.Get(), nullptr, m_d2d->pD2DBitmap[imagename].data.GetAddressOf()));

	};
	loadBitmap(LR"(Images\cursor.png)", L"Cursor");
	loadBitmap(LR"(Images\form.png)", L"Form");
	loadBitmap(LR"(Images\window.png)", L"Window");
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
	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
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

const XMFLOAT3 MyApp::Convert3Dto2D(FXMVECTOR pos)
{
	XMVECTOR _pos = XMVector3Transform(pos, XMLoadFloat4x4(&mView));
	_pos = XMVector3Transform(_pos, XMLoadFloat4x4(&mProj));

	XMFLOAT3 newPos;
	XMStoreFloat3(&newPos, _pos);

	newPos.x /= newPos.z;
	newPos.y /= newPos.z;

	newPos.x = mClientWidth * (newPos.x + 1.0f) / 2.0f;
	newPos.y = mClientHeight * (1.0f - ((newPos.y + 1.0f) / 2.0f));

	return newPos;
}

void MyApp::DrawUI()
{
	auto& g = m_d2d->d2dDeviceContext;

	//if (mRadius < 64.f)


	g->DrawText(mUser.DebugText.c_str(), (UINT32)mUser.DebugText.length(),
		m_d2d->textFormat[L"Debug"].Get(), D2D1::RectF(0.0f, 0.0f, mClientWidth / 3.f, 1.f * mClientHeight),
		m_d2d->Brush[L"White"].Get());

	m_DrawItems->Sort();
	for (auto& O : m_DrawItems->data)
	{
		auto& A = O.Attribute;
		if (A[L"enable"] == L"disable")
			continue;

		try
		{
			m_d2d->Brush[L"Temp"]->SetColor(D2D1::ColorF(Float(A[L"color-r"]), Float(A[L"color-g"]), Float(A[L"color-b"])));
			m_d2d->Brush[L"Temp"]->SetOpacity(1.0f);// Float(A[L"opacity"]));

			point.x = Float(A[L"left"]);
			point.y = Float(A[L"top"]);

			size.width = Float(A[L"width"]);
			size.height = Float(A[L"height"]);

			if (A[L"horizontal-align"] == L"center")
			{
				rect.left = point.x - size.width / 2.f;
				rect.right = point.x + size.width / 2.f;
			}
			else if (A[L"horizontal-align"] == L"right")
			{
				rect.left = point.x - size.width;
				rect.right = point.x;
			}
			else
			{
				rect.left = point.x;
				rect.right = point.x + size.width;
			}

			if (A[L"vertical-align"] == L"center")
			{
				rect.top = point.y - size.height / 2.f;
				rect.bottom = point.y + size.height / 2.f;
			}
			else if (A[L"vertical-align"] == L"top")
			{
				rect.top = point.y - size.height;
				rect.bottom = point.y;
			}
			else
			{
				rect.top = point.y;
				rect.bottom = point.y + size.height;
			}


			if (A[L"tag"] == L"div")
			{
				g->FillRectangle(rect, m_d2d->Brush[L"Temp"].Get());
				//OutputDebugStringW(((A[L"left"]) + L" : " + (A[L"top"]) + L"\n").c_str());
			}
			else if (A[L"tag"] == L"a")
			{
				m_d2d->textLayout.Reset();
				ThrowIfFailed(m_d2d->dwriteFactory->CreateTextLayout(
					A[L"text"].c_str(), (UINT32)A[L"text"].length(),
					m_d2d->textFormat[L"Above Province"].Get(),
					size.width, size.height,
					m_d2d->textLayout.GetAddressOf()
				));
				m_d2d->textLayout->SetFontSize(size.height, { 0U,  (UINT32)A[L"text"].length() });

				g->DrawTextLayout(D2D1::Point2F(rect.left, rect.top), m_d2d->textLayout.Get(),
					m_d2d->Brush[L"Temp"].Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP
				);
			}
			else if (A[L"tag"] == L"img")
			{
				auto& B = m_d2d->pD2DBitmap[A[L"src"]];
				m_d2d->BitmapBrush->SetBitmap(B.data.Get());

				scale.width = (rect.right - rect.left) / B.width;
				scale.height = (rect.bottom - rect.top) / B.height;

				m_d2d->BitmapBrush->SetTransform(
					D2D1::Matrix3x2F::Translation(rect.left / scale.width, rect.top / scale.height) *
					D2D1::Matrix3x2F::Scale(scale.width, scale.height)
				);
				//g->FillRectangle(rect, m_d2d->Brush[L"White"].Get());
				g->FillRectangle(rect, m_d2d->BitmapBrush.Get());
			}
		}
		catch (const std::exception&)
		{
			OutputDebugStringW(L"[");
			OutputDebugStringW(A[L"id"].c_str());
			OutputDebugStringW(L"]:[");
			OutputDebugStringW(A[L"class"].c_str());
			OutputDebugStringW(L"]<");
			OutputDebugStringW(A[L"tag"].c_str());
			OutputDebugStringW(L">\n");
			A[L"enable"] = L"disable";
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
		ID3D11Resource* ppResources[] = { m_d2d->wrappedRenderTargets[m_frameIndex].Get() };

		m_d2d->d2dDeviceContext->SetTarget(m_d2d->d2dRenderTargets[m_frameIndex].Get());

		m_d2d->d3d11On12Device->AcquireWrappedResources(ppResources, _countof(ppResources));
		m_d2d->d2dDeviceContext->BeginDraw();

		DrawUI();

		m_d2d->d2dDeviceContext->EndDraw();
		m_d2d->d3d11On12Device->ReleaseWrappedResources(ppResources, _countof(ppResources));

		m_d2d->d3d11DeviceContext->Flush();

	}
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	mCurrFrameResource->Fence = ++mCurrentFence;

	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void MyApp::Query(const std::wstring& query)
{
	if (mQuery.enable)
	{
		if (query == L"SAVE\tEND")
		{
			mQuery.enable = false;
			OutputDebugStringW(L"SAVE END\n");
			return;
		}


		mQuery.word.clear();

		for (mQuery.pos = 0; mQuery.pos < query.size(); ++mQuery.pos)
		{
			auto ch = query.at(mQuery.pos);

			if (mQuery.isLineComment)
			{
				if (ch == L'\n' || ch == L'\r')
					mQuery.isLineComment = false;
			}
			else if (mQuery.isComment)
			{
				if (ch == L'*' && mQuery.pos + 1 < query.size())
					if (query.at(mQuery.pos + 1) == L'*')
					{
						mQuery.isComment = false;
						++mQuery.pos;
					}
			}
			else if (ch == L'"')
				mQuery.isString = !mQuery.isString;
			else if (mQuery.isString)
				mQuery.word.rbegin()->push_back(ch);
			else if (ch == L'\t' || ch == L' ' || ch == L'\n' || ch == L'\r')
				mQuery.word.push_back(L"");
			else if (ch == L'/' && mQuery.pos + 1 < query.size())
				if (query.at(mQuery.pos + 1) == L'/')
				{
					mQuery.isLineComment = true;
					++mQuery.pos;
				}
				else if (query.at(mQuery.pos + 1) == L'*')
				{
					mQuery.isComment = true;
					++mQuery.pos;
				}
				else
					mQuery.word.rbegin()->push_back(ch);
			else
				mQuery.word.rbegin()->push_back(ch);
		}

		if (mQuery.word.size() > 0)
		{
			mQuery.index = L"";
			if (*mQuery.word.cbegin() == L"PROVINCE")
			{
				mQuery.tag_prov_it = m_gamedata->province.end();

				for (auto O : mQuery.word)
				{
					if (O.at(0) == L'-')
						mQuery.index = O;
					else
					{
						if (mQuery.index == L"-id")
						{
							mQuery.tag_prov = Long(O);
							mQuery.tag_prov_it = m_gamedata->province.find(mQuery.tag_prov);
						}
						else if (mQuery.index == L"-ruler")
						{
							if (mQuery.tag_prov_it != m_gamedata->province.end())
							{
								mQuery.tag_prov_it->second->ruler = Long(O);
							}
						}
						else if (mQuery.index == L"-owner")
						{
							if (mQuery.tag_prov_it != m_gamedata->province.end())
							{
								mQuery.tag_prov_it->second->owner = Long(O);
							}
						}

					}
				}
			}

		}
		else
		{
			OutputDebugStringA("E");
		}
	}
	else if (query == L"SAVE\tSTART")
	{
		mQuery.enable = true;
		OutputDebugStringW(L"SAVE START\n");
	}
}
void MyApp::GameSave()
{
	captions[L"파일 입출력"] = L"보관시작";
	std::wofstream file(L"UserData/Nation");


	file << L"SAVE\tSTART" << L";\n";

	for (const auto& O : m_gamedata->province)
	{
		file << L"PROVINCE" << L"\t" << L"-id " << std::to_wstring(O.first)
			<< L" -ruler " << std::to_wstring(O.second->ruler)
			<< L" -owner " << std::to_wstring(O.second->owner) << L";\n";
	}

	file << L"SAVE\tEND" << L";\n";

	file.close();
	captions[L"파일 입출력"] = L"보관종료";
}
void MyApp::GameLoad()
{
	captions[L"파일 입출력"] = L"적재시작";
	std::wifstream file(L"UserData/Nation");

	if (!file) {
		log_main << "[GameLoad Failed]";
		return;
	}

	file.seekg(0, std::ios::end);
	size_t length = file.tellg();

	std::vector<wchar_t> buf(length);

	file.seekg(0);
	file.read(&buf[0], length);
	file.close();

	std::wstring wstr;
	wstr.assign(buf.begin(), buf.end());
	size_t cursor = 0, next;
	while (1)
	{
		next = wstr.find(L';', cursor);
		if (next == std::wstring::npos)
		{
			Query(wstr.substr(cursor));
			break;
		}
		Query(wstr.substr(cursor, next - cursor));
		cursor = next + 1;
	}


	captions[L"파일 입출력"] = L"적재종료";

}

void MyApp::GameInit()
{
	NationId nation_count = 0;
	{
		std::unique_ptr<Nation> 신라 = std::make_unique<Nation>();
		신라->MainColor = XMFLOAT4(0.5f, 0.5f, 0.1f, 0.75f);
		신라->MainName = L"신라";
		m_gamedata->nations[++nation_count] = std::move(신라);

		std::unique_ptr<Nation> 백제 = std::make_unique<Nation>();
		백제->MainColor = XMFLOAT4(0.1f, 0.5f, 0.45f, 0.75f);
		백제->MainName = L"백제";
		m_gamedata->nations[++nation_count] = std::move(백제);

		std::unique_ptr<Nation> 고구려 = std::make_unique<Nation>();
		고구려->MainColor = XMFLOAT4(0.4f, 0.0f, 0.0f, 0.75f);
		고구려->MainName = L"고구려";
		m_gamedata->nations[++nation_count] = std::move(고구려);
	}

	m_gamedata->leaders.push_back(std::make_unique<Leader>(10));

	m_DrawItems->Insert(LR"(<img id="leader0" src="Window" z-index="100000" left="0" top="0" width="40" height="40" pointer-events="none">)");


	wchar_t buf[256];
	for (const auto& O : m_gamedata->province)
	{
		if (O.second->p_num)
		{
			swprintf_s(buf, LR"(<div id="prov%.0lf" enable="disable" opacity="0.5">)", (double)O.first);
			m_DrawItems->Insert(buf);
			swprintf_s(buf, LR"(<div id="provarmy%.0lf" enable="disable" opacity="0.5">)", (double)O.first);
			m_DrawItems->Insert(buf);
			swprintf_s(buf, LR"(<a id="provtext%.0lf" enable="disable" opacity="1.0">)", (double)O.first);
			m_DrawItems->Insert(buf);
		}
		//captions[buf] = L"";
	}


	m_DrawItems->Insert(LR"(<img class="myForm" src="Form" left="100" top="100" width="200" height="240" mousedown="FormHold" mouseup="FormUnhold" z-index="2">)");
	m_DrawItems->Insert(LR"(<img class="myForm" src="Form" left="400" top="100" width="200" height="240" mousedown="FormHold" mouseup="FormUnhold" z-index="2">)");
	m_DrawItems->Insert(LR"(<img id="myDiv" src="Cursor" z-index="100000" left="0" top="0" width="40" height="40" pointer-events="none">)");

	GameLoad();
}

void MyApp::Execute(const std::wstring& func_name, const std::uint64_t& uuid)
{
	auto& O = m_DrawItems->withUUID(uuid);

	if (func_name == L"FormHold")
	{
		O->Attribute[L"hold"] = L"1";
		O->Attribute[L"FirstMousePos.x"] = Str(mLastMousePos.x);
		O->Attribute[L"FirstMousePos.y"] = Str(mLastMousePos.y);
		O->Attribute[L"z-index"] = L"2.5";
	}
	else if (func_name == L"FormUnhold")
	{
		O->Attribute[L"hold"] = L"0";
		O->Attribute[L"z-index"] = L"2";
	}

	//O->Attribute[L"left"] = Str(Float(O->Attribute[L"left"]) + 5.f);



}

void MyApp::GameUpdate()
{
	m_DrawItems->withID(L"myDiv",
		{
			L"left", Str(mLastMousePos.x),
			L"top", Str(mLastMousePos.y)
		}
	);

	{
		//OutputDebugStringW(Str(m_DrawItems->withClass(L"myForm").size()).c_str());
		auto List = m_DrawItems->withClass(L"myForm");
		//OutputDebugStringW(Str(List.size()).c_str());
		for (auto& O : List)
		{
			if (O->Attribute[L"hold"] == L"1")
			{
				O->Attribute[L"left"] = Str(Float(O->Attribute[L"left"]) + mLastMousePos.x - Float(O->Attribute[L"FirstMousePos.x"]));
				O->Attribute[L"top"] = Str(Float(O->Attribute[L"top"]) + mLastMousePos.y - Float(O->Attribute[L"FirstMousePos.y"]));
				O->Attribute[L"FirstMousePos.x"] = Str(mLastMousePos.x);
				O->Attribute[L"FirstMousePos.y"] = Str(mLastMousePos.y);

				//O->z_index = 1.f;
			}
			else
			{
				//O->z_index = 0.f;
			}

		}
	}


	XMFLOAT4 rgb;
	for (const auto& O : m_gamedata->province)
	{
		if (!O.second->p_num)
		{
			continue;
		}
		rgb = XMFLOAT4(0.f, 0.f, 0.f, 0.f);
		mMainPassCB.gSubProv[O.first] = XMFLOAT4(0.f, 0.f, 0.f, 0.f);
		mMainPassCB.gProv[O.first] = { 0.f,0.f,0.f,0.f };

		if (const auto& P = m_gamedata->nations.find(O.second->owner); P != m_gamedata->nations.end())
		{
			rgb = P->second->MainColor;
			mMainPassCB.gProv[O.first] = P->second->MainColor;
			mMainPassCB.gSubProv[O.first] = P->second->MainColor;
		}
		if (const auto& P = m_gamedata->nations.find(O.second->ruler); P != m_gamedata->nations.end())
		{
			mMainPassCB.gSubProv[O.first] = P->second->MainColor;
		}


		XMFLOAT3 pos = O.second->on3Dpos;
		pos.x /= 2.f;
		pos.y /= 2.f;
		pos.z /= 2.f;
		pos.y += 3.f;
		XMFLOAT3 s = Convert3Dto2D(XMLoadFloat3(&pos));
		pos.y -= 3.f;
		XMFLOAT3 s2 = Convert3Dto2D(XMLoadFloat3(&pos));
//		OutputDebugStringW((Str(pos.x) + L" : " + Str(pos.y) + L" : " + Str(pos.z) + L"\n").c_str());
//		OutputDebugStringW((Str(s.x) + L" : " + Str(s.y) + L" : " + Str(s.z) + L"\n\n").c_str());

		float w = mClientHeight / 30.f * O.second->name.length() + 10.f;
		float h = mClientHeight / 25.f;
		float size = 25.0f / s.z;
		float depth = (s.z - 1.f) / (1000.f - 1.f);
		if (s.z >= 1.f && s.z <= 1000.0f)
		{
			w *= size;
			h *= size;
			m_DrawItems->withID(L"prov" + Str(O.first),
				{
					L"color-r", Str(rgb.x / 1.5f),
					L"color-g", Str(rgb.y / 1.5f),
					L"color-b", Str(rgb.z / 1.5f),
					L"enable", L"enable",
					L"left", Str(s.x),
					L"top",Str(s.y),
					L"width", Str(w),
					L"height",Str(h),
					L"z-index", Str(1 - depth),
					L"border", L"line",
					L"horizontal-align", L"center",
					L"vertical-align", L"center"
				}
			);

			m_DrawItems->withID(L"provtext" + Str(O.first),
				{
					L"color-r",  L"1",
					L"color-g", L"1",
					L"color-b",  L"1",
					L"enable", L"enable",
					L"left", Str(s.x),
					L"top", Str(s.y),
					L"width", Str(w),
					L"height", Str(h),
					L"text", O.second->name,
					L"z-index", Str(1 - depth),
					L"horizontal-align", L"center",
					L"vertical-align", L"center"
				}
			);
		}
		else
		{
			m_DrawItems->withID(L"prov" + Str(O.first),
				{
					L"enable", L"disable",
					L"left", L"0",
					L"top", L"0"
				}
			);

			m_DrawItems->withID(L"provtext" + Str(O.first),
				{
					L"enable", L"disable",
					L"left", L"0",
					L"top", L"0"
				}
			);
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
}

void MyApp::GameClose()
{
	for (auto& O : mBitmap)
		DeleteObject(O.second);
	log_main.close();
	if (m_fullscreenMode)
		ToggleFullscreenWindow();
	m_d2d.reset();
}

void MyApp::ProvinceMousedown(WPARAM btnState, ProvinceId id)
{
	auto prov = m_gamedata->province.find(id);
	if (prov == m_gamedata->province.end())
		return;
	captions[L"선택한 프로빈스"] = std::to_wstring(id);
	captions[L"선택한 프로빈스.x"] = std::to_wstring(2.f * prov->second->on3Dpos.x / prov->second->p_num);
	captions[L"선택한 프로빈스.z"] = std::to_wstring(2.f * prov->second->on3Dpos.z / prov->second->p_num);

	if (btnState & MK_LBUTTON)
	{
		if (prov->second->owner == 0)
		{
			prov->second->ruler = mUser.nationPick;
			prov->second->owner = mUser.nationPick;
		}
		else if (prov->second->ruler != mUser.nationPick)
		{
			prov->second->ruler = mUser.nationPick;
		}
		else
		{
			prov->second->owner = mUser.nationPick;
		}
		return;
	}
	if (btnState & MK_RBUTTON)
	{
		prov->second->ruler = mUser.nationPick;

		return;
	}
	if (btnState & MK_MBUTTON)
	{

		if (const auto& O = m_gamedata->nations.find(prov->second->owner); O != m_gamedata->nations.end())
		{
			mUser.nationPick = prov->second->owner;
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
		const auto& key = std::next(std::begin(m_gamedata->nations), rand() % m_gamedata->nations.size());
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

	//if (true)//m_enableUI)
	{
		if (!mUI_isInitial)
		{
			UILayerInitialize();
			BuildImage();
			//m_textBlocks.resize(1);
			mUI_isInitial = true;
		}
		UILayerResize();
	}
}
void MyApp::UILayerResize()
{
	//auto m_width = static_cast<float>(mClientWidth);
	auto m_height = static_cast<float>(mClientHeight);

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
	for (UINT i = 0; i < m_d2d->wrappedRenderTargets.size(); i++)
	{
		D3D11_RESOURCE_FLAGS d3d11Flags = { D3D11_BIND_RENDER_TARGET };
		ThrowIfFailed(m_d2d->d3d11On12Device->CreateWrappedResource(
			mSwapChainBuffer[i].Get(),
			&d3d11Flags,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT,
			IID_PPV_ARGS(&m_d2d->wrappedRenderTargets[i])));

		// Create a render target for D2D to draw directly to this back buffer.
		ComPtr<IDXGISurface> surface;
		ThrowIfFailed(m_d2d->wrappedRenderTargets[i].As(&surface));
		ThrowIfFailed(m_d2d->d2dDeviceContext->CreateBitmapFromDxgiSurface(
			surface.Get(),
			&bitmapProperties,
			&m_d2d->d2dRenderTargets[i]));
	}

	ThrowIfFailed(m_d2d->d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_d2d->d2dDeviceContext));
	m_d2d->d2dDeviceContext->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
	CreateBrush();

	// Create DWrite text format objects.
	const float fontSize = m_height / 30.0f;
	const float smallFontSize = m_height / 40.0f;


	ThrowIfFailed(m_d2d->dwriteFactory->CreateTextFormat(
		L"EBSJSK",
		m_d2d->dwFontColl.Get(),
		DWRITE_FONT_WEIGHT_NORMAL,
		DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_NORMAL,
		fontSize,
		L"",
		&m_d2d->textFormat[L"Above Province"]));

	ThrowIfFailed(m_d2d->textFormat[L"Above Province"]->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER));
	ThrowIfFailed(m_d2d->textFormat[L"Above Province"]->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR));


	ThrowIfFailed(m_d2d->dwriteFactory->CreateTextFormat(
		L"Consolas",
		nullptr,
		DWRITE_FONT_WEIGHT_NORMAL,
		DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_NORMAL,
		fontSize,
		L"",
		&m_d2d->textFormat[L"Debug"]));

	ThrowIfFailed(m_d2d->textFormat[L"Debug"]->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER));
	ThrowIfFailed(m_d2d->textFormat[L"Debug"]->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR));

}
void MyApp::UILayerInitialize()
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
	ID3D12CommandQueue* ppCommandQueues[] = { mCommandQueue.Get() };
	ThrowIfFailed(D3D11On12CreateDevice(
		md3dDevice.Get(),
		d3d11DeviceFlags,
		nullptr,
		0,
		reinterpret_cast<IUnknown**>(ppCommandQueues),
		_countof(ppCommandQueues),
		0,
		&d3d11Device,
		&m_d2d->d3d11DeviceContext,
		nullptr
	));

	// Query the 11On12 device from the 11 device.
	ThrowIfFailed(d3d11Device.As(&m_d2d->d3d11On12Device));

#if defined(_DEBUG) || defined(DBG)
	// Filter a debug error coming from the 11on12 layer.
	ComPtr<ID3D12InfoQueue> infoQueue;
	if (SUCCEEDED(md3dDevice.Get()->QueryInterface(IID_PPV_ARGS(&infoQueue))))
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
		ThrowIfFailed(m_d2d->d3d11On12Device.As(&dxgiDevice));

		ThrowIfFailed(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory3), &d2dFactoryOptions, &m_d2d->d2dFactory));
		ThrowIfFailed(m_d2d->d2dFactory->CreateDevice(dxgiDevice.Get(), &m_d2d->d2dDevice));
		ThrowIfFailed(m_d2d->d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_d2d->d2dDeviceContext));

		m_d2d->d2dDeviceContext->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

		CreateBrush();

		ThrowIfFailed(m_d2d->d2dDeviceContext->CreateSolidColorBrush(
			D2D1::ColorF(D2D1::ColorF::Black), &m_d2d->Brush[L"Temp"]));

		ThrowIfFailed(m_d2d->d2dDeviceContext->CreateBitmapBrush(nullptr, m_d2d->BitmapBrush.GetAddressOf()));

		ThrowIfFailed(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), &m_d2d->dwriteFactory));
	}

	ComPtr<IDWriteFontFile> tFontFile = nullptr;
	ComPtr<IDWriteFontFileLoader> fontFileLoader = nullptr;
	ComPtr<IDWriteFontCollectionLoader> fontCollLoader = nullptr;

	ThrowIfFailed(AddFontResourceW(L"Resource\\Fonts\\Ruzicka TypeK.ttf"));
	ThrowIfFailed(m_d2d->dwriteFactory->GetSystemFontCollection(m_d2d->dwFontColl.GetAddressOf(), false))

		//for (UINT32 i = 0U; i < m_d2d->dwFontColl->GetFontFamilyCount(); ++i)
		//{
		//	//OutputDebugStringW(std::to_wstring(i).c_str());
		//	//OutputDebugStringW(L" : try\n");
		//	IDWriteFontFamily* ff = nullptr;
		//	m_d2d->dwFontColl->GetFontFamily(i, &ff);

		//	IDWriteLocalizedStrings* pFamilyNames = nullptr;

		//	ff->GetFamilyNames(&pFamilyNames);

		//	UINT32 index = 0U;

		//	wchar_t localeName[LOCALE_NAME_MAX_LENGTH];
		//	BOOL exists = false;

		//	GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH);

		//	pFamilyNames->FindLocaleName(localeName, &index, &exists);

		//	if (!exists)
		//	{
		//		pFamilyNames->FindLocaleName(L"en-us", &index, &exists);
		//	}

		//	if (!exists)
		//		index = 0U;

		//	UINT32 length = 0U;

		//	pFamilyNames->GetStringLength(index, &length);

		//	std::vector<wchar_t> name(length + 1);
		//	pFamilyNames->GetString(index, name.data(), length + 1U);

		//	//OutputDebugStringW(name.data());
		//	//OutputDebugStringW(L"\n");
		//}

}

void MyApp::CreateBrush()
{
	ThrowIfFailed(m_d2d->d2dDeviceContext->CreateSolidColorBrush(
		D2D1::ColorF(D2D1::ColorF::White), &m_d2d->Brush[L"White"]));
	ThrowIfFailed(m_d2d->d2dDeviceContext->CreateSolidColorBrush(
		D2D1::ColorF(D2D1::ColorF::Black), &m_d2d->Brush[L"Black"]));
}

void MyApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	for (auto& O = m_DrawItems->data.rbegin(); O != m_DrawItems->data.rend(); ++O)
	{
		auto& A = O->Attribute;
		if (A[L"enable"] == L"disable" || A[L"pointer-events"] == L"none")
			continue;

		point.x = Float(A[L"left"]);
		point.y = Float(A[L"top"]);

		size.width = Float(A[L"width"]);
		size.height = Float(A[L"height"]);

		if (A[L"horizontal-align"] == L"center")
		{
			rect.left = point.x - size.width / 2.f;
			rect.right = point.x + size.width / 2.f;
		}
		else if (A[L"horizontal-align"] == L"right")
		{
			rect.left = point.x - size.width;
			rect.right = point.x;
		}
		else
		{
			rect.left = point.x;
			rect.right = point.x + size.width;
		}

		if (A[L"vertical-align"] == L"center")
		{
			rect.top = point.y - size.height / 2.f;
			rect.bottom = point.y + size.height / 2.f;
		}
		else if (A[L"vertical-align"] == L"top")
		{
			rect.top = point.y - size.height;
			rect.bottom = point.y;
		}
		else
		{
			rect.top = point.y;
			rect.bottom = point.y + size.height;
		}

		if (rect.left <= x && rect.right >= x &&
			rect.top <= y && rect.bottom >= y)
		{
			focus = O->uuid;

			Execute(A[L"mousedown"], O->uuid);

			SetCapture(mhMainWnd);
			return;
		}
	}

	{
		focus = 0;
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
	for (auto& O = m_DrawItems->data.rbegin(); O != m_DrawItems->data.rend(); ++O)
	{
		auto& A = O->Attribute;
		if (A[L"enable"] == L"disable")
			continue;

		point.x = Float(A[L"left"]);
		point.y = Float(A[L"top"]);

		size.width = Float(A[L"width"]);
		size.height = Float(A[L"height"]);

		if (A[L"horizontal-align"] == L"center")
		{
			rect.left = point.x - size.width / 2.f;
			rect.right = point.x + size.width / 2.f;
		}
		else if (A[L"horizontal-align"] == L"right")
		{
			rect.left = point.x - size.width;
			rect.right = point.x;
		}
		else
		{
			rect.left = point.x;
			rect.right = point.x + size.width;
		}

		if (A[L"vertical-align"] == L"center")
		{
			rect.top = point.y - size.height / 2.f;
			rect.bottom = point.y + size.height / 2.f;
		}
		else if (A[L"vertical-align"] == L"top")
		{
			rect.top = point.y - size.height;
			rect.bottom = point.y;
		}
		else
		{
			rect.top = point.y;
			rect.bottom = point.y + size.height;
		}

		if (rect.left <= x && rect.right >= x &&
			rect.top <= y && rect.bottom >= y)
		{
			if (A[L"pointer-events"] == L"none")
			{
				continue;
			}
			Execute(A[L"mouseup"], O->uuid);

			SetCapture(mhMainWnd);
			return;
		}
	}

	ReleaseCapture();
}

void MyApp::OnMouseMove(WPARAM btnState, int x, int y)
{

	if (focus == 0)
	{
		if ((btnState & MK_LBUTTON) != 0)
		{
			float dx = XMConvertToRadians(0.25f*static_cast<float>(x - mLastMousePos.x));
			float dy = XMConvertToRadians(0.25f*static_cast<float>(y - mLastMousePos.y));

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
		else if ((btnState & MK_RBUTTON) != 0)
		{
			// Make each pixel correspond to 0.2 unit in the scene.
			float dx = 0.2f*static_cast<float>(x - mLastMousePos.x);
			float dy = 0.2f*static_cast<float>(y - mLastMousePos.y);

			// Update the camera radius based on input.
			mRadius += dx - dy;

			// Restrict the radius.
			mRadius = MathHelper::Clamp(mRadius, 10.0f, 240.0f);
		}

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
	mEyePos.x = mRadius * sinf(mPhi + 0.0001f)*cosf(mTheta);
	mEyePos.z = mRadius * sinf(mPhi + 0.0001f)*sinf(mTheta);
	mEyePos.y = mRadius * cosf(mPhi + 0.0001f) - 5.0f;

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

	if (tu >= 1.0f)
		tu -= 1.0f;

	if (tv >= 1.0f)
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
	for (auto& e : mAllRitems)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if (e->NumFramesDirty > 0)
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
	for (auto& e : mMaterials)
	{
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0)
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
	mMainPassCB.AmbientLight = { 0.f,0.f,0.f,0.f };//{ 0.25f, 0.25f, 0.35f, 1.0f };

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
	for (int i = 0; i < mWaves->VertexCount(); ++i)
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

	if (errorBlob != nullptr)
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


void MyApp::BuildLandGeometry()
{
	std::ifstream file("Map/map.bmp");
	std::wifstream prov_list("Map/prov.txt");
	std::ifstream prov_file("Map/prov.bmp");
	assert(file && prov_file && prov_list);
	{
		std::map<Color32, std::pair<ProvinceId, std::wstring>> prov_key;

		{
			std::wstring name, index, r, g, b;
			for (std::wstring line; std::getline(prov_list, line); )
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
		mWaves = std::make_unique<Waves>(map_h / 3, map_w / 3, 3.0f, 0.03f, 4.0f, 0.2f);
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

				//mWaves->mPrevSolution[x + (h - 1 - y) * w].earth = mLandVertices[x + y * w].Pos.y;

				mLandVertices[x + y * w].TexC = { 1.f / (w - 1) * x, 1.f / (h - 1) * y };
				mLandVertices[x + y * w].Prov = 0;

				XMVECTOR Pos = XMLoadFloat3(&mLandVertices[x + y * w].Pos);

				vMin = XMVectorMin(vMin, Pos);
				vMax = XMVectorMax(vMax, Pos);

				dex = rgb2dex(prov_buf.at(addr + 2), prov_buf.at(addr + 1), prov_buf.at(addr));

				//Registed Province Color
				if (auto search = prov_key.find(dex); search != prov_key.end())
				{
					mLandVertices[x + y * w].Prov = search->second.first;
					if (auto search_stack = m_gamedata->province.find(search->second.first); search_stack == m_gamedata->province.end())
					{
						m_gamedata->province.insert(std::make_pair(search->second.first, std::make_unique<Province>(search->second.second, dex, XMFLOAT3(mLandVertices[x + y * w].Pos.x, mLandVertices[x + y * w].Pos.y, mLandVertices[x + y * w].Pos.z))));
					}
					else
					{
						search_stack->second->pixel.x += mLandVertices[x + y * w].Pos.x;
						search_stack->second->pixel.y += mLandVertices[x + y * w].Pos.y;
						search_stack->second->pixel.z += mLandVertices[x + y * w].Pos.z;
						++search_stack->second->p_num;
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
						unregisted_color.insert(std::make_pair(dex, std::make_pair(XMUINT3(std::move(u3)), 1)));
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

		for (auto& O : m_gamedata->province)
		{
			int x = (int)(1.f * O.second->pixel.x / O.second->p_num + (map_w - 1.f) / 2.f);
			int y = (int)(1.f * O.second->pixel.z / O.second->p_num + (map_h - 1.f) / 2.f);


			if (x >= 0 && x < map_w && y >= 0 && y < map_h)
			{
				O.second->on3Dpos.x = 2.f * O.second->pixel.x / O.second->p_num;
				O.second->on3Dpos.y = 2.f * mLandVertices.at(x + map_w * y).Pos.y - 2.f;
				O.second->on3Dpos.z = 2.f * O.second->pixel.z / O.second->p_num;
			}
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
	for (int i = 0; i < m - 1; ++i)
	{
		for (int j = 0; j < n - 1; ++j)
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

	UINT vbByteSize = mWaves->VertexCount() * sizeof(Vertex);
	UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

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
	for (int i = 0; i < gNumFrameResources; ++i)
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

	{
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
			XMMatrixTranslation(-1.0f * map_w - border_wdith / 2.f, border_wdith / 2.f, 0.0f)
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
	}
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
	for (size_t i = 0; i < ritems.size(); ++i)
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


