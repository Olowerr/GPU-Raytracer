
#include "GPU-Utilities.hlsli"
#include "ShaderResourceRegisters.h"

// ---- Defines and constants
#define NUM_BOUNCES (5)


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
    uint numMeshes;
    
    uint2 textureDims;
    float2 viewPlaneDims;
    
    float4x4 cameraInverseProjectionMatrix;
    float4x4 cameraInverseViewMatrix;
    float3 cameraPosition;
    float cameraNearZ;
};

struct AtlasTextureDesc
{
    float2 uvRatio;
    float2 uvOffset;
};

struct Node
{
    AABB boundingBox;
    uint triStart;
    uint triEnd;
    uint childIdxs[2];
    uint parentIdx;
};


// ---- Resources
RWTexture2D<unorm float4> resultBuffer : register(RESULT_BUFFER_GPU_REG);
RWTexture2D<float4> accumulationBuffer : register(ACCUMULATION_BUFFER_GPU_REG);
cbuffer RenderDataBuffer : register(RENDER_DATA_GPU_REG)
{
    RenderData renderData;
}
SamplerState simp : register(s0);

// Scene data
StructuredBuffer<Sphere> sphereData : register(SPHERE_DATA_GPU_REG);
StructuredBuffer<Mesh> meshData : register(MESH_DATA_GPU_REG);
StructuredBuffer<Triangle> triangleData : register(TRIANGLE_DATA_GPU_REG);
StructuredBuffer<Node> bvhNodes : register(BVH_TREE_GPU_REG);

StructuredBuffer<AtlasTextureDesc> textureDescs : register(TEXTURE_ATLAS_DESC_GPU_REG);
Texture2D<unorm float4> textureAtlas : register(TEXTURE_ATLAS_GPU_REG);

/*
    TODO: Check if seperating triangles into 3 buffers: positions, normals and uv, can be faster than current.
    That way we don't need to access the normals and uv while doing intersection tests,
    Meaning the cache can get more positions at once. (is this how it works?)
    OR use one big buffer, but structure it like: all positions -> all normals -> all uvs
*/


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

float3 getNightLight(float3 direction)
{
    //uint asd = pcg_hash2(abs(direction.x) * UINT_MAX) + pcg_hash2(abs(direction.y) * UINT_MAX) + pcg_hash2(abs(direction.z) * UINT_MAX);
    //return pcg_hash2(asd) % 100 < 5 ? float3(1.f, 1.f, 1.f) : float3(0.f, 0.f, 0.f);
    
    float aqwe = pseudorandomNumber(direction);
    return float3(aqwe, aqwe, aqwe);

}

float3 barycentricInterpolation(float3 uvw, float3 value0, float3 value1, float3 value2)
{
    return value0 * uvw.x + value1 * uvw.y + value2 * uvw.z;
}

float2 barycentricInterpolation(float3 uvw, float2 value0, float2 value1, float2 value2)
{
    return value0 * uvw.x + value1 * uvw.y + value2 * uvw.z;
}

bool isValidIdx(uint idx)
{
    return idx != UINT_MAX;
}

float3 sampleTexture(uint textureIdx, float2 meshUVs)
{
    AtlasTextureDesc texDesc = textureDescs[textureIdx];
    meshUVs *= texDesc.uvRatio;
    meshUVs += texDesc.uvOffset;
    
    return textureAtlas.SampleLevel(simp, meshUVs, 0.f).rgb;
}

void findMaterialTextureColours(inout Material material, float2 meshUVs)
{
    if (isValidIdx(material.albedo.textureIdx))
        material.albedo.colour = sampleTexture(material.albedo.textureIdx, meshUVs);
    
    if (isValidIdx(material.roughness.textureIdx))
        material.roughness.colour = sampleTexture(material.roughness.textureIdx, meshUVs).r;
    
    if (isValidIdx(material.metallic.textureIdx))
        material.metallic.colour = sampleTexture(material.metallic.textureIdx, meshUVs).r;
}

Payload findClosestHit(Ray ray)
{
    Payload payload;
    
    float2 meshUVs = float2(0.f, 0.f);
    float closestHitDistance = FLT_MAX;
    uint hitIdx = UINT_MAX;
    uint hitType = 0;
    
    uint i = 0;
    for (i = 0; i < renderData.numSpheres; i++)
    {
        float distanceToHit = Collision::RayAndSphere(ray, sphereData[i]);
        
        if (distanceToHit > 0.f && distanceToHit < closestHitDistance)
        {
            closestHitDistance = distanceToHit;
            hitIdx = i;
            hitType = 0;
        }
    }
    
#if 0
    for (uint j = 0; j < renderData.numMeshes; j++)
    {
        if (Collision::RayAndAABB(ray, meshData[j].boundingBox))
        {
            uint triStart = meshData[j].triangleStartIdx;
            uint triEnd = meshData[j].triangleEndIdx;
            
            for (uint k = triStart; k < triEnd; k++)
            {
                //float3 translation = meshData[j].transformMatrix[3].xyz;
                Triangle tri = triangleData[k];
                
                float3 pos0 = tri.p0.position/* + translation*/;
                float3 pos1 = tri.p1.position/* + translation*/;
                float3 pos2 = tri.p2.position/* + translation*/;
                
                float3 baryUVCoord;
                float distanceToHit = Collision::RayAndTriangle(ray, pos0, pos1, pos2, baryUVCoord.xy);
                
                if (distanceToHit > 0.f && distanceToHit < closestHitDistance)
                {
                    closestHitDistance = distanceToHit;
                    hitIdx = j;
                    hitType = 1;
                    
                    baryUVCoord.z = 1.f - (baryUVCoord.x + baryUVCoord.y);
                    float3 normal = barycentricInterpolation(baryUVCoord, tri.p0.normal, tri.p1.normal, tri.p2.normal);
                    float2 lerpedUV = barycentricInterpolation(baryUVCoord, tri.p0.uv, tri.p1.uv, tri.p2.uv);
                    
                    payload.worldNormal = normalize(normal);
                    meshUVs = lerpedUV;
                }
            }
        }
    }
#else
    // TODO: Rework the 'for each mesh' loop to go through the BVH tree for each mesh.
    // Recursion is fine to start with but I believe a loop would be better to avoid stack overflow
    // NOTE: Recursive functions are not allowed in cs_5_0, using a stack approach instead
    
    // TODO: Rewrite the upcoming loop to reduce nesting
    
    static const uint MAX_STACK_SIZE = 300;
    half stack[MAX_STACK_SIZE];
    uint stackSize = 0;
    
    uint triHitIdx = UINT_MAX;
    for (uint j = 0; j < renderData.numMeshes && false; j++)
    {
        uint currentNodeIdx = meshData[j].bvhNodeStartIdx;
        stack[0] = currentNodeIdx;
        stackSize = 1;
        
        while (stackSize > 0)
        {
            currentNodeIdx = stack[--stackSize];
            
            Node node = bvhNodes[currentNodeIdx];
            if (!Collision::RayAndAABB(ray, node.boundingBox))
                continue; // Missed AABB
            
            if (node.childIdxs[0] == UINT_MAX) // Is leaf?
            {
                for (i = node.triStart; i < node.triEnd; i++)
                {
                    //float3 translation = meshData[j].transformMatrix[3].xyz;
                    Triangle tri = triangleData[i];
            
                    float3 pos0 = tri.p0.position/* + translation*/;
                    float3 pos1 = tri.p1.position/* + translation*/;
                    float3 pos2 = tri.p2.position/* + translation*/;
            
                    float3 baryUVCoord;
                    float distanceToHit = Collision::RayAndTriangle(ray, pos0, pos1, pos2, baryUVCoord.xy);
            
                    if (distanceToHit > 0.f && distanceToHit < closestHitDistance)
                    {
                        closestHitDistance = distanceToHit;
                        hitIdx = j;
                        hitType = 1;
            
                        baryUVCoord.z = 1.f - (baryUVCoord.x + baryUVCoord.y);
                        float3 normal = barycentricInterpolation(baryUVCoord, tri.p0.normal, tri.p1.normal, tri.p2.normal);
                        float2 lerpedUV = barycentricInterpolation(baryUVCoord, tri.p0.uv, tri.p1.uv, tri.p2.uv);
                            
                        payload.worldNormal = normalize(normal);
                        meshUVs = lerpedUV;
                    }
                }
            }
            else
            {
                stack[stackSize++] = node.childIdxs[0];
                stack[stackSize++] = node.childIdxs[1];
            }
        }
    }
#endif
    
    payload.hit = hitIdx != UINT_MAX;
    payload.worldPosition = ray.origin + ray.direction * closestHitDistance;
    
    // TODO: Make better system
    switch (hitType)
    {
        case 0: // Sphere
            payload.material = sphereData[hitIdx].material;
            payload.worldNormal = normalize(payload.worldPosition - sphereData[hitIdx].position);
            break;
        
        case 1: // Mesh
            payload.material = meshData[hitIdx].material;
            findMaterialTextureColours(payload.material, meshUVs);
            break;
    }
       
    return payload;
}

float chiGGX(float v)
{
    return v > 0.f ? 1.f : 0.f;
}

float GGX_PartialGeometryTerm(float3 viewVec, float3 normal, float3 halfwayVec, float roughness)
{
    float VoH2 = saturate(dot(viewVec, halfwayVec));
    float chi = chiGGX(VoH2 / max(dot(viewVec, normal), 0.f));
    VoH2 = VoH2 * VoH2;
    float tan2 = (1.f - VoH2) / VoH2;
    return (chi * 2.f) / (1.f + sqrt(1.f + roughness * roughness * tan2));
}

float3 FresnelSchlick(float3 F0, float cosTheta)
{
    return F0 + (float3(1.f, 1.f, 1.f) - F0) * pow(1.f - cosTheta, 5.f);
}

float GGX_Distribution(float3 normal, float3 halfwayVec, float roughness)
{
    float NoH = dot(normal, halfwayVec);
    float roughness2 = roughness * roughness;
    float NoH2 = NoH * NoH;
    
    float thing = pow((1.f + NoH2) * (roughness2 - 1.f), 2.f);
    return (chiGGX(NoH) * roughness2) / (PI * thing);
    
    float den = NoH2 * roughness2 + (1.f - NoH2);
    return (chiGGX(NoH) * roughness2) / (PI * den * den);
}

struct EvaluationPoint
{
    Material material;
    float3 viewDir;
    float3 lightDir;
    float3 normal;
    bool isLast;
};

//float3 trace(Ray ray, float3 contribution)
//{
//    Payload hitData = findClosestHit(ray);
//    if (!hitData.hit)
//        return getEnvironmentLight(ray.direction) * contribution;
//        
//    float3 hitPoint = hitData.worldPosition + hitData.worldNormal * 0.001f;
//
//    float3 diffuseReflection = normalize(hitData.worldNormal + getRandomVector(0));
//    float3 specularReflection = reflect(ray.direction, hitData.worldNormal);
//    float3 reflectedDir = normalize(lerp(specularReflection, diffuseReflection, hitData.material.roughness.colour));
//
//    Ray reflectedRay;
//    reflectedRay.origin = hitPoint;
//    reflectedRay.direction = reflectedDir;
//    
//    float3 shading = getShading(ray, reflectedRay, hitData, contribution);
//    
//    return trace(reflectedRay, contribution * hitData.material.albedo.colour) + shading;
//}

// ---- Main part of shader
[numthreads(16, 9, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    /*
        Flip Y-coordinate to convert to worldspace
        [0,0] in D3D11 textures is top left corner, but a higher Y-value should be higher in worldspace
        So it is flipped
    */
     
    uint seed = DTid.x + (DTid.y + 74813) * renderData.textureDims.x * (renderData.numAccumulationFrames + 1);
    
    float3 pos = float3((float) DTid.x, float(renderData.textureDims.y - DTid.y), renderData.cameraNearZ);
    
    // Simple AA
    pos.x += randomFloat(seed) - 0.5f;
    pos.y += randomFloat(seed) - 0.5f;
    
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
    ray.direction = mul(float4(normalize(target.xyz / target.z), 0.f), renderData.cameraInverseViewMatrix).xyz;
#endif

    Payload hitData;
    
    EvaluationPoint hits[NUM_BOUNCES + 1];
    int pathLength = 0;
   
    for (int i = 0; i <= NUM_BOUNCES; i++)
    {
        hitData = findClosestHit(ray);
        if (!hitData.hit)
        {
            break;
        }

        pathLength++;
        
        float3 hitPoint = hitData.worldPosition + hitData.worldNormal * 0.001f;

        float3 diffuseReflection = normalize(hitData.worldNormal + getRandomVector(seed));
        float3 specularReflection = reflect(ray.direction, hitData.worldNormal);
        float3 reflectedDir = normalize(lerp(specularReflection, diffuseReflection, hitData.material.roughness.colour));
        
        hits[i].material = hitData.material;
        hits[i].lightDir = reflectedDir;
        hits[i].viewDir = -ray.direction;
        hits[i].normal = hitData.worldNormal;
        hits[i].isLast = i == NUM_BOUNCES;
        
        ray.origin = hitPoint;
        ray.direction = reflectedDir;
    }
    
    /*
    bounces | maxPathLength
    0 | 1
    1 | 2
    2 | 3
    3 | 4
    */
    
    float3 light = float3(0.f, 0.f, 0.f);
    float3 contribution = float3(1.f, 1.f, 1.f);
    if (pathLength != NUM_BOUNCES + 1)
        light += getEnvironmentLight(ray.direction);
        
    Material material;
    
    for (i = pathLength - 1; i >= 0 && pathLength != 0; i--)
    {
        material = hits[i].material;

        float3 viewDir = hits[i].viewDir;
        float3 lightDir = hits[i].lightDir;
        float3 normal = hits[i].normal;
        
        float3 halfwayVec = normalize(lightDir + viewDir);
        
        float D = GGX_Distribution(normal, halfwayVec, material.roughness.colour);
        
        float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), material.specularColour, material.metallic.colour);
        float3 F = FresnelSchlick(F0, dot(halfwayVec, viewDir));
        
        float G = GGX_PartialGeometryTerm(viewDir, normal, halfwayVec, material.roughness.colour);
        
        float3 ks = (D * F * G) / (4.0 * max(dot(normal, viewDir), 0.2f) * max(dot(normal, lightDir), 0.2f));
        ks = clamp(ks, 0.f, 1.f);
        
        float3 kd = (1.f - ks) * (1.f - material.metallic.colour);
        
        float3 specularLight = light;
        float3 albedo = material.albedo.colour;
        
        float3 finalLight = kd * albedo + ks * specularLight;
         
        //light += (finalLight + material.emissionColour * material.emissionPower) * albedo;
        //light = light * material.albedo.colour + material.emissionColour * material.emissionPower;
        light = light * (albedo + finalLight) + material.emissionColour * material.emissionPower;
    }
    
    light = saturate(light);
    
    if (renderData.accumulationEnabled == 1)
    {
        accumulationBuffer[DTid.xy] += float4(light, 0.f);
        resultBuffer[DTid.xy] = accumulationBuffer[DTid.xy] / (float) renderData.numAccumulationFrames;
    }
    else
    {
        resultBuffer[DTid.xy] = float4(light, 1.f);
    }
}