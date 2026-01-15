
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
    float3 lPosition : POSITION;
    float2 texCoord  : TEXCOORD;
    float3 normalW   : NORMAL;
    float3 tangentW  : TANGENT;
};

VertexOut VS(VertexIn Input)
{
    VertexOut Output;
    float4 WorldPos = mul(float4(Input.lPosition, 1.0f), World);
    WorldPos.xyz += Eye;
    Output.hPosition = mul(WorldPos, ViewProj).xyww;
    Output.lPosition = Input.lPosition;
    
    Output.texCoord = Input.texCoord;
    Output.normalW = normalize(mul(Input.normalL, (float3x3) World));
    Output.tangentW = normalize(mul(Input.tangentL, (float3x3) World));
    return Output;
}

float4 PS(VertexOut VOutput)    : SV_TARGET
{
    float4 BaseAlbedo = TexSkyBox.Sample(gsamLinearWrap, VOutput.lPosition);
    return BaseAlbedo;

}

