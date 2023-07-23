
#include "GPU-Utilities.hlsli"
#include "ShaderResourceRegisters.h"

// ---- Defines and constants
#define NUM_BOUNCES (4u)


// ---- Structs
struct Payload
{
    bool hit;
    Material material;
    float3 worldPosition;
    float3 worldNormal;
};

struct RenderData
{
    uint accumulationEnabled;
    uint numAccumulationFrames;
    
    uint numSpheres;
    uint numTriangles;
    uint numMeshes;
    
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
cbuffer RenderDataBuffer : register(GPU_REG_RENDER_DATA)
{
    RenderData renderData;
}

// Scene data
StructuredBuffer<Sphere> sphereData : register(GPU_REG_SPHERE_DATA);
StructuredBuffer<Mesh> meshData : register(GPU_REG_MESH_DATA);
StructuredBuffer<Triangle> triangleData : register(GPU_REG_TRIANGLE_DATA);
StructuredBuffer<Mesh> meshData : register(GPU_REG_TRIANGLE_DATA);


// ---- Functions
float3 getEnvironmentLight(float3 direction)
{
    const static float3 SKY_COLOUR = float3(102.f, 204.f, 255.f) / 255.f;
    const static float3 GROUND_COLOUR = float3(164.f, 177.f, 178.f) / 255.f;
    
    const float dotty = dot(direction, float3(0.f, 1.f, 0.f));
    float3 colour = lerp(float3(1.f, 1.f, 1.f), SKY_COLOUR, pow(max(dotty, 0.f), 0.6f));
    
    const float transitionDotty = clamp((dotty + 0.005f) / 0.01f, 0.f, 1.f);
    return lerp(GROUND_COLOUR, colour, transitionDotty);
}

Payload findClosestHit(Ray ray)
{
    Payload payload;
    
    float cloestHitDistance = FLT_MAX;
    uint hitIdx = UINT_MAX;
    uint hitType = 0;
    
    for (uint i = 0; i < renderData.numSpheres; i++)
    {
        float distanceToHit = Collision::RayAndSphere(ray, sphereData[i]);
        
        if (distanceToHit > 0.f && distanceToHit < cloestHitDistance)
        {
            cloestHitDistance = distanceToHit;
            hitIdx = i;
            hitType = 0;
        }
    }
    
    for (uint j = 0; j < renderData.numTriangles; j++)
    {
        float distanceToHit = Collision::RayAndTriangle(ray, triangleData[j]);
        
        if (distanceToHit > 0.f && distanceToHit < cloestHitDistance)
        {
            cloestHitDistance = distanceToHit;
            hitIdx = j;
            hitType = 1;
        }
    }
    //for (uint j = 0; j < renderData.numMeshes; j++)
    //{
    //    float distanceToHit = Collision::RayAndTriangle(ray, triangleData[j]);
    //    
    //    if (distanceToHit > 0.f && distanceToHit < cloestHitDistance)
    //    {
    //        cloestHitDistance = distanceToHit;
    //        hitIdx = j;
    //        hitType = 1;
    //    }
    //}
    
    payload.hit = hitIdx != UINT_MAX;
    payload.worldPosition = ray.origin + ray.direction * cloestHitDistance;
    
    // TODO: Make better system
    switch (hitType)
    {
        case 0: // Sphere
            payload.material = sphereData[hitIdx].material;
            payload.worldNormal = normalize(payload.worldPosition - sphereData[hitIdx].position);
            break;
        
        case 1: // Triangle
            payload.material.albedoColour = float3(0.9f, 0.1f, 0.1f);
            payload.material.specularColour = float3(1.f, 1.f, 1.f);
            payload.material.smoothness = 0.f;
            payload.material.specularProbability = 1.f;
            payload.material.emissionColour = float3(0.f, 0.f, 0.f);
            payload.material.emissionPower = 0.f;
        
            Triangle tri = triangleData[hitIdx];
            float3 e1 = tri.p1 - tri.p0, e2 = tri.p2 - tri.p0;
            payload.worldNormal = normalize(cross(e1, e2));
            break;
    }
       
    return payload;
}


// ---- Main part of shader
[numthreads(16, 9, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    /*
        Flip Y-coordinate to convert to worldspace
        [0,0] in D3D11 textures is top left corner, but a higher Y-value should be higher in worldspace
        So it is flipped
    */
     
    uint seed = DTid.x + (DTid.y + 1) * renderData.textureDims.x * (renderData.numAccumulationFrames + 1);
    
    float3 pos = float3((float)DTid.x, float(renderData.textureDims.y - DTid.y), renderData.cameraNearZ);
    
    // Simple AA
    pos.x += randomFloat(seed) - 0.5f;
    pos.y += randomFloat(seed) - 0.5f;
    
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
    ray.direction = mul(float4(normalize(target.xyz / target.z), 0.f), renderData.cameraInverseViewMatrix).xyz;
#endif

    
    float3 light = float3(0.f, 0.f, 0.f);
    float3 contribution = float3(1.f, 1.f, 1.f);
    
    Payload hitData;
    Sphere currentSphere;
    Material material;
    
    for (uint i = 0; i <= NUM_BOUNCES; i++)
    {
        hitData = findClosestHit(ray);
        if (!hitData.hit)
        {
            light += getEnvironmentLight(ray.direction) * contribution;
            break;
        }
        
        material = hitData.material;
        
        const float3 diffuseReflection = randomInHemisphere(seed, hitData.worldNormal);
        const float3 specularReflection = reflect(ray.direction, hitData.worldNormal);
        const float specularFactor = float(material.specularProbability >= randomFloat(seed));
        
        ray.origin = hitData.worldPosition + hitData.worldNormal * 0.001f;
        ray.direction = lerp(diffuseReflection, specularReflection, material.smoothness * specularFactor);
        
        light += material.emissionColour * material.emissionPower * contribution;
        contribution *= lerp(material.albedoColour, material.specularColour, specularFactor);
        contribution *= material.albedoColour;
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