#ifndef PCG_GLSL
#define PCG_GLSL

const float PI = radians(180.0);

uint seed;

uint pcg(uint v) {
    uint state = v * uint(747796405) + uint(2891336453);
    uint word = ((state >> ((state >> uint(28)) + uint(4))) ^ state) * uint(277803737);
    return (word >> uint(22)) ^ word;
}

void pcg_init_seed(uint s) {
    seed = pcg(s);
}

uint pcg_next() {
    seed = pcg(seed);
    return seed;
}

float pcg_next_float() {
    float flt_01 = float(pcg_next()) / float(uint(0xffffffff));
    return flt_01 * 2.0 - 1.0;
}

vec2 pcg_next_vec2() {
    return vec2(pcg_next_float(), pcg_next_float());
}

vec3 pcg_next_vec3() {
    return vec3(pcg_next_float(), pcg_next_float(), pcg_next_float());
}

vec4 pcg_next_vec4() {
    return vec4(pcg_next_float(), pcg_next_float(), pcg_next_float(), pcg_next_float());
}

vec3 pcg_next_unit_vec3() {
    float theta = pcg_next_float() * PI * 2.0;
    float phi = pcg_next_float() * PI * 2.0;
    return vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
}

vec3 pcg_next_unit_vec3_hemisphere(const vec3 normal) {
    vec3 v = pcg_next_unit_vec3();
    return dot(v, normal) > 0.0 ? v : -v;
}

vec2 pcg_next_unit_vec2() {
    float theta = pcg_next_float() * PI * 2.0;
    return vec2(cos(theta), sin(theta));
}

#endif
