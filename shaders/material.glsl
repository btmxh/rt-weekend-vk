#ifndef MATERIAL_GLSL
#define MATERIAL_GLSL

#include "geom.glsl"
#include "pcg.glsl"

struct material {
    // the last two bits of data.w determines the material kind
    //
    // material specific data:
    // data.rgb: albedo for lambert, metal
    // first 30 bits of data.w is the metal fuzz
    // for dielectric, data.r is the index of refraction
    vec4 data;
};

#define material_kind_lambert 0
#define material_kind_dielectric 1
#define material_kind_metal 2

bool material_scatter_lambert(
    const vec3 albedo,
    const ray r,
    const hit_info info,
    out vec3 attenuation,
    out ray scattered
) {
    vec3 scatter_dir = info.normal + pcg_next_unit_vec3_hemisphere(info.normal);
    if (length2(scatter_dir) < epsilon) {
        scatter_dir = info.normal;
    }
    scattered = ray(info.point, scatter_dir);
    attenuation = albedo;
    return true;
}

bool material_scatter_dielectric(
    const float index_of_refraction,
    const ray r,
    const hit_info info,
    out vec3 attenuation,
    out ray scattered
) {
    float rr = info.front ? (1.0 / index_of_refraction) : index_of_refraction;
    vec3 unit_dir = normalize(r.direction);
    float cos_theta = min(1.0, -dot(unit_dir, info.normal));
    float sin_theta = sqrt(1.0 - cos_theta * cos_theta);

    float reflectance = (1.0 - rr) / (1.0 + rr);
    reflectance = reflectance * reflectance;
    reflectance = mix(1.0, pow(1.0 - cos_theta, 5.0), reflectance);

    scattered.origin = info.point;
    if (rr * sin_theta > 1.0 || reflectance < pcg_next_float()) {
        scattered.direction = reflect(r.direction, info.normal);
    } else {
        scattered.direction = refract(unit_dir, info.normal, rr);
    }

    attenuation = vec3(1.0);
    return true;
}

bool material_scatter_metal(
    const vec3 albedo,
    const float fuzz,
    const ray r,
    const hit_info info,
    out vec3 attenuation,
    out ray scattered
) {
    vec3 reflected = reflect(r.direction, info.normal);
    reflected = normalize(reflected) + fuzz * pcg_next_unit_vec3();
    scattered = ray(info.point, reflected);
    attenuation = albedo;
    return dot(reflected, info.normal) > 0.0;
}

bool material_scatter(
    const material mat,
    const ray r,
    const hit_info info,
    out vec3 attenuation,
    out ray scattered
) {
    uint w = uint(floatBitsToInt(mat.data.w));
    switch (w & 3) {
        case material_kind_lambert:
        return material_scatter_lambert(mat.data.xyz, r, info, attenuation, scattered);
        case material_kind_dielectric:
        return material_scatter_dielectric(mat.data.x, r, info, attenuation, scattered);
        case material_kind_metal:
        float fuzz = float(w >> 2) / float(0x3fffffff);
        return material_scatter_metal(mat.data.xyz, fuzz, r, info, attenuation, scattered);
    }
    return false;
}

#endif
