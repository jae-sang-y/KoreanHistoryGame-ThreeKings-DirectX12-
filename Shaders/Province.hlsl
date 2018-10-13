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


struct VertexIn
{
	float3 PosL    : POSITION;
	float3 NormalL : NORMAL;
	float2 TexC    : TEXCOORD;
	uint Prov : PROVCOLOR;
};

struct VertexOut
{
	float3 PosL    : VT_POSITION;
	float4 PosH    : SV_POSITION;
	float3 PosW    : POSITION;
	float3 NormalW : NORMAL;
	float2 TexC    : TEXCOORD;
	float4 Prov : PROVCOLOR;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout = (VertexOut)0.0f;

	float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
	vout.PosW = posW.xyz;

	vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);

	vout.PosH = mul(posW, gViewProj);
	vout.PosL = vin.PosL;

	vout.Prov = gProv[vin.Prov];

	float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
	vout.TexC = mul(texC, gMatTransform).xy;

	return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	float4 diffuseAlbedo = gDiffuseMap.Sample(gsamAnisotropicWrap, pin.TexC) * gDiffuseAlbedo;


	pin.NormalW = normalize(pin.NormalW);

	float3 toEyeW = gEyePosW - pin.PosW;
	float distToEye = length(toEyeW);
	toEyeW /= distToEye;

	float4 ambient = gAmbientLight * diffuseAlbedo;

	float tmp = 1.f - pow((pin.PosW.y - diffuseAlbedo.g / 1.f - 0.5f) / 0.5f, 2.f);

	if (tmp > 0.f)
	{
		diffuseAlbedo.r *= 1.f + tmp;
		diffuseAlbedo.g *= 1.f + tmp;
		diffuseAlbedo.b *= 1.f + tmp / 2.f;
	}
	if (pin.PosW.y < 0.5f)
	{
		diffuseAlbedo.b = 1.f;
	}


	//if (pin.PosW.y >= 0.5f)
	{
		//if (true)
		{
			diffuseAlbedo.r = (diffuseAlbedo.r + pin.Prov.r * pin.Prov.a) / (1.f + pin.Prov.a);
			diffuseAlbedo.g = (diffuseAlbedo.g + pin.Prov.g * pin.Prov.a) / (1.f + pin.Prov.a);
			diffuseAlbedo.b = (diffuseAlbedo.b + pin.Prov.b * pin.Prov.a) / (1.f + pin.Prov.a);
		}
		diffuseAlbedo.b *= (pin.PosW.y) / 2.5f;
	}
	
	const float shininess = 1.0f - gRoughness;
	Material mat = { diffuseAlbedo, gFresnelR0, shininess };
	float3 shadowFactor = 1.0f;
	float4 directLight = ComputeLighting(gLights, mat, pin.PosW,
		pin.NormalW, toEyeW, shadowFactor);

	float4 litColor = ambient + directLight;

#ifdef FOG
	float fogAmount = saturate((distToEye - gFogStart) / gFogRange);
	litColor = lerp(litColor, gFogColor, fogAmount);
#endif

	litColor.a = diffuseAlbedo.a;

	if (pin.PosL.x > gTotalTime)
	{
		litColor.r = 0.f;
	}

	return litColor;
}

