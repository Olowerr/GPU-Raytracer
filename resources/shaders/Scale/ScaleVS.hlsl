
struct SCALE_VS_OUT
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
};

static SCALE_VS_OUT QUAD_VERTICIES[6] =
{
    { float4(-1.f, 1.f, 0.f, 1.f),	float2(0.f, 0.f) },
	{ float4(1.f, 1.f, 0.f, 1.f),	float2(1.f, 0.f) },
	{ float4(-1.f, -1.f, 0.f, 1.f), float2(0.f, 1.f) },
	{ float4(-1.f, -1.f, 0.f, 1.f), float2(0.f, 1.f) },
	{ float4(1.f, 1.f, 0.f, 1.f),	float2(1.f, 0.f) },
	{ float4(1.f, -1.f, 0.f, 1.f),	float2(1.f, 1.f) }
};


SCALE_VS_OUT main(uint vertexId : SV_VertexID)
{
    return QUAD_VERTICIES[vertexId];
}