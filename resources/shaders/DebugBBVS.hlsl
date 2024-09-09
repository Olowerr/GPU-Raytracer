#include "GPU-Utilities.hlsli"  
#include "ShaderResourceRegisters.h"

StructuredBuffer<float3> lineData : register(TRIANGLE_POS_GPU_REG);
StructuredBuffer<BvhNode> bvhNodes : register(BVH_TREE_GPU_REG);
StructuredBuffer<OctTreeNode> octTreeNodes : register(OCT_TREE_GPU_REG);

cbuffer RenderDataBuffer : register(DBG_RENDER_DATA_GPU_REG)
{
    DBGRenderData renderData;
}

float4 main(uint localVertIdx : SV_VertexID) : SV_Position
{
    AABB boundingBox;
    boundingBox.min = boundingBox.max = float3(0.f, 0.f, 0.f);

    switch (renderData.mode)
    {
        case 0:
            boundingBox = bvhNodes[renderData.nodeIdx].boundingBox;
            break;
        case 1:
            boundingBox = octTreeNodes[renderData.nodeIdx].boundingBox;
            break;
    }
    float3 vertex = lineData[localVertIdx];
    
    vertex *= (boundingBox.max - boundingBox.min) * 0.5f; // Scale
    vertex += (boundingBox.max + boundingBox.min) * 0.5f; // Offset
    
    return mul(mul(float4(vertex, 1.f), renderData.objectWorldMatrix), renderData.camViewProjMatrix);
}