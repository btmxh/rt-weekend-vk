#ifndef CAMERA_GLSL
#define CAMERA_GLSL

#include "pcg.glsl"

struct camera {
    vec3 lookfrom;
    float fov;
    vec3 lookat;
    float defocus_angle;
    vec3 up;
    float focus_dist;
};

float vfov_to_hfov(float vfov, float aspect_ratio) {
    return 2.0 * atan(tan(vfov * 0.5) * aspect_ratio);
}

ray get_camera_ray(const camera cam, ivec2 i_tex_coords, ivec2 i_img_size, vec2 aa_offset) {
    vec2 tex_coords = vec2(i_tex_coords);
    vec2 img_size = vec2(i_img_size);
    tex_coords.y = img_size.y - 1.0 - tex_coords.y;

    vec3 lookdir = normalize(cam.lookat - cam.lookfrom);
    vec3 u = normalize(cross(lookdir, cam.up));
    vec3 v = cross(u, lookdir);

    // simple camera: up vector is yhat, camera is positioned at (0,0,0), looking along -zhat
    const float viewport_width = 2.0 * tan(cam.fov * 0.5);
    const float viewport_height = viewport_width * img_size.y / img_size.x;
    const vec2 viewport_size = vec2(viewport_width, viewport_height);
    const vec2 delta_xy = viewport_size / img_size;
    vec3 coeffs =
        vec3(
            delta_xy * (vec2(tex_coords) + aa_offset - 0.5 * (img_size - 1.0)),
            1.0
        );

    // map from the simple coordinate system back to the one formed by u, v and lookdir
    vec3 look_point = cam.lookfrom + mat3(u, v, lookdir) * coeffs * cam.focus_dist;
    vec2 disk_vec = pcg_next_float() * pcg_next_unit_vec2();
    vec2 defocus_coeffs = disk_vec * cam.focus_dist * tan(cam.defocus_angle * 0.5);
    vec3 origin = cam.lookfrom + mat2x3(u, v) * defocus_coeffs;
    vec3 direction = normalize(look_point - origin);
    return ray(origin, direction);
    // return ray(cam.lookfrom, normalize(mat3(u, v, lookdir) * coeffs));
}

#endif
