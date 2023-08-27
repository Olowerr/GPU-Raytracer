
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
    
    for (uint j = 0; j < renderData.numMeshes; j++)
    {
        if (Collision::RayAndAABB(ray, meshData[j].boundingBox))
        {
            uint triStart = meshData[j].triangleStartIdx;
            uint triEnd = meshData[j].triangleEndIdx;
            
            for (uint k = triStart; k < triEnd; k++)
            {
                float3 translation = meshData[j].transformMatrix[3].xyz;
                Triangle tri = triangleData[k];
                
                float3 pos0 = tri.p0.position + translation;
                float3 pos1 = tri.p1.position + translation;
                float3 pos2 = tri.p2.position + translation;
                
                float3 baryUVCoord;
                float distanceToHit = Collision::RayAndTriangle(ray, pos0, pos1, pos2, baryUVCoord.xy);
                
                if (distanceToHit > 0.f && distanceToHit < cloestHitDistance)
                {
                    cloestHitDistance = distanceToHit;
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
    
    payload.hit = hitIdx != UINT_MAX;
    payload.worldPosition = ray.origin + ray.direction * cloestHitDistance;
    
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
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
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
    ray.direction = mul(float4(normalize(target.xyz / target.z), 0.f), renderData.cameraInverseViewMatrix).xyz;
#endif

    
    float3 light = float3(0.f, 0.f, 0.f);
    float3 contribution = float3(1.f, 1.f, 1.f);
    
    Payload hitData;
    Sphere currentSphere;
    Material material;
    
#if 1
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
        
        static const float2 MinMaxMetallic = float2(0.0f, 0.2f);
        //const float metallic = material.metallic.colour * (MinMaxMetallic.y - MinMaxMetallic.x) + MinMaxMetallic.x;
        material.metallic.colour = clamp(material.metallic.colour, MinMaxMetallic.x, MinMaxMetallic.y);
        #if 1

        const float3 diffuseReflection = normalize(hitData.worldNormal + getRandomVector(seed));
        const float3 specularReflection = reflect(ray.direction, hitData.worldNormal);
        const float specularFactor = float(material.metallic.colour >= randomFloat(seed));
        
        ray.origin = hitData.worldPosition + hitData.worldNormal * 0.001f;
        //ray.direction = normalize(lerp(diffuseReflection, specularReflection, (1.f - material.roughness.colour) * specularFactor));
        ray.direction = normalize(lerp(diffuseReflection, specularReflection, (1.f - material.roughness.colour) * specularFactor));
        
        light += material.emissionColour * material.emissionPower * contribution;
        contribution *= lerp(material.albedo.colour, material.specularColour, specularFactor);
        
#elif 0
        
        const float3 diffuseReflection = normalize(hitData.worldNormal + getRandomVector(seed));
        const float3 specularReflection = reflect(ray.direction, hitData.worldNormal);
        const float specularFactor = float(material.metallic.colour >= randomFloat(seed));
        
        ray.origin = hitData.worldPosition + hitData.worldNormal * 0.001f;
        ray.direction = normalize(lerp(specularReflection, diffuseReflection, material.roughness.colour));
        
        float albedoIntensity = material.albedo.colour.x + material.albedo.colour.y + material.albedo.colour.z;
        
        light += material.emissionColour * material.emissionPower * contribution;
        contribution *= lerp(material.albedo.colour, material.specularColour, (albedoIntensity * 0.3f) * material.metallic.colour);
        #else

        const float3 diffuseReflection = normalize(hitData.worldNormal + getRandomVector(seed));
        const float3 specularReflection = reflect(ray.direction, hitData.worldNormal);
        const float specularFactor = float(material.metallic.colour >= randomFloat(seed));

        ray.origin = hitData.worldPosition + hitData.worldNormal * 0.001f;
        ray.direction = normalize(lerp(specularReflection, diffuseReflection, material.roughness.colour));
        
        light += material.emissionColour * material.emissionPower * contribution;
        contribution *= lerp(material.albedo.colour, material.specularColour, randomFloat(seed) * material.metallic.colour);
        #endif
    }
#else
    for (uint i = 0; i <= NUM_BOUNCES; i++)
    {
        hitData = findClosestHit(ray);
        if (!hitData.hit)
        {
            light += getEnvironmentLight(ray.direction) * contribution;
            break;
        }

        material = hitData.material;

        float3 hitPoint = hitData.worldPosition + hitData.worldNormal * 0.001f;

        // Calculate reflection vector for specular reflection
        //float3 reflectedDir = reflect(ray.direction, hitData.worldNormal);
        float3 diffuseReflection = normalize(hitData.worldNormal + getRandomVector(seed));
        float3 specularReflection = reflect(ray.direction, hitData.worldNormal);
        float3 reflectedDir = normalize(lerp(specularReflection, diffuseReflection, material.roughness.colour));
        
        // Calculate the microfacet distribution (GGX distribution)
        float roughness = material.roughness.colour;
        float3 halfVector = normalize(reflectedDir - ray.direction);
        float D = GGX_Distribution(hitData.worldNormal, halfVector, roughness);

        // Calculate the Fresnel-Schlick approximation for metallic and dielectric materials
        float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), material.specularColour, material.metallic.colour);
        float3 F = FresnelSchlick(F0, dot(halfVector, -ray.direction));

        // Calculate the geometric attenuation (Smith-GGX)
        float G = GGX_PartialGeometryTerm(-ray.direction, hitData.worldNormal, halfVector, material.roughness.colour);

        // Calculate the specular reflection contribution
        float3 specularContribution = (D * F * G) / (4.0 * /*max(dot(hitData.worldNormal, -ray.direction), 0.2f) **/
                                                 max(dot(hitData.worldNormal, reflectedDir), 0.f));
        
        // Update ray direction for the next bounce
        ray.origin = hitPoint;
        ray.direction = reflectedDir;


        // Apply specular and diffuse shading
        float3 albedo = material.albedo.colour;
        float3 diffuseContribution = albedo * (1.0 - material.metallic.colour);
        float3 shadingContribution = diffuseContribution + specularContribution;

        // Add emission contribution from the material
        light += material.emissionColour * material.emissionPower * contribution;
        
        // Update contribution for next bounce
        contribution *= shadingContribution;
    }
    #endif
    
    if (renderData.accumulationEnabled == 1)
    {
        accumulationBuffer[DTid.xy] += float4(light, 0.f) + float4(triangleData[0].p0.position.xyz * 0.00000001f, 0.f);
        resultBuffer[DTid.xy] = accumulationBuffer[DTid.xy] / (float)renderData.numAccumulationFrames;
    }
    else
    {
        resultBuffer[DTid.xy] = float4(light, 1.f);
    }
}