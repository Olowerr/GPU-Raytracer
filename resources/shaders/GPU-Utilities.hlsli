
#define UINT_MAX (~0u)
#define FLT_MAX (3.402823466e+38F)
#define PI (3.14159265f)

struct Material
{
    float3 albedoColour;

    float3 specularColour;
    float smoothness;
    float specularProbability;

    float3 emissionColour;
    float emissionPower;
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
    float3 p0;
    float3 p1;
    float3 p2;
};

struct MeshComponent
{
    uint triangleStartIdx;
    uint triangleCount;
 
    AABB boundingBox;
    Material material;
};


uint pcg_hash(inout uint seed)
{
    // Ty Cherno
    seed *= 747796405u + 2891336453u;
    seed = ((seed >> ((seed >> 28u) + 4u)) ^ seed) * 277803737u;
    seed = (seed >> 22u) ^ seed;
    return seed;
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

float3 randomInHemisphere(inout uint seed, float3 normal)
{
    const float3 randVector = getRandomVector(seed);
    return randVector * (dot(randVector, normal) > 0.f ? 1.f : -1.f);
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
    
        float sideB = sphereRadiusSqrd - sideA;
        return distToClosestPoint - sqrt(sideB);
    }
    
    // Returns distance to hit. -1 if miss
    float RayAndTriangle(Ray ray, Triangle tri)
    {
        static const float EPSILON = 0.000001f;

        float3 E1 = tri.p1 - tri.p0, E2 = tri.p2 - tri.p0;
        float3 cross1 = cross(ray.direction, E2);
        float determinant = dot(E1, cross1);

	    // ray parallel with triangle
        if (determinant < EPSILON && determinant > -EPSILON)
            return false;

        float inverseDet = 1.f / determinant;

        float3 rayMoved = ray.origin - tri.p0;
        float u = dot(rayMoved, cross1) * inverseDet;

	    // p is outside the triangle, barycentric coordinates shows: u >= 0
        if (u < -EPSILON)
            return -1.f;

        float3 cross2 = cross(rayMoved, E1);
        float v = dot(ray.direction, cross2) * inverseDet;

	    // p is outside the triangle, barycentric coordinates shows: v >= 0 && u + v <= 1
        if (v < -EPSILON || u + v > 1.0)
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
        float t1 = (aabb.min.x - ray.origin.x) / ray.direction.x;
        float t2 = (aabb.max.x - ray.origin.x) / ray.direction.x;
        float tmin = min(t1, t2);
        float tmax = max(t1, t2);

        float t3 = (aabb.min.y - ray.origin.y) / ray.direction.y;
        float t4 = (aabb.max.y - ray.origin.y) / ray.direction.y;
        tmin = max(tmin, min(t3, t4));
        tmax = min(tmax, max(t3, t4));

        float t5 = (aabb.min.z - ray.origin.z) / ray.direction.z;
        float t6 = (aabb.max.z - ray.origin.z) / ray.direction.z;
        tmin = max(tmin, min(t5, t6));
        tmax = min(tmax, max(t5, t6));

        // If tmax is less than 0, the AABB is behind the ray, so no intersection.
        // If tmin is greater than tmax, there is no intersection.
        return tmax >= 0 && tmin <= tmax;
    }
}