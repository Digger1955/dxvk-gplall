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

const float ALPHA_SQ_EPS = 0.0001;
const float WEIGHT_EPS   = 0.00001;

// Accumulates one color sample using alpha as the sample weight.
void accumulateSample(
    vec4        color,
    float       spatial_weight,
    inout vec4  accum_color,
    inout float total_weight) {
  if ((color.a * color.a) <= ALPHA_SQ_EPS)
    return;

  float weight = spatial_weight * color.a;

  accum_color += color * weight;
  total_weight += weight;
}

// Dedicated direct-fetch path for one source sample.
void accumulateCoordinateSamples1(
    ivec3      coord_layer,
    float      spatial_weight,
    inout vec4 accum_color,
    inout float total_weight) {

  accumulateSample(texelFetch(s_image_ms, coord_layer, 0), spatial_weight, accum_color, total_weight);

}

// Dedicated direct-fetch path for two source samples.
void accumulateCoordinateSamples2(
    ivec3      coord_layer,
    float      spatial_weight,
    inout vec4 accum_color,
    inout float total_weight) {

  accumulateSample(texelFetch(s_image_ms, coord_layer, 0), spatial_weight, accum_color, total_weight);
  accumulateSample(texelFetch(s_image_ms, coord_layer, 1), spatial_weight, accum_color, total_weight);

}

// Dedicated direct-fetch path for four source samples.
void accumulateCoordinateSamples4(
    ivec3      coord_layer,
    float      spatial_weight,
    inout vec4 accum_color,
    inout float total_weight) {

  accumulateSample(texelFetch(s_image_ms, coord_layer, 0), spatial_weight, accum_color, total_weight);
  accumulateSample(texelFetch(s_image_ms, coord_layer, 1), spatial_weight, accum_color, total_weight);
  accumulateSample(texelFetch(s_image_ms, coord_layer, 2), spatial_weight, accum_color, total_weight);
  accumulateSample(texelFetch(s_image_ms, coord_layer, 3), spatial_weight, accum_color, total_weight);

}

// Dedicated direct-fetch path for eight source samples.
void accumulateCoordinateSamples8(
    ivec3      coord_layer,
    float      spatial_weight,
    inout vec4 accum_color,
    inout float total_weight) {

  accumulateSample(texelFetch(s_image_ms, coord_layer, 0), spatial_weight, accum_color, total_weight);
  accumulateSample(texelFetch(s_image_ms, coord_layer, 1), spatial_weight, accum_color, total_weight);
  accumulateSample(texelFetch(s_image_ms, coord_layer, 2), spatial_weight, accum_color, total_weight);
  accumulateSample(texelFetch(s_image_ms, coord_layer, 3), spatial_weight, accum_color, total_weight);
  accumulateSample(texelFetch(s_image_ms, coord_layer, 4), spatial_weight, accum_color, total_weight);
  accumulateSample(texelFetch(s_image_ms, coord_layer, 5), spatial_weight, accum_color, total_weight);
  accumulateSample(texelFetch(s_image_ms, coord_layer, 6), spatial_weight, accum_color, total_weight);
  accumulateSample(texelFetch(s_image_ms, coord_layer, 7), spatial_weight, accum_color, total_weight);

}

// Generic fallback for source sample counts other than 1, 2, 4, 8.
void accumulateCoordinateSamplesGeneric(
    ivec3      coord_layer,
    float      spatial_weight,
    inout vec4 accum_color,
    inout float total_weight) {
  [[unroll]]
  for (uint sample_idx = 0u; sample_idx < c_src_samples; ++sample_idx) {
    accumulateSample(texelFetch(s_image_ms, coord_layer, int(sample_idx)), spatial_weight, accum_color, total_weight);
  }
}

/*
 * Dispatches to a fixed-count direct-fetch implementation.
 * c_src_samples is a specialization constant. After specialization, the
 * compiler can remove all unused branches and retain only the selected
 * source-sample implementation.
 */
void accumulateCoordinateSamples(
    ivec3      coord_layer,
    float      spatial_weight,
    inout vec4 accum_color,
    inout float total_weight) {
  if (spatial_weight <= WEIGHT_EPS)
    return;

  switch (c_src_samples) {

    case 1u: {
    accumulateCoordinateSamples1(coord_layer, spatial_weight, accum_color, total_weight);
  } break;

    case 2u: {
    accumulateCoordinateSamples2(coord_layer, spatial_weight, accum_color, total_weight);
  } break;

    case 4u: {
    accumulateCoordinateSamples4(coord_layer, spatial_weight, accum_color, total_weight);
  } break;

    case 8u: {
    accumulateCoordinateSamples8(coord_layer, spatial_weight, accum_color, total_weight);
  } break;

    default: {
    accumulateCoordinateSamplesGeneric(coord_layer, spatial_weight, accum_color, total_weight);
  } break;

  }
}

// Accumulates one bilinear coordinate using coordinate-major ordering.
void accumulateLinearCoordinate(
    ivec2      coord,
    int        target_layer,
    float      spatial_weight,
    inout vec4 accum_color,
    inout float total_weight) {
  if (spatial_weight <= WEIGHT_EPS)
    return;

  accumulateCoordinateSamples(ivec3(coord, target_layer), spatial_weight, accum_color, total_weight);

}

/*
 * Accumulates the active bilinear coordinates selected by mask.

 * Mask layout:
 *   bit 0: 00
 *   bit 1: 10
 *   bit 2: 01
 *   bit 3: 11

 * Explicit paths cover all masks:
 *   1  = 00
 *   2  = 10
 *   3  = 00 + 10
 *   4  = 01
 *   5  = 00 + 01
 *   6  = 10 + 01
 *   7  = 00 + 10 + 01
 *   8  = 11
 *   9  = 00 + 11
 *   10 = 10 + 11
 *   11 = 00 + 10 + 11
 *   12 = 01 + 11
 *   13 = 00 + 01 + 11
 *   14 = 10 + 01 + 11
 *   15 = 00 + 10 + 01 + 11
 */

void accumulateLinearSamples(
    ivec2      base_coord,
    int        target_layer,
    uint       mask,
    vec4       spatial_weights,
    inout vec4 accum_color,
    inout float total_weight) {
  switch (mask) {
    case 1u: {
      accumulateLinearCoordinate(base_coord, target_layer, spatial_weights.x, accum_color, total_weight);
    } break;

    case 2u: {
      accumulateLinearCoordinate(base_coord + ivec2(1, 0), target_layer, spatial_weights.y, accum_color, total_weight);
    } break;

    case 3u: {
      accumulateLinearCoordinate(base_coord, target_layer, spatial_weights.x, accum_color, total_weight);
      accumulateLinearCoordinate(base_coord + ivec2(1, 0), target_layer, spatial_weights.y, accum_color, total_weight);
    } break;

    case 4u: {
      accumulateLinearCoordinate(base_coord + ivec2(0, 1), target_layer, spatial_weights.z, accum_color, total_weight);
    } break;

    case 5u: {
      accumulateLinearCoordinate(base_coord, target_layer, spatial_weights.x, accum_color, total_weight);
      accumulateLinearCoordinate(base_coord + ivec2(0, 1), target_layer, spatial_weights.z, accum_color, total_weight);
    } break;

    case 6u: {
      accumulateLinearCoordinate(base_coord + ivec2(1, 0), target_layer, spatial_weights.y, accum_color, total_weight);
      accumulateLinearCoordinate(base_coord + ivec2(0, 1), target_layer, spatial_weights.z, accum_color, total_weight);
    } break;

    case 7u: {
      accumulateLinearCoordinate(base_coord, target_layer, spatial_weights.x, accum_color, total_weight);
      accumulateLinearCoordinate(base_coord + ivec2(1, 0), target_layer, spatial_weights.y, accum_color, total_weight);
      accumulateLinearCoordinate(base_coord + ivec2(0, 1), target_layer, spatial_weights.z, accum_color, total_weight);
    } break;

    case 8u: {
      accumulateLinearCoordinate(base_coord + ivec2(1, 1), target_layer, spatial_weights.w, accum_color, total_weight);
    } break;

    case 9u: {
      accumulateLinearCoordinate(base_coord, target_layer, spatial_weights.x, accum_color, total_weight);
      accumulateLinearCoordinate(base_coord + ivec2(1, 1), target_layer, spatial_weights.w, accum_color, total_weight);
    } break;

    case 10u: {
      accumulateLinearCoordinate(base_coord + ivec2(1, 0), target_layer, spatial_weights.y, accum_color, total_weight);
      accumulateLinearCoordinate(base_coord + ivec2(1, 1), target_layer, spatial_weights.w, accum_color, total_weight);
    } break;

    case 11u: {
      accumulateLinearCoordinate(base_coord, target_layer, spatial_weights.x, accum_color, total_weight);
      accumulateLinearCoordinate(base_coord + ivec2(1, 0), target_layer, spatial_weights.y, accum_color, total_weight);
      accumulateLinearCoordinate(base_coord + ivec2(1, 1), target_layer, spatial_weights.w, accum_color, total_weight);
    } break;

    case 12u: {
      accumulateLinearCoordinate(base_coord + ivec2(0, 1), target_layer, spatial_weights.z, accum_color, total_weight);
      accumulateLinearCoordinate(base_coord + ivec2(1, 1), target_layer, spatial_weights.w, accum_color, total_weight);
    } break;

    case 13u: {
      accumulateLinearCoordinate(base_coord, target_layer, spatial_weights.x, accum_color, total_weight);
      accumulateLinearCoordinate(base_coord + ivec2(0, 1), target_layer, spatial_weights.z, accum_color, total_weight);
      accumulateLinearCoordinate(base_coord + ivec2(1, 1), target_layer, spatial_weights.w, accum_color, total_weight);
    } break;

    case 14u: {
      accumulateLinearCoordinate(base_coord + ivec2(1, 0), target_layer, spatial_weights.y, accum_color, total_weight);
      accumulateLinearCoordinate(base_coord + ivec2(0, 1), target_layer, spatial_weights.z, accum_color, total_weight);
      accumulateLinearCoordinate(base_coord + ivec2(1, 1), target_layer, spatial_weights.w, accum_color, total_weight);
    } break;

    case 15u: {
      accumulateLinearCoordinate(base_coord, target_layer, spatial_weights.x, accum_color, total_weight);
      accumulateLinearCoordinate(base_coord + ivec2(1, 0), target_layer, spatial_weights.y, accum_color, total_weight);
      accumulateLinearCoordinate(base_coord + ivec2(0, 1), target_layer, spatial_weights.z, accum_color, total_weight);
      accumulateLinearCoordinate(base_coord + ivec2(1, 1), target_layer, spatial_weights.w, accum_color, total_weight);
    } break;
  }
}

// Average-resolve helpers for common source-to-destination sample ratios.
vec4 resolveAverage1(
    ivec3 coord_layer,
    uint sample_base) {

  return texelFetch(s_image_ms, coord_layer, int(sample_base));

}

vec4 resolveAverage2(
    ivec3 coord_layer,
    uint sample_base) {

  vec4 accum = vec4(0.0);

  accum += texelFetch(s_image_ms, coord_layer, int(sample_base + 0u));
  accum += texelFetch(s_image_ms, coord_layer, int(sample_base + 1u));

  return accum * 0.5;
}

vec4 resolveAverage4(
    ivec3 coord_layer,
    uint sample_base) {

  vec4 accum = vec4(0.0);

  accum += texelFetch(s_image_ms, coord_layer, int(sample_base + 0u));
  accum += texelFetch(s_image_ms, coord_layer, int(sample_base + 1u));
  accum += texelFetch(s_image_ms, coord_layer, int(sample_base + 2u));
  accum += texelFetch(s_image_ms, coord_layer, int(sample_base + 3u));

  return accum * 0.25;
}

vec4 resolveAverage8(
    ivec3 coord_layer,
    uint sample_base) {

  vec4 accum = vec4(0.0);

  accum += texelFetch(s_image_ms, coord_layer, int(sample_base + 0u));
  accum += texelFetch(s_image_ms, coord_layer, int(sample_base + 1u));
  accum += texelFetch(s_image_ms, coord_layer, int(sample_base + 2u));
  accum += texelFetch(s_image_ms, coord_layer, int(sample_base + 3u));
  accum += texelFetch(s_image_ms, coord_layer, int(sample_base + 4u));
  accum += texelFetch(s_image_ms, coord_layer, int(sample_base + 5u));
  accum += texelFetch(s_image_ms, coord_layer, int(sample_base + 6u));
  accum += texelFetch(s_image_ms, coord_layer, int(sample_base + 7u));

  return accum * 0.125;
}

// Generic average resolve for ratios other than 1, 2, 4, 8.
vec4 resolveAverageGeneric(
    ivec3 coord_layer,
    uint sample_base,
    uint sample_count) {

  vec4 accum = vec4(0.0);

  [[unroll]]
  for (uint i = 0u; i < sample_count; ++i) {
    accum += texelFetch( s_image_ms, coord_layer, int(sample_base + i));
  }

  return accum / float(sample_count);
}

// Resolves common source-to-destination sample ratios with specific functions, others use fallback.
vec4 resolveAverage(
    ivec3 coord_layer) {
  if (c_src_samples == c_dst_samples) {
    return resolveAverage1(coord_layer, uint(gl_SampleID));
  }

  if (c_src_samples == 2u && c_dst_samples == 1u) {
    return resolveAverage2(coord_layer, 0u);
  }

  if (c_src_samples == 4u && c_dst_samples == 1u) {
    return resolveAverage4(coord_layer, 0u);
  }

  if (c_src_samples == 4u && c_dst_samples == 2u) {
    return resolveAverage2(coord_layer, uint(gl_SampleID) * 2u);
  }

  if (c_src_samples == 8u && c_dst_samples == 1u) {
    return resolveAverage8(coord_layer, 0u);
  }

  if (c_src_samples == 8u && c_dst_samples == 2u) {
    return resolveAverage4(coord_layer, uint(gl_SampleID) * 4u);
  }

  if (c_src_samples == 8u && c_dst_samples == 4u) {
    return resolveAverage2(coord_layer, uint(gl_SampleID) * 2u);
  }

  /*
   * Generic fallback
   * Source samples are partitioned into c_dst_samples groups, with each
   * Destination sample receiving c_src_samples / c_dst_samples samples.
   */
  uint sample_count = max(1u, c_src_samples / c_dst_samples);
  uint sample_base = (uint(gl_SampleID) * c_src_samples) / c_dst_samples;

  return resolveAverageGeneric(coord_layer, sample_base, sample_count);
}


void main() {

  vec2 coord = vec2(p_src_coord0_x, p_src_coord0_y) + vec2(p_src_coord1_x - p_src_coord0_x, p_src_coord1_y - p_src_coord0_y) * i_pos;
  ivec2 base_coord = ivec2(floor(coord));
  int target_layer = int(gl_Layer);
  ivec3 coord_00 = ivec3(base_coord, target_layer);

  // Resolve average
  if (c_resolve_mode == RESOLVE_AVERAGE) {
    o_color = resolveAverage(coord_00);
    return;
  }

  // Resolve nearest
  if (c_resolve_mode == FILTER_NEAREST) {
    vec4 accumulated_color = vec4(0.0);
    float total_weight = 0.0;
    accumulateCoordinateSamples(coord_00, 1.0, accumulated_color, total_weight);

    if (total_weight > 0.0) {
      o_color = accumulated_color / total_weight;
    } else {
      o_color = texelFetch(s_image_ms, coord_00, 0);
    }
    return;
  }

  // Resolve linear
  vec2 f = fract(coord);

  bool fx_near_boundary = (f.x <= WEIGHT_EPS) || (f.x >= 1.0 - WEIGHT_EPS);
  bool fy_near_boundary = (f.y <= WEIGHT_EPS) || (f.y >= 1.0 - WEIGHT_EPS);

  vec4 spatial_weights;

  if (fx_near_boundary || fy_near_boundary) {
  // Clamp only axes sufficiently close to exact boundaries. The other interpolation axis remains intact.
    float fx = (f.x <= WEIGHT_EPS) ? 0.0 : (f.x >= 1.0 - WEIGHT_EPS ? 1.0 : f.x);
    float fy = (f.y <= WEIGHT_EPS) ? 0.0 : (f.y >= 1.0 - WEIGHT_EPS ? 1.0 : f.y);

    spatial_weights = vec4(
      (1.0 - fx) * (1.0 - fy),  // Weight for Top-Left (00)
      fx         * (1.0 - fy),  // Weight for Top-Right (10)
      (1.0 - fx) * fy,          // Weight for Bottom-Left (01)
      fx         * fy);         // Weight for Bottom-Right (11)
  } else {
    spatial_weights = vec4(
      (1.0 - f.x) * (1.0 - f.y), // Weight for Top-Left (00)
      f.x         * (1.0 - f.y), // Weight for Top-Right (10)
      (1.0 - f.x) * f.y,         // Weight for Bottom-Left (01)
      f.x         * f.y);        // Weight for Bottom-Right (11)
  }

// Build the active-coordinate mask before fetching source samples.
  uint mask = 0u;

  if (spatial_weights.x > WEIGHT_EPS)
    mask |= 1u;

  if (spatial_weights.y > WEIGHT_EPS)
    mask |= 2u;

  if (spatial_weights.z > WEIGHT_EPS)
    mask |= 4u;

  if (spatial_weights.w > WEIGHT_EPS)
    mask |= 8u;

  vec4 accumulated_color = vec4(0.0);
  float total_weight = 0.0;

  accumulateLinearSamples(base_coord, target_layer, mask, spatial_weights, accumulated_color, total_weight);

  if (total_weight > 0.0) {
    o_color = accumulated_color / total_weight;
  } else {
    // Fallback to coordinate 00, sample zero.
    o_color = texelFetch(s_image_ms, coord_00, 0);
  }
}
