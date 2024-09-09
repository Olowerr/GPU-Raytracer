#include "GPU-Utilities.hlsli"  
#include "ShaderResourceRegisters.h"

cbuffer RenderDataBuffer : register(DBG_RENDER_DATA_GPU_REG)
{
    DBGRenderData renderData;
}

float4 main() : SV_TARGET
{
    return float4(renderData.albedo.colour, 1.0f);
}