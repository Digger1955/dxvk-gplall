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

void main() {
  vec2 coord = vec2(p_src_coord0_x, p_src_coord0_y) + 
               vec2(p_src_coord1_x - p_src_coord0_x, p_src_coord1_y - p_src_coord0_y) * i_pos;
               
  ivec2 base_coord = ivec2(floor(coord));

  if (c_resolve_mode == RESOLVE_AVERAGE) {
    uint sample_count = max(1u, c_src_samples / c_dst_samples);
    o_color = vec4(0.0f);

    [[unroll]]
    for (uint i = 0u; i < sample_count; i++) {
      uint sample_index = (gl_SampleID * c_src_samples) / c_dst_samples + i;
      o_color += texelFetch(s_image_ms, ivec3(base_coord, gl_Layer), int(sample_index));
    }
    o_color /= float(sample_count);
    return;
  }

  ivec2 coord_00 = base_coord;
  ivec2 coord_10 = base_coord + ivec2(1, 0);
  ivec2 coord_01 = base_coord + ivec2(0, 1);
  ivec2 coord_11 = base_coord + ivec2(1, 1);

  vec2 f = fract(coord);
  vec4 spatial_weights = vec4(
    (1.0f - f.x) * (1.0f - f.y), // Weight for Top-Left (00)
    f.x * (1.0f - f.y),          // Weight for Top-Right (10)
    (1.0f - f.x) * f.y,          // Weight for Bottom-Left (01)
    f.x * f.y                    // Weight for Bottom-Right (11)
  );

  if (c_resolve_mode == FILTER_NEAREST) {
    spatial_weights = vec4(1.0f, 0.0f, 0.0f, 0.0f);
  }

  vec4 accumulated_color = vec4(0.0f);
  float total_weight = 0.0f;

  [[unroll]]
  for (uint s = 0u; s < c_src_samples; s++) {
    int sample_idx = int(s);

    vec4 c00 = texelFetch(s_image_ms, ivec3(coord_00, gl_Layer), sample_idx);
    vec4 c10 = texelFetch(s_image_ms, ivec3(coord_10, gl_Layer), sample_idx);
    vec4 c01 = texelFetch(s_image_ms, ivec3(coord_01, gl_Layer), sample_idx);
    vec4 c11 = texelFetch(s_image_ms, ivec3(coord_11, gl_Layer), sample_idx);

    vec4 gathered_alphas = vec4(c00.a, c10.a, c01.a, c11.a);

    if (dot(gathered_alphas, gathered_alphas) <= 0.0001f) {
      continue;
    }

    vec4 final_weights = spatial_weights * gathered_alphas;

    accumulated_color += c00 * final_weights.x;
    accumulated_color += c10 * final_weights.y;
    accumulated_color += c01 * final_weights.z;
    accumulated_color += c11 * final_weights.w;

    total_weight += dot(final_weights, vec4(1.0f));
  }

  if (total_weight > 0.0f) {
    o_color = accumulated_color / total_weight;
  } else {
    o_color = texelFetch(s_image_ms, ivec3(base_coord, gl_Layer), 0);
  }
}
