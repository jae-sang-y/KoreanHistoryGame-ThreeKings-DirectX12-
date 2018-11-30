
struct VertexIn
{
	float3 PosL    : POSITION;
	float3 NormalL : NORMAL;
	float2 TexC    : TEXCOORD;
	uint Prov : PROVCOLOR;
};

struct VertexOut
{
	float4 PosH    : SV_POSITION;
	float3 PosW    : POSITION;
	float3 NormalW : NORMAL;
	float2 TexC    : TEXCOORD;
	uint ProvIndex : PROVINDEX;
	float4 Prov : PROVCOLOR;
	float4 SubProv : SUBPROVCOLOR;
};

struct GSOutput
{
	float4 PosH    : SV_POSITION;
	float3 PosW    : POSITION;
	float3 NormalW : NORMAL;
	float2 TexC    : TEXCOORD;
	uint ProvIndex : PROVINDEX;
	float4 Prov : PROVCOLOR;
	float4 SubProv : SUBPROVCOLOR;
};