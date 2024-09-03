#include "GPU-Utilities.hlsli"
#include "ShaderResourceRegisters.h"

struct AtlasTextureDesc
{
    float2 uvRatio;
    float2 uvOffset;
};

SamplerState simp : register(s0);

Texture2DArray<unorm float4> textures : register(TEXTURES_GPU_REG);

cbuffer RenderDataBuffer : register(DBG_RENDER_DATA_GPU_REG)
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
    return textures.SampleLevel(simp, float3(meshUVs, (float) textureIdx), 0.f).rgb;
}

float4 main(PS_Input inputData) : SV_TARGET
{
    float4 finalColour;
    if (albedo.textureIdx != UINT_MAX)
        finalColour.rgb = sampleTexture(albedo.textureIdx, inputData.uv);
    else
        finalColour.rgb = albedo.colour;
    
    inputData.normal = normalize(inputData.normal);
    
    finalColour *= max(dot(-cameraDir, inputData.normal), 0.2);
    finalColour.a = 1.f;
    
    return finalColour;
}