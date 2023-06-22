
/*
    Code may be temporary and only used for testing
*/

#define NUM_BOUNCES (2)
#define INVALID_UINT (~0u)
#define FLT_MAX (3.402823466e+38F)

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

RWTexture2D<float4> resultBuffer : register(u0);
StructuredBuffer<Sphere> sphereData : register(t0);



Payload findClosestHit(Ray ray)
{
    Payload payload;
    payload.hitIdx = INVALID_UINT;
    
    Sphere currentSphere;
    float cloestHitDistance = FLT_MAX;
    
    uint numSpheres = 0, stride = 0;
    sphereData.GetDimensions(numSpheres, stride);
    
    for (uint i = 0; i < numSpheres; i++)
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
    static float3 camPos = float3(800.f, 450.f, -1000.f);
    float3 pixelPos = float3(DTid.xy, 0.f);
    
    Ray ray;
    ray.origin = camPos;
    ray.direction = normalize(pixelPos - camPos);
    
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
        ray.direction = reflect(ray.direction, hitData.worldNormal);
    }
       
    resultBuffer[DTid.xy] = float4(light, 1.f);
}