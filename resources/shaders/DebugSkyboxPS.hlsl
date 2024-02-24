#include "ShaderResourceRegisters.h"

SamplerState simp : register(s0);
TextureCube environmentMap : register(RM_ENVIRONMENT_MAP_GPU_REG);

struct Input
{
    float4 pos : SV_POSITION;
    float3 localPos : LOCAL_POS;
};

float4 main(Input input) : SV_TARGET
{
    return environmentMap.Sample(simp, input.localPos);;
}