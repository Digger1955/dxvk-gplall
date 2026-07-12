#version 450

#extension GL_EXT_control_flow_attributes : enable

layout(set = 0, binding = 0) 
uniform sampler2DMSArray s_image_ms;

layout(location = 0) in vec2 i_pos;
layout(location = 0) out vec4 o_color;

layout(push_constant)
uniform push_block {
  float p_src_coord0_x, p_src_coord0_y, p_src_coord0_z;
  float p_src_coord1_x, p_src_coord1_y, p_src_coord1_z;
  uint p_layer_count;
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
  ivec2 i_coord = ivec2(floor(coord));

  if (c_resolve_mode == RESOLVE_AVERAGE) {
    uint sample_count = max(1u, c_src_samples / c_dst_samples);
    o_color = vec4(0.0f);

    [[unroll]]
    for (uint i = 0u; i < sample_count; i++) {
      uint sample_index = (gl_SampleID * c_src_samples) / c_dst_samples + i;
      o_color += texelFetch(s_image_ms, ivec3(i_coord, gl_Layer), int(sample_index));
    }
    o_color /= float(sample_count);
    return;
  }

  vec4 accumulated_color = vec4(0.0f);
  float total_weight = 0.0f;

  [[unroll]]
  for (uint s = 0u; s < c_src_samples; s++) {
    if (i_coord.x < 0 || i_coord.y < 0) {
      continue; 
    }

    vec4 sample_color = texelFetch(s_image_ms, ivec3(i_coord, gl_Layer), int(s));

    if (sample_color.a <= 0.0001f) {
      continue;
    }

    float weight = 1.0f;
    if (c_resolve_mode == FILTER_LINEAR) {
      weight = sample_color.a; 
    }

    accumulated_color += sample_color * weight;
    total_weight += weight;
  }

  if (total_weight > 0.0f) {
    o_color = accumulated_color / total_weight;
  } else {
    o_color = texelFetch(s_image_ms, ivec3(i_coord, gl_Layer), 0);
  }
}
