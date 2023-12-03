#include "GPU-Utilities.hlsli"  
#include "ShaderResourceRegisters.h"

cbuffer RenderDataBuffer : register(RZ_RENDER_DATA_GPU_REG)
{
    float4x4 camViewProjMatrix;
    float4x4 objectWorldMatrix;
    uint vertStartIdx;
    uint bvhNodeIdx;
    float2 pad0;
    MaterialColour3 albedo;
}

float4 main() : SV_TARGET
{
    return float4(albedo.colour, 0.0f);
}