
#include "GPU-Utilities.hlsli"
#include "ShaderResourceRegisters.h"

StructuredBuffer<Triangle> triangleData : register(RM_TRIANGLE_DATA_GPU_REG);

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
    float3 camPos;
    float pad2;
}

struct Output
{
    float4 pos : SV_POSITION;
    float3 localPos : LOCAL_POS;
};

Output main(uint vertIdx : SV_VertexID)
{
    Triangle tri = triangleData[vertIdx / 3u];
    float3 pos = tri.verticies[vertIdx % 3u].position;
    
    Output output;
    output.localPos = pos;
    output.pos = mul(float4(camPos + pos, 1.f), camViewProjMatrix).xyww;

    return output;
}