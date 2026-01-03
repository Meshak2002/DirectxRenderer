
Texture2D DiffuseTexture : register(t0);
SamplerState SampleType : register(s0);

cbuffer PassData : register(b0)
{
    float4x4 View;
    float4x4 Proj;
    float4x4 ViewProj;
}

cbuffer ObjData : register(b1)
{
    float4x4 World;
}

struct VertexIn
{
    float3 lPosition : POSITION;
    float4 color     : COLOR;
    float2 texCoord  : TEXCOORD;
};

struct VertexOut
{
    float4 hPosition : SV_POSITION;
    float4 color     : COLOR;
    float2 texCoord  : TEXCOORD;
};

VertexOut VS(VertexIn Input)
{
    VertexOut Output;
    float4 WorldPos = mul(float4(Input.lPosition, 1.0f), World);
    Output.hPosition = mul(WorldPos, ViewProj);
    Output.color = Input.color;
    Output.texCoord = Input.texCoord;
    return Output;
}

float4 PS(VertexOut VOutput)    : SV_TARGET
{
    //return VOutput.color;
    return DiffuseTexture.Sample(SampleType,VOutput.texCoord);
}