#version 430 core
layout(vertices=4) out;

uniform mat4 model;
uniform mat4 view;

// legacy, ignored for tess levels (TES doesn't use it)
uniform vec2 worldOffset;

// >>> match TES <<<
uniform vec2 chunk_scale_xz = vec2(1.0);
uniform vec2 chunk_offset_xz = vec2(0.0);

uniform float min_distance = 2.0;
uniform float max_distance = 16.0;

in  vec2 TexCoord[];
out vec2 TextureCoord[];
out vec3 debug_color[];

void main()
{
    // pass-through control points & UVs
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    TextureCoord[gl_InvocationID]       = TexCoord[gl_InvocationID];

    // ---- Build TES-equivalent world-space positions for distance ----
    // Apply the SAME affine as in TES: scale X/Z, then translate X/Z.
    vec4 P0 = gl_in[0].gl_Position;
    vec4 P1 = gl_in[1].gl_Position;
    vec4 P2 = gl_in[2].gl_Position;
    vec4 P3 = gl_in[3].gl_Position;

    // non-uniform X/Z scale
    P0.x *= chunk_scale_xz.x;  P0.z *= chunk_scale_xz.y;
    P1.x *= chunk_scale_xz.x;  P1.z *= chunk_scale_xz.y;
    P2.x *= chunk_scale_xz.x;  P2.z *= chunk_scale_xz.y;
    P3.x *= chunk_scale_xz.x;  P3.z *= chunk_scale_xz.y;

    // per-chunk translation (world placement)
    P0.x += chunk_offset_xz.x; P0.z += chunk_offset_xz.y;
    P1.x += chunk_offset_xz.x; P1.z += chunk_offset_xz.y;
    P2.x += chunk_offset_xz.x; P2.z += chunk_offset_xz.y;
    P3.x += chunk_offset_xz.x; P3.z += chunk_offset_xz.y;

    // view-space positions
    vec4 V0 = view * (model * P0);
    vec4 V1 = view * (model * P1);
    vec4 V2 = view * (model * P2);
    vec4 V3 = view * (model * P3);

    // robust distance → 0..1 falloff
    float denom = max(1e-6, (max_distance - min_distance));
    float d00 = clamp((length(V0.xyz) - min_distance) / denom, 0.0, 1.0);
    float d01 = clamp((length(V1.xyz) - min_distance) / denom, 0.0, 1.0);
    float d10 = clamp((length(V2.xyz) - min_distance) / denom, 0.0, 1.0);
    float d11 = clamp((length(V3.xyz) - min_distance) / denom, 0.0, 1.0);

    // edge tess factors (use min of the two endpoints on each edge to avoid cracks)
    const int MIN_TESS_LEVEL = 1;
    const int MAX_TESS_LEVEL = 16;

    float t0 = mix(float(MAX_TESS_LEVEL), float(MIN_TESS_LEVEL), min(d10, d00)); // outer[0]
    float t1 = mix(float(MAX_TESS_LEVEL), float(MIN_TESS_LEVEL), min(d00, d01)); // outer[1]
    float t2 = mix(float(MAX_TESS_LEVEL), float(MIN_TESS_LEVEL), min(d01, d11)); // outer[2]
    float t3 = mix(float(MAX_TESS_LEVEL), float(MIN_TESS_LEVEL), min(d11, d10)); // outer[3]

    if (gl_InvocationID == 0) {
        gl_TessLevelOuter[0] = t0;
        gl_TessLevelOuter[1] = t1;
        gl_TessLevelOuter[2] = t2;
        gl_TessLevelOuter[3] = t3;

        // inner levels: conservative (use max of adjacent outers)
        gl_TessLevelInner[0] = max(t1, t3);
        gl_TessLevelInner[1] = max(t0, t2);
    }

    // simple debug color per vertex (recompute from the same t's)
    float av_inner = 0.5 * (max(t1, t3) + max(t0, t2));
    if      (av_inner <  2.0) debug_color[gl_InvocationID] = vec3(1,1,0);
    else if (av_inner <  4.0) debug_color[gl_InvocationID] = vec3(0,1,0);
    else if (av_inner <  8.0) debug_color[gl_InvocationID] = vec3(0,0,1);
    else                      debug_color[gl_InvocationID] = vec3(1,0,0);
}

