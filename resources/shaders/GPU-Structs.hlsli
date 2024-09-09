
#define UINT_MAX (~0u)
#define FLT_MAX (3.402823466e+38F)
#define PI (3.14159265f)
#define AIR_REFRACTION_INDEX (1.f) // Approx

struct MaterialColour3
{
    float3 colour;
    uint textureIdx;
    
    static MaterialColour3 create()
    {
        MaterialColour3 materialColour3;
        materialColour3.colour = float3(0.f, 0.f, 0.f);
        materialColour3.textureIdx = UINT_MAX;

        return materialColour3;
    }
};

struct MaterialColour1
{
    float colour;
    uint textureIdx;
        
    static MaterialColour1 create()
    {
        MaterialColour1 materialColour1;
        materialColour1.colour = 0.f;
        materialColour1.textureIdx = UINT_MAX;

        return materialColour1;
    }
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
    
    static Material create()
    {
        Material material;
   
        material.albedo = MaterialColour3::create();
        material.roughness = MaterialColour1::create();
        material.metallic = MaterialColour1::create();
        material.specular = MaterialColour1::create();
    
        material.normalMapIdx = UINT_MAX;
    
        material.emissionColour = float3(0.f, 0.f, 0.f);
        material.emissionPower = 0.f;
    
        material.transparency = 0.f;
        material.indexOfRefraction = 0.f;
        
        return material;
    }
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
    
    float effectiveAngle;
    
    float3 direction;
};

struct PointLight
{
    float3 colour;
    float intensity;
    
    float radius;
    
    float3 position;
};

struct SpotLight
{
    float3 colour;
    float intensity;
    
    float radius;
    float maxAngle;
    
    float3 position;
    float3 direction;
};

struct BvhNode
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

    uint firstChildIdx;
    uint numChildren;
};

struct DBGRenderData
{
    float4x4 camViewProjMatrix;
    float4x4 objectWorldMatrix;
    uint vertStartIdx;
    uint nodeIdx;
    float2 pad0;
    MaterialColour3 albedo;
    float3 cameraDir;
    float pad1;
    float3 cameraPos;
    uint mode;
};
