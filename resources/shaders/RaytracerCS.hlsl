
#include "GPU-Utilities.hlsli"
#include "ShaderResourceRegisters.h"

// ---- Defines and constants
#define NUM_BOUNCES (5u)


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


// ---- Resources
RWTexture2D<unorm float4> resultBuffer : register(GPU_REG_RESULT_BUFFER);
RWTexture2D<float4> accumulationBuffer : register(GPU_REG_ACCUMULATION_BUFFER);
StructuredBuffer<Sphere> sphereData : register(GPU_REG_SPHERE_DATA);
cbuffer RenderDataBuffer : register(GPU_REG_RENDER_DATA)
{
    RenderData renderData;
}


// ---- Functions
Payload findClosestHit(Ray ray)
{
    Payload payload;
    payload.hitIdx = UINT_MAX;
    
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


// ---- Main part of shader
[numthreads(16, 9, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    /*
        Flip Y-coordinate to convert to worldspace
        [0,0] in D3D11 textures is top left corner, but a higher Y-value should be higher in worldspace
        So it is flipped
    */
     
    float3 pos = float3((float)DTid.x, float(renderData.textureDims.y - DTid.y), renderData.cameraNearZ);
    
#if 1 // Very simplified AA, just offset the ray by a random float2 between [-0.5, 0.5] every frame
    const uint seed = DTid.x + DTid.y * renderData.textureDims.x * (renderData.numAccumulationFrames + 1);
    const float xOffset = randomFloat(seed);
    const float yOffset = randomFloat(xOffset * UINT_MAX);

    pos.x += xOffset - 0.5f;
    pos.y += yOffset - 0.5f;
#endif
    
    pos.xy /= (float2)renderData.textureDims;
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
        if (hitData.hitIdx == UINT_MAX)
        {
            //light += float3(0.2f, 0.4f, 0.6f) * contribution;
            break;
        }
        
        currentSphere = sphereData[hitData.hitIdx];
        contribution *= currentSphere.colour;
        light += currentSphere.emissionColour * currentSphere.emissonPower * contribution;

        ray.origin = hitData.worldPosition + hitData.worldNormal * 0.001f;
        
        const uint seed = DTid.x + DTid.y * renderData.textureDims.x * (i + 1) * (renderData.numAccumulationFrames + 1);
        ray.direction = randomInHemisphere(seed, hitData.worldNormal);
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