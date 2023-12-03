#include "GPU-Utilities.hlsli"  
#include "ShaderResourceRegisters.h"

StructuredBuffer<Triangle> triangleData : register(RM_TRIANGLE_DATA_GPU_REG);

cbuffer RenderDataBuffer : register(RZ_RENDER_DATA_GPU_REG)
{
    float4x4 camViewProjMatrix;
    float4x4 objectWorldMatrix;
    uint vertStartIdx;
    float3 pad0;
    MaterialColour3 albedo;
}

PS_Input main(uint localVertidx : SV_VertexID)
{
    PS_Input outData;
	
    uint vertexIdx = vertStartIdx + localVertidx;
    Triangle tri = triangleData[vertexIdx / 3u];
    Vertex vert = tri.verticies[vertexIdx % 3u];
	
    outData.position = mul(float4(vert.position, 1.f), objectWorldMatrix).xyz;
    outData.svPos = mul(float4(outData.position, 1.f), camViewProjMatrix);
    
    outData.normal = mul(float4(vert.normal, 0.f), objectWorldMatrix).xyz;
    outData.uv = vert.uv;
    
	return outData;
}