#include "GPU-Utilities.hlsli"  
#include "ShaderResourceRegisters.h"

struct Node
{
    AABB boundingBox;
    uint triStart;
    uint triEnd;
    uint firstChildIdx;
};

StructuredBuffer<float3> lineData : register(TRIANGLE_POS_GPU_REG);
StructuredBuffer<Node> bvhNodes : register(BVH_TREE_GPU_REG);

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

float4 main(uint localVertIdx : SV_VertexID) : SV_Position
{
    Node node = bvhNodes[bvhNodeIdx];
    float3 vertex = lineData[localVertIdx];
    
    vertex *= (node.boundingBox.max - node.boundingBox.min) * 0.5f; // Scale
    vertex += (node.boundingBox.max + node.boundingBox.min) * 0.5f; // Offset
    
    return mul(mul(float4(vertex, 1.f), objectWorldMatrix), camViewProjMatrix);
}