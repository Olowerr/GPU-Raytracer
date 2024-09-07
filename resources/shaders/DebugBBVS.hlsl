#include "GPU-Utilities.hlsli"  
#include "ShaderResourceRegisters.h"

struct Node
{
    AABB boundingBox;
    uint triStart;
    uint triEnd;
    uint firstChildIdx;
};

struct OctTreeNode
{
    AABB boundingBox;

    uint meshesStartIdx;
    uint meshesEndIdx;

    uint spheresStartIdx;
    uint spheresEndIdx;

    uint children[8u];
};

StructuredBuffer<float3> lineData : register(TRIANGLE_POS_GPU_REG);
StructuredBuffer<Node> bvhNodes : register(BVH_TREE_GPU_REG);
StructuredBuffer<OctTreeNode> octTreeNodes : register(OCT_TREE_GPU_REG);

cbuffer RenderDataBuffer : register(DBG_RENDER_DATA_GPU_REG)
{
    float4x4 camViewProjMatrix;
    float4x4 objectWorldMatrix;
    uint vertStartIdx;
    uint nodeIdx;
    float2 pad0;
    MaterialColour3 albedo;
    float3 cameraDir;
    float pad1;
    float3 cameraPos;
    uint mode;
}

float4 main(uint localVertIdx : SV_VertexID) : SV_Position
{
    AABB boundingBox;
    switch (mode)
    {
        case 0:
            boundingBox = bvhNodes[nodeIdx].boundingBox;
            break;
        case 1:
            boundingBox = octTreeNodes[nodeIdx].boundingBox;
            break;
    }
    float3 vertex = lineData[localVertIdx];
    
    vertex *= (boundingBox.max - boundingBox.min) * 0.5f; // Scale
    vertex += (boundingBox.max + boundingBox.min) * 0.5f; // Offset
    
    return mul(mul(float4(vertex, 1.f), objectWorldMatrix), camViewProjMatrix);
}