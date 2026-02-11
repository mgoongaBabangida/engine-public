#version 460 core

// ==== Inputs ====
in float Height;
in vec2 texCoord;
in vec3 thePositionWorld;
in vec4 LocalSpacePos;       // world/local coord used for triplanar
in mat3 TBN;                 // columns: T, B, N in WORLD space
in vec3 tes_debug_color;

out vec4 FragColor;

// ==== Lighting ====
struct Light 
{
    vec4 position;
    vec4 direction;
    
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;

    float constant;
    float linear;
    float quadratic;

    float cutOff;
    float outerCutOff;
};

uniform Light lights[1];
uniform vec4  eyePositionWorld;

// ==== Material / control ====
uniform float shininess = 16.0f;

bool  gamma_correction = true;
const int   max_texture_array_size = 8;
const float epsilon = 0.001f;

uniform float min_height = 0.0f;
uniform float max_height = 1.75f;

// Heights array size must be >= color_count+1
uniform float base_start_heights[max_texture_array_size];
uniform int   color_count = 4;

// Index of the snow layer in the texture arrays
uniform int   snow_color  = 3;

// Global snow/melt control (higher => more snow). Your parameter.
uniform float snowness = 0.65f;

// Triplanar scale per layer
uniform float textureScale[max_texture_array_size];

// Renderer switches
uniform bool  pbr_renderer              = false;
uniform bool  use_normal_texture_pbr    = true;
uniform bool  use_roughness_texture_pbr = true;
uniform bool  use_metalic_texture_pbr   = true;
uniform bool  use_ao_texture_pbr        = true;

// Normal detail mix vs geometric normal
uniform float normal_detail_strength = 0.5; // 0..1 blend
uniform bool  normal_y_flip = false;        // flip green channel if needed

// Triplanar normal weighting exponent (how hard projections pick a dominant axis)
uniform float triplanar_weight_exp = 4.0;

//snow mask softness controls
uniform float snow_height_softness = 0.05;  // how softly snow fades in around the snow height boundary
uniform float snow_melt_softness   = 0.10;  // how softly snow responds to the 'snowness' threshold

// ==== Textures ====
layout(binding=4)  uniform sampler2D       normalMap; // (unused in PBR path)

layout(binding=12) uniform sampler2DArray  texture_array_albedo;
layout(binding=13) uniform sampler2DArray  texture_array_normal;
layout(binding=14) uniform sampler2DArray  texture_array_metallic;
layout(binding=15) uniform sampler2DArray  texture_array_roughness;
layout(binding=16) uniform sampler2DArray  texture_array_ao;

// ==== PBR helpers ====
const float PI = 3.14159265359;
float DistributionGGX(vec3 N, vec3 H, float roughness);
float GeometrySchlickGGX(float NdotV, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);
vec3  fresnelSchlick(float cosTheta, vec3 F0);

// ==== Utilities ====
float inverseLerp(float a, float b, float value)
{
    return clamp((value-a)/(b-a), 0.0f, 1.0f);
}

vec3 decodeNormal(vec3 n)
{
    n = n * 2.0 - 1.0;
    if (normal_y_flip) n.g = -n.g;
    return normalize(n);
}

// Right-handed basis: ensure cross(T,B)==N (flip B if needed)
mat3 makeBasis(vec3 T, vec3 B, vec3 N)
{
    if (dot(cross(T, B), N) < 0.0) B = -B;
    return mat3(T, B, N); // columns
}

// ==== Triplanar ALBEDO (basic) ====
vec3 triplaner(int layer, vec3 blendAxes, vec4 localSpacePos)
{
    layer = clamp(layer, 0, color_count-1);
    vec3 scaledWorldPos = vec3(localSpacePos) / textureScale[layer];

    vec3 xProjection, yProjection, zProjection;
    if (gamma_correction)
    {
        xProjection = pow(texture(texture_array_albedo, vec3(scaledWorldPos.yz, layer)).rgb, vec3(2.2f)) * abs(blendAxes.x);
        yProjection = pow(texture(texture_array_albedo, vec3(scaledWorldPos.xz, layer)).rgb, vec3(2.2f)) * abs(blendAxes.y);
        zProjection = pow(texture(texture_array_albedo, vec3(scaledWorldPos.xy, layer)).rgb, vec3(2.2f)) * abs(blendAxes.z);
    }
    else
    {
        xProjection = texture(texture_array_albedo, vec3(scaledWorldPos.yz, layer)).rgb * abs(blendAxes.x);
        yProjection = texture(texture_array_albedo, vec3(scaledWorldPos.xz, layer)).rgb * abs(blendAxes.y);
        zProjection = texture(texture_array_albedo, vec3(scaledWorldPos.xy, layer)).rgb * abs(blendAxes.z);
    }
    return xProjection + yProjection + zProjection;
}

// Triplanar NORMAL (decode -> rotate to world -> blend -> renormalize) ====
vec3 triplanarNormalWorld(int layer, vec3 nGeomWorld, vec4 localSpacePos)
{
    layer = clamp(layer, 0, color_count-1);

    // Axis weights (bias slopes to reduce smear)
    vec3 w = abs(nGeomWorld);
    w = pow(w, vec3(triplanar_weight_exp));
    float sumW = max(w.x + w.y + w.z, 1e-5);
    w /= sumW;

    // UVs per axis
    vec2 uvX = localSpacePos.yz / textureScale[layer]; // U:+Y, V:+Z, normal ±X
    vec2 uvY = localSpacePos.xz / textureScale[layer]; // U:+X, V:+Z, normal ±Y
    vec2 uvZ = localSpacePos.xy / textureScale[layer]; // U:+X, V:+Y, normal ±Z

    // Sample & decode TS normals
    vec3 nxTS = decodeNormal(texture(texture_array_normal, vec3(uvX, layer)).xyz);
    vec3 nyTS = decodeNormal(texture(texture_array_normal, vec3(uvY, layer)).xyz);
    vec3 nzTS = decodeNormal(texture(texture_array_normal, vec3(uvZ, layer)).xyz);

    // Projection axes with sign from geom normal
    vec3 Nx = vec3(sign(nGeomWorld.x), 0.0, 0.0);
    vec3 Ny = vec3(0.0, sign(nGeomWorld.y), 0.0);
    vec3 Nz = vec3(0.0, 0.0, sign(nGeomWorld.z));

    // Axis-aligned bases
    mat3 BX = makeBasis(vec3(0, 1, 0), vec3(0, 0, 1), Nx); // U:+Y, V:+Z, N:±X
    mat3 BY = makeBasis(vec3(1, 0, 0), vec3(0, 0, 1), Ny); // U:+X, V:+Z, N:±Y
    mat3 BZ = makeBasis(vec3(1, 0, 0), vec3(0, 1, 0), Nz); // U:+X, V:+Y, N:±Z

    // TS -> WORLD
    vec3 nX = normalize(BX * nxTS);
    vec3 nY = normalize(BY * nyTS);
    vec3 nZ = normalize(BZ * nzTS);

    return normalize(nX * w.x + nY * w.y + nZ * w.z);
}

// ==== Unified SNOW MASK (overlay) ====
// Smoothly depends on height boundary of snow layer and the 'snowness' melt control.
float ShouldBeSnow(vec3 Normal)  // keeps original lighting-based component
{
    const int numLights = 3;
    vec3 lightDirs[numLights];
    lightDirs[0] = normalize(vec3(-1, 1, -1));
    lightDirs[1] = vec3(0, 1, 0);
    lightDirs[2] = normalize(vec3(1, 1, 1));
	
    float totalLightIntensity = 0.0;
    for (int i = 0; i < numLights; ++i) {
        totalLightIntensity += dot(normalize(Normal), normalize(lightDirs[i]));
    }
    totalLightIntensity /= float(numLights);
    return clamp(totalLightIntensity, 0.0, 1.0);
}

float SnowMask(vec3 nGeomWorld, float heightPercent)
{
    if (snow_color < 0 || snow_color >= color_count) return 0.0;

    // Height gate around the *start* of the snow layer
    float hStart = base_start_heights[snow_color];
    float S_h = smoothstep(hStart - snow_height_softness,
                           hStart + snow_height_softness,
                           heightPercent);

    // Melt/accumulation gate using your 'snowness' and ShouldBeSnow()
    float S_dir = smoothstep(snowness - snow_melt_softness,
                             snowness + snow_melt_softness,
                             ShouldBeSnow(nGeomWorld));

    // Final snow mask
    return clamp(S_h * S_dir, 0.0, 1.0);
}

// ==== "Base" samplers (ignore snow layer in the height blend) ====
// Treat the snow layer as if it were its underlying layer to avoid double-mixing.
int layerNoSnow(int layer)
{
    if (layer == snow_color) layer = max(layer - 1, 0);
    return clamp(layer, 0, color_count-1);
}

vec3 triplaner_noSnow(int layer, vec3 blendAxes, vec4 localSpacePos)
{
    return triplaner(layerNoSnow(layer), blendAxes, localSpacePos);
}

vec3 SampleAlbedoBase_NoSnow(vec3 Ngeom, vec4 localSpacePos)
{
    vec3 blendAxes = normalize(Ngeom);
    float heightPercent = inverseLerp(min_height, max_height, Height);

    vec3 colorAlbedo = vec3(0.0);
    for (int i = 0; i < color_count; ++i)
    {
        if (heightPercent < base_start_heights[i] || heightPercent > base_start_heights[i + 1]) continue;

        vec3 primary = triplaner_noSnow(i, blendAxes, localSpacePos);

        // blend to next layer only if exists
        float edge = base_start_heights[i + 1] - heightPercent;
        if (edge < 0.05 && (i + 1) < color_count)
        {
            float t = clamp(edge * (1.0 / 0.05), 0.0, 1.0);
            vec3 nextC = triplaner_noSnow(i + 1, blendAxes, localSpacePos);
            colorAlbedo = mix(nextC, primary, t);
        }
        else
        {
            colorAlbedo = primary;
        }
        break;
    }
    return colorAlbedo;
}

vec3 SampleNormalWorldBase_NoSnow(vec3 nGeomWorld, vec4 localSpacePos)
{
    float heightPercent = inverseLerp(min_height, max_height, Height);

    for (int i = 0; i < color_count; ++i)
    {
        if (heightPercent < base_start_heights[i] || heightPercent > base_start_heights[i + 1]) continue;

        vec3 n0 = triplanarNormalWorld(layerNoSnow(i), nGeomWorld, localSpacePos);

        float edge = base_start_heights[i + 1] - heightPercent;
        if (edge < 0.05 && (i + 1) < color_count)
        {
            float t  = clamp(edge * (1.0 / 0.05), 0.0, 1.0);
            vec3 n1  = triplanarNormalWorld(layerNoSnow(i + 1), nGeomWorld, localSpacePos);
            return normalize(mix(n1, n0, t));
        }
        return n0;
    }
    return nGeomWorld; // fallback
}

// ==== Snow-overlay helpers (apply same mask across all maps) ====
vec3  SampleAlbedoWithSnow(vec3 Ngeom, vec4 localSpacePos)
{
    float heightPercent = inverseLerp(min_height, max_height, Height);
    float S = SnowMask(Ngeom, heightPercent);

    vec3 baseC = SampleAlbedoBase_NoSnow(Ngeom, localSpacePos);

    // Sample dedicated snow layer trigram
    vec3 snowC = triplaner(snow_color, normalize(Ngeom), localSpacePos);

    return mix(baseC, snowC, S);
}

vec3  SampleNormalWorldWithSnow(vec3 nGeomWorld, vec4 localSpacePos)
{
    float heightPercent = inverseLerp(min_height, max_height, Height);
    float S = SnowMask(nGeomWorld, heightPercent);

    vec3 nBase = SampleNormalWorldBase_NoSnow(nGeomWorld, localSpacePos);
    vec3 nSnow = triplanarNormalWorld(snow_color, nGeomWorld, localSpacePos);

    return normalize(mix(nBase, nSnow, S));
}

float SampleScalar_NoSnow(sampler2DArray tex, vec3 Ngeom, vec4 localSpacePos) // for metallic/roughness/AO
{
    float heightPercent = inverseLerp(min_height, max_height, Height);
    vec3  blendAxes     = normalize(Ngeom);

    for (int i = 0; i < color_count; ++i)
    {
        if (heightPercent < base_start_heights[i] || heightPercent > base_start_heights[i + 1]) continue;

        int idx0 = layerNoSnow(i);
        vec3 scaled = vec3(localSpacePos) / textureScale[idx0];
        float x0 = texture(tex, vec3(scaled.yz, idx0)).r * abs(blendAxes.x);
        float y0 = texture(tex, vec3(scaled.xz, idx0)).r * abs(blendAxes.y);
        float z0 = texture(tex, vec3(scaled.xy, idx0)).r * abs(blendAxes.z);
        float s0 = x0 + y0 + z0;

        float edge = base_start_heights[i + 1] - heightPercent;
        if (edge < 0.05 && (i + 1) < color_count)
        {
            int idx1 = layerNoSnow(i + 1);
            vec3 scaled1 = vec3(localSpacePos) / textureScale[idx1];
            float x1 = texture(tex, vec3(scaled1.yz, idx1)).r * abs(blendAxes.x);
            float y1 = texture(tex, vec3(scaled1.xz, idx1)).r * abs(blendAxes.y);
            float z1 = texture(tex, vec3(scaled1.xy, idx1)).r * abs(blendAxes.z);
            float s1 = x1 + y1 + z1;

            float t = clamp(edge * (1.0 / 0.05), 0.0, 1.0);
            return clamp(mix(s1, s0, t), 0.0, 1.0);
        }
        return clamp(s0, 0.0, 1.0);
    }
    return 0.0;
}

float SampleScalar_Snow(sampler2DArray tex, vec3 Ngeom, vec4 localSpacePos) // snow-only value
{
    int idx = clamp(snow_color, 0, color_count-1);
    vec3 scaled = vec3(localSpacePos) / textureScale[idx];
    vec3 blendAxes = normalize(Ngeom);
    float x = texture(tex, vec3(scaled.yz, idx)).r * abs(blendAxes.x);
    float y = texture(tex, vec3(scaled.xz, idx)).r * abs(blendAxes.y);
    float z = texture(tex, vec3(scaled.xy, idx)).r * abs(blendAxes.z);
    return clamp(x + y + z, 0.0, 1.0);
}

float SampleMetallicWithSnow (vec3 Ngeom, vec4 localSpacePos)
{
    float heightPercent = inverseLerp(min_height, max_height, Height);
    float S = SnowMask(Ngeom, heightPercent);
    float baseVal = SampleScalar_NoSnow(texture_array_metallic, Ngeom, localSpacePos);
    float snowVal = SampleScalar_Snow  (texture_array_metallic, Ngeom, localSpacePos);
    return mix(baseVal, snowVal, S);
}

float SampleRoughnessWithSnow (vec3 Ngeom, vec4 localSpacePos)
{
    float heightPercent = inverseLerp(min_height, max_height, Height);
    float S = SnowMask(Ngeom, heightPercent);
    float baseVal = SampleScalar_NoSnow(texture_array_roughness, Ngeom, localSpacePos);
    float snowVal = SampleScalar_Snow  (texture_array_roughness, Ngeom, localSpacePos);
    return mix(baseVal, snowVal, S);
}

float SampleAOWithSnow (vec3 Ngeom, vec4 localSpacePos)
{
    float heightPercent = inverseLerp(min_height, max_height, Height);
    float S = SnowMask(Ngeom, heightPercent);
    float baseVal = SampleScalar_NoSnow(texture_array_ao, Ngeom, localSpacePos);
    float snowVal = SampleScalar_Snow  (texture_array_ao, Ngeom, localSpacePos);
    return mix(baseVal, snowVal, S);
}

// ==== Shading ====
vec4 PhongModel()
{
    vec4 localSpacePos = LocalSpacePos;

    // Macro normal
    vec3 Ngeom = normalize(TBN[2]);

    // Detail normal with snow overlay
    vec3 N = Ngeom;
    if (use_normal_texture_pbr) {
        vec3 Ndetail = SampleNormalWorldWithSnow(Ngeom, localSpacePos);
        float slope  = 1.0 - abs(Ngeom.y);
        float w      = normal_detail_strength * mix(1.0, 0.6, slope);
        N = normalize(mix(Ngeom, Ndetail, clamp(w, 0.0, 1.0)));
    }

    // Albedo with snow overlay
    vec3 color = SampleAlbedoWithSnow(Ngeom, localSpacePos);

    // Diffuse
    vec3 Ldir = -normalize(vec3(lights[0].direction));
    float NdotL = clamp(dot(Ldir, N), 0.0, 1.0);
    vec3 diffuseColor = color * NdotL * lights[0].diffuse.xyz;

    // Specular
    vec3 V = normalize(eyePositionWorld.xyz - thePositionWorld);
    vec3 H = normalize(V + Ldir);
    float spec = pow(max(dot(N, H), 0.0), shininess);
    vec3 specular = clamp(lights[0].specular.xyz * spec * color, 0.0, 1.0);

    // Ambient
    vec3 ambient = lights[0].ambient.xyz * color;

    return vec4(diffuseColor + specular + ambient, 1.0);
}

vec4 PBRModel()
{
    vec4 localSpacePos = LocalSpacePos;

    vec3 Ngeom = normalize(TBN[2]);

    // Albedo/params with unified snow overlay
    vec3  albedo    = SampleAlbedoWithSnow(Ngeom, localSpacePos);
    float metallic  = use_metalic_texture_pbr   ? SampleMetallicWithSnow (Ngeom, localSpacePos) : 1.0;
    float roughness = use_roughness_texture_pbr ? SampleRoughnessWithSnow(Ngeom, localSpacePos) : 0.0;
    float ao        = use_ao_texture_pbr        ? SampleAOWithSnow       (Ngeom, localSpacePos) : 1.0;

    // Detail normal with snow overlay
    vec3 N = Ngeom;
    if (use_normal_texture_pbr)
    {
        vec3 Ndetail = SampleNormalWorldWithSnow(Ngeom, localSpacePos);
        float slope  = 1.0 - abs(Ngeom.y);
        float w      = normal_detail_strength * mix(1.0, 0.6, slope);
        N = normalize(mix(Ngeom, Ndetail, clamp(w, 0.0, 1.0)));
    }

    vec3 V = normalize(eyePositionWorld.xyz - thePositionWorld);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);
    for (int i = 0; i < 1; ++i) {
        vec3 L = -normalize(vec3(lights[i].direction)); // directional
        vec3 H = normalize(V + L);
        vec3 radiance = lights[i].ambient.xyz * 15.0;

        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        vec3  F   = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

        float NdotV = max(dot(N, V), 0.0);
        float NdotL = max(dot(N, L), 0.0);
        vec3 spec = (NDF * G * F) / max(4.0 * NdotV * NdotL + 1e-4, 1e-4);

        Lo += (kD * albedo / PI + spec) * radiance * NdotL;
    }

    vec3 ambient = vec3(0.15) * albedo * ao;
    vec3 color   = ambient + Lo;
    return vec4(color, 1.0);
}

void main()
{
    FragColor = pbr_renderer ? PBRModel() : PhongModel();
}

// ==== PBR helpers ====
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a  = roughness*roughness;
    float a2 = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
