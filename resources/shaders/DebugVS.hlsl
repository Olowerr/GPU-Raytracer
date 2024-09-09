#include "GPU-Utilities.hlsli"  
#include "ShaderResourceRegisters.h"

StructuredBuffer<Triangle> trianglePosData : register(TRIANGLE_POS_GPU_REG);
StructuredBuffer<TriangleInfo> triangleIinfoData : register(TRIANGLE_INFO_GPU_REG);

cbuffer RenderDataBuffer : register(DBG_RENDER_DATA_GPU_REG)
{
    DBGRenderData renderData;
}

struct PS_Input
{
    float4 svPos : SV_Position;
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXTURE_COORDS;
};

PS_Input main(uint localVertIdx : SV_VertexID)
{
    PS_Input outData;
	
    uint vertexIdx = renderData.vertStartIdx + localVertIdx;
    uint localVertexIdx = vertexIdx % 3;
    
    Triangle triPos = trianglePosData[vertexIdx / 3u];
    TriangleInfo triInfo = triangleIinfoData[vertexIdx / 3u];

    VertexInfo vertInfo = triInfo.vertexInfo[vertexIdx % 3u];
	
    outData.position = mul(float4(triPos.position[localVertexIdx], 1.f), renderData.objectWorldMatrix).xyz;
    outData.svPos = mul(float4(outData.position, 1.f), renderData.camViewProjMatrix);
    
    outData.normal = mul(float4(vertInfo.normal, 0.f), renderData.objectWorldMatrix).xyz;
    outData.uv = vertInfo.uv;
    
	return outData;
}