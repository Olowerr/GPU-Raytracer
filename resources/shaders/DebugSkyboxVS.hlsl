
#include "GPU-Utilities.hlsli"
#include "ShaderResourceRegisters.h"

StructuredBuffer<Triangle> triangleData : register(TRIANGLE_POS_GPU_REG);

cbuffer RenderDataBuffer : register(DBG_RENDER_DATA_GPU_REG)
{
    DBGRenderData renderData;
}

struct Output
{
    float4 pos : SV_POSITION;
    float3 localPos : LOCAL_POS;
};

Output main(uint vertIdx : SV_VertexID)
{
    Triangle tri = triangleData[vertIdx / 3u];
    float3 pos = tri.position[vertIdx % 3u];
    
    Output output;
    output.localPos = pos;
    output.pos = mul(float4(renderData.cameraPos + pos, 1.f), renderData.camViewProjMatrix).xyww;

    return output;
}