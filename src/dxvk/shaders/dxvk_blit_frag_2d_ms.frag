#version 450

layout(set = 1, binding = 0) uniform sampler2DMSArray s_image_ms;

layout(location = 0) sample in vec2 i_pos;
layout(location = 0) out vec4 o_color;

layout(push_constant)
uniform push_block {
  float p_src_coord0_x, p_src_coord0_y, p_src_coord0_z;
  float p_src_coord1_x, p_src_coord1_y, p_src_coord1_z;
  uint p_layer_count;
};

layout(constant_id = 0) const int c_samples = 1;
layout(constant_id = 1) const bool c_point_filter = false;

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
  vec2 coord = vec2(p_src_coord0_x, p_src_coord0_y) + vec2(p_src_coord1_x - p_src_coord0_x - 1.0f, p_src_coord1_y - p_src_coord0_y - 1.0f) * i_pos;

  int lookup_index = max(findLSB(c_samples) - 1, 0);

  uvec2 scale = sample_scale[lookup_index];
  uvec2 map = sample_maps[lookup_index];

  coord *= vec2(scale);

  vec2 i_coord = trunc(coord);
  vec2 f_coord = c_point_filter ? vec2(0.0f) : vec2(coord - i_coord);

  o_color = vec4(0.0f);

  for (uint i = 0u; i < (c_point_filter ? 1u : 4u); i++) {
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

  o_color = o_color;
}