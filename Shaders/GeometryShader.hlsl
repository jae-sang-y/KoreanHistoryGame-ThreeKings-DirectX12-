#include "Common.hlsl"

#ifndef NUM_DIR_LIGHTS
#define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
#define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
#define NUM_SPOT_LIGHTS 0
#endif

#define MaxProvs 256
#include "LightingUtil.hlsl"

Texture2D    gDiffuseMap : register(t0);

SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

cbuffer cbPerObject : register(b0)
{
	float4x4 gWorld;
	float4x4 gTexTransform;
};

cbuffer cbPass : register(b1)
{
	float4x4 gView;
	float4x4 gInvView;
	float4x4 gProj;
	float4x4 gInvProj;
	float4x4 gViewProj;
	float4x4 gInvViewProj;
	float3 gEyePosW;
	float cbPerObjectPad1;
	float2 gRenderTargetSize;
	float2 gInvRenderTargetSize;
	float gNearZ;
	float gFarZ;
	float gTotalTime;
	float gDeltaTime;
	float4 gAmbientLight;

	float4 gFogColor;
	float gFogStart;
	float gFogRange;
	float2 cbPerObjectPad2;

	Light gLights[MaxLights];
	float4 gProv[MaxProvs];
};

cbuffer cbMaterial : register(b2)
{
	float4   gDiffuseAlbedo;
	float3   gFresnelR0;
	float    gRoughness;
	float4x4 gMatTransform;
};


bool CompareFloat4(float4 L, float4 R, float limit)
{
	return pow(L.x - R.x, 2) + pow(L.y - R.y, 2) + pow(L.z - R.z, 2) + pow(L.w - R.w, 2) < limit;
}


void Subdivide(VertexOut inVerts[3], out VertexOut outVerts[6])
{
	VertexOut m[3];

	m[0] = (VertexOut)0.f;
	m[1] = (VertexOut)0.f;
	m[2] = (VertexOut)0.f;

	m[0].PosH = 0.5*(inVerts[0].PosH + inVerts[1].PosH);
	m[1].PosH = 0.5*(inVerts[1].PosH + inVerts[2].PosH);
	m[2].PosH = 0.5*(inVerts[2].PosH + inVerts[0].PosH);

	m[0].PosW = 0.5*(inVerts[0].PosW + inVerts[1].PosW);
	m[1].PosW = 0.5*(inVerts[1].PosW + inVerts[2].PosW);
	m[2].PosW = 0.5*(inVerts[2].PosW + inVerts[0].PosW);

	m[0].SubProv = inVerts[0].SubProv;
	m[1].SubProv = inVerts[1].SubProv;
	m[2].SubProv = inVerts[2].SubProv;

	m[0].Prov = inVerts[0].Prov;
	m[1].Prov = inVerts[1].Prov;
	m[2].Prov = inVerts[2].Prov;

	//m[0].PosH = normalize(m[0].PosH);
	//m[1].PosH = normalize(m[1].PosH);
	//m[2].PosH = normalize(m[2].PosH);
	
	m[0].NormalW = 0.5*(inVerts[0].NormalW + inVerts[1].NormalW);
	m[1].NormalW = 0.5*(inVerts[1].NormalW + inVerts[2].NormalW);
	m[2].NormalW = 0.5*(inVerts[2].NormalW + inVerts[0].NormalW);

	bool side = false;
	bool same = false;
	for (uint i = 0; i < 3; i++)
	{
		if (inVerts[i].ProvIndex == inVerts[(i + 1) % 3].ProvIndex)
		{
			m[i].Prov = (inVerts[i].Prov + inVerts[(i + 1) % 3].Prov) / 2;
			m[i].SubProv = (inVerts[i].SubProv + inVerts[(i + 1) % 3].SubProv) / 2;
		}
		else if (inVerts[i].ProvIndex != 0 && inVerts[(i + 1) % 3].ProvIndex != 0)
		{
			if (
				inVerts[i].Prov.r == inVerts[(i + 1) % 3].Prov.r &&
				inVerts[i].Prov.g == inVerts[(i + 1) % 3].Prov.g &&
				inVerts[i].Prov.b == inVerts[(i + 1) % 3].Prov.b				
				)
			{
				same = true;
			}
			side = true;
		}
	}
	
	if (side)
	{
		float4 mainColor = float4(0.f, 0.f, 0.f, 0.5f);
		float4 subColor = float4(0.f, 0.f, 0.f, 0.5f);
		if (same)
		{
			mainColor = inVerts[0].Prov;
			mainColor.rgb *= 0.75f;
			subColor = inVerts[0].Prov;
			subColor.rgb *= 0.75f;
		}
		
		{
			for (uint i = 0; i < 3; i++)
			{
				m[i].Prov = mainColor;
				m[i].SubProv = subColor;
				inVerts[i].Prov = mainColor;
				inVerts[i].SubProv = subColor;
			}
		}
	}


	//m[0].Prov = inVerts[0].Prov;
	//m[1].Prov = inVerts[1].Prov;
	//m[2].Prov = inVerts[2].Prov;

	m[0].TexC = 0.5*(inVerts[0].TexC + inVerts[1].TexC);
	m[1].TexC = 0.5*(inVerts[1].TexC + inVerts[2].TexC);
	m[2].TexC = 0.5*(inVerts[2].TexC + inVerts[0].TexC);

	outVerts[0] = inVerts[0];
	outVerts[1] = m[0];
	outVerts[2] = m[2];
	outVerts[3] = inVerts[1];
	outVerts[4] = inVerts[2];
	outVerts[5] = m[1];
}

[maxvertexcount(6)]
void GS(
	triangle VertexOut input[3],
	inout TriangleStream< GSOutput > output
)
{
	/*output.Append(input[0]);
	output.Append(input[1]);
	output.Append(input[2]);*/
	VertexOut out_m[6];
	Subdivide(input, out_m);

	for (uint i = 0; i < 6; i++)
	{
		GSOutput element;
		element = out_m[i];
		output.Append(element);
	}

	
}