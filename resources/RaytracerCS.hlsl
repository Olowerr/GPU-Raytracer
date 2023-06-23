
/*
    Code may be temporary and only used for testing
*/


// ---- Defines and constants

#define NUM_BOUNCES (5)
#define INVALID_UINT (~0u)
#define FLT_MAX (3.402823466e+38F)
#define NUM_RANDOM_VECTORS (100u)

// ---- Strcuts

struct Sphere
{
    float3 position;
    float3 colour;
    float3 emissionColour;
    float emissonPower;
    float radius;
};

struct Ray
{
    float3 origin;
    float3 direction;
};

struct Payload
{
    uint hitIdx;
    float3 worldPosition;
    float3 worldNormal;
};

struct RenderData
{
    uint accumulationEnabled;
    uint numAccumulationFrames;
    uint numSpheres;
    float renderDataPadding0;
    
    uint2 textureDims;
    float2 viewPlaneDims;
    
    float4x4 cameraInverseProjectionMatrix;
    float4x4 cameraInverseViewMatrix;
    float3 cameraPosition;
    float cameraNearZ;
};

struct RandVector
{
    float3 randVector;
};


// ---- Resources
RWTexture2D<unorm float4> resultBuffer : register(u0);
RWTexture2D<float4> accumulationBuffer : register(u1);
StructuredBuffer<Sphere> sphereData : register(t0);
StructuredBuffer<RandVector> randomVectors : register(t1);
cbuffer RenderDataBuffer : register(b0)
{
    RenderData renderData;
}

// ---- Functions

// Ty ChatGPT
uint randomize(uint seed)
{
    seed = seed ^ (seed >> 11);
    seed = seed ^ ((seed << 7) & 0x9D2C5680);
    seed = seed ^ ((seed << 15) & 0xEFC60000);
    return seed ^ (seed >> 18);
}

Payload findClosestHit(Ray ray)
{
    Payload payload;
    payload.hitIdx = INVALID_UINT;
    
    Sphere currentSphere;
    float cloestHitDistance = FLT_MAX;
    
    uint numSpheres = 0, stride = 0;
    sphereData.GetDimensions(numSpheres, stride);
    
    for (uint i = 0; i < renderData.numSpheres; i++)
    {
        if (payload.hitIdx == i)
            continue;
        
        currentSphere = sphereData[i];
    
        float3 rayToSphere = currentSphere.position - ray.origin;
        float distToClosestPoint = dot(rayToSphere, ray.direction);
        float rayToSphereMagSqrd = dot(rayToSphere, rayToSphere);
        float sphereRadiusSqrd = currentSphere.radius * currentSphere.radius;
 
        if (distToClosestPoint < 0.f && rayToSphereMagSqrd > sphereRadiusSqrd)
            continue;
    
        float sideA = rayToSphereMagSqrd - distToClosestPoint * distToClosestPoint;
        if (sideA > sphereRadiusSqrd)
            continue;
    
        float sideB = sphereRadiusSqrd - sideA;
        float distanceTohit = distToClosestPoint - sqrt(sideB);
        
        if (distanceTohit < cloestHitDistance)
        {
            cloestHitDistance = distanceTohit;
            payload.hitIdx = i;
        }
    }
    
    payload.worldPosition = ray.origin + ray.direction * cloestHitDistance;
    payload.worldNormal = normalize(payload.worldPosition - sphereData[payload.hitIdx].position);
    
    return payload;
}

[numthreads(16, 9, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    float3 pos = float3(DTid.xy, renderData.cameraNearZ);
    pos.xy /= (float2) renderData.textureDims;
    pos.xy *= 2.f;
    pos.xy -= 1.f;
    
#if 0 // My way
    pos.xy *= renderData.viewPlaneDims;
    pos = mul(float4(pos, 1.f), renderData.cameraInverseViewMatrix);
    
    Ray ray;
    ray.origin = renderData.cameraPosition;
    ray.direction = normalize(pos - ray.origin);
   
#else // Cherno way
    float4 target = mul(float4(pos, 1.f), renderData.cameraInverseProjectionMatrix);
    
    Ray ray;
    ray.origin = renderData.cameraPosition;
    ray.direction = mul(float4(normalize(target.xyz / target.z), 0.f), renderData.cameraInverseViewMatrix);
#endif

    
    float3 light = float3(0.f, 0.f, 0.f);
    float3 contribution = float3(1.f, 1.f, 1.f);
    
    Payload hitData;
    Sphere currentSphere;
    
    for (uint i = 0; i < NUM_BOUNCES; i++)
    {
        hitData = findClosestHit(ray);
        if (hitData.hitIdx == INVALID_UINT)
        {
            //light += float3(0.2f, 0.4f, 0.6f) * contribution;
            break;
        }
        
        currentSphere = sphereData[hitData.hitIdx];
        contribution *= currentSphere.colour;
        light += currentSphere.emissionColour * currentSphere.emissonPower * contribution;

        ray.origin = hitData.worldPosition + hitData.worldNormal * 0.1f;
        //ray.direction = normalize(reflect(ray.direction, hitData.worldNormal) + randomVectors.Load(int3(DTid.xy, 0)).xyz);
        
        
        int randomVecIdx = randomize(DTid.x + DTid.y * renderData.textureDims.x) % NUM_RANDOM_VECTORS;
        ray.direction = normalize(hitData.worldNormal + randomVectors.Load(randomVecIdx).randVector);
    }
    
    if (renderData.accumulationEnabled == 1)
    {
        accumulationBuffer[DTid.xy] += float4(light, 0.f);
        resultBuffer[DTid.xy] = accumulationBuffer[DTid.xy] / (float)renderData.numAccumulationFrames;
    }
    else
    {
        resultBuffer[DTid.xy] = float4(light, 1.f);
    }
}