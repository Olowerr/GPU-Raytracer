
#include "GPU-Utilities.hlsli"
#include "ShaderResourceRegisters.h"

// ---- Defines and constants
#define NUM_BOUNCES (10)


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
  
    float3 cameraUpDir;
    float dofStrength;
    float3 cameraRightDir;
    float dofDistance;
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

// From GPUResourceManager
StructuredBuffer<Triangle> triangleData : register(RM_TRIANGLE_DATA_GPU_REG);
StructuredBuffer<Node> bvhNodes : register(RM_BVH_TREE_GPU_REG);
StructuredBuffer<AtlasTextureDesc> textureDescs : register(RM_TEXTURE_ATLAS_DESC_GPU_REG);
Texture2D<unorm float4> textureAtlas : register(RM_TEXTURE_ATLAS_GPU_REG);
TextureCube environmentMap : register(RM_ENVIRONMENT_MAP_GPU_REG);

SamplerState simp : register(s0);

// From RayTracer
RWTexture2D<unorm float4> resultBuffer : register(RT_RESULT_BUFFER_GPU_REG);
RWTexture2D<float4> accumulationBuffer : register(RT_ACCUMULATION_BUFFER_GPU_REG);
StructuredBuffer<Sphere> sphereData : register(RT_SPHERE_DATA_GPU_REG);
StructuredBuffer<Mesh> meshData : register(RT_MESH_ENTITY_DATA_GPU_REG);
cbuffer RenderDataBuffer : register(RT_RENDER_DATA_GPU_REG)
{
    RenderData renderData;
}


/*
    TODO: Check if seperating triangles into 3 buffers: positions, normals and uv, can be faster than current.
    That way we don't need to access the normals and uv while doing intersection tests,
    Meaning the cache can get more positions at once. (is this how it works?)
    OR use one big buffer, but structure it like: all positions -> all normals -> all uvs
*/


// ---- Functions
float2 normalToUV(float3 normal)
{
    float2 uv;
    uv.x = atan2(normal.x, normal.z) / (2 * PI) + 0.5;
    uv.y = normal.y * 0.5 + 0.5;
    
    return uv;
}

float3 getEnvironmentLight(float3 direction)
{
    return environmentMap.SampleLevel(simp, direction, 0.f).rgb;
    
    const static float3 SKY_COLOUR = float3(102.f, 204.f, 255.f) / 255.f;
    const static float3 GROUND_COLOUR = float3(164.f, 177.f, 178.f) / 255.f;
    
    const float dotty = dot(direction, float3(0.f, 1.f, 0.f));
    float3 colour = lerp(float3(1.f, 1.f, 1.f), SKY_COLOUR, pow(max(dotty, 0.f), 0.6f));
    
    const float transitionDotty = clamp((dotty + 0.005f) / 0.01f, 0.f, 1.f);
    return lerp(GROUND_COLOUR, colour, transitionDotty);
}

float reflectance(float cosine, float reflectionIdx)
{
    float r0 = (1.f - reflectionIdx) / (1.f + reflectionIdx);
    r0 *= r0;
    return r0 + (1.f + r0) * pow(1.f - cosine, 5.f);
}

float3 findReflectDirection(float3 direction, float3 normal, float roughness, inout uint seed)
{
    float3 diffuseReflection = normalize(normal + getRandomVector(seed));
    float3 specularReflection = reflect(direction, normal);
    return normalize(lerp(specularReflection, diffuseReflection, roughness));
}

float3 findTransparencyBounce(float3 direction, float3 normal, float refractionIdx, inout uint seed)
{
    bool hitFrontFace = dot(direction, normal) < 0.f;
    float refractionRatio = hitFrontFace ? AIR_REFRACTION_INDEX / refractionIdx : refractionIdx;
    
    if (!hitFrontFace)
        normal *= -1.f;
    
    float cos_theta = dot(-direction, normal);
    float sin_theta = sqrt(1.f - cos_theta * cos_theta);
    
    bool cannot_refract = refractionRatio * sin_theta > 1.f;

    if (cannot_refract || reflectance(cos_theta, refractionRatio) > randomFloat(seed))
        direction = reflect(direction, normal);
    
    return refract(direction, normal, refractionRatio);
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
    {
        material.albedo.colour = sampleTexture(material.albedo.textureIdx, meshUVs);
    }
    
    if (isValidIdx(material.roughness.textureIdx))
    {
        float roughness = sampleTexture(material.roughness.textureIdx, meshUVs).r;
        roughness = clamp(roughness, material.roughness.colour, 1.f);
        material.roughness.colour = roughness;
    }
    
    if (isValidIdx(material.metallic.textureIdx))
    {
        float metallic = sampleTexture(material.metallic.textureIdx, meshUVs).r;
        metallic = clamp(metallic, material.metallic.colour, 1.f);
        material.metallic.colour = metallic;
    }
    
     if (isValidIdx(material.specular.textureIdx))
    {
        float specular = sampleTexture(material.specular.textureIdx, meshUVs).r;
        specular = clamp(specular, material.specular.colour, 1.f);
        material.specular.colour = specular;
    }
}

float3 sampleNormalMap(uint textureIdx, float2 meshUVs, float3 normal, float3 tangent, float3 bitangent)
{
    float3 sampledNormal = sampleTexture(textureIdx, meshUVs);
    sampledNormal = sampledNormal * 2.f - 1.f;
    
    float3x3 tbn = float3x3(tangent, bitangent, normal);
 
    return normalize(mul(sampledNormal, tbn));
}

Payload findClosestHit(Ray ray)
{
    Payload payload;
    
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
    
    // TODO: Rewrite the upcoming loop to reduce nesting

    static const uint MAX_STACK_SIZE = 50;
    half stack[MAX_STACK_SIZE];
    uint stackSize = 0;

    uint triHitIdx = UINT_MAX;
    float3 hitBaryUVCoords = float3(0.f, 0.f, 0.f);
    
    // TODO: Rewrite the upcoming loop to reduce nesting
    for (uint j = 0; j < renderData.numMeshes; j++)
    {
        uint currentNodeIdx = meshData[j].bvhNodeStartIdx;
        stack[0] = currentNodeIdx;
        stackSize = 1;
        
        float4x4 invTraMatrix = meshData[j].inverseTransformMatrix;
        
        Ray localRay;
        localRay.origin = mul(float4(ray.origin, 1.f), invTraMatrix).xyz;
        localRay.direction = normalize(mul(float4(ray.direction, 0.f), invTraMatrix).xyz);
        
        while (stackSize > 0)
        {
            currentNodeIdx = stack[--stackSize];
            
            Node node = bvhNodes[currentNodeIdx];
            if (!Collision::RayAndAABB(localRay, node.boundingBox))
                continue; // Missed AABB
            
            if (node.childIdxs[0] == UINT_MAX) // Is leaf?
            {
                for (i = node.triStart; i < node.triEnd; i++)
                {
                    Triangle tri = triangleData[i];
            
                    Vertex p0 = tri.verticies[0];
                    Vertex p1 = tri.verticies[1];
                    Vertex p2 = tri.verticies[2];
            
                    float2 baryUVCoords = float2(0.f, 0.f);
                    float distanceToHit = Collision::RayAndTriangle(localRay, p0.position, p1.position, p2.position, baryUVCoords);
                    
                    if (distanceToHit <= 0.f)
                    {
                        continue;
                    }
                    
                    // Convert distanceToHit to worldspace for accurate comparison
                    float4x4 traMatrix = meshData[j].transformMatrix;
                    float4 localRayToHit = float4(localRay.direction * distanceToHit, 0.f);
                    distanceToHit = length(mul(localRayToHit, traMatrix).xyz);
                    
                    if (distanceToHit < closestHitDistance)
                    {
                        closestHitDistance = distanceToHit;
                        hitIdx = j;
                        hitType = 1;
                        triHitIdx = i;
                        
                        hitBaryUVCoords.xy = baryUVCoords;
                        hitBaryUVCoords.z = 1.f - (hitBaryUVCoords.x + hitBaryUVCoords.y);
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
    
    payload.hit = hitIdx != UINT_MAX;
    payload.worldPosition = ray.origin + ray.direction * closestHitDistance;
    
    // TODO: Make better system
    switch (hitType)
    {
        case 0: // Sphere
            payload.material = sphereData[hitIdx].material;
            
            float3 hitNormal = normalize(payload.worldPosition - sphereData[hitIdx].position);
            float2 sphereUVs = normalToUV(hitNormal);
            
            if (isValidIdx(payload.material.normalMapIdx))
            {
                float3 normal = hitNormal;
                float3 tangent = normalize(cross(float3(0.f, 1.f, 0.f), hitNormal));
                float3 bitangent = normalize(cross(tangent, hitNormal));
        
                payload.worldNormal = sampleNormalMap(payload.material.normalMapIdx, sphereUVs, normal, tangent, bitangent);
            }
            else
            {
                payload.worldNormal = hitNormal;
            }
            
            
            findMaterialTextureColours(payload.material, sphereUVs);
            break;
        
        case 1: // Mesh
            payload.material = meshData[hitIdx].material;

            Triangle tri = triangleData[triHitIdx];
            Vertex p0 = tri.verticies[0];
            Vertex p1 = tri.verticies[1];
            Vertex p2 = tri.verticies[2];
        
            float2 lerpedUV = barycentricInterpolation(hitBaryUVCoords, p0.uv, p1.uv, p2.uv);
            float3 normal = mul(float4(barycentricInterpolation(hitBaryUVCoords, p0.normal, p1.normal, p2.normal), 0.f), meshData[hitIdx].transformMatrix).xyz;
            float3 tangent = mul(float4(barycentricInterpolation(hitBaryUVCoords, p0.tangent, p1.tangent, p2.tangent), 0.f), meshData[hitIdx].transformMatrix).xyz;
            float3 bitangent = mul(float4(barycentricInterpolation(hitBaryUVCoords, p0.bitangent, p1.bitangent, p2.bitangent), 0.f), meshData[hitIdx].transformMatrix).xyz;
        
            normal = normalize(normal);
            tangent = normalize(tangent);
            bitangent = normalize(bitangent);
        
            if (isValidIdx(payload.material.normalMapIdx))
            {
                payload.worldNormal = sampleNormalMap(payload.material.normalMapIdx, lerpedUV, normal, tangent, bitangent);
            }
            else
            {
                payload.worldNormal = normal;
            }
        
            findMaterialTextureColours(payload.material, lerpedUV);
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
};


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
    
    // DOF
    float2 rayJitter = randomPointInCircle(seed) * renderData.dofStrength;
    float3 rayOffset = renderData.cameraRightDir * rayJitter.x + renderData.cameraUpDir * rayJitter.y;
    float3 focusPoint = ray.origin + ray.direction * (renderData.dofDistance + renderData.cameraNearZ); // + nearZ enables dofDistance = 0

    ray.origin += rayOffset;
    ray.direction = normalize(focusPoint - ray.origin);

    float3 light = float3(0.f, 0.f, 0.f);
    float3 contribution = float3(1.f, 1.f, 1.f);
    
    Payload hitData;
    Material material;
    
    for (uint i = 0; i <= NUM_BOUNCES; i++)
    {
        hitData = findClosestHit(ray);
        if (!hitData.hit)
        {
            light += getEnvironmentLight(ray.direction) * contribution;
            //light += getNightLight(ray.direction) * contribution;
            break;
        }
        
        material = hitData.material;

        /*
            roughness defines if diffuse or perfect reflection entirely, still use randomFloat < value thing I think
                float roughnessFactor = material.roughness.colour >= RandomFloat(seed)
        
            metal * (1- roughness) lerps between albedo and specular (white) (0 -> albedo, 1 -> specular)
                float metalFactor = material.metallic.color >= RandomFloat(seed)
                float metalFactor = (metal * (1- roughness)) >= RandomFloat(seed)
        
            chill with transparency for now
        */
        float roughnessFactor = material.roughness.colour >= randomFloat(seed); 
        float metallicFactor = (material.metallic.colour * (1 - material.roughness.colour)) >= randomFloat(seed);
        float specularFactor = (material.specular.colour * (1.f - material.roughness.colour)) >= randomFloat(seed);
        
        float3 bounceDir = findReflectDirection(ray.direction, hitData.worldNormal, material.roughness.colour * (1.f - specularFactor), seed);
        float3 hitPoint = hitData.worldPosition + bounceDir * 0.001f;
        
        float3 blinnPhongLight = float3(0.f, 0.f, 0.f);
        {
            static const float3 SUN_COLOUR = float3(1.f, 1.f, 1.f);
            static const float3 SUN_DIR = normalize(float3(1.f, 1.f, 1.f));
            static const float SUN_STRENGTH = 1.f;
            
            const float3 lightReflection = reflect(-SUN_DIR, hitData.worldNormal);
            const float lightDotNormal = clampedDot(SUN_DIR, hitData.worldNormal);
            const float lightReflectDotView= clampedDot(lightReflection, -ray.direction);
            
            float3 diffuse = material.albedo.colour * lightDotNormal;
            float3 specular = float3(1.f, 1.f, 1.f) * pow(lightReflectDotView, max(100.f * smoothstep(0.f, 1.f, material.specular.colour), 1.f));
            
            Ray toLightRay;
            toLightRay.origin = hitPoint;
            toLightRay.direction = normalize(float3(1.f, 1.f, 1.f) + getRandomVector(seed) * 0.1f);
            Payload lightPayLoad = findClosestHit(toLightRay);
            
            blinnPhongLight = lerp(diffuse, specular * 10, specularFactor) * SUN_COLOUR * SUN_STRENGTH * !lightPayLoad.hit;
        }

        /*
            Fix normal maps
            Change specularColour to a float1, only controls the specular pow exponent (maybe still 0-1 & multiply with some value like 300? then can still use textures)
            Fix entity components for the 3 light types (directional, point, spot)
            Fix metal calculations
            Fix transparency calculations
        
        
        
        maybe change so there are normal (diffuse) bounces & specular bounces.
        where normal/diffuse bounces will just bounce off in bounceDir and calculate lighing like normal
        and specular bounces will do the specular pow calculations?
        but how do we determine when which bounce is gonna happen?
        maybe just use roughnessFactor as it is, and let specular.colour be the exponent like normal?   
        not sure what to do with lightPayLoad in that version tho... do we cast it on diffuse bounces too, or only specular ones..?
        only on specular doesn't make sense, since then diffuse materials have no shadows? but then we need to do light calcs for diffuse and we're just back at square one?
        
        */
        
        contribution *= lerp(material.albedo.colour, float3(1.f, 1.f, 1.f), metallicFactor);
        light += material.emissionColour * material.emissionPower * contribution;
        light += blinnPhongLight * contribution;
       
        ray.origin = hitPoint;
        ray.direction = bounceDir;
    }
    
    float4 result;
    if (renderData.accumulationEnabled == 1)
    {
        accumulationBuffer[DTid.xy] += float4(light, 1.f);
        result = saturate(accumulationBuffer[DTid.xy] / (float) renderData.numAccumulationFrames);
    }
    else
    {
        result = float4(saturate(light), 1.f);
    }
    
    float gamma = 2.f;
    //result = pow(result, float4(gamma, gamma, gamma, 0.f));
    resultBuffer[DTid.xy] = result;

}