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
	Water,
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

struct Province {
	std::wstring name;
	Color32 color;
	std::uint64_t p_num = 1;
	XMFLOAT3 pixel;
	XMFLOAT3 on3Dpos;


	bool is_rebel = false;
	NationId owner = 0;
	NationId ruler = 0;

	std::int64_t man = 1000 + rand() % 1600;
	std::int64_t maxman = 4000 + rand() % 6400;
	std::int64_t hp = 1000;

	float length_from_contry_side = 0;
	float prioriy, require;

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
	std::unordered_map<std::wstring, std::wstring> flag;
	bool Ai = true;

	float abb_disp = 1;
	float abb_man = 1;
	float abb_army_sieze = 1;
	float abb_army_move = 1;
	float abb_attr = 1;

	size_t own_province = 0;
	size_t rule_province = 0;
	size_t own_leaders = 0;

	NationId rival = -1;
};
enum class CommandType
{
	Move,
	Sieze,
	Attack
};
struct Command
{
	const CommandType type;
	const ProvinceId target_prov;
	const LeaderId target_leader;
	const float need;
	Command(const CommandType _type, const ProvinceId prov = 0, const LeaderId leader = 0, const float _need = 0) : type(_type), target_prov(prov), target_leader(leader), need(_need) {}
};

enum class LeaderType
{
	Attack,
	Defend,
	All
};
struct Leader
{
	std::list<Command> cmd;
	float cmd_pr = 0.f;
	bool enable = true;
	ProvinceId location;
	bool selected = false;

	std::int64_t size = 1000;
	LeaderType type = (LeaderType)(rand() % (int)LeaderType::All);

	float abb_sieze = 1;
	float abb_move = 1;
	float abb_disp = 1;

	NationId owner;
	Leader(const ProvinceId& loc, const NationId& own, const std::int64_t& _size) : location(loc), owner(own), size(_size) {};
};
enum class ProvincePathOutState {
	NotOut,
	WaitOut,
	Out
};
struct ProvincePathNode
{
	float Length = FLT_MAX;
	ProvinceId Nearest = 0;
	ProvincePathOutState Out = ProvincePathOutState::NotOut;
};
struct ProvincePath
{
	std::list<ProvinceId> path;
	std::map<ProvinceId, ProvincePathNode> prv;
	float length;
	decltype(path)::iterator begin() { return path.begin(); }
	decltype(path)::iterator end() { return path.end(); }

	ProvincePath(const std::map<ProvinceId, std::unique_ptr<Province>>&  _prv, const std::map<std::pair<ProvinceId, ProvinceId>, float> conn, const ProvinceId& Start, const ProvinceId& End)
	{
		if (Start == End) return;
		for (const auto& O : _prv) prv[O.first] = ProvincePathNode();

		prv.at(End).Length = 0;
		prv.at(End).Out = ProvincePathOutState::WaitOut;

		size_t limit = 0;
		while (limit++ <= prv.size())
		{
			for (auto& O : prv)
			{
				if (O.second.Out == ProvincePathOutState::WaitOut)
				{
					for (auto& P : prv)
					{
						if (P.second.Out == ProvincePathOutState::NotOut)
						{
							if (auto Q = conn.find(std::make_pair(O.first, P.first)); Q != conn.end())
							{
								if (P.second.Length > O.second.Length + Q->second)
								{
									P.second.Length = O.second.Length + Q->second;
									P.second.Nearest = O.first;
								}
							}
						}
					}
					O.second.Out = ProvincePathOutState::Out;
				}
			}

			/*if (prv.at(Start).Out == ProvincePathOutState::Out)
			{
				break;
			}*/

			float meter = FLT_MAX;
			decltype(prv)::iterator itr = prv.end();
			for (auto O = prv.begin(); O != prv.end(); ++O)
			{
				if (O->second.Out == ProvincePathOutState::NotOut && meter > O->second.Length)
				{
					meter = O->second.Length;
					itr = O;
				}
			}
			if (itr != prv.end())
			{
				itr->second.Out = ProvincePathOutState::WaitOut;
			}
			else
			{
				break;
			}
		}

		if (prv.at(Start).Out != ProvincePathOutState::NotOut)
		{
			ProvinceId Index = Start;
			length = prv.at(Start).Length;
			do
			{
				Index = prv.at(Index).Nearest;
				path.push_back(Index);
			} while (Index != End);
		}
	}
	ProvincePath() 
	{

	};
};
class Arrows
{
public:
	std::vector<Vertex> vertices;
	std::vector<std::uint16_t> indices;
	std::vector<XMFLOAT3> points;
	float width = 1.f;
	void BuildLine()
	{
		float width2 = width / 2.f;
		size_t offset = vertices.size();

		if (points.size() == 0) return;

		if (points.size() > 0x2000)
			points.resize(0x2000);
		float center = (points.size() - 1) / 2.f;
		for (int i = 0; i < points.size() - 1; ++i)
		{
			//float y = 2 - powf((points.size() - 1) / 2 - i, 2)/((points.size() - 1) / 2);

			float y = (powf((i - center) / center, 2) - 1.f) * 1 ;

			XMVECTOR v = XMLoadFloat2(new XMFLOAT2(points[i + 1].x - points[i].x, points[i +1].z - points[i].z));
			v = XMVector2Orthogonal(v);
			v = XMVector2Normalize(v);
			XMFLOAT2 f;
			XMStoreFloat2(&f, v);

			vertices.push_back({ XMFLOAT3(points[i].x - f.x * width2,points[i].y - y, points[i].z - f.y * width2) ,XMFLOAT3(0,1,0), XMFLOAT2(1, 1) });
			vertices.push_back({ XMFLOAT3(points[i].x + f.x * width2,points[i].y - y, points[i].z + f.y * width2) ,XMFLOAT3(0,1,0), XMFLOAT2(0, 1) });
		}

		for (size_t i = points.size() - 1;  i < points.size();++i)
		{
			XMVECTOR v = XMLoadFloat2(new XMFLOAT2(points[i].x - points[i - 1].x, points[i].z - points[i - 1].z));
			v = XMVector2Orthogonal(v);
			v = XMVector2Normalize(v);
			XMFLOAT2 f;
			XMStoreFloat2(&f, v);

			vertices.push_back({ XMFLOAT3(points[i].x - f.x * width2,points[i].y, points[i].z - f.y * width2) ,XMFLOAT3(0,1,0), XMFLOAT2(1, 0) });
			vertices.push_back({ XMFLOAT3(points[i].x + f.x * width2,points[i].y, points[i].z + f.y * width2) ,XMFLOAT3(0,1,0), XMFLOAT2(0, 0) });
		}

		for (std::uint16_t i = 0; i < points.size() - 1; ++i)
		{
			indices.push_back(static_cast<std::uint16_t>(offset + i * 2));
			indices.push_back(static_cast<std::uint16_t>(offset + i * 2 + 1));
			indices.push_back(static_cast<std::uint16_t>(offset + i * 2 + 2));

			indices.push_back(static_cast<std::uint16_t>(offset + i * 2 + 1));
			indices.push_back(static_cast<std::uint16_t>(offset + i * 2 + 2));
			indices.push_back(static_cast<std::uint16_t>(offset + i * 2 + 3));
			/*indices.push_back(i * 2);
			indices.push_back(i * 2 + 3);
			indices.push_back(i * 2 + 2);
			indices.push_back(i * 2 + 2);
			indices.push_back(i * 2 + 3);
			indices.push_back(i * 2);*/

		}
	}
	Arrows()
	{	

		BuildLine();
	}
};
struct Data
{
	bool run = true;

	std::map<ProvinceId, std::unique_ptr<Province>> province;
	std::map<std::pair<ProvinceId, ProvinceId>, float> province_connect;
	std::unordered_map<NationId, std::unique_ptr<Nation>>  nations;

	std::unordered_map<LeaderId, std::shared_ptr<Leader>> leaders;
	std::uint64_t leader_progress = 1;

	LeaderId last_leader_id = 0;
	ProvinceId last_prov_id = 0;

	std::random_device rd;
	std::mt19937 mt;

	Data()
	{
		mt = std::mt19937(rd());
	}

	Leader NewLeader(const ProvinceId& loc, const NationId& own, const std::int64_t& _size)
	{
		return Leader(loc, own, _size);
	}
};

enum class DragType
{
	None,
	View,
	Leader
};

enum class GameControlType
{
	View,
	Leader
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
	void BuildArrowGeometry();
	void BuildBoxGeometry();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildMaterials();
	void BuildRenderItems();
	void BuildImage();
	void BuildProvinceConnect();
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

	void InsertArrow(const ProvinceId& start, ProvincePath& path, bool clear);
	void UpdateArrow();
	
	void Query(const std::wstring& query);
	void Execute(const std::wstring& func_name, const std::uint64_t& uuid);

	void MainGame();

	std::list<YTML::DrawItem>::reverse_iterator MyApp::MouseOnUI(int x, int y);
private:
	void GUIUpdatePanelLeader(LeaderId leader_id = 0);
	void GUIUpdatePanelProvince(ProvinceId prov_id = 0);

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
	RenderItem* mArrowsRitem = nullptr;

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
	Arrows mArrows;


	struct {
		std::wstring DebugText = L"선택 되지 않음";
		NationId nationPick = 0;
	} mUser;



	std::unordered_map<std::wstring, std::wstring> Act(std::wstring wstr, std::initializer_list<std::wstring> args, bool need_return = false, bool try_lock = true);


	std::shared_ptr<Data> m_gamedata = std::make_shared<Data>();

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
		LeaderId tag_leader;
		decltype(Data::province)::iterator tag_prov_it;

	} mQuery;

	D2D1_POINT_2F Draw_point;
	D2D1_RECT_F Draw_rect;
	D2D1_SIZE_F Draw_size;
	D2D1_SIZE_F Draw_scale;

	std::uint64_t focus = 0;
	std::future<void> trdGame;

	DragType dragtype;
	int dragx, dragy;

	GameControlType game_contype = GameControlType::View;
	
	std::mutex draw_mutex;

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
	GameClose();
	if (md3dDevice != nullptr)
		FlushCommandQueue();
}

void MyApp::MainGame() //!@
{
	//std::this_thread::sleep_for(std::chrono::seconds(1));	

	OutputDebugStringA("Start Thread\n");
	float NowTime = mTimer.TotalTime();
	bool flag_update_leaders = false;
	while (m_gamedata->run)
	{
		for (auto& N : m_gamedata->nations)
		{
			N.second->own_province = 0;
			N.second->rule_province = 0;
			N.second->own_leaders = 0;
		}
		for (auto& O : m_gamedata->province)
		{
			O.second->man += (int64_t)round(min(max((O.second->maxman - O.second->man)/1200.0,-10),10));
			
			if (O.second->owner != O.second->ruler && O.second->man > O.second->maxman / 4)
			{
				if (O.second->man + O.second->hp * 2 > rand() % (1 + (int)(1.0 * rand() / RAND_MAX * 30000)) + O.second->maxman / 4)
				{
					Act(L"Draft", { L"location", Str(O.first), L"size", Str(O.second->man), L"owner", Str(O.second->owner) });
				}
			}
			if (O.second->owner == O.second->ruler)
			{
				auto& N = m_gamedata->nations.find(O.second->ruler);
				if (N != m_gamedata->nations.end())
				{
					O.second->man += (int64_t)round(min(max((O.second->maxman - O.second->man) / 1200.0 * N->second->abb_man, -10), 10));
				}
				else if (O.second->hp < O.second->p_num / 5 && O.second->man > 6000)
				{
					Act(L"Draft", { L"location", Str(O.first), L"size", Str(O.second->man), L"owner", Str(O.second->owner) });
				}
			}

			if (auto& N = m_gamedata->nations.find(O.second->owner); N != m_gamedata->nations.end()) ++N->second->own_province;
			if (auto& N = m_gamedata->nations.find(O.second->ruler); N != m_gamedata->nations.end()) ++N->second->rule_province;
			

			if (O.second->hp < 0) O.second->hp = 0;
			else if (O.second->hp >= O.second->p_num)
			{
				O.second->hp = O.second->p_num;
				O.second->owner = O.second->ruler;
			}
			else O.second->hp += 1;

			if (m_gamedata->last_prov_id == O.first)
			{
				GUIUpdatePanelProvince();
			}

			
		}

		draw_mutex.lock();

		for (auto O = m_gamedata->leaders.begin(); O != m_gamedata->leaders.end(); ++O)
		{
			if (O->second->size <= 0)
			{
				if (m_gamedata->last_leader_id == O->first) m_gamedata->last_leader_id = 0;
				
				for (const auto& E : m_DrawItems->$(L"#leader" + Str(O->first) + L" flag")) m_DrawItems->data.erase(E);
				for (const auto& E : m_DrawItems->$(L"#leader" + Str(O->first) + L" state")) m_DrawItems->data.erase(E);
				for (const auto& E : m_DrawItems->$(L"#leader" + Str(O->first) + L" num")) m_DrawItems->data.erase(E);
				for (const auto& E : m_DrawItems->$(L"#leader" + Str(O->first) + L" background")) m_DrawItems->data.erase(E);
				for (const auto& E : m_DrawItems->$(L"#leader" + Str(O->first) + L" progress")) m_DrawItems->data.erase(E);
				for (const auto& E : m_DrawItems->$(L"#leader" + Str(O->first))) m_DrawItems->data.erase(E);
				m_gamedata->leaders.erase((O--)->first);
			}
			else
			{
				if (auto& N = m_gamedata->nations.find(O->second->owner); N != m_gamedata->nations.end()) ++N->second->own_leaders;
			}
		}
		draw_mutex.unlock();

		flag_update_leaders = false;
		for (auto& O : m_gamedata->leaders)
		{
			if (O.second->owner != m_gamedata->province.at(O.second->location)->ruler)
			{
				if (const auto & N = m_gamedata->nations.find(m_gamedata->province.at(O.second->location)->ruler); N != m_gamedata->nations.end())
				{
					O.second->size -= (std::int64_t)std::round(O.second->size * N->second->abb_attr / 10000.f * 2 * rand() / RAND_MAX);
				}
				else
				{
					O.second->size -= (std::int64_t)std::round(O.second->size * 5 / 10000.f * 2 * rand() / RAND_MAX);
				}
				///if (O.second->size > 1000) O.second->size -= (O.second->size - 1000) / 10000;
			}
			else 
			{
				if (auto& P = m_gamedata->province.at(O.second->location); true)
				{
					if (P->owner == P->ruler)
					{
						if (O.second->owner == P->owner)
						{
							for (int i = 1; i < 100 && i * i * 9 <= P->man; ++i)
							{
								O.second->size += 9 * i * i;
								P->man -= 9 * i * i;
							}
						}
					}
					else if (P->ruler == O.second->owner)
					{
						if (P->hp < P->p_num) P->hp += 1;
					}
				}
			}
			if (O.second->cmd.size() > 0)
			{
				auto B = O.second->cmd.begin();

				if (O.second->cmd_pr >= B->need)
				{
					O.second->cmd_pr = 0;

					switch (B->type)
					{
					case CommandType::Move:
						O.second->location = B->target_prov;
						break;
					case CommandType::Sieze:
						{
							auto& P = m_gamedata->province.at(O.second->location);
							P->hp -= (77 + rand() % 100 + O.second->size / 600) * 2;
							//O.second->size = (int)(0.9 * O.second->size);
							if (P->hp <= 0)
							{
								P->ruler = O.second->owner;
								//O.second->size += m_gamedata->province.at(O.second->location)->man;
								//m_gamedata->province.at(O.second->location)->man = 0;
								P->hp = 0;
							}
						}
						break;
					case CommandType::Attack:
						auto L = m_gamedata->leaders.find(std::move(B->target_leader));
						if (L != m_gamedata->leaders.end() && L->second->location == O.second->location && O.second->size > 0 && L->second->size > 0)
						{
							if (L->second->owner == m_gamedata->province.at(O.second->location)->owner) {
								L->second->size -= O.second->size / 4 * 100 / 200;
							}
							else {
								L->second->size -= O.second->size / 4;
							}
							
							if (L->second->size > 0)
							{
								O.second->size -= L->second->size / 4;
								if (L->second->cmd.size() > 0 && (L->second->cmd.begin()->type == CommandType::Sieze || L->second->cmd.begin()->type == CommandType::Move))
								{
									L->second->cmd_pr = 0;
									L->second->cmd.pop_front();
								}
							}
						}
						break;
					}
					O.second->cmd.pop_front();
				}
				else
				{
					switch (B->type)
					{
					case CommandType::Sieze:
						if (m_gamedata->province.at(O.second->location)->ruler == O.second->owner)
						{
							O.second->cmd.pop_front();
							O.second->cmd_pr = -1;
						}
						break;
					case CommandType::Attack:
						auto L = m_gamedata->leaders.find(std::move(B->target_leader));
						if (L == m_gamedata->leaders.end())
						{
							O.second->cmd.pop_front();
							O.second->cmd_pr = -1;
						}
						else if (L->second->location != O.second->location)
						{
							O.second->cmd.pop_front();
							O.second->cmd_pr = -1;
						}
						break;
					}
					O.second->cmd_pr += 1;
				}
				if (O.second->selected) flag_update_leaders = true;


			}
			else
			{
				std::vector<LeaderId> sameLocLeader;
				for (auto& L : m_gamedata->leaders)
				{
					if (L.second->location == O.second->location && L.second->owner != O.second->owner)
					{
						if (L.second->cmd.size() > 0 && L.second->cmd.begin()->type == CommandType::Sieze)
						{
							sameLocLeader.clear();
							sameLocLeader.push_back(L.first);
							break;
						}
						else
						{
							sameLocLeader.push_back(L.first);
						}
					}
				}
				std::shuffle(sameLocLeader.begin(), sameLocLeader.end(), m_gamedata->rd);
				if (sameLocLeader.size() > 0) O.second->cmd.push_back(Command(CommandType::Attack, 0, *sameLocLeader.begin(), 10));
				else if (m_gamedata->province.at(O.second->location)->ruler != O.second->owner)
				{
					O.second->cmd.push_back(Command(CommandType::Sieze, O.second->location, 0, 20 / O.second->abb_sieze));
				}
				else
				{
					if (m_gamedata->province.at(O.second->location)->hp < 1000) m_gamedata->province.at(O.second->location)->hp += O.second->size / 1000;
				}
			}
		}


		//AI
		for (auto& N : m_gamedata->nations)
		{
			if (N.second->Ai && N.second->own_province > 0 && N.second->rule_province > 0)
			{
				if (N.second->rival != -1)
				{
					if (auto & n = m_gamedata->nations.find(N.second->rival); n != m_gamedata->nations.end())
					{
						if (n->second->own_province == 0 && n->second->rule_province == 0)
						{
							N.second->rival = -1;
						}
					}
					else
					{
						N.second->rival = -1;
					}
				}

				std::list<ProvinceId> myProv;
				std::list<LeaderId> myLead;

				for (auto& P : m_gamedata->province) 
				{ 
					if (P.second->owner == N.first || P.second->ruler == N.first) myProv.push_back(P.first); 

					if (P.second->owner == N.first)
					{
						if (P.second->ruler == N.first) //내 영토의 내 소유
						{
							P.second->prioriy = 1 * (2000 - P.second->hp);
						}
						else							//내 영토의 적 소유
						{
							P.second->prioriy = 3 * (2000 - P.second->hp);
						}
					}
					else
					{
						if (P.second->ruler == N.first)	 //적 영토의 내 소유
						{
							P.second->prioriy = 2 * (2000 - P.second->hp) * (N.second->rival == P.second->ruler ? 2 : 1);
						}
						else							 //적 영토의 적 소유
						{
							P.second->prioriy = 1 * (2000 - P.second->hp) * (N.second->rival == P.second->ruler ? 2 : 1);
						}
					}
				}
				for (auto& L : m_gamedata->leaders) 
				{ 
					ProvinceId lastLoc = L.second->location;
					if (L.second->cmd.size() > 0)
					{
						float rate_time = -L.second->cmd_pr;
						for (auto& C : L.second->cmd)
						{
							rate_time += C.need;
							if (C.type == CommandType::Move) lastLoc = C.target_prov;
						}
					}
					auto& P = m_gamedata->province.at(lastLoc);

					if (L.second->owner == N.first)
					{
						myLead.push_back(L.first);
						P->require -= L.second->size;
						if (P->ruler == N.first)// 내 땅에 내 군사
							P->prioriy -= 2 * L.second->size;
						else					// 남 땅에 내 군사
							P->prioriy -= 0.1f * L.second->size * (N.second->rival == P->owner ? 0.5f : 1.f);
					}
					else
					{
						P->require += L.second->size;
						if (P->ruler == N.first)// 내 땅에 남 군사
							P->prioriy += 2 * L.second->size * (N.second->rival == P->owner ? 2 : 1);
						else					// 남 땅에 남 군사
							P->prioriy -= 1 * L.second->size * (N.second->rival == P->owner ? 0.5 : 1);
					}
				}

				if (N.second->rival == -1)
				{
					float syn = -FLT_MAX;
					for (auto& n : m_gamedata->nations)
					{
						if (n.second->own_province > 0 && n.second->rule_province > 0 && n.first != N.first)
						{
							float my_syn = 0;
							for (auto& P : m_gamedata->province)
							{
								if (P.second->owner == N.first && P.second->ruler == n.first)
								{
									my_syn += P.second->maxman / 1000.f;
								}
								else if (P.second->ruler == N.first && P.second->owner == n.first)
								{
									my_syn += P.second->maxman / 1000.f;
								}
								else if (P.second->ruler == n.first && P.second->owner == n.first)
								{
									float distance = FLT_MAX;
									for (const auto& p : myProv)
									{
										auto path = ProvincePath(m_gamedata->province, m_gamedata->province_connect, P.first, p);
										if (path.path.size() > 0)
										{
											if (path.length < distance)
												distance = path.length;
										}
									}
									my_syn += (P.second->maxman / 1000.f) * 30 / pow(distance, 2);
								}
							}
							if (my_syn > syn)
							{
								syn = my_syn;
								N.second->rival = n.first;
							}
						}
					}
				}

				size_t LeaderCount = myLead.size();
				for (const auto& p : myProv)
				{
					if (LeaderCount >= myProv.size() / 2 + 1 ) break;
					auto& P = m_gamedata->province[p];
					if (N.first == P->ruler && P->man >= 1000)
					{
						if (auto X = Act(L"Draft", { L"location", Str(p), L"size", Str(P->man), L"owner", Str(P->owner), L"abb_sieze", Str(N.second->abb_army_sieze), L"abb_move", Str(N.second->abb_army_move) }); X.find(L"SUCCESS") != X.end()) ++LeaderCount;
					}
				}

				for (const auto& l : myLead)
				{
					auto& L = m_gamedata->leaders[l];
					if (L->cmd.size() == 0)
					{
						ProvinceId target = L->location;
						float org_syn = m_gamedata->province[L->location]->prioriy + L->size;
						float syn = org_syn;
						float tmp = 0;

						for (auto& P : m_gamedata->province)
						{
							if (P.second->prioriy < org_syn) continue;
							auto path = ProvincePath(m_gamedata->province, m_gamedata->province_connect, L->location, P.first);
							if (syn < P.second->prioriy - path.length * 16)
							{
								syn = P.second->prioriy - path.length * 16;
								target = P.first;
							}
						}

						auto path = ProvincePath(m_gamedata->province, m_gamedata->province_connect, L->location, target);

						if (path.path.size() > 0)
						{
							m_gamedata->province[target]->prioriy -= L->size;
							m_gamedata->province[L->location]->prioriy += L->size;

							L->cmd_pr = 0;
							L->cmd.clear();
							ProvinceId lastLoc = L->location;
							for (auto P : path)
							{
								L->cmd.push_back(Command(CommandType::Move, P, 0, m_gamedata->province_connect.at(std::make_pair(lastLoc, P)) / L->abb_move));
								lastLoc = P;
								break;
							}
						}						
					}
				}



			}
			else
			{
				size_t myProvCount = 0;
				size_t myLeaderCount = 0;
				for (auto& P : m_gamedata->province)
				{
					if (P.second->ruler == N.first || P.second->owner == N.first) ++myProvCount;
				}
				for (auto& L : m_gamedata->leaders)
				{
					ProvinceId lastLoc = L.second->location;
					auto& P = m_gamedata->province.at(lastLoc);

					if (L.second->owner == N.first) ++myLeaderCount;
				}
				for (auto& P : m_gamedata->province)
				{
					if (!(myLeaderCount < myProvCount / 2 + 1))break;
					if (N.first == P.second->ruler && P.second->man >= 1000)
					{

						Act(L"Draft", { L"location", Str(P.first), L"size", Str(P.second->man), L"owner", Str(P.second->owner), L"abb_sieze", Str(N.second->abb_army_sieze), L"abb_move", Str(N.second->abb_army_move) });

						++myLeaderCount;
					}
				}
			}
		}

		if (flag_update_leaders)
		{
			UpdateArrow();
			GUIUpdatePanelLeader();
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	OutputDebugStringA("End Thread\n");
	return;
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
	BuildArrowGeometry();
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
	DirectX::XMStoreFloat3(&newPos, _pos);

	newPos.x /= newPos.z;
	newPos.y /= newPos.z;

	newPos.x = mClientWidth * (newPos.x + 1.0f) / 2.0f;
	newPos.y = mClientHeight * (1.0f - ((newPos.y + 1.0f) / 2.0f));

	return newPos;
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
	loadBitmap(LR"(Images\window-highlight.png)", L"WindowHighlight");
	loadBitmap(LR"(Images\button.png)", L"Button");
	loadBitmap(LR"(Images\dark-button.png)", L"DarkButton");
	loadBitmap(LR"(Images\flags\모름.bmp)", L"모르는국기");


	loadBitmap(LR"(Images\ui\leader\attack.png)", L"Leader-attack");
	loadBitmap(LR"(Images\ui\leader\move.png)", L"Leader-move");
	loadBitmap(LR"(Images\ui\leader\sieze.png)", L"Leader-sieze");


	wchar_t buf[256];
	for (const auto& O : m_gamedata->nations)
	{
		swprintf_s(buf, LR"(Images\flags\%ls.bmp)", O.second->MainName.c_str());
		loadBitmap(buf, O.second->MainName);

	}
}
void MyApp::DrawUI()
{
	auto& g = m_d2d->d2dDeviceContext;

	if (dragtype == DragType::Leader)
	{
		Draw_rect = { (float)dragx, (float)dragy, (float)mLastMousePos.x, (float)mLastMousePos.y };
		g->DrawRectangle(Draw_rect, m_d2d->Brush[L"White"].Get(), 2.f);
	}


	g->DrawText(mUser.DebugText.c_str(), (UINT32)mUser.DebugText.length(),
		m_d2d->textFormat[L"Debug"].Get(), D2D1::RectF(0.0f, 0.0f, mClientWidth / 3.f, 1.f * mClientHeight),
		m_d2d->Brush[L"White"].Get());

	draw_mutex.lock();
	m_DrawItems->Sort();
	for (auto& O : m_DrawItems->data)
	{
		if (!O.enable)
			continue;

		try
		{
			m_d2d->Brush[L"Main"]->SetColor(D2D1::ColorF(O.color_r, O.color_g, O.color_b));
			m_d2d->Brush[L"Main"]->SetOpacity(O.opacity);
			m_d2d->Brush[L"Back"]->SetColor(D2D1::ColorF(O.background_color_r, O.background_color_g, O.background_color_b));
			m_d2d->Brush[L"Back"]->SetOpacity(O.opacity);

			Draw_point.x = O.left;
			Draw_point.y = O.top;
			Draw_point.x += O.inherit_left;
			Draw_point.y += O.inherit_top;

			Draw_size.width = O.width;
			Draw_size.height = O.height;

			if (O[L"horizontal-align"] == L"center")
			{
				Draw_rect.left = Draw_point.x - Draw_size.width / 2.f;
				Draw_rect.right = Draw_point.x + Draw_size.width / 2.f;
			}
			else if (O[L"horizontal-align"] == L"right")
			{
				Draw_rect.left = Draw_point.x - Draw_size.width;
				Draw_rect.right = Draw_point.x;
			}
			else
			{
				Draw_rect.left = Draw_point.x;
				Draw_rect.right = Draw_point.x + Draw_size.width;
			}

			if (O[L"vertical-align"] == L"center")
			{
				Draw_rect.top = Draw_point.y - Draw_size.height / 2.f;
				Draw_rect.bottom = Draw_point.y + Draw_size.height / 2.f;
			}
			else if (O[L"vertical-align"] == L"top")
			{
				Draw_rect.top = Draw_point.y - Draw_size.height;
				Draw_rect.bottom = Draw_point.y;
			}
			else
			{
				Draw_rect.top = Draw_point.y;
				Draw_rect.bottom = Draw_point.y + Draw_size.height;
			}

			if (O.background)
				g->FillRectangle(Draw_rect, m_d2d->Brush[L"Back"].Get());
			//if (O.border)
			//	g->DrawRectangle(Draw_rect, m_d2d->Brush[L"Main"].Get());

			if (O[L"tag"] == L"div")
			{
				//OutputDebugStringW(((O[L"left"]) + L" : " + (O[L"top"]) + L"\n").c_str());
			}
			else if (O[L"tag"] == L"a")
			{
				m_d2d->textLayout.Reset();
				ThrowIfFailed(m_d2d->dwriteFactory->CreateTextLayout(
					O[L"text"].c_str(), (UINT32)O[L"text"].length(),
					m_d2d->textFormat[L"Above Province"].Get(),
					Draw_size.width, Draw_size.height,
					m_d2d->textLayout.GetAddressOf()
				));
				m_d2d->textLayout->SetFontSize(Draw_size.height * 0.8f, { 0U,  (UINT32)O[L"text"].length() });

				g->DrawTextLayout(D2D1::Point2F(Draw_rect.left, Draw_rect.top), m_d2d->textLayout.Get(),
					m_d2d->Brush[L"Main"].Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP
				);
			}
			else if (O[L"tag"] == L"img")
			{
				auto& B = m_d2d->pD2DBitmap[O[L"src"]];
				m_d2d->BitmapBrush->SetBitmap(B.data.Get());

				Draw_scale.width = (Draw_rect.right - Draw_rect.left) / B.width;
				Draw_scale.height = (Draw_rect.bottom - Draw_rect.top) / B.height;

				m_d2d->BitmapBrush->SetTransform(
					D2D1::Matrix3x2F::Translation(Draw_rect.left / Draw_scale.width, Draw_rect.top / Draw_scale.height) *
					D2D1::Matrix3x2F::Scale(Draw_scale.width, Draw_scale.height)
				);
				//g->FillRectangle(rect, m_d2d->Brush[L"White"].Get());
				g->FillRectangle(Draw_rect, m_d2d->BitmapBrush.Get());
			}
			//g->DrawRectangle(Draw_rect, m_d2d->Brush[L"Main"].Get());
		}
		catch (const std::exception&)
		{
			OutputDebugStringW(L"[");
			OutputDebugStringW(O[L"id"].c_str());
			OutputDebugStringW(L"]:[");
			OutputDebugStringW(O[L"class"].c_str());
			OutputDebugStringW(L"]<");
			OutputDebugStringW(O[L"tag"].c_str());
			OutputDebugStringW(L">\n");
			O[L"enable"] = L"disable";
		}
	}
	draw_mutex.unlock();
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
	
	mCommandList->SetPipelineState(mPSOs["province"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Province]);

	mCommandList->SetPipelineState(mPSOs["transparent"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Water]); 

	mCommandList->SetPipelineState(mPSOs["alphaTested"].Get());
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
								for (const auto& N : m_gamedata->nations)
								{
									if (N.second->MainName == O)
									{
										mQuery.tag_prov_it->second->ruler = N.first;
										break;
									}
								}
							}
						}
						else if (mQuery.index == L"-owner")
						{
							if (mQuery.tag_prov_it != m_gamedata->province.end())
							{
								for (const auto& N : m_gamedata->nations)
								{
									if (N.second->MainName == O)
									{
										mQuery.tag_prov_it->second->owner = N.first;
										break;
									}
								}
							}
						}
						else if (mQuery.index == L"-develop")
						{
							if (mQuery.tag_prov_it != m_gamedata->province.end())
							{
								mQuery.tag_prov_it->second->maxman = (4 + powf(Int(O) / 2.f, 2)) * 1000;
								mQuery.tag_prov_it->second->man = mQuery.tag_prov_it->second->maxman / 10;
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
		file << L"PROVINCE" << L"\t" << L"-id " << std::to_wstring(O.first);

		if (auto find = m_gamedata->nations.find(O.second->ruler); find != m_gamedata->nations.end())
		{
			file << L" -ruler " << m_gamedata->nations.at(O.second->ruler)->MainName;
		}
		if (auto find = m_gamedata->nations.find(O.second->owner); find != m_gamedata->nations.end())
		{
			file << L" -owner " << m_gamedata->nations.at(O.second->owner)->MainName;
		}

		file << L";\n";
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
		신라->abb_attr = 2;
		m_gamedata->nations[++nation_count] = std::move(신라);

		std::unique_ptr<Nation> 백제 = std::make_unique<Nation>();
		백제->MainColor = XMFLOAT4(0.1f, 0.5f, 0.45f, 0.75f);
		백제->MainName = L"백제";
		m_gamedata->nations[++nation_count] = std::move(백제);
		//mUser.nationPick = nation_count;

		std::unique_ptr<Nation> 고구려 = std::make_unique<Nation>();
		고구려->MainColor = XMFLOAT4(0.4f, 0.0f, 0.0f, 0.75f);
		고구려->MainName = L"고구려";
		고구려->abb_attr = 3;
		m_gamedata->nations[++nation_count] = std::move(고구려);

		std::unique_ptr<Nation> 당나라 = std::make_unique<Nation>();
		당나라->MainColor = XMFLOAT4(0.8f, 0.5f, 0.0f, 0.75f);
		당나라->MainName = L"당나라";
		m_gamedata->nations[++nation_count] = std::move(당나라);

		std::unique_ptr<Nation> 말갈 = std::make_unique<Nation>();
		말갈->MainColor = XMFLOAT4(0.25f, 0.0f, 0.0f, 0.75f);
		말갈->MainName = L"말갈";
		말갈->abb_man = 1.2f;
		말갈->abb_attr = 6;
		m_gamedata->nations[++nation_count] = std::move(말갈);

		std::unique_ptr<Nation> 왜 = std::make_unique<Nation>();
		왜->MainColor = XMFLOAT4(1.0f, 0.25f, 0.25f, 0.75f);
		왜->MainName = L"왜";
		왜->abb_man = 1.5f;
		왜->abb_attr = 12;
		m_gamedata->nations[++nation_count] = std::move(왜);

		std::unique_ptr<Nation> 원나라 = std::make_unique<Nation>();
		원나라->MainColor = XMFLOAT4(0.3f, 0.5f, 0.8f, 0.75f);
		원나라->MainName = L"원나라";
		원나라->abb_man = 0.2f;
		원나라->abb_army_move = 5.f;
		원나라->abb_army_sieze = 4.f;
		m_gamedata->nations[++nation_count] = std::move(원나라);


		std::unique_ptr<Nation> 조선 = std::make_unique<Nation>();
		조선->MainColor = XMFLOAT4(0.125f, 0.25f, 0.5f, 0.75f);
		조선->MainName = L"조선";
		m_gamedata->nations[++nation_count] = std::move(조선);

		std::unique_ptr<Nation> 고려 = std::make_unique<Nation>();
		고려->MainColor = XMFLOAT4(1.f, 0.f, 0.f, 0.75f);
		고려->MainName = L"고려";
		고려->abb_man = 1.25f;
		m_gamedata->nations[++nation_count] = std::move(고려);

		std::unique_ptr<Nation> 가야 = std::make_unique<Nation>();
		가야->MainColor = XMFLOAT4(0.25f, 0.5f, 0.0625f, 0.75f);
		가야->MainName = L"가야";
		가야->abb_man = 1.5f;
		가야->abb_attr = 8;
		m_gamedata->nations[++nation_count] = std::move(가야);

		std::unique_ptr<Nation> 한국 = std::make_unique<Nation>();
		한국->MainColor = XMFLOAT4(1.f, 1.f, 1.f, 0.75f);
		한국->MainName = L"한국";
		한국->abb_attr = 0.75f;
		한국->abb_man = 10;
		m_gamedata->nations[++nation_count] = std::move(한국);

		std::unique_ptr<Nation> 발해 = std::make_unique<Nation>();
		발해->MainColor = XMFLOAT4(0.75f, 0.5f, 1.f, 0.75f);
		발해->MainName = L"발해";
		발해->abb_attr = 2;
		m_gamedata->nations[++nation_count] = std::move(발해);

		std::unique_ptr<Nation> 낙랑 = std::make_unique<Nation>();
		낙랑->MainColor = XMFLOAT4(1.f, 0.6f, 0.0625f, 0.75f);
		낙랑->MainName = L"낙랑";
		m_gamedata->nations[++nation_count] = std::move(낙랑);

		std::unique_ptr<Nation> 탐라 = std::make_unique<Nation>();
		탐라->MainColor = XMFLOAT4(0.9f, 0.3f, 0.9f, 0.75f);
		탐라->MainName = L"탐라";
		m_gamedata->nations[++nation_count] = std::move(탐라);

		std::unique_ptr<Nation> 거란 = std::make_unique<Nation>();
		거란->MainColor = XMFLOAT4(0.5f, 0.0f, 0.3f, 0.75f);
		거란->MainName = L"거란";
		m_gamedata->nations[++nation_count] = std::move(거란);
	}
	//m_gamedata->nations.at(mUser.nationPick)->Ai = false;


	draw_mutex.lock();
	wchar_t buf[256];
	for (const auto& O : m_gamedata->province)
	{
		if (O.second->p_num)
		{
			swprintf_s(buf, LR"(<div id="prov%.0lf" enable="disable" background="enable" opacity="0.5">)", (double)O.first);
			auto E = m_DrawItems->Insert(buf);
			swprintf_s(buf, LR"(<a id="provtext%.0lf" enable="disable" opacity="1.0" color-hex="FFFFFF">)", (double)O.first);
			m_DrawItems->Insert(buf, E);
		}
	}

	



	m_DrawItems->Insert(LR"(<img src="신라" id="myNationFlag" left="81" top="80" width="132" height="132" z-index="99" horizontal-align="center" vertical-align="center">)");
	m_DrawItems->Insert(LR"(<img src="Window" left="0" top="0" width="161" height="160" z-index="100">)");

	
	{
		std::uint64_t EM = m_DrawItems->Insert(LR"(<img class="myForm" id="myForm0" src="Form" left="200" top="200" width="400" height="480" mousedown="FormHold" mouseup="FormUnhold" z-index="1000">)");

		m_DrawItems->Insert(LR"(<a id="head" text="알 수 없음" left="26" top="60" width="348" height="48" z-index="1e-4" border="disable" pointer-events="none" color-g="0" color-r="0"  color-b="0">)", EM);
		m_DrawItems->Insert(LR"(<a id="tail" text="영토 확인 메뉴" left="26" top="460" width="348" height="24" z-index="1e-4" border="disable" color-g="0" color-r="0"  color-b="0">)", EM);
		m_DrawItems->Insert(LR"(<img id="flag" src="모르는국기" left="113" top="196" width="117" height="117" z-index="1e-4" horizontal-align="center" vertical-align="center">)", EM);
		m_DrawItems->Insert(LR"(<img src="Window" left="40" top="124" width="144" height="144" z-index="1e-4">)", EM);

		std::uint64_t EME = m_DrawItems->Insert(LR"(<div id="textContainer" src="Window" left="200" top="152" width="160" height="88" z-index="1e-4">)", EM);
		m_DrawItems->Insert(LR"(<a id="text0" text="Window" left="0" top="-8" width="160" height="40" z-index="1e-4" border="disable" color-g="0" color-r="0"  color-b="0">)", EME);
		m_DrawItems->Insert(LR"(<a id="text1" text="Window" left="0" top="36" width="160" height="24" z-index="1e-4" border="disable" color-g="0" color-r="0"  color-b="0">)", EME);
		m_DrawItems->Insert(LR"(<a id="text2" text="Window" left="0" top="64" width="160" height="24" z-index="1e-4" border="disable" color-g="0" color-r="0"  color-b="0">)", EME);
		m_DrawItems->Insert(LR"(<a id="text3" text="Window" left="0" top="92" width="160" height="24" z-index="1e-4" border="disable" color-g="0" color-r="0"  color-b="0">)", EME);

		std::uint64_t EMEE = m_DrawItems->Insert(LR"(<div id="buttonbar" src="Window" left="26" top="292" width="174" height="20" z-index="1e-4">)", EM);
		std::uint64_t EMBT = m_DrawItems->Insert(LR"(<img id="button0" src="Button" left="2" top="0" width="112" height="40" z-index="1e-4">)", EMEE);
		m_DrawItems->Insert(LR"(<a id="text" text="Window" left="0" top="6" width="112" height="30" z-index="1e-4" border="disable" color-g="0" color-r="0"  color-b="0">)", EMBT);
		EMBT = m_DrawItems->Insert(LR"(<img id="button1" src="Button" left="118" top="0" width="112" height="40" z-index="1e-4">)", EMEE);
		m_DrawItems->Insert(LR"(<a id="text" text="Window" left="0" top="6" width="112" height="30" z-index="1e-4" border="disable" color-g="0" color-r="0"  color-b="0">)", EMBT);
		EMBT = m_DrawItems->Insert(LR"(<img id="button2" src="Button" left="234" top="0" width="112" height="40" z-index="1e-4">)", EMEE);
		m_DrawItems->Insert(LR"(<a id="text" text="Window" left="0" top="6" width="112" height="30" z-index="1e-4" border="disable" color-g="0" color-r="0"  color-b="0">)", EMBT);
	}
	{
		std::uint64_t EME;
		std::uint64_t EM = m_DrawItems->Insert(LR"(<img class="consoleForm" src="Form" left="79" top="157" width="167" height="439" mousedown="FormHold" mouseup="FormUnhold" z-index="1000">)");
		
		m_DrawItems->Insert(LR"(<a id="head" text="이벤트 시작" left="16" top="50" width="135" height="28" z-index="1e-4" pointer-events="none" color-g="0" color-r="0"  color-b="0">)", EM);
		
		EME = m_DrawItems->Insert(LR"(<img id="button0" src="Button" left="10" top="72" width="149" height="31" z-index="1e-4" mousedown="button0">)", EM);
		m_DrawItems->Insert(LR"(<a id="text" text="당나라 침공" left="0" top="0" width="149" height="31" z-index="2e-4" pointer-events="none" color-hex="FFFFFF">)", EME);
		
		EME = m_DrawItems->Insert(LR"(<img id="button1" src="Button" left="10" top="103" width="149" height="31" z-index="1e-4" mousedown="button1">)", EM);
		m_DrawItems->Insert(LR"(<a id="text" text="말갈족의 남하" left="0" top="0" width="149" height="31" z-index="1e10" pointer-events="none" color-hex="FFFFFF">)", EME);
		
		EME = m_DrawItems->Insert(LR"(<img id="button2" src="Button" left="10" top="134" width="149" height="31" z-index="1e-4" mousedown="button2">)", EM);
		m_DrawItems->Insert(LR"(<a id="text" text="왜구의 침략" left="0" top="0" width="149" height="31" z-index="2e-4" pointer-events="none" color-hex="FFFFFF">)", EME);
		
		EME = m_DrawItems->Insert(LR"(<img id="button3" src="Button" left="10" top="165" width="149" height="31" z-index="1e-4" mousedown="button3">)", EM);
		m_DrawItems->Insert(LR"(<a id="text" text="원나라의 부흥" left="0" top="0" width="149" height="31" z-index="2e-4" pointer-events="none" color-hex="FFFFFF">)", EME);
		
		EME = m_DrawItems->Insert(LR"(<img id="button4" src="Button" left="10" top="196" width="149" height="31" z-index="1e-4" mousedown="button4">)", EM);
		m_DrawItems->Insert(LR"(<a id="text" text="위화도 회군" left="0" top="0" width="149" height="31" z-index="2e-4" pointer-events="none" color-hex="FFFFFF">)", EME);

		EME = m_DrawItems->Insert(LR"(<img id="button5" src="Button" left="10" top="227" width="149" height="31" z-index="1e-4" mousedown="button5">)", EM);
		m_DrawItems->Insert(LR"(<a id="text" text="고려의 건국" left="0" top="0" width="149" height="31" z-index="2e-4" pointer-events="none" color-hex="FFFFFF">)", EME);



		EME = m_DrawItems->Insert(LR"(<img id="button6" src="Button" left="10" top="258" width="149" height="31" z-index="1e-4" mousedown="button6">)", EM);
		m_DrawItems->Insert(LR"(<a id="text" text="가야의 반란" left="0" top="0" width="149" height="31" z-index="2e-4" pointer-events="none" color-hex="FFFFFF">)", EME);

		EME = m_DrawItems->Insert(LR"(<img id="button7" src="Button" left="10" top="289" width="149" height="31" z-index="1e-4" mousedown="button7">)", EM);
		m_DrawItems->Insert(LR"(<a id="text" text="발해의 건국" left="0" top="0" width="149" height="31" z-index="2e-4" pointer-events="none" color-hex="FFFFFF">)", EME);

		EME = m_DrawItems->Insert(LR"(<img id="button8" src="Button" left="10" top="320" width="149" height="31" z-index="1e-4" mousedown="button8">)", EM);
		m_DrawItems->Insert(LR"(<a id="text" text="의병단의 봉기" left="0" top="0" width="149" height="31" z-index="2e-4" pointer-events="none" color-hex="FFFFFF">)", EME);

		EME = m_DrawItems->Insert(LR"(<img id="button9" src="Button" left="10" top="351" width="149" height="31" z-index="1e-4" mousedown="button9">)", EM);
		m_DrawItems->Insert(LR"(<a id="text" text="낙랑군의 부활" left="0" top="0" width="149" height="31" z-index="2e-4" pointer-events="none" color-hex="FFFFFF">)", EME);

		EME = m_DrawItems->Insert(LR"(<img id="button10" src="Button" left="10" top="382" width="149" height="31" z-index="1e-4" mousedown="button10">)", EM);
		m_DrawItems->Insert(LR"(<a id="text" text="-" left="0" top="0" width="149" height="31" z-index="2e-4" pointer-events="none" color-hex="FFFFFF" mousedown="button10">)", EME);
		
		m_DrawItems->Insert(LR"(<a id="tail" text="콘솔" left="33" top="419" width="101" height="20" z-index="1e-4" pointer-events="none" color-hex="FFFFFF">)", EM);
	}
					

	m_DrawItems->Insert(LR"(<img id="myDiv" src="Cursor" z-index="1e10" left="0" top="0" width="40" height="40" pointer-events="none">)");
	draw_mutex.unlock();
	GameLoad(); 

	trdGame = std::async(&MyApp::MainGame, this);
	//trdGame.join();
}

std::unordered_map<std::wstring, std::wstring> MyApp::Act(std::wstring func_name, std::initializer_list<std::wstring> args, bool only_test, bool try_lock)
{
	std::unordered_map<std::wstring, std::wstring> _Return;
	std::unordered_map<std::wstring, std::wstring> arg;
	std::wstring head;
	for (auto P = args.begin();;)
	{
		if (P == args.end()) break;
		head = *(P++);
		if (P == args.end()) break;
		arg[head] = *(P++);
	}

	if (func_name == L"Draft")
	{	
		ProvinceId id = std::stoull(arg[L"location"]);
		auto prov = m_gamedata->province.find(id);
		std::int64_t draft_size = 1000;
		NationId owner = 0;
		bool force = false;

		

		if (arg.find(L"size") != arg.end()) draft_size = Long(arg[L"size"]);
		if (arg.find(L"owner") != arg.end()) owner = std::stoull(arg[L"owner"]);
		else if (auto N = m_gamedata->nations.find(prov->second->ruler); N != m_gamedata->nations.end())
		{
			owner = N->first;
			N->second->abb_army_move;
		}
		if (arg.find(L"force") != arg.end()) force = true;
		
		if (prov->second->man >= draft_size || force)
		{
			if (only_test)
			{
				_Return.insert(std::make_pair(L"SUCCESS", L""));
			}
			else
			{
				if (!force)	prov->second->man -= draft_size;

				wchar_t buf[256];
				if (try_lock) draw_mutex.lock();
				swprintf_s(buf, LR"(<img id="leader%llu" src="Window" enable="disable" pointer-events="none" gamedata-leaderid="%llu">)", m_gamedata->leader_progress, m_gamedata->leader_progress);
				std::uint64_t EM = m_DrawItems->Insert(buf);
				std::wstring nation_name = owner > 0 ? m_gamedata->nations.at(owner)->MainName : L"모르는국기";
				
				
				

				m_DrawItems->Insert(LR"(<div id="background" enable="disable" background-color-r="1" background-color-g="1" background-color-b="1" pointer-events="none" background="enable">)", EM);
				if (owner)
				{
					auto Color = m_gamedata->nations.find(owner)->second->MainColor;
					swprintf_s(buf, LR"(<div id="progress" enable="disable" background-color-r="%f" background-color-g="%f" background-color-b="%f" pointer-events="none" z-index="1e-4" background="enable">)", Color.x, Color.y, Color.z);
					m_DrawItems->Insert(buf, EM);
				}
				else m_DrawItems->Insert(LR"(<div id="progress" enable="disable" background-color="777777" pointer-events="none" z-index="1e-4" background="enable">)", EM);
				m_DrawItems->Insert(LR"(<a id="num" text="000" enable="disable" color-r="0" color-g="0" color-b="0" pointer-events="none">)", EM);
				m_DrawItems->Insert(LR"(<img id="state" src="Window" enable="disable" pointer-events="none">)", EM);

				_Return.insert(std::make_pair(L"leaderid", Str(m_gamedata->leader_progress)));
				auto L = m_gamedata->NewLeader(id, owner, draft_size);

				if (auto N = m_gamedata->nations.find(prov->second->ruler); N != m_gamedata->nations.end())
				{
					L.abb_move = N->second->abb_army_move;
					L.abb_sieze = N->second->abb_army_sieze;
					L.abb_disp = N->second->abb_disp;
				}

				if (arg.find(L"abb_move") != arg.end()) L.abb_move = Float(arg[L"abb_move"]);
				if (arg.find(L"abb_sieze") != arg.end()) L.abb_sieze = Float(arg[L"abb_sieze"]);
				if (arg.find(L"abb_disp") != arg.end()) L.abb_disp = Float(arg[L"abb_disp"]);

				m_gamedata->leaders.insert(std::make_pair(m_gamedata->leader_progress++, std::make_unique<Leader>(L)));

				if (prov->second->owner == mUser.nationPick || mUser.nationPick == 0)
					swprintf_s(buf, LR"(<img id="flag" src="%ls" enable="disable" mousedown="SelectLeader">)", nation_name.c_str());
				else
					swprintf_s(buf, LR"(<img id="flag" src="%ls" enable="disable" mousedown="SelectLeader">)", nation_name.c_str());
				
				m_DrawItems->Insert(buf, EM);
				if (try_lock) draw_mutex.unlock();
			}
		}
	}
	return _Return;
}


void MyApp::GUIUpdatePanelLeader(LeaderId leader_id)
{
	if (leader_id == 0)
		leader_id = m_gamedata->last_leader_id;	
	else
		m_gamedata->last_leader_id = leader_id;
	m_gamedata->last_prov_id = 0;
	std::wstring nation_name = L"알 수 없음";
	std::wstring present_com = L"알 수 없음";
	std::wstring progress = L"알 수 없음";
	LeaderId leader = leader_id;
	std::wstring head = L"LeaderId : " + Str(leader_id);
	if (auto Q = m_gamedata->leaders.find(leader); Q != m_gamedata->leaders.end())
	{
		head.resize(256);
		swprintf_s(head.data(), 256, L"%llu번째 군대 %lld명", leader_id, Q->second->size);

		if (Q->second->selected)
		{
			if (Q->second->cmd.size() > 0)
			{
				float sum = 0;
				auto R = Q->second->cmd.cbegin();
				switch (R->type)
				{
				case CommandType::Move:
					present_com = m_gamedata->province.at(R->target_prov)->name + L"로 이동중";
					
					std::for_each(Q->second->cmd.begin(), Q->second->cmd.end(), [&S = sum](std::list<Command>::const_reference O) { S += O.need; });
					progress = Str((int)Q->second->cmd_pr) + L" / " + Str((int)R->need) + L"일 : 총( " + Str((int)sum) + L")";
					break;
				case CommandType::Sieze:
					present_com = m_gamedata->province.at(R->target_prov)->name + L"를 공성중";
					progress = Str((int)Q->second->cmd_pr) + L" / " + Str((int)R->need) + L"일 뒤에 공성";
					break;
				}

			}
			else
			{
				present_com = L"대기 중";
			}
		}
		if (auto R = m_gamedata->nations.find(Q->second->owner); R != m_gamedata->nations.end())
		{
			nation_name = R->second->MainName;
		}
	}

	m_DrawItems->$(L".myForm").css(
		{
			L"gamedata-provinceid", Str(leader_id)
		});
	m_DrawItems->$(L".myForm #flag").css(
		{
			L"src", nation_name
		});
	m_DrawItems->$(L".myForm #textContainer text0").css(
		{
			L"text", nation_name
		});
	m_DrawItems->$(L".myForm #textContainer text1").css(
		{
			L"text", present_com
		});
	m_DrawItems->$(L".myForm #head").css(
		{
			L"text", head
		});
	m_DrawItems->$(L".myForm #tail").css(
		{
			L"text", L"군사 확인 메뉴"
		});
	m_DrawItems->$(L".myForm #textContainer text2").css(
		{
			L"text", progress
		});

	m_DrawItems->$(L".myForm #buttonbar button0 text").css(
		{
			L"text", L"잠김",
			L"mousedown", L""
		});
	m_DrawItems->$(L".myForm #buttonbar button1 text").css(
		{
			L"text", L"잠김",
			L"mousedown", L""
		});
	m_DrawItems->$(L".myForm #buttonbar button2 text").css(
		{
			L"text", L"잠김",
			L"mousedown", L""
		});
}

void MyApp::GUIUpdatePanelProvince(ProvinceId prov_id)
{
	if (prov_id == 0)
		prov_id = m_gamedata->last_prov_id;
	else
		m_gamedata->last_prov_id = prov_id;
	m_gamedata->last_leader_id = 0;

	auto prov = m_gamedata->province.find(prov_id);
	if (prov == m_gamedata->province.end())
		return;

	auto N = m_gamedata->nations.find(prov->second->owner);
	std::wstring nation_name = N == m_gamedata->nations.end() ? L"모르는국기" : N->second->MainName;

	m_DrawItems->$(L".myForm").css(
		{
			L"gamedata-provinceid", Str(prov_id)
		});
	m_DrawItems->$(L".myForm #flag").css(
		{
			L"src", nation_name
		});
	m_DrawItems->$(L".myForm #textContainer text0").css(
		{
			L"text", nation_name
		});
	m_DrawItems->$(L".myForm #textContainer text2").css(
		{
			L"text", L"예비군 : " + Str(prov->second->man)
		});
	m_DrawItems->$(L".myForm #textContainer text3").css(
		{
			L"text", L"/" + Str(prov->second->maxman)
		});
	m_DrawItems->$(L".myForm #head").css(
		{
			L"text", prov->second->name + L":" + Str(prov->first)
		});
	m_DrawItems->$(L".myForm #tail").css(
		{
			L"text", L"영토 확인 메뉴"
		});
	m_DrawItems->$(L".myForm #textContainer text1").css(
		{
			L"text", L"HP " + Str(prov->second->hp) + L" / " + Str(prov->second->p_num)
		});
	if (auto X = Act(L"Draft", {L"location", Str(prov->first)}, true); X.find(L"SUCCESS") != X.end() && (prov->second->ruler == mUser.nationPick || mUser.nationPick == 0))
	{
		m_DrawItems->$(L".myForm #buttonbar button0").css(
			{
				L"src", L"Button",
			});
		m_DrawItems->$(L".myForm #buttonbar button0 text").css(
			{
				L"text", L"병사 소집",
				L"mousedown", L"DraftForP3"
			});
	}
	else
	{
		m_DrawItems->$(L".myForm #buttonbar button0").css(
			{
				L"src", L"DarkButton",
			});
		m_DrawItems->$(L".myForm #buttonbar button0 text").css(
			{
				L"text", L"병사 소집",
				L"mousedown", L""
			});
	}
	m_DrawItems->$(L".myForm #buttonbar button1 text").css(
		{
			L"text", L"잠김",
			L"mousedown", L""
		});
	m_DrawItems->$(L".myForm #buttonbar button2 text").css(
		{
			L"text", L"잠김",
			L"mousedown", L""
		});
}

void MyApp::Execute(const std::wstring& func_name, const std::uint64_t& uuid)
{
	

	auto& O = *m_DrawItems->withUUID(uuid).content.begin();
	{
		if (func_name == L"FormHold")
		{
			(*O)[L"hold"] = L"1";
			(*O)[L"FirstMousePos.x"] = Str(mLastMousePos.x);
			(*O)[L"FirstMousePos.y"] = Str(mLastMousePos.y);
			(*O)[L"z-index"] = Str(O->z_index + 1e-2);
		}
		else if (func_name == L"FormUnhold")
		{
			(*O)[L"hold"] = L"0";
			(*O)[L"z-index"] = Str(O->z_index - 1e-2);
		}
		else if (func_name == L"DraftForP3")
		{
			Act(L"Draft", {L"location", (**m_DrawItems->$(Str(uuid) + L" .. .. ..").begin())[L"gamedata-provinceid"]});
			
		}
		else if (func_name == L"SelectLeader")
		{
			game_contype = GameControlType::Leader;
			auto P = (*m_DrawItems->$(Str(uuid) + L" ..").begin());
			bool is_select = false;
			LeaderId leader = std::stoull((*P)[L"gamedata-leaderid"]);
			
			for (auto& Q : m_gamedata->leaders)
			{
				if (Q.first == leader)
				{
					Q.second->selected = !Q.second->selected;
					if (Q.second->selected)
					{
						m_DrawItems->$(L"#leader" + Str(Q.first)).css(
							{
								L"src", L"WindowHighlight"
							}
						);
						is_select = true;
					}
					else
					{
						m_DrawItems->$(L"#leader" + Str(Q.first)).css(
							{
								L"src", L"Window"
							}
						);
					}
				}
				else if (Q.second->selected && !GetAsyncKeyState(VK_LCONTROL))
				{
					m_DrawItems->$(L"#leader" + Str(Q.first)).css(
						{
							L"src", L"Window"
						}
					);
					Q.second->selected = false;
				}
			}

			if (is_select)
			{
				UpdateArrow();
				GUIUpdatePanelLeader(leader);
			}
		}

		else if (func_name == L"button0") //Tang Invasion
		{
			NationId owner = 0;
			for (auto& N : m_gamedata->nations) if (N.second->MainName == L"당나라") { owner = N.first; break; };

			Act(L"Draft", { L"location", L"18", L"size", L"25000", L"owner", Str(owner), L"force", L"" });
		}
		else if (func_name == L"button1") //Magal Invasion
		{
			NationId owner = 0;
			for (auto& N : m_gamedata->nations) if (N.second->MainName == L"말갈") { owner = N.first; break; };

			draw_mutex.lock();
			Act(L"Draft", { L"location", L"16", L"size", L"3500", L"owner", Str(owner), L"force", L"" }, false, false);
			Act(L"Draft", { L"location", L"16", L"size", L"2500", L"owner", Str(owner), L"force", L"" }, false, false);
			Act(L"Draft", { L"location", L"16", L"size", L"1500", L"owner", Str(owner), L"force", L"" }, false, false);
			draw_mutex.unlock();
		}
		else if (func_name == L"button2") //We Invasion
		{
			NationId owner = 0;
			for (auto& N : m_gamedata->nations) if (N.second->MainName == L"왜") { owner = N.first; break; };

			draw_mutex.lock();
			Act(L"Draft", { L"location", L"2", L"size", L"1100", L"owner", Str(owner), L"force", L"" }, false, false);
			Act(L"Draft", { L"location", L"3", L"size", L"1100", L"owner", Str(owner), L"force", L"" }, false, false);
			Act(L"Draft", { L"location", L"4", L"size", L"1100", L"owner", Str(owner), L"force", L"" }, false, false);
			Act(L"Draft", { L"location", L"7", L"size", L"1100", L"owner", Str(owner), L"force", L"" }, false, false);
			draw_mutex.unlock();
		}
		else if (func_name == L"button3") //Yuan Invasion
		{
			NationId owner = 0;
			for (auto& N : m_gamedata->nations) if (N.second->MainName == L"원나라") { owner = N.first; break; };
			draw_mutex.lock();
			for (int i = 0; i < 3; i++)
			{
				Act(L"Draft", { L"location", L"18", L"size", L"12000", L"owner", Str(owner), L"force", L"", L"abb_sieze", L"4", L"abb_move", L"3" }, false, false);
			}
			draw_mutex.unlock();
		}
		else if (func_name == L"button4") //Joseon Revolt
		{
			NationId owner = 0;
			for (auto& N : m_gamedata->nations) if (N.second->MainName == L"조선") { owner = N.first; break; };

			Act(L"Draft", { L"location", L"12", L"size", L"13000", L"owner", Str(owner), L"force", L"", L"abb_sieze", L"2" });
		}
		else if (func_name == L"button5") //Goryeo Revolt
		{
			NationId owner = 0;
			for (auto& N : m_gamedata->nations) if (N.second->MainName == L"고려") { owner = N.first; break; };

			draw_mutex.lock();
			for (int i = 0; i < 3; i++)
			{
				Act(L"Draft", { L"location", L"7", L"size", L"5000", L"owner", Str(owner), L"force", L"", L"abb_move", L"2" }, false, false);
			}
			draw_mutex.unlock();
		}
		else if (func_name == L"button6") //Gaya Revolt
		{
			NationId owner = 0;
			for (auto& N : m_gamedata->nations) if (N.second->MainName == L"가야") { owner = N.first; break; };

			Act(L"Draft", { L"location", L"3", L"size", L"4000", L"owner", Str(owner), L"force", L"" });
		}
		else if (func_name == L"button7") //Balhae Revolt
		{
			NationId owner = 0;
			for (auto& N : m_gamedata->nations) if (N.second->MainName == L"발해") { owner = N.first; break; };

			Act(L"Draft", { L"location", L"15", L"size", L"7000", L"owner", Str(owner), L"force", L"" });
		}
		else if (func_name == L"button8") //Balhae Revolt
		{
			NationId owner = 0;
			for (auto& N : m_gamedata->nations) if (N.second->MainName == L"한국") { owner = N.first; break; };

			Act(L"Draft", { L"location", L"2", L"size",  L"3333", L"owner", Str(owner), L"force", L"" });
			Act(L"Draft", { L"location", L"3", L"size",  L"3333", L"owner", Str(owner), L"force", L"" });
			Act(L"Draft", { L"location", L"4", L"size",  L"3333", L"owner", Str(owner), L"force", L"" });
			Act(L"Draft", { L"location", L"5", L"size",  L"3333", L"owner", Str(owner), L"force", L"" });
			Act(L"Draft", { L"location", L"6", L"size",  L"3333", L"owner", Str(owner), L"force", L"" });
			Act(L"Draft", { L"location", L"7", L"size",  L"3333", L"owner", Str(owner), L"force", L"" });
			Act(L"Draft", { L"location", L"8", L"size",  L"3333", L"owner", Str(owner), L"force", L"" });
			Act(L"Draft", { L"location", L"9", L"size",  L"3333", L"owner", Str(owner), L"force", L"" });
			Act(L"Draft", { L"location", L"10", L"size", L"3333", L"owner", Str(owner), L"force", L"" });
			Act(L"Draft", { L"location", L"11", L"size", L"3333", L"owner", Str(owner), L"force", L"" });
			Act(L"Draft", { L"location", L"12", L"size", L"3333", L"owner", Str(owner), L"force", L"" });
			Act(L"Draft", { L"location", L"13", L"size", L"3333", L"owner", Str(owner), L"force", L"" });
			Act(L"Draft", { L"location", L"14", L"size", L"3333", L"owner", Str(owner), L"force", L"" });
		}
		else if (func_name == L"button9") //Balhae Revolt
		{
			NationId owner = 0;
			for (auto& N : m_gamedata->nations) if (N.second->MainName == L"낙랑") { owner = N.first; break; };

			Act(L"Draft", { L"location", L"9", L"size", L"30000", L"owner", Str(owner), L"force", L"" });
		}
	}
}

void MyApp::GameUpdate()
{
	draw_mutex.lock();
	if (auto N = m_gamedata->nations.find(mUser.nationPick); N != m_gamedata->nations.end())
	{
		m_DrawItems->$(L"#myNationFlag").css({
			L"src", m_gamedata->nations.at(mUser.nationPick)->MainName
			});
	}
	else
	{
		m_DrawItems->$(L"#myNationFlag").css({
			L"src", L"모르는국기"
			});
	}
	m_DrawItems->$(L"#myDiv").css({
		L"left", Str(mLastMousePos.x),
		L"top", Str(mLastMousePos.y)
	});

	for (auto& D : m_DrawItems->data)
	{
		if (D[L"hold"] == L"1")
		{
			D[L"left"] = Str(D.left + mLastMousePos.x - Float(D[L"FirstMousePos.x"]));
			D[L"top"] = Str(D.top + mLastMousePos.y - Float(D[L"FirstMousePos.y"]));
			D[L"FirstMousePos.x"] = Str(mLastMousePos.x);
			D[L"FirstMousePos.y"] = Str(mLastMousePos.y);
		}
	}

	

	XMFLOAT4 rgb;
	std::list<decltype(m_gamedata->leaders)::value_type> itr_buf;
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

		float w = mClientHeight / 20.f * O.second->name.length() + 10.f;
		float h = mClientHeight / 15.f;
		float size = 25.0f / s.z;
		float depth = (s.z - 1.f) / (1000.f - 1.f);
		if (s.z >= 1.f && s.z <= 1000.0f)
		{
			w *= size;
			h *= size;
			m_DrawItems->$(L"#prov" + Str(O.first)).css(
				{
					L"background-color-r", Str(rgb.x / 1.5f),
					L"background-color-g", Str(rgb.y / 1.5f),
					L"background-color-b", Str(rgb.z / 1.5f),
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
			m_DrawItems->$(L"#provtext" + Str(O.first)).css(
				{
					L"enable", L"enable",
					L"width", Str(w),
					L"height", Str(h),
					L"text", O.second->name,// + L" " + */Str(O.first) ,
					L"z-index", Str(1 - depth + 1e-6),
					L"horizontal-align", L"center",
					L"vertical-align", L"center"
				}
			);
		}
		else
		{
			m_DrawItems->$(L"#prov" + Str(O.first)).css(
				{
					L"enable", L"disable"
				}
			);

			m_DrawItems->$(L"#provtext" + Str(O.first)).css(
				{
					L"enable", L"disable"
				}
			);
		}

		itr_buf.clear();
		for (const auto& P : m_gamedata->leaders)
		{
			if (P.second->location == O.first)
			{
				itr_buf.push_back(P);
			}
		}

		std::uint64_t  i = 0;
		for (const auto& P : itr_buf)
		{
			if (s.z >= 1.f && s.z <= 1000.0f)
			{
				m_DrawItems->$(L"#leader" + Str(P.first)).css(
					{
						L"enable", L"enable",
						L"left", Str(s.x + (i - (itr_buf.size() - 1.f) / 2) * size * 100.f),
						L"top",Str(s.y + size * 135.f),
						L"width", Str(size * 95.f),
						L"height",Str(size * 95.f),
						L"z-index", Str(1 - depth),
						L"horizontal-align", L"center",
						L"vertical-align", L"center"
					}
				);
				m_DrawItems->$(L"#leader" + Str(P.first) + L" flag").css(
					{
						L"enable", L"enable",
						L"width", Str(size * 95.f / 32 * 26),
						L"height",Str(size * 95.f / 32 * 26),
						L"z-index", Str(0 - depth),
						L"horizontal-align", L"center",
						L"vertical-align", L"center"
					}
				);
				std::wstring state = L"";

				if (P.second->cmd.size() > 0)
				{
					switch (P.second->cmd.begin()->type)
					{
					case CommandType::Move:
						state = L"Leader-move";
						break;
					case CommandType::Attack:
						state = L"Leader-attack";
						break;
					case CommandType::Sieze:
						state = L"Leader-sieze";
						break;
					}
				}



				m_DrawItems->$(L"#leader" + Str(P.first) + L" state").css(
					{
						L"src", state,
						L"enable", state == L"" ? L"disable" : L"enable",
						L"width", Str(size * 95.f / 32 * 26),
						L"height",Str(size * 95.f / 32 * 26),
						L"z-index", Str(2 - depth),
						L"horizontal-align", L"center",
						L"vertical-align", L"center"
					}
				);
				m_DrawItems->$(L"#leader" + Str(P.first) + L" num").css(
					{
						L"text", Str(P.second->size),
						L"enable", L"enable",
						L"width", Str(size * 95.f / 32 * 26),
						L"top", Str(size * 95.f / 32 * 26 * 2 / 3),
						L"height",Str(size * 95.f / 32 * 26 / 3),
						L"z-index", Str(3 - depth),
						L"horizontal-align", L"center",
						L"vertical-align", L"top"
					}
				);
				m_DrawItems->$(L"#leader" + Str(P.first) + L" background").css(
					{
						L"enable", L"enable",
						L"left", Str(-size * 95.f / 32 * 13),
						L"width", Str(size * 95.f / 32 * 26),
						L"top", Str(size * 95.f / 32 * 26 * 1 / 3),
						L"height",Str(size * 95.f / 32 * 26 / 4),
						L"z-index", Str(2 - depth),
						L"horizontal-align", L"left",
						L"vertical-align", L"bottom"
					}
				);

				if (P.second->cmd.size() > 0)
				{
					m_DrawItems->$(L"#leader" + Str(P.first) + L" progress").css(
						{
							L"enable", L"enable",
							L"left", Str(-size * 95.f / 32 * 13),
							L"width", Str(size * 95.f / 32 * 26 * (P.second->cmd_pr / P.second->cmd.begin()->need)),
							L"top", Str(size * 95.f / 32 * 26 * 1 / 3),
							L"height",Str(size * 95.f / 32 * 26 / 4),
							L"z-index", Str(2 - depth),
							L"horizontal-align", L"left",
							L"vertical-align", L"bottom"
						}
					);
				}
				else
				{
					m_DrawItems->$(L"#leader" + Str(P.first) + L" progress").css(
						{
							L"enable", L"enable",
							L"left", Str(-size * 95.f / 32 * 13),
							L"width", Str(size * 95.f / 32 * 26),
							L"top", Str(size * 95.f / 32 * 26 * 1 / 3),
							L"height",Str(size * 95.f / 32 * 26 / 4),
							L"z-index", Str(2 - depth),
							L"horizontal-align", L"left",
							L"vertical-align", L"bottom"
						}
					);
				}

			}
			else
			{
				m_DrawItems->$(L"#leader" + Str(P.first)).css(
					{
						L"enable", L"disable"
					}
				);
				m_DrawItems->$(L"#leader" + Str(P.first) + L" flag").css(
					{
						L"enable", L"disable"
					}
				);
				m_DrawItems->$(L"#leader" + Str(P.first) + L" state").css(
					{
						L"enable", L"disable"
					}
				);
				m_DrawItems->$(L"#leader" + Str(P.first) + L" num").css(
					{
						L"enable", L"disable"
					}
				);
				m_DrawItems->$(L"#leader" + Str(P.first) + L" background").css(
					{
						L"enable", L"disable"
					}
				);
				m_DrawItems->$(L"#leader" + Str(P.first) + L" progress").css(
					{
						L"enable", L"disable"
					}
				);
			}
			++i;
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

	for (auto& O : m_DrawItems->data)
	{
		if (O.parent > 0)
		{
			O[L"inherit-left"] = L"0";
			O[L"inherit-top"] = L"0";
			O[L"inherit-z-index"] = L"0";
			for (auto& P : m_DrawItems->withUUID(O.parent))
			{
				O[L"inherit-left"] = Str(P->inherit_left + P->left);
				O[L"inherit-top"] = Str(P->inherit_top + P->top);
				O[L"inherit-z-index"] = Str(P->inherit_z_index + P->z_index);
			}
		}
		else
		{
			O[L"inherit-left"] = L"0";
			O[L"inherit-top"] = L"0";
			O[L"inherit-z-index"] = L"0";
		}
	}
	draw_mutex.unlock();

	mEyetarget.m128_f32[0] += mEyeMoveX;
	mEyetarget.m128_f32[2] += mEyeMoveZ;
	mEyeMoveX -= mEyeMoveX;
	mEyeMoveZ -= mEyeMoveZ;
}

void MyApp::GameClose()
{
	m_gamedata->run = false;
	
	////std::this_thread::sleep_for(std::chrono::seconds(1));
	//trdGame.join()
	//trdGame.w;

	for (auto& O : mBitmap)
		DeleteObject(O.second);
	log_main.close();
	if (m_fullscreenMode)
		ToggleFullscreenWindow();
	m_d2d.reset();
}

void MyApp::InsertArrow(const ProvinceId& start, ProvincePath& path, bool clear)
{
	

	std::vector<XMFLOAT3> buf0, buf1;
	
	if (path.path.size() == 0) return;

	auto S = m_gamedata->province.at(start)->on3Dpos;
	auto E = m_gamedata->province.at(*path.path.rbegin())->on3Dpos;

	buf0.push_back({ S.x / 2, S.y / 2 + 1.f, S.z / 2 });
	for (auto Q : path)
	{
		auto pos = m_gamedata->province.at(Q)->on3Dpos;
		buf0.push_back({ pos.x / 2, pos.y / 2 + 1.f, pos.z / 2 });
	}
	
	//buf0.push_back({ E.x / 2, E.y / 2 + 1.f, E.z / 2 });

	/*XMVECTOR v = XMLoadFloat3(new XMFLOAT3((E.x - S.x) / 2, 0.f, (E.z - S.z) / 2));

	v = XMVector2Orthogonal(v);
	v = XMVector2Normalize(v);

	XMFLOAT2 F;

	XMStoreFloat2(&F, v);
*/
	


	for (float length = sqrtf(powf((S.x - E.x) / 2, 2) + powf((S.y - E.y) / 2, 2)); length > 1; length /= 2)
	{
		buf1.clear();
		for (int i = 0; i < buf0.size() - 1; ++i)
		{
			buf1.push_back(buf0.at(i));
			buf1.push_back({ (buf0.at(i).x + buf0.at(i + 1).x) / 2,(buf0.at(i).y + buf0.at(i + 1).y) / 2, (buf0.at(i).z + buf0.at(i + 1).z) / 2 });
		}
		buf1.push_back(buf0.at(buf0.size() - 1));
		buf0.swap(buf1);
	}


	if (clear) 
	{
		mArrows.vertices.clear();
		mArrows.indices.clear();
	} 

	mArrows.points.clear();
	//float center = (buf0.size() - 1) / 2.f;
	for (int i = 0; i < buf0.size(); ++i)
	{
		float ox = 1.f * buf0.at(i).x + (map_w - 1.f) / 2.f, oy = 1.f * buf0.at(i).z + (map_h - 1.f) / 2.f;
		int x = (int)floorf(ox);
		int y = (int)floorf(oy);
		ox -= x;
		oy -= y;

		if (x >= 0 && x < map_w - 1 && y >= 0 && y < map_h - 1)
		{
			//buf0.at(i).y = mLandVertices.at(x + map_w * y).Pos.y;

			float s0_0 = mLandVertices.at(x + map_w * y).Pos.y;
			float s0_1 = mLandVertices.at(x + map_w * (y + 1)).Pos.y;
			float s1_0 = mLandVertices.at(x + 1 + map_w * y).Pos.y;
			float s1_1 = mLandVertices.at(x + 1 + map_w * (y + 1)).Pos.y;

			buf0.at(i).y = s0_0 * (1 - ox) * (1 - oy) +
				s0_1 * (1 - ox) * oy +
				s1_0 * ox * (1 - oy) +
				s1_1 * ox * oy;

		}
		//buf0.at(i).x += 5 * F.x * (1 - powf((center - i) / center, 2));
		//buf0.at(i).y += 5 * F.y * (1 - powf((center - i) / center, 2));


		mArrows.points.push_back(buf0.at(i));
	}

	mArrows.BuildLine();

}

void MyApp::UpdateArrow()
{
	mArrows.vertices.clear();
	mArrows.indices.clear();
	for (auto& Q : m_gamedata->leaders)
	{
		if (Q.second->selected)
		{
			ProvinceId lastLoc = Q.second->location;
			ProvincePath path;

			for (auto C : Q.second->cmd)
			{
				if (C.type == CommandType::Move)
				{
					lastLoc = C.target_prov;
					path.path.push_back(lastLoc);
				}
			}
			InsertArrow(Q.second->location, path, false);
		}
	}
}

void MyApp::ProvinceMousedown(WPARAM btnState, ProvinceId id)
{
	auto prov = m_gamedata->province.find(id);
	if (prov == m_gamedata->province.end())
	{
		if (btnState & MK_LBUTTON)
		{
			mArrows.vertices.clear();
			mArrows.indices.clear();
			for (auto& O : m_gamedata->leaders)
			{
				if (O.second->selected)
				{
					m_DrawItems->$(L"#leader" + Str(O.first)).css(
						{
							L"src", L"Window"
						}
					);
					O.second->selected = false;
				}
			}
			game_contype = GameControlType::View;
		}
		else if (btnState & MK_RBUTTON)
		{
			if (m_gamedata->last_leader_id > 0)
			{
				game_contype = GameControlType::Leader;
			}
		}
		return;
	}
	captions[L"선택한 프로빈스"] = std::to_wstring(id);
	captions[L"선택한 프로빈스.x"] = std::to_wstring(2.f * prov->second->on3Dpos.x / prov->second->p_num);
	captions[L"선택한 프로빈스.z"] = std::to_wstring(2.f * prov->second->on3Dpos.z / prov->second->p_num);



	if (btnState & MK_LBUTTON)
	{
		/*prov->second->owner = mUser.nationPick;
		prov->second->ruler = mUser.nationPick;*/

		mArrows.vertices.clear();
		mArrows.indices.clear();
		for (auto& O : m_gamedata->leaders)
		{
			if (O.second->selected)
			{
				m_DrawItems->$(L"#leader" + Str(O.first)).css(
					{
						L"src", L"Window"
					}
				);
				O.second->selected = false;
			}
		}
		GUIUpdatePanelProvince(id);
		game_contype = GameControlType::View;
	}
	else if (btnState & MK_RBUTTON)
	{
		if (m_gamedata->last_leader_id > 0)
		{
			game_contype = GameControlType::Leader;
			mArrows.vertices.clear();
			mArrows.indices.clear();
			for (auto& O : m_gamedata->leaders)
			{
				if (O.second->selected)
				{
					if (O.second->location == id)
					{
						O.second->cmd_pr = 0;
						O.second->cmd.clear();
					}
					else
					{
						auto path = ProvincePath(m_gamedata->province, m_gamedata->province_connect, O.second->location, id);

						if (path.path.size() > 0)
						{
							O.second->cmd_pr = 0;
							O.second->cmd.clear();
							ProvinceId lastLoc = O.second->location;
							for (auto P : path)
							{
								O.second->cmd.push_back(Command(CommandType::Move, P, 0, m_gamedata->province_connect.at(std::make_pair(lastLoc, P))));
								lastLoc = P;
							}
						}
					}
					
				}
			}
			GUIUpdatePanelLeader();
			UpdateArrow();
		}
	}
	else if (btnState & MK_MBUTTON)
	{
		if (mUser.nationPick > 0) m_gamedata->nations.at(mUser.nationPick)->Ai = true;
		

		mUser.nationPick = prov->second->ruler;
		if (mUser.nationPick > 0) m_gamedata->nations.at(mUser.nationPick)->Ai = false;
		/*else {
			auto I = m_gamedata->nations.begin();
			for (int i = 0; i < rand() % m_gamedata->nations.size(); ++i) ++I;
			prov->second->owner = I->first;
			prov->second->ruler = I->first;
			
		}*/
	}

}

void MyApp::OnKeyDown(WPARAM btnState)
{
	float deltaTime = mTimer.DeltaTime();
	//bool pressAlt = !keyState.at(VK_MENU);
	bool pressShift = GetAsyncKeyState(VK_LSHIFT) != 0;
	//bool pressCtrl = !keyState.at(VK_CONTROL);
	
	switch (game_contype)
	{
	case GameControlType::Leader:
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

		if (keyState.at('E'))
		{
			for (auto& P : m_gamedata->leaders)
			{
				if (P.second->selected)
				{
					P.second->selected = false;

					m_DrawItems->$(L"#leader" + Str(P.first)).css(
						{
							L"src", L"Window"
						}
					);
					break;
				}
			}
			GUIUpdatePanelLeader();
			UpdateArrow();			
		}
		break;
	default:
		if (keyState.at('A'))
		{
			mEyeMoveX -= pressShift ? 0.48f : 0.24f * mRadius * deltaTime;
		}
		else if (keyState.at('D'))
		{
			mEyeMoveX += pressShift ? 0.48f : 0.24f * mRadius * deltaTime;
		}
		else if (keyState.at('W'))
		{
			mEyeMoveZ += pressShift ? 0.48f : 0.24f * mRadius * deltaTime;
		}
		else if (keyState.at('S'))
		{
			mEyeMoveZ -= pressShift ? 0.48f : 0.24f * mRadius * deltaTime;
		}
		else if (keyState.at('1')) //Tang Invasion
		{
			NationId owner = 0;
			for (auto& N : m_gamedata->nations) if (N.second->MainName == L"당나라") { owner = N.first; break; };

			Act(L"Draft", { L"location", L"18", L"size", L"25000", L"owner", Str(owner), L"force", L"" });
		}
		else if (keyState.at('2')) //Magal Invasion
		{
			NationId owner = 0;
			for (auto& N : m_gamedata->nations) if (N.second->MainName == L"말갈") { owner = N.first; break; };

			draw_mutex.lock();
			Act(L"Draft", { L"location", L"16", L"size", L"3500", L"owner", Str(owner), L"force", L"" }, false, false);
			Act(L"Draft", { L"location", L"16", L"size", L"2500", L"owner", Str(owner), L"force", L"" }, false, false);
			Act(L"Draft", { L"location", L"16", L"size", L"1500", L"owner", Str(owner), L"force", L"" }, false, false);
			draw_mutex.unlock();
		}
		else if (keyState.at('3')) //We Invasion
		{
			NationId owner = 0;
			for (auto& N : m_gamedata->nations) if (N.second->MainName == L"왜") { owner = N.first; break; };

			draw_mutex.lock();
			Act(L"Draft", { L"location", L"2", L"size", L"900", L"owner", Str(owner), L"force", L"" }, false, false);
			Act(L"Draft", { L"location", L"3", L"size", L"900", L"owner", Str(owner), L"force", L"" }, false, false);
			Act(L"Draft", { L"location", L"4", L"size", L"900", L"owner", Str(owner), L"force", L"" }, false, false);
			Act(L"Draft", { L"location", L"7", L"size", L"900", L"owner", Str(owner), L"force", L"" }, false, false);
			draw_mutex.unlock();
		}
		else if (keyState.at('4')) //Yuan Invasion
		{
			NationId owner = 0;
			for (auto& N : m_gamedata->nations) if (N.second->MainName == L"원나라") { owner = N.first; break; };
			draw_mutex.lock();
			for (int i = 0; i < 3; i++)
			{
				Act(L"Draft", { L"location", L"18", L"size", L"12000", L"owner", Str(owner), L"force", L"", L"abb_sieze", L"4", L"abb_move", L"5" }, false, false);
			}
			draw_mutex.unlock();
		}
		else if (keyState.at('5')) //Joseon Revolt
		{
			NationId owner = 0;
			for (auto& N : m_gamedata->nations) if (N.second->MainName == L"조선") { owner = N.first; break; };

			Act(L"Draft", { L"location", L"12", L"size", L"13000", L"owner", Str(owner), L"force", L"" });
		}
		else if (keyState.at('6')) //Goryeo Revolt
		{
			NationId owner = 0;
			for (auto& N : m_gamedata->nations) if (N.second->MainName == L"고려") { owner = N.first; break; };

			draw_mutex.lock();
			for (int i = 0; i < 3; i++)
			{
				Act(L"Draft", { L"location", L"7", L"size", L"5000", L"owner", Str(owner), L"force", L"" }, false, false);
			}
			draw_mutex.unlock();
		}
		else if (keyState.at('7')) //Gaya Revolt
		{
			NationId owner = 0;
			for (auto& N : m_gamedata->nations) if (N.second->MainName == L"가야") { owner = N.first; break; };

			Act(L"Draft", { L"location", L"3", L"size", L"4000", L"owner", Str(owner), L"force", L"" });
		}
		else if (keyState.at('8')) //Balhae Revolt
		{
			NationId owner = 0;
			for (auto& N : m_gamedata->nations) if (N.second->MainName == L"발해") { owner = N.first; break; };

			Act(L"Draft", { L"location", L"15", L"size", L"7000", L"owner", Str(owner), L"force", L"" });
		}
		else if (keyState.at('9')) //Balhae Revolt
		{
			NationId owner = 0;
			for (auto& N : m_gamedata->nations) if (N.second->MainName == L"한국") { owner = N.first; break; };

			Act(L"Draft", { L"location", L"8", L"size", L"8000", L"owner", Str(owner), L"force", L"" });
		}
		else if (keyState.at('0')) //Balhae Revolt
		{
			NationId owner = 0;
			for (auto& N : m_gamedata->nations) if (N.second->MainName == L"낙랑") { owner = N.first; break; };

			Act(L"Draft", { L"location", L"9", L"size", L"20000", L"owner", Str(owner), L"force", L"" });
		}
		break;
	}

	switch (btnState)
	{
	case VK_SPACE:
	{
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
			D2D1::ColorF(D2D1::ColorF::Black), &m_d2d->Brush[L"Main"]));
		ThrowIfFailed(m_d2d->d2dDeviceContext->CreateSolidColorBrush(
			D2D1::ColorF(D2D1::ColorF::Black), &m_d2d->Brush[L"Back"]));

		ThrowIfFailed(m_d2d->d2dDeviceContext->CreateBitmapBrush(nullptr, m_d2d->BitmapBrush.GetAddressOf()));

		ThrowIfFailed(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), &m_d2d->dwriteFactory));
	}

	ComPtr<IDWriteFontFile> tFontFile = nullptr;
	ComPtr<IDWriteFontFileLoader> fontFileLoader = nullptr;
	ComPtr<IDWriteFontCollectionLoader> fontCollLoader = nullptr;

	ThrowIfFailed(AddFontResourceW(LR"(Fonts\EBS주시경M.ttf)"));
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


	if (auto O = MouseOnUI(x, y); O != m_DrawItems->data.rend())
	{
		Execute((*O)[L"mousedown"], O->uuid);
		focus = O->uuid;
	}
	else
	{
		dragx = x;
		dragy = y;
		focus = 0;
		if (GetAsyncKeyState(VK_LSHIFT))
		{
			dragtype = DragType::Leader;
		}
		else
		{
			game_contype = GameControlType::View;
			dragtype = DragType::View;
			Pick(btnState, x, y);
		}
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
	bool flag = false;
	if (x > dragx)
	{
		Draw_rect.left = (float)dragx;
		Draw_rect.right = (float)x;
	}
	else
	{
		Draw_rect.left = (float)x;
		Draw_rect.right = (float)dragx;
	}
	if (y > dragy)
	{
		Draw_rect.top = (float)dragy;
		Draw_rect.bottom = (float)y;
	}
	else
	{
		Draw_rect.top = (float)y;
		Draw_rect.bottom = (float)dragy;
	}
	switch (dragtype)
	{
	case DragType::View:
		dragtype = DragType::None;
		break;
	case DragType::Leader:
		if (!GetAsyncKeyState(VK_LCONTROL))
		{
			for (auto& L : m_gamedata->leaders)
			{
				if (L.second->selected)
				{
					m_DrawItems->$(L"#leader" + Str(L.first)).css(
						{
							L"src", L"Window"
						}
					);
					L.second->selected = false;
				}
			}
		}
		for (const auto& O : m_gamedata->province)
		{
			if (!O.second->p_num)
			{
				continue;
			}

			XMFLOAT3 pos = O.second->on3Dpos;
			pos.x /= 2.f;
			pos.y /= 2.f;
			pos.z /= 2.f;
			XMFLOAT3 s = Convert3Dto2D(XMLoadFloat3(&pos));

			float w = mClientHeight / 20.f * O.second->name.length() + 10.f;
			float h = mClientHeight / 15.f;
			float size = 25.0f / s.z;
			float depth = (s.z - 1.f) / (1000.f - 1.f);
			if (s.z >= 1.f && s.z <= 1000.0f)
			{
				if (Draw_rect.left <= s.x && s.x <= Draw_rect.right && Draw_rect.top <= s.y && s.y <= Draw_rect.bottom)
				{
					for (auto& L : m_gamedata->leaders)
					{
						if (L.second->location == O.first && (L.second->owner == mUser.nationPick || mUser.nationPick == 0))
						{
							m_DrawItems->$(L"#leader" + Str(L.first)).css(
								{
									L"src", L"WindowHighlight"
								}
							);
							L.second->selected = true;
							flag = true;
							m_gamedata->last_leader_id = L.first;
						}
					}
				}
			}
		}
		if (flag) { GUIUpdatePanelLeader(); game_contype = GameControlType::Leader; };
		dragtype = DragType::None;
		break;
	default:
		if (auto O = MouseOnUI(x, y); O != m_DrawItems->data.rend())
		{
			Execute((*O)[L"mouseup"], O->uuid);
		}
		break;
	}

	ReleaseCapture();
}

std::list<YTML::DrawItem>::reverse_iterator MyApp::MouseOnUI(int x, int y)
{
	draw_mutex.lock();
	for (auto O = m_DrawItems->data.rbegin(); O != m_DrawItems->data.rend(); ++O)
	{
		if (!O->enable || (*O)[L"pointer-events"] == L"none")
			continue;

		Draw_point.x = O->left;
		Draw_point.y = O->top;
		Draw_point.x += O->inherit_left;
		Draw_point.y += O->inherit_top;

		Draw_size.width = O->width;
		Draw_size.height = O->height;

		if ((*O)[L"horizontal-align"] == L"center")
		{
			Draw_rect.left = Draw_point.x - Draw_size.width / 2.f;
			Draw_rect.right = Draw_point.x + Draw_size.width / 2.f;
		}
		else if ((*O)[L"horizontal-align"] == L"right")
		{
			Draw_rect.left = Draw_point.x - Draw_size.width;
			Draw_rect.right = Draw_point.x;
		}
		else
		{
			Draw_rect.left = Draw_point.x;
			Draw_rect.right = Draw_point.x + Draw_size.width;
		}

		if ((*O)[L"vertical-align"] == L"center")
		{
			Draw_rect.top = Draw_point.y - Draw_size.height / 2.f;
			Draw_rect.bottom = Draw_point.y + Draw_size.height / 2.f;
		}
		else if ((*O)[L"vertical-align"] == L"top")
		{
			Draw_rect.top = Draw_point.y - Draw_size.height;
			Draw_rect.bottom = Draw_point.y;
		}
		else
		{
			Draw_rect.top = Draw_point.y;
			Draw_rect.bottom = Draw_point.y + Draw_size.height;
		}

		if (Draw_rect.left <= x && Draw_rect.right >= x &&
			Draw_rect.top <= y && Draw_rect.bottom >= y)
		{
			draw_mutex.unlock();
			return O;
		}
	}
	draw_mutex.unlock();
	return m_DrawItems->data.rend();
}

void MyApp::OnMouseMove(WPARAM btnState, int x, int y)
{

	switch (dragtype)
	{
	case DragType::Leader:
		break;
	case DragType::View:
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
		break;
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

	float time = 0;// fmodf(mTimer.TotalTime() / 3.f + 0.f, 20.f) - 10.f;
	captions[L"현재 시간"] = Str((int)floorf((time + 10) / 20 * 23 + 1)) + L"시 " + Str((int)(fmodf((time + 10) / 20 * 23 + 1, 1) * 60)) + L"분";
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

	auto currArrowsVB = mCurrFrameResource->ArrowsVB.get();
	for (int i = 0; i < mArrows.vertices.size(); ++i)
	{
		Vertex v = mArrows.vertices.at(i);
		currArrowsVB->CopyData(i, v);
	}

	mArrowsRitem->Geo->VertexBufferGPU = currArrowsVB->Resource();



	auto currArrowsIB = mCurrFrameResource->ArrowsIB.get();
	for (int i = 0; i < mArrows.indices.size(); ++i)
	{
		currArrowsIB->CopyData(i, mArrows.indices.at(i));
	}
	mArrowsRitem->IndexCount = static_cast<UINT>(mArrows.indices.size());
	if (mArrowsRitem->IndexCount == 0)
		mArrowsRitem->Visible = false;
	else
		mArrowsRitem->Visible = true;

	mArrowsRitem->Geo->IndexBufferGPU = currArrowsIB->Resource();

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

	auto arrowTex = std::make_unique<Texture>();
	arrowTex->Name = "arrowTex";
	arrowTex->Filename = L"Textures/Arrow.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), arrowTex->Filename.c_str(),
		arrowTex->Resource, arrowTex->UploadHeap));

	mTextures[grassTex->Name] = std::move(grassTex);
	mTextures[waterTex->Name] = std::move(waterTex);
	mTextures[fenceTex->Name] = std::move(fenceTex);
	mTextures[arrowTex->Name] = std::move(arrowTex);
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
	auto arrowTex = mTextures["arrowTex"]->Resource;

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

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	//srvDesc.Format = arrowTex->GetDesc().Format;
	//md3dDevice->CreateShaderResourceView(arrowTex.Get(), &srvDesc, hDescriptor);
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
	std::ifstream file("Map/map.bmp", std::ios::binary);
	std::wifstream prov_list("Map/prov.txt");
	std::ifstream prov_file("Map/prov.bmp", std::ios::binary);
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
		std::streampos length = file.tellg();
		file.seekg(0, std::ios::beg);

		std::vector<unsigned char> buf(length);
		file.read((char*)&buf[0], length);

		file.close();

		prov_file.seekg(0, std::ios::end);
		length = prov_file.tellg();
		prov_file.seekg(0, std::ios::beg);

		std::vector<unsigned char> prov_buf(length);
		prov_file.read((char*)&prov_buf[0], length);
		prov_file.close();


		OutputDebugStringA(("File Length : " + std::to_string(length) + "\n").c_str());
		

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
		unsigned int r, g, b;
		float R0, G0, B0, R1, G1, B1, syc;
		size_t addr, naddr;
		Color32 dex, ndex;
		mWaves = std::make_unique<Waves>(map_h / 3, map_w / 3, 3.0f, 0.03f, 4.0f, 0.2f);
		size_t nx, ny;
		int W[4][2] = { {1 , 0}, {0, -1}, {-1, 0}, {0, 1} };

		for (size_t y = h - 1;; --y) {
			for (size_t x = 0; x < w; ++x) {
				//OutputDebugStringA((std::to_string(x) + ", " + std::to_string(y) + "\n").c_str());
				addr = 54 + (x + (y * w)) * 3;
				r = buf[addr + 0];
				g = buf[addr + 1];
				b = buf[addr + 2];
				
				mLandVertices[x + y * w].Pos = { (x - (w - 1) / 2.f), (float)(r + g + b) / 127 - 1.5f, (y - (h - 1) / 2.f) };
				

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
					for (int i = 0; i < 4; i++)
					{
						nx = x + W[i][0];
						ny = y + W[i][0];
						naddr = 54 + (nx + (ny * w)) * 3;
						if (nx < w && ny < h)
						{
							ndex = rgb2dex(prov_buf.at(naddr + 2), prov_buf.at(naddr + 1), prov_buf.at(naddr));
							if (dex != ndex)
							{
								if (auto nsearch = prov_key.find(ndex); nsearch != prov_key.end())
								{
									m_gamedata->province_connect.insert(std::make_pair(std::make_pair(search->second.first, nsearch->second.first), FLT_MAX));
								}
							}
						}
					}
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

		for (auto& O : m_gamedata->province)
		{
			for (auto& P : m_gamedata->province)
			{
				if (auto Q = m_gamedata->province_connect.find(std::make_pair(O.first, P.first)); Q != m_gamedata->province_connect.end())
				{
					float width = sqrtf(powf(O.second->on3Dpos.x - P.second->on3Dpos.x, 2) + powf(O.second->on3Dpos.z - P.second->on3Dpos.z, 2));
					float height = max(- O.second->on3Dpos.y +P.second->on3Dpos.y, 0);
					Q->second = width + height;
					//OutputDebugStringW((O.second->name + L"->" + P.second->name + L" " + Str(Q->second) + L"km\n").c_str());
				}
			}
		}



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
		DirectX::XMStoreFloat3(&bounds.Center, 0.5f*(vMin + vMax));
		DirectX::XMStoreFloat3(&bounds.Extents, 0.5f*(vMax - vMin));

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

void MyApp::BuildArrowGeometry()
{
	//std::vector<std::uint16_t> indices(3 * mWaves->TriangleCount()); // 3 indices per face


	std::vector<Vertex> vertices(0x8000);// = mArrows.vertices;
	std::vector<std::uint16_t> indices(0x8000);//; = mArrows.indices;

	XMFLOAT3 vMinf3(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
	XMFLOAT3 vMaxf3(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);

	//XMVECTOR vMin = XMLoadFloat3(&vMinf3);
	//XMVECTOR vMax = XMLoadFloat3(&vMaxf3);

	/*for (size_t i = 0; i < vertices.size(); ++i)
	{
		XMVECTOR P = XMLoadFloat3(&vertices[i].Pos);
		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}*/

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "arrowGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferCPU = nullptr;
	geo->VertexBufferGPU = nullptr;

	/*geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);*/

	geo->IndexBufferCPU = nullptr;
	geo->IndexBufferGPU = nullptr;

	//geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
	//	mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;
	
	//BoundingBox bounds;
	//DirectX::XMStoreFloat3(&bounds.Center, 0.5f*(vMin + vMax));
	//DirectX::XMStoreFloat3(&bounds.Extents, 0.5f*(vMax - vMin));

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;
	//submesh.Bounds = bounds;

	geo->DrawArgs["arrow"] = submesh;

	mGeometries["arrowGeo"] = std::move(geo);

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
	DirectX::XMStoreFloat3(&bounds.Center, 0.5f*(vMin + vMax));
	DirectX::XMStoreFloat3(&bounds.Extents, 0.5f*(vMax - vMin));

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

	auto arrow = std::make_unique<Material>();
	arrow->Name = "arrow";
	arrow->MatCBIndex = 3;
	arrow->DiffuseSrvHeapIndex = 3;
	arrow->DiffuseAlbedo = XMFLOAT4(0.8f, 1.0f, 1.0f, 1.0f);
	arrow->FresnelR0 = XMFLOAT3(1.f, 1.f, 1.f);
	arrow->Roughness = 0.5f;

	mMaterials["grass"] = std::move(grass);
	mMaterials["water"] = std::move(water);
	mMaterials["wirefence"] = std::move(wirefence);
	mMaterials["arrow"] = std::move(arrow);
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

	mRitemLayer[(int)RenderLayer::Water].push_back(wavesRitem.get());

	auto arrowRitem = std::make_unique<RenderItem>();
	arrowRitem->World = MathHelper::Identity4x4();
	//XMStoreFloat4x4(&arrowRitem->TexTransform, XMMatrixScaling(5.0f, 5.0f, 1.0f));
	XMStoreFloat4x4(&arrowRitem->World, XMMatrixTranslation(0.0f, 0.0f, 0.0f));
	arrowRitem->ObjCBIndex = all_CBIndex++;
#if 1
	arrowRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
#else
	arrowRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
#endif
	arrowRitem->Mat = mMaterials["wirefence"].get();
	arrowRitem->Geo = mGeometries["arrowGeo"].get();
    arrowRitem->Bounds = arrowRitem->Geo->DrawArgs["arrow"].Bounds;
	arrowRitem->IndexCount = arrowRitem->Geo->DrawArgs["arrow"].IndexCount;
	arrowRitem->StartIndexLocation = arrowRitem->Geo->DrawArgs["arrow"].StartIndexLocation;
	arrowRitem->BaseVertexLocation = arrowRitem->Geo->DrawArgs["arrow"].BaseVertexLocation;
	
	mArrowsRitem = arrowRitem.get();

	mRitemLayer[(int)RenderLayer::Transparent].push_back(arrowRitem.get());
	mAllRitems.push_back(std::move(arrowRitem));

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



	boxRitem.ObjCBIndex = all_CBIndex++;
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



