#version 460 core

in  vec2 TexCoords;
out vec4 FragColor;

// Depth texture containing resolved scene depth (0..1, non-linear)
layout(binding = 17) uniform sampler2D depthTex;
layout(binding = 19) uniform sampler2D decalTex;

// Camera matrices
uniform mat4 invProj;     // inverse of projection matrix
uniform mat4 invView;     // inverse of view (camera-to-world)

// World → decal box transform
uniform mat4 decalMatrix; // world-to-decal space

// Atlas UV transform for this decal: (scaleU, scaleV, offsetU, offsetV)
// For a non-atlas texture, set to vec4(1,1,0,0)
uniform vec4 decalUVTransform = vec4(1.0, 1.0, 0.0, 0.0);

// Flip Y inside the decal quad (for shields that are upside down in UV)
uniform bool decalInvertY     = false;

// Whether to treat decalTex as sRGB and convert to linear
uniform bool decalGammaCorrect = true;

// Soft volume tolerances
const float XY_EPS = 0.02; // looseness along X/Y
const float Z_EPS  = 0.10; // looser along depth

void main()
{
    // 1. Sample depth (non-linear OpenGL depth 0..1)
    //    textureLod(...) avoids any mip-level surprises
    float depth = textureLod(depthTex, TexCoords, 0.0).r;
    if (depth >= 0.9999)
        discard; // background / sky

    // 2. Reconstruct clip-space position
    vec2  ndc      = TexCoords * 2.0 - 1.0;   // NDC xy in [-1,1]
    float ndcDepth = depth * 2.0 - 1.0;       // NDC z in [-1,1]
    vec4  clipPos  = vec4(ndc, ndcDepth, 1.0);

    // 3. View-space position
    vec4 viewPos = invProj * clipPos;
    viewPos /= viewPos.w;

    // 4. World-space position
    vec4 worldPos = invView * viewPos;

    // 5. World → decal local space
    //    decalMatrix is world-to-decal, decal local cube is [-0.5, 0.5]^3
    vec4 decalSpace = decalMatrix * worldPos;
    vec3 local      = decalSpace.xyz; // expected in [-0.5, 0.5]

    // 6. Inside volume test with relaxed tolerances
    if (abs(local.x) > 0.5 + XY_EPS ||
        abs(local.y) > 0.5 + XY_EPS ||
        abs(local.z) > 0.5 + Z_EPS)
    {
        discard;
    }

    // 7. Map local [-0.5,0.5] → [0,1] and clamp
    vec3 uvw = local + vec3(0.5);
    uvw = clamp(uvw, vec3(0.0), vec3(1.0));

    // 8. 2D decal UV inside the box
    vec2 localUV = uvw.xy;
    if (decalInvertY)
        localUV.y = 1.0 - localUV.y;

    // 9. Atlas UV transform
    vec2 atlasUV = localUV * decalUVTransform.xy + decalUVTransform.zw;

    // 10. Sample the decal
    vec4 decalColor = texture(decalTex, atlasUV);
    if (decalColor.a < 0.01)
        discard;

    // Optional gamma correction (sRGB → linear)
    if (decalGammaCorrect)
        decalColor.rgb = pow(decalColor.rgb, vec3(2.2));

    // 11. Output — blending is handled in the pipeline
    FragColor = decalColor;
}
