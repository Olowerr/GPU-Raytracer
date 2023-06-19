
/*
    All code here is temporary and is only for testing
*/

struct Sphere
{
    float3 position;
    float3 colour;
    float radius;
};

RWTexture2D<float4> resultBuffer : register(u0);
StructuredBuffer<Sphere> sphereData : register(t0);

[numthreads(16, 9, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    Sphere tempSphere;
    
    uint numSpheres = 0, stride = 0;
    sphereData.GetDimensions(numSpheres, stride);
    
    for (uint i = 0; i < numSpheres; i++)
    {
        tempSphere = sphereData[i];
        
        float3 rayOrigin = float3(DTid.xy, 0.f);
        float3 rayDirection = float3(0.f, 0.f, 1.f);
    
        float3 rayToSphere = tempSphere.position - rayOrigin;
        float distToClosestPoint = dot(rayToSphere, rayDirection);
        float rayToSphereMagSqrd = dot(rayToSphere, rayToSphere);
        float sphereRadiusSqrd = tempSphere.radius * tempSphere.radius;
 
        if (distToClosestPoint < 0.f && rayToSphereMagSqrd > sphereRadiusSqrd)
            continue;
    
        float sideA = rayToSphereMagSqrd - distToClosestPoint * distToClosestPoint;
    
        if (sideA > sphereRadiusSqrd)
            continue;
    
        resultBuffer[DTid.xy] = float4(tempSphere.colour, 1.f);
    }   
}