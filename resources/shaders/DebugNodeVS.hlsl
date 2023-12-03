#include "GPU-Utilities.hlsli"  
#include "ShaderResourceRegisters.h"

struct Node
{
    AABB boundingBox;
    uint triStart;
    uint triEnd;
    uint childIdxs[2];
    uint parentIdx;
};

StructuredBuffer<float3> lineData : register(RM_TRIANGLE_DATA_GPU_REG);
StructuredBuffer<Node> bvhNodes : register(RM_BVH_TREE_GPU_REG);

cbuffer RenderDataBuffer : register(RZ_RENDER_DATA_GPU_REG)
{
    float4x4 camViewProjMatrix;
    float4x4 objectWorldMatrix;
    uint vertStartIdx;
    uint bvhNodeIdx;
    float2 pad0;
    MaterialColour3 albedo;
}

float4 main(uint localVertidx : SV_VertexID) : SV_Position
{
    Node node = bvhNodes[bvhNodeIdx];
    float3 vertex = lineData[localVertidx];
    
    vertex *= (node.boundingBox.max - node.boundingBox.min) * 0.5f; // Scale
    vertex += (node.boundingBox.max + node.boundingBox.min) * 0.5f; // Offset
    
    return mul(mul(float4(vertex, 1.f), objectWorldMatrix), camViewProjMatrix);
}