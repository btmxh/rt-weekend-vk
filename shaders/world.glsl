#ifndef WORLD_GLSL
#define WORLD_GLSL

#include "common.glsl"
#include "geom.glsl"
#include "material.glsl"

#define NUM_SPHERES 512
#define NUM_MATERIALS NUM_SPHERES

#define SIZEOF_WORLD (NUM_MATERIALS * 16 + NUM_SPHERES * 20 + 8)
#if SIZEOF_WORLD <= (16 << 10)
#define WORLD_UNIFORM uniform
#else
#define WORLD_UNIFORM buffer
#endif

struct world {
    material materials[NUM_MATERIALS];
    sphere spheres[NUM_SPHERES];
    ivec4 sphere_mat[NUM_SPHERES >> 2];
    int num_spheres, num_materials;
};

bool ray_hit_world(
    const world w,
    const ray ray,
    float_interval t_int,
    out hit_info info
) {
    bool hit = false;
    for (int i = 0; i < w.num_spheres; ++i) {
        hit_info local_info;
        if (ray_hit_sphere(ray, w.spheres[i], t_int, local_info)) {
            local_info.material_idx = w.sphere_mat[i >> 2][i&3];
            info = local_info;
            t_int.max_value = min(t_int.max_value, info.t);
            hit = true;
        }
    }

    return hit;
}

#endif
