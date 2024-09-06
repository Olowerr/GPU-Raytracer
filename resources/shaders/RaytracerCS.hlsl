
#include "GPU-Utilities.hlsli"
#include "ShaderResourceRegisters.h"

// ---- Defines and constants
#define NUM_BOUNCES (3)


// ---- Structs
struct Payload
{
    bool hit;
    float distance;
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
    
    uint numDirLights;
    uint numPointLights;
    uint numSpotLights;
    uint debugMode;
    
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
    
    uint debugMaxCount;
    float3 pad0;
};

struct Node
{
    AABB boundingBox;
    uint triStart;
    uint triEnd;
    uint firstChildIdx;
};

struct OctTreeNode
{
    AABB boundingBox;

    uint meshesStartIdx;
    uint meshesEndIdx;

    uint spheresStartIdx;
    uint spheresEndIdx;

    uint children[8u];
};

// ---- Resources

StructuredBuffer<Triangle> trianglePosData : register(TRIANGLE_POS_GPU_REG);
StructuredBuffer<TriangleInfo> triangleInfoData : register(TRIANGLE_INFO_GPU_REG);
StructuredBuffer<Node> bvhNodes : register(BVH_TREE_GPU_REG);
Texture2DArray<unorm float4> textures : register(TEXTURES_GPU_REG);
TextureCube environmentMap : register(ENVIRONMENT_MAP_GPU_REG);

SamplerState simp : register(s0);

RWTexture2D<unorm float4> resultBuffer : register(RESULT_BUFFER_GPU_REG);
RWTexture2D<float4> accumulationBuffer : register(ACCUMULATION_BUFFER_GPU_REG);
StructuredBuffer<Sphere> sphereData : register(SPHERE_DATA_GPU_REG);
StructuredBuffer<Mesh> meshData : register(MESH_ENTITY_DATA_GPU_REG);
StructuredBuffer<DirectionalLight> directionalLights : register(DIRECTIONAL_LIGHT_DATA_GPU_REG);
StructuredBuffer<PointLight> pointLights : register(POINT_LIGHT_DATA_GPU_REG);
StructuredBuffer<SpotLight> spotLights : register(SPOT_LIGHT_DATA_GPU_REG);

StructuredBuffer<OctTreeNode> octTreeNodes : register(OCT_TREE_GPU_REG);

cbuffer RenderDataBuffer : register(RENDER_DATA_GPU_REG)
{
    RenderData renderData;
}

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
    return textures.SampleLevel(simp, float3(meshUVs, (float)textureIdx), 0.f).rgb;
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

Payload findClosestHit(Ray ray, inout uint bbCheckCount, inout uint triCheckCount)
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
    
    static const uint OCT_MAX_STACK_SIZE = 20;
    half octStack[OCT_MAX_STACK_SIZE];
    octStack[0] = 0;
    uint octStackSize = 1;
    
    static const uint BVH_MAX_STACK_SIZE = 50;
    half bvhStack[BVH_MAX_STACK_SIZE];
    uint bvhStackSize = 0;

    uint triHitIdx = UINT_MAX;
    float3 hitBaryUVCoords = float3(0.f, 0.f, 0.f);
    
    while (octStackSize > 0)
    {
        uint octCurrentNodeIdx = octStack[--octStackSize];
        OctTreeNode octNode = octTreeNodes[octCurrentNodeIdx];
        
        if (Collision::RayAndAABBDist(ray, octNode.boundingBox) >= closestHitDistance)
            continue;
        
        for (i = octNode.meshesStartIdx; i < octNode.meshesEndIdx; i++)
        {
            bbCheckCount += 1;
            uint currentNodeIdx = meshData[i].bvhNodeStartIdx;
            bvhStack[0] = currentNodeIdx;
            bvhStackSize = 1;
        
            float4x4 invTraMatrix = meshData[i].inverseTransformMatrix;
        
            Ray localRay;
            localRay.origin = mul(float4(ray.origin, 1.f), invTraMatrix).xyz;
            localRay.direction = normalize(mul(float4(ray.direction, 0.f), invTraMatrix).xyz);
        
            while (bvhStackSize > 0)
            {
                currentNodeIdx = bvhStack[--bvhStackSize];
            
                Node node = bvhNodes[currentNodeIdx];
            
                bbCheckCount += 1;
                if (Collision::RayAndAABBDist(localRay, node.boundingBox) >= closestHitDistance)
                    continue; // Missed AABB
            
                if (node.firstChildIdx != UINT_MAX) // Is not leaf?
                {
                    bvhStack[bvhStackSize++] = node.firstChildIdx;
                    bvhStack[bvhStackSize++] = node.firstChildIdx + 1;
                    continue;
                }
                    
                for (uint j = node.triStart; j < node.triEnd; j++)
                {
                    Triangle tri = trianglePosData[j];
        
                    float3 p0 = tri.position[0];
                    float3 p1 = tri.position[1];
                    float3 p2 = tri.position[2];
        
                    float2 baryUVCoords = float2(0.f, 0.f);
                
                    triCheckCount += 1;
                    float distanceToHit = Collision::RayAndTriangle(localRay, p0, p1, p2, baryUVCoords);
                
                    if (distanceToHit <= 0.f)
                        continue;
                
                    // Convert distanceToHit to worldspace for accurate comparison
                    float4x4 traMatrix = meshData[i].transformMatrix;
                    float4 localRayToHit = float4(localRay.direction * distanceToHit, 0.f);
                    distanceToHit = length(mul(localRayToHit, traMatrix).xyz);
                
                    if (distanceToHit < closestHitDistance)
                    {
                        closestHitDistance = distanceToHit;
                        hitIdx = i;
                        hitType = 1;
                        triHitIdx = j;
                    
                        hitBaryUVCoords.xy = baryUVCoords;
                        hitBaryUVCoords.z = 1.f - (hitBaryUVCoords.x + hitBaryUVCoords.y);
                    }
                }
            }
        }
        
        for (uint k = 0; k < 8; k++)
        {
            uint childIdx = octNode.children[k];
            if (isValidIdx(childIdx))
            {
                octStack[octStackSize++] = childIdx;
            }
        }
    }
    
    payload.hit = hitIdx != UINT_MAX;
    payload.worldPosition = ray.origin + ray.direction * closestHitDistance;
    payload.distance = closestHitDistance;
    
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

            TriangleInfo tri = triangleInfoData[triHitIdx];
            VertexInfo p0 = tri.vertexInfo[0];
            VertexInfo p1 = tri.vertexInfo[1];
            VertexInfo p2 = tri.vertexInfo[2];
        
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

float3 getLighting(Ray ray, Payload hitPayload, uint bounce, float3 lightPos, float lightRadius, float3 lightColour, float3 lightIntensity)
{
    Sphere lightSphere;
    lightSphere.position = lightPos;
    lightSphere.radius = lightRadius;
           
    float dist = Collision::RayAndSphere(ray, lightSphere);
    if (dist < 0.f)
        return float3(0.f, 0.f, 0.f);
                
    float lightStrengthModifier = 1.f;
    if (hitPayload.hit && hitPayload.distance > dist)
    {
        lightStrengthModifier *= clampedDot(ray.direction, -hitPayload.worldNormal);
    }
    else if (hitPayload.hit && hitPayload.distance < dist)
    {
        lightStrengthModifier *= hitPayload.material.transparency * clampedDot(ray.direction, -hitPayload.worldNormal);
    }
                
    return lightColour * lightIntensity * lightStrengthModifier;
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
    
    uint bbCheckCount = 0;
    uint triCheckCount = 0;
    
    for (uint i = 0; i <= NUM_BOUNCES; i++)
    {
        hitData = findClosestHit(ray, bbCheckCount, triCheckCount);
        
        for (uint p = 0u; p < renderData.numPointLights; p++)
        {
            PointLight pointLight = pointLights[p];
        
            float3 lightning = getLighting(ray, hitData, i, pointLight.position, pointLight.radius, pointLight.colour, pointLight.intensity);
            light += lightning * contribution;
        }
        
        for (uint s = 0u; s < renderData.numSpotLights; s++)
        {
            SpotLight spotLight = spotLights[s];
            
            float3 rayToLight = normalize((spotLight.position + getRandomVector(seed) * 0.01f) - ray.origin);
            float cosTheta = dot(rayToLight, -spotLight.direction);
            if (cosTheta < spotLight.maxAngle)
                continue;
            
            float3 lightning = getLighting(ray, hitData, i, spotLight.position, spotLight.radius, spotLight.colour, spotLight.intensity);
            light += lightning * contribution;
        }
        
        for (uint d = 0u; d < renderData.numDirLights; d++)
        {
            DirectionalLight dirLight = directionalLights[d];

            if (clampedDot(ray.direction, -dirLight.direction) < dirLight.effectiveAngle)
                continue;
            
            float lightStrengthModifier = 1.f;
            if (hitData.hit && i)
            {
                lightStrengthModifier *= hitData.material.transparency * clampedDot(ray.direction, -hitData.worldNormal);
            }
            else if (!i) // Fixes issue where light source was visible through objects
            {
                lightStrengthModifier = 0.f;
            }

            light += dirLight.colour * dirLight.intensity * contribution * lightStrengthModifier;
        }
        
        if (!hitData.hit)
        {
            light += getEnvironmentLight(ray.direction) * contribution;
            break;
        }
        
        material = hitData.material;

        float metallicFactor = (material.metallic.colour * (1.f - material.roughness.colour)) >= randomFloat(seed);
        float specularFactor = (material.specular.colour * (1.f - material.roughness.colour)) >= randomFloat(seed);
        float transparencyFactor = material.transparency >= randomFloat(seed);
        
        float3 reflectDir = findReflectDirection(ray.direction, hitData.worldNormal, material.roughness.colour * (1.f - specularFactor), seed);
        float3 refractDir = findTransparencyBounce(ray.direction, hitData.worldNormal, material.indexOfRefraction, seed);
        
        float3 bounceDir = normalize(lerp(reflectDir, refractDir, transparencyFactor));
        float3 hitPoint = hitData.worldPosition + bounceDir * 0.001f;
        
        contribution *= lerp(material.albedo.colour, float3(1.f, 1.f, 1.f), metallicFactor);
        light += material.emissionColour * material.emissionPower * contribution * (1.f - transparencyFactor);
       
        ray.origin = hitPoint;
        ray.direction = bounceDir;
    }
    
    uint debugModeMaxCount = renderData.debugMaxCount;
    if (renderData.debugMode == 1)
        light = bbCheckCount > debugModeMaxCount ? float3(1.f, 0.f, 0.f) : float3(1.f, 1.f, 1.f) * (bbCheckCount / (float)debugModeMaxCount);
    else if (renderData.debugMode == 2)
        light = triCheckCount > debugModeMaxCount ? float3(1.f, 0.f, 0.f) : float3(1.f, 1.f, 1.f) * (triCheckCount / (float)debugModeMaxCount);
    
    float gamma = 2.f;
    //light = pow(light, float3(gamma, gamma, gamma));
    
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
    
    resultBuffer[DTid.xy] = result;
}