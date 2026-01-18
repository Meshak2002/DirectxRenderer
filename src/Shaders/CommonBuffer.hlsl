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

Texture2D gTextureMaps[512] : register(t0, space0);

Texture2D ShadowMap : register(t0, space1);
TextureCube TexSkyBox : register(t1, space1);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);

cbuffer PassData : register(b0)
{
    float4x4 View;
    float4x4 Proj;
    float4x4 ViewProj;
    float4x4 ShadowTransform;
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
    float UvTileValue;  // Changed to float for fractional tiling
    uint DiffuseTexIndex;
    uint NormalTexIndex;
    uint Padding1;  // Match C++ struct padding
}




float CalcShadowFactor(float4 shadowPosH)
{
    // Complete projection by doing division by w.
    shadowPosH.xyz /= shadowPosH.w;

    // Depth in NDC space.
    float depth = shadowPosH.z;

    uint width, height, numMips;
    ShadowMap.GetDimensions(0, width, height, numMips);

    // Texel size.
    float dx = 1.0f / (float) width;
    float percentLit = 0.0f;
    const float2 offsets[9] =
    {
        float2(-dx, -dx), float2(0.0f, -dx), float2(dx, -dx),
        float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
        float2(-dx, +dx), float2(0.0f, +dx), float2(dx, +dx)
    };

    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        percentLit += ShadowMap.SampleCmpLevelZero(gsamShadow,
            shadowPosH.xy + offsets[i], depth).r;
    }
    return percentLit / 9.0f;
}

