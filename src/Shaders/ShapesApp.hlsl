
#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 0
#endif

#include "LightingUtil.hlsl"

Texture2D Texture[512] : register(t0);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

cbuffer PassData : register(b0)
{
    float4x4 View;
    float4x4 Proj;
    float4x4 ViewProj;
    float3 Eye;
    float Padding;
    Light TotalLights[MaxLights];
}

cbuffer ObjData : register(b1)
{
    float4x4 World;
}

cbuffer MaterialData : register(b2)
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Shininess;
    uint DiffuseTexIndex;
    uint NormalTexIndex;
}

struct VertexIn
{
    float3 lPosition : POSITION;
    float2 texCoord  : TEXCOORD;
    float3 normalL   : NORMAL;
    float3 tangentL  : TANGENT;
};

struct VertexOut
{
    float4 hPosition : SV_POSITION;
    float3 wPosition : POSITION;
    float2 texCoord  : TEXCOORD;
    float3 normalW   : NORMAL;
    float3 tangentW  : TANGENT;
};

VertexOut VS(VertexIn Input)
{
    VertexOut Output;
    float4 WorldPos = mul(float4(Input.lPosition, 1.0f), World);
    Output.hPosition = mul(WorldPos, ViewProj);
    Output.wPosition = WorldPos.xyz;
    Output.texCoord = Input.texCoord;
    Output.normalW = normalize(mul(Input.normalL, (float3x3) World));
    Output.tangentW = normalize(mul(Input.tangentL, (float3x3) World));
    return Output;
}

float4 PS(VertexOut VOutput)    : SV_TARGET
{
    float4 BaseAlbedo = Texture[DiffuseTexIndex].Sample(gsamAnisotropicWrap, VOutput.texCoord);
    float4 NormalMapCoord = Texture[NormalTexIndex].Sample(gsamAnisotropicWrap, VOutput.texCoord);
    
    float3 NormalW = normalize(VOutput.normalW);
    float3 BumpedNormalWPos = NormalSampleToWorldPos(NormalMapCoord.rgb, NormalW, VOutput.tangentW);
    
    //BumpedNormalWPos = NormalW; // Dumb thing
    
    float3 ToEye = normalize(Eye - VOutput.wPosition);
    float Shine = Shininess * NormalMapCoord.a;
    Material Mat = { DiffuseAlbedo, FresnelR0, Shine };
    float3 ShadowFactor = float3(1, 1, 1);
    float4 LightColor = ComputeLighting(TotalLights, Mat, VOutput.wPosition, BumpedNormalWPos, ToEye, ShadowFactor);

    float4 FinalColor = BaseAlbedo * (LightColor + float4(0.1f, 0.1f, 0.1f, 0.1f));
    FinalColor.a = DiffuseAlbedo.a;
    return FinalColor;
}

