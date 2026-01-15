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
    float2 texCoord  : TEXCOORD;
};


VertexOut VS(VertexIn Input)
{
    VertexOut Output;
    Output.texCoord = Input.texCoord;
    Output.hPosition = float4(Input.lPosition, 1.0f);
    return Output;
}

float4 PS(VertexOut VOutput)   : SV_TARGET
{
    return float4(ShadowMap.Sample(gsamLinearWrap, VOutput.texCoord).rrr,1);

}

