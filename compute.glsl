#version 460

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(binding = 0, rgba32f) uniform image2D image;

void main() {
    ivec2 tex_coords = ivec2(gl_GlobalInvocationID.xy);
    vec4 color = imageLoad(image, tex_coords);
    color.b = 0.5;
    imageStore(image, tex_coords, color);
}
