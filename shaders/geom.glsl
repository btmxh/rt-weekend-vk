#ifndef GEOM_GLSL
#define GEOM_GLSL

#include "common.glsl"

struct ray {
    vec3 origin;
    vec3 direction;
};

vec3 ray_at(const ray r, float t) {
    return r.origin + t * r.direction;
}

struct sphere {
    vec4 centerAndRadius;
};

struct hit_info {
    vec3 point;
    float t;
    vec3 normal;
    bool front;
    int material_idx;
};

bool ray_hit_sphere(
    const ray r,
    const sphere sph,
    const float_interval t_int,
    out hit_info info
) {
    vec3 sph_center = sph.centerAndRadius.xyz;
    float sph_radius = sph.centerAndRadius.w;
    float sph_radius2 = sph_radius * sph_radius;
    vec3 sph_center_sub_origin = sph_center - r.origin;

    // equation: |t * direction - sph_center_sub_origin|^2 = sph_radius2
    // t^2 dir^2 - 2t dir sph_center_sub_origin + sph_center^2 - sph_radius2 = 0
    float a = length2(r.direction);
    float b_prime = -dot(r.direction, sph_center_sub_origin);
    float c = length2(sph_center_sub_origin) - sph_radius2;
    float discriminant = b_prime * b_prime - a * c;
    if (discriminant < 0.0) {
        return false;
    }

    float sqrt_discriminant = sqrt(discriminant);
    // float t_1 = (-b_prime - sqrt_discriminant) / a;
    // float t_2 = (-b_prime + sqrt_discriminant) / a;
    // t_1 = float_interval_contains(t_int, t_1) ? t_1 : pos_infinity;
    // t_2 = float_interval_contains(t_int, t_2) ? t_2 : pos_infinity;

    float t = (-b_prime - sqrt_discriminant) / a;
    if (!float_interval_contains(t_int, t)) {
        t = (-b_prime + sqrt_discriminant) / a;
        if (!float_interval_contains(t_int, t)) {
            return false;
        }
    }

    info.t = t;
    info.point = ray_at(r, info.t);
    info.normal = (info.point - sph_center) / sph_radius;
    info.front = dot(r.direction, info.normal) < 0.0;
    info.normal *= info.front ? 1.0 : -1.0;

    return true;
}

#endif
