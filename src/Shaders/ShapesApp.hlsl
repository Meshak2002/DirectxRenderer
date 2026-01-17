
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
    float3 wPosition : POSITION0;
    float4 hShadowPosition : POSITION1;
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
    Output.hShadowPosition = mul(WorldPos, ShadowTransform);
    
    return Output;
}

float4 PS(VertexOut VOutput)    : SV_TARGET
{
    float4 ambientLight = float4(0.1f, 0.1f, 0.1f, 1.0f);
    float4 mDiffuseAlbedo = DiffuseAlbedo;
    mDiffuseAlbedo *= gTextureMaps[DiffuseTexIndex].Sample(gsamAnisotropicWrap, VOutput.texCoord);
    float4 NormalMapCoord = gTextureMaps[NormalTexIndex].Sample(gsamAnisotropicWrap, VOutput.texCoord);
    
    float3 NormalW = normalize(VOutput.normalW);
    float3 BumpedNormalWPos = NormalSampleToWorldPos(NormalMapCoord.rgb, NormalW, VOutput.tangentW);
    
    //BumpedNormalWPos = NormalW; // Dumb thing
    
    float4 ambient = ambientLight * mDiffuseAlbedo;
    
    float3 ToEye = normalize(Eye - VOutput.wPosition);
    float Shine = Shininess * NormalMapCoord.a;
    Material Mat = { mDiffuseAlbedo, FresnelR0, Shine };
    float3 ShadowFactor = float3(1, 1, 1);
    
    ShadowFactor[0] = CalcShadowFactor(VOutput.hShadowPosition);
    
    float4 DirectLight = ComputeLighting(TotalLights, Mat, VOutput.wPosition, BumpedNormalWPos, ToEye, ShadowFactor);
    
    DirectLight *= ShadowFactor[0];
    float4 LightColor = ambient + DirectLight;
    
    //Speclular Reflectiom
    float3 EyeToPixel = -ToEye;
    float3 ReflectedRay = reflect(EyeToPixel, NormalW);
    float3 ReflectionColor = TexSkyBox.Sample(gsamLinearWrap, ReflectedRay).rgb;
    float3 FresnelEffect = SchlickFresnel(FresnelR0, NormalW, ReflectedRay);
    LightColor.rgb += Shine * FresnelEffect * ReflectionColor;
    
    LightColor.a = mDiffuseAlbedo.a;
    return LightColor;
}

