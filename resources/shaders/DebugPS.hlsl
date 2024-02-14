#include "GPU-Utilities.hlsli"
#include "ShaderResourceRegisters.h"

struct AtlasTextureDesc
{
    float2 uvRatio;
    float2 uvOffset;
};

SamplerState simp : register(s0);

Texture2D<unorm float4> textureAtlas : register(RM_TEXTURE_ATLAS_GPU_REG);
StructuredBuffer<AtlasTextureDesc> textureDescs : register(RM_TEXTURE_ATLAS_DESC_GPU_REG);

cbuffer RenderDataBuffer : register(RZ_RENDER_DATA_GPU_REG)
{
    float4x4 camViewProjMatrix;
    float4x4 objectWorldMatrix;
    uint vertStartIdx;
    uint bvhNodeIdx;
    float2 pad0;
    MaterialColour3 albedo;
    float3 cameraDir;
    float pad1;
}

float3 sampleTexture(uint textureIdx, float2 meshUVs)
{
    AtlasTextureDesc texDesc = textureDescs[textureIdx];
    meshUVs *= texDesc.uvRatio;
    meshUVs += texDesc.uvOffset;
    
    return textureAtlas.SampleLevel(simp, meshUVs, 0.f).rgb;
}

float4 main(PS_Input inputData) : SV_TARGET
{
    float4 finalColour;
    if (albedo.textureIdx != UINT_MAX)
        finalColour.rgb = sampleTexture(albedo.textureIdx, inputData.uv);
    else
        finalColour.rgb = albedo.colour;
    
    inputData.normal = normalize(inputData.normal);
    
    finalColour.a = 0.f;
    finalColour *= max(dot(-cameraDir, inputData.normal), 0.2);
    
    return finalColour;
}