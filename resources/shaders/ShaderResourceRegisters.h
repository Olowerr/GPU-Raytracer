
#define NUM_U_REGISTERS 2u
#define NUM_B_REGISTERS 1u
#define NUM_T_REGISTERS 7u


// ---  CPU Slots ---
// t register
#define RM_TRIANGLE_DATA_SLOT 0
#define RM_BVH_TREE_SLOT 1
#define RM_TEXTURE_ATLAS_SLOT 2
#define RM_TEXTURE_ATLAS_DESC_SLOT 3
#define RM_ENVIRONMENT_MAP_SLOT 4

#define RT_SPHERE_DATA_SLOT 5
#define RT_MESH_ENTITY_DATA_SLOT 6

// b register
#define RT_RENDER_DATA_SLOT 0

// u register
#define RT_RESULT_BUFFER_SLOT 0
#define RT_ACCUMULATION_BUFFER_SLOT 1


// --- GPU Registers ---
// t register
#define RM_TRIANGLE_DATA_GPU_REG t0 // Writing t[RM_TRIANGLE_DATA_SLOT] compiles and runs, but shows "errors" in RaytracerCS.hlsl
#define RM_BVH_TREE_GPU_REG t1
#define RM_TEXTURE_ATLAS_GPU_REG t2
#define RM_TEXTURE_ATLAS_DESC_GPU_REG t3
#define RM_ENVIRONMENT_MAP_GPU_REG t4

#define RT_SPHERE_DATA_GPU_REG t5
#define RT_MESH_ENTITY_DATA_GPU_REG t6

// b register
#define RT_RENDER_DATA_GPU_REG b0

// u register
#define RT_RESULT_BUFFER_GPU_REG u0
#define RT_ACCUMULATION_BUFFER_GPU_REG u1