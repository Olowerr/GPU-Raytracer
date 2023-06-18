
/*
    All code here is temporary and is only for testing
*/

RWTexture2D<float4> resultBuffer : register(u0);

static float3 sphereCenter = float3(800.f, 450.f, 1000.f);
static float sphereRadius = 300.f;

[numthreads(16, 9, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    float3 rayOrigin = float3(DTid.xy, 0.f);
    float3 rayDirection = float3(0.f, 0.f, 1.f);
    
    float3 rayToSphere = sphereCenter - rayOrigin;
    float distToClosestPoint = dot(rayToSphere, rayDirection);
    float rayToSphereMagSqrd = dot(rayToSphere, rayToSphere);
    float sphereRadiusSqrd = sphereRadius * sphereRadius;
 
    if (distToClosestPoint < 0.f && rayToSphereMagSqrd > sphereRadiusSqrd)
        return;
    
    float sideA = rayToSphereMagSqrd - distToClosestPoint * distToClosestPoint;
    
    if (sideA > sphereRadiusSqrd)
        return;
    
    resultBuffer[DTid.xy] = float4(1.f, 0.7f, 0.5f, 1.f);

}