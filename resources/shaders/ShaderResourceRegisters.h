
#define NUM_U_REGISTERS 2u
#define NUM_B_REGISTERS 1u
#define NUM_T_REGISTERS 10u


// ---  CPU Slots ---
// t register
#define TRIANGLE_POS_SLOT 0
#define TRIANGLE_INFO_SLOT 1
#define BVH_TREE_SLOT 2
#define TEXTURES_SLOT 3
#define ENVIRONMENT_MAP_SLOT 4

#define SPHERE_DATA_SLOT 5
#define MESH_ENTITY_DATA_SLOT 6
#define DIRECTIONAL_LIGHT_DATA_SLOT 7
#define POINT_LIGHT_DATA_SLOT 8
#define SPOT_LIGHT_DATA_SLOT 9

#define OCT_TREE_CPU_SLOT 10


// b register
#define RENDER_DATA_SLOT 0
#define DBG_RENDER_DATA_SLOT 1

// u register
#define RESULT_BUFFER_SLOT 0
#define ACCUMULATION_BUFFER_SLOT 1


// --- GPU Registers ---
// t register
#define TRIANGLE_POS_GPU_REG t0 // Writing t[RM_TRIANGLE_DATA_SLOT] compiles and runs, but shows "errors" in RaytracerCS.hlsl
#define TRIANGLE_INFO_GPU_REG t1
#define BVH_TREE_GPU_REG t2
#define TEXTURES_GPU_REG t3
#define ENVIRONMENT_MAP_GPU_REG t4

#define SPHERE_DATA_GPU_REG t5
#define MESH_ENTITY_DATA_GPU_REG t6
#define DIRECTIONAL_LIGHT_DATA_GPU_REG t7
#define POINT_LIGHT_DATA_GPU_REG t8
#define SPOT_LIGHT_DATA_GPU_REG t9

#define OCT_TREE_GPU_REG t10

// b register
#define RENDER_DATA_GPU_REG b0
#define DBG_RENDER_DATA_GPU_REG b1

// u register
#define RESULT_BUFFER_GPU_REG u0
#define ACCUMULATION_BUFFER_GPU_REG u1