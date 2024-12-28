#include "GPU-Utilities.hlsli"
#include "ShaderResourceRegisters.h"


SamplerState simp : register(s0);

Texture2DArray<unorm float4> textures : register(TEXTURES_GPU_REG);

cbuffer RenderDataBuffer : register(DBG_RENDER_DATA_GPU_REG)
{
    DBGRenderData renderData;
}

float3 sampleTexture(uint textureIdx, float2 meshUVs)
{
    return textures.SampleLevel(simp, float3(meshUVs, (float) textureIdx), 0.f).rgb;
}

struct PS_Input
{
    float4 svPos : SV_Position;
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXTURE_COORDS;
};

float4 main(PS_Input inputData) : SV_TARGET
{
    float4 finalColour;
    if (renderData.albedo.textureIdx != UINT_MAX)
        finalColour.rgb = sampleTexture(renderData.albedo.textureIdx, inputData.uv);
    else
        finalColour.rgb = renderData.albedo.colour;
    
    inputData.normal = normalize(inputData.normal);
    
    finalColour *= max(dot(normalize(renderData.cameraPos - inputData.position), inputData.normal), 0.f);
    finalColour.a = 1.f;
    
    return finalColour;
}