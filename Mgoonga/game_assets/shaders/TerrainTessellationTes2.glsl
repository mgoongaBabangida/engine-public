#version 430 core
layout(quads, equal_spacing, cw) in;

layout(binding=2) uniform sampler2D heightMap;

uniform mat4 model, view, projection;
uniform vec2 worldOffset; // legacy

uniform vec2 chunk_scale_xz = vec2(1.0);
uniform vec2 chunk_offset_xz = vec2(0.0);

uniform float height_scale = 1.75;
uniform float max_height = 1.75;
uniform float min_height = 0.0;
uniform float heightMapResolution = 1024.0;

in  vec2 TextureCoord[];
in  vec3 debug_color[];

out float Height;
out vec2 texCoord;
out vec4 LocalSpacePos;
out vec3 thePositionWorld;
out mat3 TBN;
out vec3 tes_debug_color;

void main() 
{
    float u = gl_TessCoord.x;
    float v = gl_TessCoord.y;

    // Interpolate UVs (unchanged)
    vec2 t00 = TextureCoord[0];
    vec2 t01 = TextureCoord[1];
    vec2 t10 = TextureCoord[2];
    vec2 t11 = TextureCoord[3];

    vec2 t0 = mix(t00, t01, u);
    vec2 t1 = mix(t10, t11, u);
    texCoord = mix(t0, t1, v);

    // Base control points & bilinear position (unchanged)
    vec4 p00 = gl_in[0].gl_Position;
    vec4 p01 = gl_in[1].gl_Position;
    vec4 p10 = gl_in[2].gl_Position;
    vec4 p11 = gl_in[3].gl_Position;

    vec4 p0 = mix(p00, p01, u);
    vec4 p1 = mix(p10, p11, u);
    vec4 p  = mix(p0,  p1,  v);

    // --- Half-texel inset to avoid ever sampling the outermost texel ---
    float texel = 1.0 / heightMapResolution;      // assumes width==height (1024)
    float pad   = 0.5 * texel;                    // half-texel inset
    vec2  uvSamp = clamp(texCoord, vec2(pad), vec2(1.0 - pad));

    // Height (center sample uses inset UV)
    Height = texture(heightMap, uvSamp).r * height_scale;
    Height = clamp(Height, min_height, max_height);
    p.y += Height;

    // ===== Chain-rule geometric derivatives (unchanged math) =====
    // 1) Geometric partials of the base plane
    vec4 dPdu4 = mix(p01 - p00, p11 - p10, v);
    vec4 dPdv4 = mix(p10 - p00, p11 - p01, u);

    // 2) UV derivatives for this patch
    vec2 dTdu = mix(t01 - t00, t11 - t10, v);
    vec2 dTdv = mix(t10 - t00, t11 - t01, u);

    // 3) Height gradient in UV space (central diff, also inset & clamped)
    vec2 uvL = clamp(uvSamp + vec2(-texel,    0.0), vec2(pad), vec2(1.0 - pad));
    vec2 uvR = clamp(uvSamp + vec2( texel,    0.0), vec2(pad), vec2(1.0 - pad));
    vec2 uvD = clamp(uvSamp + vec2(   0.0, -texel), vec2(pad), vec2(1.0 - pad));
    vec2 uvU = clamp(uvSamp + vec2(   0.0,  texel), vec2(pad), vec2(1.0 - pad));

    float hL = texture(heightMap, uvL).r * height_scale;
    float hR = texture(heightMap, uvR).r * height_scale;
    float hD = texture(heightMap, uvD).r * height_scale;
    float hU = texture(heightMap, uvU).r * height_scale;

    vec2 gradH = vec2((hR - hL) / (2.0 * texel),
                      (hU - hD) / (2.0 * texel));

    // 4) Chain rule: height change per (u,v)
    float dhdu = dot(gradH, dTdu);
    float dhdv = dot(gradH, dTdv);

    // 5) Final geometric partials including height (local, pre-scale)
    vec3 dPdu = vec3(dPdu4) + vec3(0.0, dhdu, 0.0);
    vec3 dPdv = vec3(dPdv4) + vec3(0.0, dhdv, 0.0);

    // ---- Non-uniform X/Z scale (unchanged) ----
    dPdu.x *= chunk_scale_xz.x;  dPdu.z *= chunk_scale_xz.y;
    dPdv.x *= chunk_scale_xz.x;  dPdv.z *= chunk_scale_xz.y;

    // 6) Normal/TBN from scaled geometric partials
    vec3 N = normalize(cross(dPdv, dPdu));
    vec3 T = normalize(dPdu);
    vec3 B = normalize(cross(N, T));
    T = normalize(cross(B, N)); // re-orthonormalize

    mat3 normalMat = transpose(inverse(mat3(model)));
    TBN = mat3(normalize(normalMat * T),
               normalize(normalMat * B),
               normalize(normalMat * N));

    // Apply the same scale to position X/Z, then world offset (translation)
    p.x *= chunk_scale_xz.x;
    p.z *= chunk_scale_xz.y;
    p.xz += chunk_offset_xz;

    LocalSpacePos    = p;                 // unchanged usage in FS
    vec4 worldPos    = model * p;
    thePositionWorld = worldPos.xyz;
    gl_Position      = projection * view * worldPos;

    tes_debug_color = debug_color[0];
}
