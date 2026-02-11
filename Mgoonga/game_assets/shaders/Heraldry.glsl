// Heraldry.glsl
#ifndef HERALDRY_GLSL_INCLUDED
#define HERALDRY_GLSL_INCLUDED

// === Shared uniforms ===

// Shield rect in base texture UV space: (minU, minV, sizeU, sizeV)
uniform vec4 uShieldRectUV = vec4(0.0);

// Enable/disable heraldry for this material/draw
uniform bool enable_heraldry = false;

// Flip local shield UV vertically (for upside-down patches in base texture)
uniform bool uHeraldryFlipY = true;

// Backend selection: 0 = off, 1 = color atlas, 2 = MSDF (now supports up to 2 layers)
uniform int uHeraldryMode = 1;

// MSDF smoothing tweak (in screen-space units)
uniform float uMsdfPixelRange = 0.75;

// === Textures ===

// Color atlas (your existing coats.png)
layout(binding = 19) uniform sampler2D heraldryAtlas;

// MSDF atlas (one atlas containing both ordinaries & charges)
layout(binding = 20) uniform sampler2D heraldryMsdfAtlas;

// === Per-instance buffers ===
// NOTE: we now have two heraldry "layers":
//   Layer 0: background ordinary
//   Layer 1: foreground charge

// Layer 0: (scaleU, scaleV, offsetU, offsetV)
layout(std430, binding = 8) readonly buffer HeraldryBuffer0 {
    vec4 heraldryUvTransform0[];
};

// Layer 0 tint: (r,g,b,a)
layout(std430, binding = 9) readonly buffer HeraldryTintBuffer0 {
    vec4 heraldryTint0[];
};

// Layer 1: (scaleU, scaleV, offsetU, offsetV)
layout(std430, binding = 10) readonly buffer HeraldryBuffer1 {
    vec4 heraldryUvTransform1[];
};

// Layer 1 tint: (r,g,b,a)
layout(std430, binding = 11) readonly buffer HeraldryTintBuffer1 {
    vec4 heraldryTint1[];
};

// From vertex shader
flat in int InstanceID;

// === Helpers ===

float median3(vec3 v)
{
    return max(min(v.r, v.g), min(max(v.r, v.g), v.b));
}

// Sample *color atlas* coat
vec4 SampleColorAtlasCoat(vec2 atlasUV)
{
    return texture(heraldryAtlas, atlasUV);
}

// Sample a single MSDF sprite at given atlas UV with given tint
vec4 SampleSingleMsdf(vec2 atlasUV, vec3 tintColor)
{
    vec4 msdfSample = texture(heraldryMsdfAtlas, atlasUV);

    // Standard MSDF decode
    float sd = median3(msdfSample.rgb) - 0.5;
    float pxRange = fwidth(sd) * uMsdfPixelRange;
    float alpha = clamp(smoothstep(-pxRange, pxRange, -sd), 0.0, 1.0);

    return vec4(tintColor, alpha);
}

// Composite two MSDF layers given local shield UV (0..1),
// using per-instance transforms/tints for background & foreground.
//
// Returns vec4(color.rgb, alpha), alpha=0 if no layers.
vec4 SampleMsdfLayers(vec2 localShieldUV)
{
    vec4 result = vec4(0.0); // premultiplied color + alpha

    // --- Background ordinary (layer 0) ---
    vec4 t0 = heraldryUvTransform0[InstanceID]; // (scaleU, scaleV, offsetU, offsetV)
    if (t0.x != 0.0 || t0.y != 0.0)
    {
        vec2 uv0 = localShieldUV * t0.xy + t0.zw;
        vec3 c0  = heraldryTint0[InstanceID].rgb;
        vec4 layer0 = SampleSingleMsdf(uv0, c0);

        // Start with background (premultiplied)
        result.rgb = layer0.rgb * layer0.a;
        result.a   = layer0.a;
    }

    // --- Foreground charge (layer 1) ---
    vec4 t1 = heraldryUvTransform1[InstanceID];
    if (t1.x != 0.0 || t1.y != 0.0)
    {
        vec2 uv1 = localShieldUV * t1.xy + t1.zw;
        vec3 c1  = heraldryTint1[InstanceID].rgb;
        vec4 layer1 = SampleSingleMsdf(uv1, c1);

        vec3 chColor = layer1.rgb;
        float chAlpha = layer1.a;

        // Alpha-composite: charge over background (both premultiplied)
        // out = C1 + C0 * (1 - A1);   Aout = A1 + A0 * (1 - A1)
        vec3 bgColor = result.a > 0.0 ? result.rgb / max(result.a, 1e-4) : result.rgb;
        vec3 c0 = bgColor;
        float a0 = result.a;

        vec3 outColor = chColor * chAlpha + c0 * a0 * (1.0 - chAlpha);
        float outAlpha = chAlpha + a0 * (1.0 - chAlpha);

        result.rgb = outColor * outAlpha; // store premultiplied
        result.a   = outAlpha;
    }

    // Convert from premultiplied back to straight alpha if alpha > 0
    if (result.a > 0.0)
    {
        result.rgb /= result.a;
    }

    return result;
}

// Core: compute coat color in atlas space from base UV
// Returns vec4(color.rgb, alpha), alpha = 0 when no coat should be applied.
vec4 SampleHeraldryCoat(vec2 baseUV)
{
    if (!enable_heraldry || uHeraldryMode == 0)
        return vec4(0.0);

    // Shield rect not configured?
    if (uShieldRectUV.z <= 0.0 || uHeraldryFlipY && uShieldRectUV.w <= 0.0 || (!uHeraldryFlipY && uShieldRectUV.w <= 0.0))
        return vec4(0.0);

    vec2 shieldMin  = uShieldRectUV.xy;
    vec2 shieldSize = uShieldRectUV.zw;

    // Outside shield rect → no heraldry
    if (baseUV.x < shieldMin.x || baseUV.x > shieldMin.x + shieldSize.x ||
        baseUV.y < shieldMin.y || baseUV.y > shieldMin.y + shieldSize.y)
    {
        return vec4(0.0);
    }

    // Local 0..1 UV inside the shield patch
    vec2 localShieldUV = (baseUV - shieldMin) / shieldSize;

    // Optional vertical flip to fix upside-down shield patch
    if (uHeraldryFlipY) {
        localShieldUV.y = 1.0 - localShieldUV.y;
    }

    // Color atlas path: use layer 0 transform only
    if (uHeraldryMode == 1)
    {
        vec4 t0 = heraldryUvTransform0[InstanceID];
        if (t0.x == 0.0 && t0.y == 0.0)
            return vec4(0.0);

        vec2 atlasUV = localShieldUV * t0.xy + t0.zw;
        return SampleColorAtlasCoat(atlasUV);
    }

    // MSDF path: use up to two layers (ordinary + charge)
    if (uHeraldryMode == 2)
    {
        return SampleMsdfLayers(localShieldUV);
    }

    // Unknown mode → no coat
    return vec4(0.0);
}

// Blend helper: apply coat over base albedo color
vec3 ApplyHeraldryToAlbedo(vec2 baseUV, vec3 baseAlbedo)
{
    vec4 coat = SampleHeraldryCoat(baseUV);
    // Mix using coat alpha; if coat.a == 1 → full heraldry
    return mix(baseAlbedo, coat.rgb, coat.a);
}

#endif // HERALDRY_GLSL_INCLUDED
