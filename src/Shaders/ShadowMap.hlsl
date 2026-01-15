#include "CommonBuffer.hlsl"

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
};


VertexOut VS(VertexIn Input)
{
    VertexOut Output;
    float4 WorldPos = mul(float4(Input.lPosition, 1.0f), World);
    Output.hPosition = mul(WorldPos, ViewProj);
    return Output;
}

void PS(VertexOut VOutput)  
{
    
}

