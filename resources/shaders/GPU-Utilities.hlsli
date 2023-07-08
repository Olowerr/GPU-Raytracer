
#define UINT_MAX (~0u)
#define FLT_MAX (3.402823466e+38F)


uint pcg_hash(uint seed)
{
    uint state = seed * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float3 getRandomVector(uint seed)
{
    seed = pcg_hash(seed);
    float x = ((float)seed / (float)UINT_MAX) * 2.f - 1.f;

    seed = pcg_hash(seed);
    float y = ((float)seed / (float)UINT_MAX) * 2.f - 1.f;
   
    seed = pcg_hash(seed);
    float z = ((float)seed / (float)UINT_MAX) * 2.f - 1.f;
    
    return normalize(float3(x, y, z));
}

float3 randomInHemisphere(uint seed, float3 normal)
{
    const float3 randVector = getRandomVector(seed);
    return randVector * (dot(randVector, normal) > 0.f ? 1.f : -1.f);
}