
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

Texture2D DiffuseTexture : register(t0);
SamplerState SampleType : register(s0);

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
}

struct VertexIn
{
    float3 lPosition : POSITION;
    float4 color     : COLOR;
    float2 texCoord  : TEXCOORD;
    float3 normalL    : NORMAL;
};

struct VertexOut
{
    float4 hPosition : SV_POSITION;
    float3 wPosition : POSITION;
    float4 color     : COLOR;
    float2 texCoord  : TEXCOORD;
    float3 normalW    : NORMAL;
};

VertexOut VS(VertexIn Input)
{
    VertexOut Output;
    float4 WorldPos = mul(float4(Input.lPosition, 1.0f), World);
    Output.hPosition = mul(WorldPos, ViewProj);
    Output.wPosition = WorldPos.xyz;
    Output.color = Input.color;
    Output.texCoord = Input.texCoord;
    Output.normalW = normalize(mul(Input.normalL, (float3x3) World));
    return Output;
}

float4 PS(VertexOut VOutput)    : SV_TARGET
{
    float3 NormalW = normalize(VOutput.normalW);
    float4 BaseAlbedo = DiffuseTexture.Sample(SampleType, VOutput.texCoord);
    
    float3 ToEye = normalize(Eye - VOutput.wPosition);
    Material Mat = { DiffuseAlbedo , FresnelR0 , Shininess };
    float3 ShadowFactor = float3(1, 1, 1);
    float4 LightColor = ComputeLighting(TotalLights, Mat, VOutput.wPosition, NormalW, ToEye, ShadowFactor);

    float4 FinalColor = BaseAlbedo * (LightColor + float4(0.1f, 0.1f, 0.1f, 0.1f));
    FinalColor.a = DiffuseAlbedo.a;
    return FinalColor;
}

