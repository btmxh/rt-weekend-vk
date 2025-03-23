#ifndef COMMON_GLSL
#define COMMON_GLSL

float length2(vec3 v) {
    return dot(v, v);
}

const float epsilon = 1e-5;
const float flt_max = 1e5;
const float pos_infinity = 1.0 / 0.0;
const float neg_infinity = -pos_infinity;

struct float_interval {
    float min_value, max_value;
};

bool float_interval_contains(float_interval interval, float value) {
    return value >= interval.min_value && value <= interval.max_value;
}

bool float_interval_surrounds(float_interval interval, float value) {
    return value > interval.min_value && value < interval.max_value;
}

float float_interval_length(float_interval interval) {
    return interval.max_value - interval.min_value;
}

float float_interval_clamp(float_interval interval, float value) {
    return clamp(value, interval.min_value, interval.max_value);
}

const float_interval int_empty = float_interval(pos_infinity, neg_infinity);
const float_interval int_universe = float_interval(neg_infinity, pos_infinity);

#endif
