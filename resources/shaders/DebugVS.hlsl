#include "GPU-Utilities.hlsli"  
#include "ShaderResourceRegisters.h"

StructuredBuffer<Triangle> trianglePosData : register(RM_TRIANGLE_POS_GPU_REG);
StructuredBuffer<TriangleInfo> triangleIinfoData : register(RM_TRIANGLE_INFO_GPU_REG);

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
}

PS_Input main(uint localVertIdx : SV_VertexID)
{
    PS_Input outData;
	
    uint vertexIdx = vertStartIdx + localVertIdx;
    uint localVertexIdx = vertexIdx % 3;
    
    Triangle triPos = trianglePosData[vertexIdx / 3u];
    TriangleInfo triInfo = triangleIinfoData[vertexIdx / 3u];

    VertexInfo vertInfo = triInfo.vertexInfo[vertexIdx % 3u];
	
    outData.position = mul(float4(triPos.position[localVertexIdx], 1.f), objectWorldMatrix).xyz;
    outData.svPos = mul(float4(outData.position, 1.f), camViewProjMatrix);
    
    outData.normal = mul(float4(vertInfo.normal, 0.f), objectWorldMatrix).xyz;
    outData.uv = vertInfo.uv;
    
	return outData;
}