
#define UINT_MAX (~0u)
#define FLT_MAX (3.402823466e+38F)
#define PI (3.14159265f)
#define AIR_REFRACTION_INDEX (1.f) // Approx

struct MaterialColour3
{
    float3 colour;
    uint textureIdx;
};

struct MaterialColour1
{
    float colour;
    uint textureIdx;
};

struct Material
{
    MaterialColour3 albedo;
    MaterialColour1 roughness;
    MaterialColour1 metallic;
    MaterialColour1 specular;
    
    uint normalMapIdx;
    
    float3 emissionColour;
    float emissionPower;
    
    float transparency;
    float indexOfRefraction;
};

struct AABB
{
    float3 min;
    float3 max;
};

struct Ray
{
    float3 origin;
    float3 direction;
};

struct Sphere
{
    float3 position;

    Material material;

    float radius;
};

struct Triangle
{
    float3 position[3];
};

struct VertexInfo
{
    float3 normal;
    float2 uv;
    float3 tangent;
    float3 bitangent;
};

struct TriangleInfo
{
    VertexInfo vertexInfo[3];
};

struct Mesh
{
    float4x4 transformMatrix;
    float4x4 inverseTransformMatrix;
    
    uint triangleStartIdx;
    uint triangleEndIdx;
    AABB boundingBox;
 
    Material material;
    
    uint bvhNodeStartIdx;
};

struct DirectionalLight
{
    float3 colour;
    float intensity;
    float specularStrength;
    
    float penumbraSizeModifier;
    
    float3 direction;
};

struct PointLight
{
    float3 colour;
    float intensity;
    float specularStrength;
    
    float2 attenuation;
    
    float penumbraRadius;
    
    float3 position;
};

struct SpotLight
{
    float3 colour;
    float intensity;
    float specularStrength;
    
    float2 attenuation;
    
    float penumbraRadius;
    float maxAngle;
    
    float3 position;
    float3 direction;
};

struct PS_Input
{
    float4 svPos : SV_Position;
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXTURE_COORDS;
};

uint pcg_hash(inout uint seed)
{
    // Ty Cherno
    seed *= 747796405u + 2891336453u;
    seed = ((seed >> ((seed >> 28u) + 4u)) ^ seed) * 277803737u;
    seed = (seed >> 22u) ^ seed;
    return seed;
}

uint pcg_hash2(uint seed)
{
    seed *= 747796405u + 2891336453u;
    seed = ((seed >> ((seed >> 28u) + 4u)) ^ seed) * 277803737u;
    seed = (seed >> 22u) ^ seed;
    return seed;
}

float pseudorandomNumber(float3 normalizedDirection)
{
    float3 permute = float3(1.0, 57.0, 113.0); // Permutation constants
    float3 grad = normalize(float3(1.0, 1.0, 1.0)); // Gradient direction

    float3 p = frac(float3(normalizedDirection.xyx) * permute);
    p = p * 2.0 - 1.0;

    float3 d = abs(p) - 0.5;
    float3 h = 1.0 - abs(d);
    float3 n = max(h, 0.0) * h * h;
    float3 g = grad * dot(d, p);

    return dot(n, g);
}

float randomFloat(inout uint seed)
{
    return pcg_hash(seed) / (float)UINT_MAX;
}

float randomFloatNormalDistribution(inout uint seed)
{
    // Ty Sebastian Lague
    float theta = 2 * PI * randomFloat(seed);
    float rho = sqrt(-2 * log(randomFloat(seed)));
    return rho * cos(theta);
}

float3 getRandomVector(inout uint seed)
{
    float x = randomFloatNormalDistribution(seed);
    float y = randomFloatNormalDistribution(seed);
    float z = randomFloatNormalDistribution(seed);  
    return normalize(float3(x, y, z));
}

float2 randomPointInCircle(inout uint seed)
{
    // Ty Sebastian Lague
    float angle = randomFloat(seed) * 2.f * PI;
    float2 pointOnCircle = float2(cos(angle), sin(angle));
    return pointOnCircle * sqrt(randomFloat(seed));
}

float clampedDot(float3 v, float3 u)
{
    return max(dot(v, u), 0.f);
}

float3 refract(float3 direction, float3 normal, float refractionRatio)
{
    float cosTheta = min(dot(-direction, normal), 1.f);
    float3 rayOutPerpendicular = refractionRatio * (direction + cosTheta * normal);
    float ropLengthSqrd = dot(rayOutPerpendicular, rayOutPerpendicular);
    
    float3 rayOutParallel = -sqrt(abs(1.f - ropLengthSqrd)) * normal;
    return rayOutPerpendicular + rayOutParallel;
}

namespace Collision
{
    // Returns distance to hit. -1 if miss
    float RayAndSphere(Ray ray, Sphere sphere)
    {
        float3 rayToSphere = sphere.position - ray.origin;
        float distToClosestPoint = dot(rayToSphere, ray.direction);
        float rayToSphereMagSqrd = dot(rayToSphere, rayToSphere);
        float sphereRadiusSqrd = sphere.radius * sphere.radius;
 
        if (distToClosestPoint < 0.f && rayToSphereMagSqrd > sphereRadiusSqrd)
            return -1.f;
    
        float sideA = rayToSphereMagSqrd - distToClosestPoint * distToClosestPoint;
        if (sideA > sphereRadiusSqrd)
            return -1.f;
    
        float sideB = sqrt(sphereRadiusSqrd - sideA);
        return rayToSphereMagSqrd > sphereRadiusSqrd ? distToClosestPoint - sideB : distToClosestPoint + sideB;
    }
    
    // Returns distance to hit. -1 if miss
    float RayAndTriangle(Ray ray, float3 p0, float3 p1, float3 p2, inout float2 baryUVCoord)
    {
        static const float EPSILON = 0.000001f;

        // Which vectors we use (E1 = (p0 - p2) or (p1 - p0)) will affect the barycentric interpolation later on
        // Doing it this way allows us to pass in the values to barycentric functions nicely
        // (p0 -> p1 -> p2) instead of: (p1 -> p2 -> p0)
        float3 E1 = p0 - p2, E2 = p1 - p2;
        float3 cross1 = cross(ray.direction, E2);
        float determinant = dot(E1, cross1);

	    // ray parallel with triangle
        if (determinant < EPSILON && determinant > -EPSILON)
            return false;

        float inverseDet = 1.f / determinant;

        float3 rayMoved = ray.origin - p2;
        baryUVCoord.x = dot(rayMoved, cross1) * inverseDet;

	    // p is outside the triangle, barycentric coordinates shows: u >= 0
        if (baryUVCoord.x < -EPSILON)
            return -1.f;

        float3 cross2 = cross(rayMoved, E1);
        baryUVCoord.y = dot(ray.direction, cross2) * inverseDet;

	    // p is outside the triangle, barycentric coordinates shows: v >= 0 && u + v <= 1
        if (baryUVCoord.y < -EPSILON || baryUVCoord.x + baryUVCoord.y > 1.0)
            return -1.f;

        float t1 = dot(E2, cross2) * inverseDet;

	    // Triangle behind the ray
        if (t1 < -EPSILON)
            return -1.f;

        return t1;
    }
    
    // Slab Method (Ty ChatGPT)
    bool RayAndAABB(Ray ray, AABB aabb)
    {
        float3 inverseRayDir = 1.f / ray.direction;
        
        float t1 = (aabb.min.x - ray.origin.x) * inverseRayDir.x;
        float t2 = (aabb.max.x - ray.origin.x) * inverseRayDir.x;
        float tmin = min(t1, t2);
        float tmax = max(t1, t2);

        float t3 = (aabb.min.y - ray.origin.y) * inverseRayDir.y;
        float t4 = (aabb.max.y - ray.origin.y) * inverseRayDir.y;
        tmin = max(tmin, min(t3, t4));
        tmax = min(tmax, max(t3, t4));

        float t5 = (aabb.min.z - ray.origin.z) * inverseRayDir.z;
        float t6 = (aabb.max.z - ray.origin.z) * inverseRayDir.z;
        tmin = max(tmin, min(t5, t6));
        tmax = min(tmax, max(t5, t6));

        // If tmax is less than 0, the AABB is behind the ray, so no intersection.
        // If tmin is greater than tmax, there is no intersection.
        return tmax >= 0 && tmin <= tmax;
    }
    
    // Sebastian Lague Way
    float RayAndAABBDist(Ray ray, AABB aabb)
    {
        float3 inverseRayDir = 1.f / ray.direction;
        
        float3 tMin = (aabb.min - ray.origin) * inverseRayDir;
        float3 tMax = (aabb.max - ray.origin) * inverseRayDir;
        float3 t1 = min(tMin, tMax);
        float3 t2 = max(tMin, tMax);

        float distFar = min(min(t2.x, t2.y), t2.z);
        float distNear = max(max(t1.x, t1.y), t1.z);

        bool didHit = distFar >= distNear && distFar > 0.f;
        return didHit ? distNear : FLT_MAX;
    }
}