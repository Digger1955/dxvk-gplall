#version 450

#extension GL_EXT_control_flow_attributes : enable

layout(set = 0, binding = 0)
uniform sampler2DMSArray s_image_ms;

layout(location = 0) in vec2 i_pos;
layout(location = 0) out vec4 o_color;

layout(push_constant)
uniform push_block {
  float p_src_coord0_x, p_src_coord0_y, p_src_coord0_z;
  uint  p_pad1;
  float p_src_coord1_x, p_src_coord1_y, p_src_coord1_z;
  uint  p_layer_count;
};

#define FILTER_NEAREST  (0u)
#define FILTER_LINEAR   (1u)
#define RESOLVE_AVERAGE (2u)

layout(constant_id = 0) const uint c_src_samples = 1;
layout(constant_id = 1) const uint c_dst_samples = 1;
layout(constant_id = 2) const uint c_resolve_mode = FILTER_LINEAR;

const float ALPHA_SQ_EPS = 0.0001; // 1e-4

const float WEIGHT_EPS = 1e-5;

void main() {
  vec2 coord = vec2(p_src_coord0_x, p_src_coord0_y)
             + vec2(p_src_coord1_x - p_src_coord0_x, p_src_coord1_y - p_src_coord0_y) * i_pos;

  ivec2 base_coord = ivec2(floor(coord));

  if (c_resolve_mode == RESOLVE_AVERAGE) {
    uint sample_count = max(1u, c_src_samples / c_dst_samples);
    uint sample_base  = (uint(gl_SampleID) * c_src_samples) / c_dst_samples;

    vec4 accum = vec4(0.0);
    [[unroll]]
    for (uint i = 0u; i < sample_count; ++i) {
      uint sample_index = sample_base + i;
      accum += texelFetch(s_image_ms, ivec3(base_coord, gl_Layer), int(sample_index));
    }

    o_color = accum / float(sample_count);
    return;
  }

  ivec2 coord_00 = base_coord;
  ivec2 coord_10 = base_coord + ivec2(1, 0);
  ivec2 coord_01 = base_coord + ivec2(0, 1);
  ivec2 coord_11 = base_coord + ivec2(1, 1);

  vec2 f = fract(coord);

  vec4 spatial_weights_full = vec4(
    (1.0 - f.x) * (1.0 - f.y), // Weight for Top-Left (00)
    f.x         * (1.0 - f.y), // Weight for Top-Right (10)
    (1.0 - f.x) * f.y,         // Weight for Bottom-Left (01)
    f.x         * f.y          // Weight for Bottom-Right (11)
  );

  bool fx_near_zero = (f.x <= WEIGHT_EPS) || (f.x >= 1.0 - WEIGHT_EPS);
  bool fy_near_zero = (f.y <= WEIGHT_EPS) || (f.y >= 1.0 - WEIGHT_EPS);
  bool onBoundary   = (fx_near_zero || fy_near_zero);

  if (c_resolve_mode == FILTER_NEAREST) {
    vec4 accumulated_color = vec4(0.0);
    float total_weight = 0.0;

    [[unroll]]
    for (uint s = 0u; s < c_src_samples; ++s) {
      int sample_idx = int(s);

      vec4 c00 = texelFetch(s_image_ms, ivec3(coord_00, gl_Layer), sample_idx);

      float alpha_sq_sum = c00.a * c00.a;
      if (alpha_sq_sum <= ALPHA_SQ_EPS) continue;

      float w00 = 1.0 * c00.a;
      accumulated_color += c00 * w00;
      total_weight += w00;
    }

    if (total_weight > 0.0) {
      o_color = accumulated_color / total_weight;
    } else {
      o_color = texelFetch(s_image_ms, ivec3(base_coord, gl_Layer), 0);
    }
    return;
  }

  vec4 spatial_weights;
  if (onBoundary) {
    float fx = f.x;
    float fy = f.y;
    float fx_c = (fx <= WEIGHT_EPS) ? 0.0 : (fx >= 1.0 - WEIGHT_EPS ? 1.0 : fx);
    float fy_c = (fy <= WEIGHT_EPS) ? 0.0 : (fy >= 1.0 - WEIGHT_EPS ? 1.0 : fy);

    spatial_weights = vec4(
      (1.0 - fx_c) * (1.0 - fy_c), // Weight for Top-Left (00)
      fx_c         * (1.0 - fy_c), // Weight for Top-Right (10)
      (1.0 - fx_c) * fy_c,         // Weight for Bottom-Left (01)
      fx_c         * fy_c          // Weight for Bottom-Right (11)
    );
  } else {
    spatial_weights = spatial_weights_full;
  }

  // Build fetch mask once per fragment (bit0 = 00, bit1 = 10, bit2 = 01, bit3 = 11)
  uint mask = 0u;
  if (spatial_weights.x > WEIGHT_EPS) mask |= 1u;
  if (spatial_weights.y > WEIGHT_EPS) mask |= 2u;
  if (spatial_weights.z > WEIGHT_EPS) mask |= 4u;
  if (spatial_weights.w > WEIGHT_EPS) mask |= 8u;

  vec4 accumulated_color = vec4(0.0);
  float total_weight = 0.0;

  // Specialized handling for the common masks: {1,3,5}
  // - 1: only top-left (00)
  // - 3: top row (00 + 10)
  // - 5: left column (00 + 01)
  // Default: fallback to full-4 fetch inner loop which covers mask==15 and others.
  switch (mask) {
    case 1u: { // 00 (top-left)
      [[unroll]]
      for (uint s = 0u; s < c_src_samples; ++s) {
        int sample_idx = int(s);
        vec4 c00 = texelFetch(s_image_ms, ivec3(coord_00, gl_Layer), sample_idx);
        float alpha_sq_sum = c00.a * c00.a;
        if (alpha_sq_sum <= ALPHA_SQ_EPS) continue;
        float w00 = spatial_weights.x * c00.a;
        accumulated_color += c00 * w00;
        total_weight += w00;
      }
    } break;

    case 3u: { // 00 + 10 (top row)
      [[unroll]]
      for (uint s = 0u; s < c_src_samples; ++s) {
        int sample_idx = int(s);
        vec4 c00 = texelFetch(s_image_ms, ivec3(coord_00, gl_Layer), sample_idx);
        vec4 c10 = texelFetch(s_image_ms, ivec3(coord_10, gl_Layer), sample_idx);
        float alpha_sq_sum = c00.a*c00.a + c10.a*c10.a;
        if (alpha_sq_sum <= ALPHA_SQ_EPS) continue;
        float w00 = spatial_weights.x * c00.a;
        float w10 = spatial_weights.y * c10.a;
        accumulated_color += c00 * w00;
        accumulated_color += c10 * w10;
        total_weight += (w00 + w10);
      }
    } break;

    case 5u: { // 00 + 01 (left column)
      [[unroll]]
      for (uint s = 0u; s < c_src_samples; ++s) {
        int sample_idx = int(s);
        vec4 c00 = texelFetch(s_image_ms, ivec3(coord_00, gl_Layer), sample_idx);
        vec4 c01 = texelFetch(s_image_ms, ivec3(coord_01, gl_Layer), sample_idx);
        float alpha_sq_sum = c00.a*c00.a + c01.a*c01.a;
        if (alpha_sq_sum <= ALPHA_SQ_EPS) continue;
        float w00 = spatial_weights.x * c00.a;
        float w01 = spatial_weights.z * c01.a;
        accumulated_color += c00 * w00;
        accumulated_color += c01 * w01;
        total_weight += (w00 + w01);
      }
    } break;

    default: { // fallback to full-4 fetch inner loop which covers mask==15 and others.
      [[unroll]]
      for (uint s = 0u; s < c_src_samples; ++s) {
        int sample_idx = int(s);
        vec4 c00 = texelFetch(s_image_ms, ivec3(coord_00, gl_Layer), sample_idx);
        vec4 c10 = texelFetch(s_image_ms, ivec3(coord_10, gl_Layer), sample_idx);
        vec4 c01 = texelFetch(s_image_ms, ivec3(coord_01, gl_Layer), sample_idx);
        vec4 c11 = texelFetch(s_image_ms, ivec3(coord_11, gl_Layer), sample_idx);
        float alpha_sq_sum = c00.a*c00.a + c10.a*c10.a + c01.a*c01.a + c11.a*c11.a;
        if (alpha_sq_sum <= ALPHA_SQ_EPS) continue;
        float w00 = spatial_weights.x * c00.a;
        float w10 = spatial_weights.y * c10.a;
        float w01 = spatial_weights.z * c01.a;
        float w11 = spatial_weights.w * c11.a;
        accumulated_color += c00 * w00;
        accumulated_color += c10 * w10;
        accumulated_color += c01 * w01;
        accumulated_color += c11 * w11;
        total_weight += (w00 + w10 + w01 + w11);
      }
    } break;
  }

  if (total_weight > 0.0) {
    o_color = accumulated_color / total_weight;
  } else {
    o_color = texelFetch(s_image_ms, ivec3(base_coord, gl_Layer), 0);
  }
}
