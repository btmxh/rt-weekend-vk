#version 460

// a flag for error handling, used in debugging
bool error = false;

#include "geom.glsl"
#include "camera.glsl"
#include "pcg.glsl"
#include "world.glsl"

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(binding = 0, rgba32f) uniform image2D image;

layout(binding = 1, std140) WORLD_UNIFORM RenderData {
    world world;
} data;

vec3 sky_color(const ray r) {
    vec3 unit_dir = length2(r.direction) < epsilon ? vec3(0.0) : normalize(r.direction);
    float y = 0.5 * unit_dir.y + 0.5;
    return mix(vec3(1.0, 1.0, 1.0), vec3(0.5, 0.7, 1.0), y);
}

vec3 ray_trace(ray r) {
    vec3 color = vec3(1.0);

    hit_info closest;
    for (int i = 0; i < 10; ++i) {
        if (ray_hit_world(data.world, r, float_interval(0.01, 1e4), closest)) {
            vec3 attenuation;
            ray scattered;
            if (material_scatter(data.world.materials[closest.material_idx], r, closest, attenuation, scattered)) {
                color *= attenuation;
                r = scattered;
            } else {
                break;
            }
        } else {
            color *= sky_color(r);
            return color;
        }
    }

    return vec3(0.0);
}

vec4 gamma_correct(vec4 color) {
    return pow(color, vec4(1.0 / 2.2));
}

void main() {
    ivec2 i_tex_coords = ivec2(gl_GlobalInvocationID.xy);
    ivec2 i_img_size = imageSize(image);

    camera cam;
    cam.fov = vfov_to_hfov(radians(20.0), float(i_img_size.x) / float(i_img_size.y));
    cam.lookfrom = vec3(13.0, 2.0, 3.0);
    cam.lookat = vec3(0.0, 0.0, 0.0);
    cam.up = vec3(0.0, 1.0, 0.0);
    cam.focus_dist = 10.0;
    cam.defocus_angle = radians(0.6);

    int num_samples = 500;
    vec4 total_color = vec4(0.0);

    pcg_init_seed(i_tex_coords.x + i_tex_coords.y * i_img_size.x);

    for (int i = 0; i < num_samples; ++i) {
        ray cam_ray = get_camera_ray(cam, i_tex_coords, i_img_size, pcg_next_vec2() * 0.5);
        vec4 color = vec4(ray_trace(cam_ray), 1.0);
        total_color += color / float(num_samples);
    }

    total_color = clamp(gamma_correct(total_color), vec4(0.0), vec4(1.0));
    if (error) {
        total_color = vec4(0.0, 1.0, 0.0, 1.0);
    }
    imageStore(image, i_tex_coords, total_color);
}
