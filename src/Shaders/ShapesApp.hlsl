
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
};

struct VertexOut
{
    float4 hPosition : SV_POSITION;
    float4 color     : COLOR;
};

VertexOut VS(VertexIn Input)
{
    VertexOut Output;
    float4 WorldPos = mul(float4(Input.lPosition, 1.0f), World);
    Output.hPosition = mul(WorldPos, ViewProj);
    Output.color = Input.color;
    return Output;
}

float4 PS(VertexOut VOutput)    : SV_TARGET
{
    return VOutput.color;
}