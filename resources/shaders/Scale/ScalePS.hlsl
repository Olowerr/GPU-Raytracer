
Texture2D<unorm float4> sourceTexture : register(t0);
SamplerState simp : register(s0);

float4 main(float4 position : SV_Position, float2 uv : TEXCOORD) : SV_TARGET
{
    return sourceTexture.Sample(simp, uv);
}