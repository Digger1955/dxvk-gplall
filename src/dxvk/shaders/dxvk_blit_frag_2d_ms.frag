#version 450

layout(set = 0, binding = 0) uniform sampler2DMSArray s_image_ms;

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

/* Sample grid layout for each pixel */
const uvec2 sample_scale[] = {
  uvec2(2u, 1u),
  uvec2(2u, 2u),
  uvec2(4u, 2u),
  uvec2(4u, 4u),
};

/* Order of samples within the grid in row-major order 
*  Emulated 64-bit uint64_t maps using standard 32-bit uvec2 definitions.
* .x holds lower 32 bits (nibbles 0-7), .y holds upper 32 bits (nibbles 8-15).
*/
const uvec2 sample_maps[] = {
  uvec2(0x00000001u, 0x00000000u), // 0x0000000000000001ul
  uvec2(0x00003210u, 0x00000000u), // 0x0000000000003210ul
  uvec2(0x26147035u, 0x00000000u), // 0x0000000026147035ul
  uvec2(0x3714d9afu, 0xe58b602cu), // 0xe58b602c3714d9aful
};

void main() {
  if (c_resolve_mode == RESOLVE_AVERAGE) {
    vec2 coord = vec2(p_src_coord0_x, p_src_coord0_y) + vec2(p_src_coord1_x - p_src_coord0_x, p_src_coord1_y - p_src_coord0_y) * i_pos;
    ivec2 i_coord = ivec2(coord);

    uint sample_count = max(1u, c_src_samples / c_dst_samples);
    o_color = vec4(0.0f);

    for (uint i = 0u; i < sample_count; i++) {
      uint sample_index = (gl_SampleID * c_src_samples) / c_dst_samples + i;
      o_color += texelFetch(s_image_ms, ivec3(i_coord, gl_Layer), int(sample_index));
    }

    o_color /= float(sample_count);
  } else {

    vec2 coord = fma(interpolateAtSample(i_pos, gl_SampleID),
      vec2(p_src_coord1_x - p_src_coord0_x - 1.0f, p_src_coord1_y - p_src_coord0_y - 1.0f),
      vec2(p_src_coord0_x, p_src_coord0_y));

    int lookup_index = max(findLSB(c_src_samples) - 1, 0);

    uvec2 scale = sample_scale[lookup_index];
    uvec2 map = sample_maps[lookup_index];

    coord *= vec2(scale);

    vec2 i_coord = trunc(coord);
    vec2 f_coord = (c_resolve_mode == FILTER_NEAREST) ? vec2(0.0f) : vec2(coord - i_coord);

    o_color = vec4(0.0f);

    for (uint i = 0u; i < ((c_resolve_mode == FILTER_NEAREST) ? 1u : 4u); i++) {
      uvec2 lookup_offset = uvec2(i & 1u, i >> 1u);

      uvec2 sample_coord = uvec2(i_coord + lookup_offset) % scale;
      uvec2 pixel_coord = uvec2(i_coord + lookup_offset) / scale;

      uint sample_index = scale.x * sample_coord.y + sample_coord.x;

      uint extracted_sample = 0u;
      if (sample_index < 8u) {
        extracted_sample = (map.x >> (4u * sample_index)) & 0xfu;
      } else {
        extracted_sample = (map.y >> (4u * (sample_index - 8u))) & 0xfu;
      }

      vec4 color = texelFetch(s_image_ms, ivec3(pixel_coord, gl_Layer), int(extracted_sample));

      vec2 factor = mix(f_coord, 1.0f - f_coord, equal(lookup_offset, uvec2(0u)));

      o_color += color * factor.x * factor.y;
    }
  }
}