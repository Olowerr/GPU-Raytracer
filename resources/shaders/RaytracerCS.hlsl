
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
    uint firstChildIdx;
};

struct LightEvaluation
{
    float3 direction;
    float3 colour;
    float intensity;
    float specularStrengthModifier;
    float2 attentuation;
};

// ---- Resources

// From GPUResourceManager
StructuredBuffer<Triangle> trianglePosData : register(RM_TRIANGLE_POS_GPU_REG);
StructuredBuffer<TriangleInfo> triangleInfoData : register(RM_TRIANGLE_INFO_GPU_REG);
StructuredBuffer<Node> bvhNodes : register(RM_BVH_TREE_GPU_REG);
Texture2DArray<unorm float4> textures : register(RM_TEXTURES_GPU_REG);
TextureCube environmentMap : register(RM_ENVIRONMENT_MAP_GPU_REG);

SamplerState simp : register(s0);

// From RayTracer
RWTexture2D<unorm float4> resultBuffer : register(RT_RESULT_BUFFER_GPU_REG);
RWTexture2D<float4> accumulationBuffer : register(RT_ACCUMULATION_BUFFER_GPU_REG);
StructuredBuffer<Sphere> sphereData : register(RT_SPHERE_DATA_GPU_REG);
StructuredBuffer<Mesh> meshData : register(RT_MESH_ENTITY_DATA_GPU_REG);
StructuredBuffer<DirectionalLight> directionalLights : register(RT_DIRECTIONAL_LIGHT_DATA_GPU_REG);
StructuredBuffer<PointLight> pointLights : register(RT_POINT_LIGHT_DATA_GPU_REG);
StructuredBuffer<SpotLight> spotLights : register(RT_SPOT_LIGHT_DATA_GPU_REG);
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
            
            bbCheckCount += 1;
            if (Collision::RayAndAABBDist(localRay, node.boundingBox) >= closestHitDistance)
                continue; // Missed AABB
            
            if (node.firstChildIdx == UINT_MAX) // Is leaf?
            {
                for (i = node.triStart; i < node.triEnd; i++)
                {
                    Triangle tri = trianglePosData[i];
            
                    float3 p0 = tri.position[0];
                    float3 p1 = tri.position[1];
                    float3 p2 = tri.position[2];
            
                    float2 baryUVCoords = float2(0.f, 0.f);
                    
                    triCheckCount += 1;
                    float distanceToHit = Collision::RayAndTriangle(localRay, p0, p1, p2, baryUVCoords);
                    
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
                stack[stackSize++] = node.firstChildIdx;
                stack[stackSize++] = node.firstChildIdx + 1;
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

float3 calculateLightning(LightEvaluation evaluationData, float distanceToHit, float3 hitPoint, float3 normal, float3 viewDir, float3 matAlbedo, float matSpecular, float specularFactor, inout uint bbCheckCount, inout uint triCheckCount)
{
    Ray lightRay;
    lightRay.origin = hitPoint;
    lightRay.direction = evaluationData.direction;

    Payload lightPayload = findClosestHit(lightRay, bbCheckCount, triCheckCount);

    if (lightPayload.hit && length(lightPayload.worldPosition - hitPoint) < distanceToHit)
        return float3(0.f, 0.f, 0.f);
    
    float3 lightReflection = reflect(-lightRay.direction, normal);
    float lightDotNormal = clampedDot(lightRay.direction, normal);
    float lightReflectDotView = clampedDot(lightReflection, viewDir);
            
    float3 diffuse = matAlbedo * lightDotNormal;
    float3 specular = float3(1.f, 1.f, 1.f) * pow(lightReflectDotView, max(100.f * smoothstep(0.f, 1.f, matSpecular), 1.f));
    
    float attenuation = 1.f / (1.f + evaluationData.attentuation.x * distanceToHit + evaluationData.attentuation.y * distanceToHit * distanceToHit);
    
    return lerp(diffuse, specular * evaluationData.specularStrengthModifier, specularFactor) * evaluationData.colour * evaluationData.intensity * attenuation;
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
        if (!hitData.hit)
        {
            //light += getEnvironmentLight(ray.direction) * contribution;
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
        float metallicFactor = (material.metallic.colour * (1.f - material.roughness.colour)) >= randomFloat(seed);
        float specularFactor = (material.specular.colour * (1.f - material.roughness.colour)) >= randomFloat(seed);
        
        float3 bounceDir = findReflectDirection(ray.direction, hitData.worldNormal, material.roughness.colour * (1.f - specularFactor), seed);
        float3 hitPoint = hitData.worldPosition + bounceDir * 0.001f;
        
        float3 phongLight = float3(0.f, 0.f, 0.f);

        for (uint d = 0u; d < renderData.numDirLights; d++)
        {
            DirectionalLight dirLight = directionalLights[d];
            
            LightEvaluation evaluationData;
            evaluationData.direction = normalize(dirLight.direction + getRandomVector(seed) * dirLight.penumbraSizeModifier);
            evaluationData.colour = dirLight.colour;
            evaluationData.intensity = dirLight.intensity;
            evaluationData.specularStrengthModifier = dirLight.specularStrength;
            evaluationData.attentuation = float2(0.f, 0.f);
            float distanceToHit = FLT_MAX;
            
            phongLight += calculateLightning(evaluationData, distanceToHit, hitPoint, hitData.worldNormal, -ray.direction, material.albedo.colour, material.specular.colour, specularFactor, bbCheckCount, triCheckCount);
        }
        
        for (uint p = 0u; p < renderData.numPointLights; p++)
        {
            PointLight pointLight = pointLights[p];
            
            float3 hitToLight = pointLight.position - hitPoint;
            hitToLight += getRandomVector(seed) * randomFloat(seed) * pointLight.penumbraRadius;
            hitToLight = normalize(hitToLight);
            
            LightEvaluation evaluationData;
            evaluationData.direction = hitToLight;
            evaluationData.colour = pointLight.colour;
            evaluationData.intensity = pointLight.intensity;
            evaluationData.specularStrengthModifier = pointLight.specularStrength;
            evaluationData.attentuation = pointLight.attenuation;
            
            float distanceToHit = length(hitPoint - pointLight.position);
            
            phongLight += calculateLightning(evaluationData, distanceToHit, hitPoint, hitData.worldNormal, -ray.direction, material.albedo.colour, material.specular.colour, specularFactor, bbCheckCount, triCheckCount);
        }
            
        for (uint s = 0u; s < renderData.numSpotLights; s++)
        {
            SpotLight spotLight = spotLights[s];
         
            float3 randVec = getRandomVector(seed) * spotLight.penumbraRadius;
            spotLight.position += randVec;

            float3 lightToHit = normalize(hitPoint - spotLight.position);
            float3 lightDir = normalize(-spotLight.direction);
            
            float cosTheta = dot(lightDir, lightToHit);
            if (cosTheta < cos(spotLight.maxAngle))
                continue;
            
            LightEvaluation evaluationData;
            evaluationData.direction = normalize(-lightToHit);
            evaluationData.colour = spotLight.colour;
            evaluationData.intensity = spotLight.intensity;
            evaluationData.specularStrengthModifier = spotLight.specularStrength;
            evaluationData.attentuation = spotLight.attenuation;
            
            float distanceToHit = length(hitPoint - spotLight.position);
            
            phongLight += calculateLightning(evaluationData, distanceToHit, hitPoint, hitData.worldNormal, -ray.direction, material.albedo.colour, material.specular.colour, specularFactor, bbCheckCount, triCheckCount);
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
        light += phongLight * contribution;
       
        ray.origin = hitPoint;
        ray.direction = bounceDir;
    }
    
    uint debugModeMaxCount = renderData.debugMaxCount;
    if (renderData.debugMode == 1)
        light = bbCheckCount > debugModeMaxCount ? float3(1.f, 0.f, 0.f) : float3(1.f, 1.f, 1.f) * (bbCheckCount / (float)debugModeMaxCount);
    else if (renderData.debugMode == 2)
        light = triCheckCount > debugModeMaxCount ? float3(1.f, 0.f, 0.f) : float3(1.f, 1.f, 1.f) * (triCheckCount / (float)debugModeMaxCount);
    
    float gamma = 2.f;
    light = pow(light, float3(gamma, gamma, gamma));
    
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