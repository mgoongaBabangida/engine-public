// Refactored PBR Fragment Shader with Area Light Support
#version 460 core

layout(location = 0) out vec4 FragColor;
layout(location = 1) out float mask;

in vec2 Texcoord;
in vec3 thePosition; //WorldPos
in vec3 theNormal;
in mat3 TBN;
in vec4 LightSpacePos;
flat in vec3 SolidColor;

// --- Uniforms (Grouped + Cleaned Up) ---

// Material properties
uniform vec4  albedo;
uniform float metallic;
uniform float roughness;
uniform float ao;
uniform float opacity = 1.f;
uniform vec4 emission_color;
uniform float emission_strength = 1.f;

// Flags
uniform bool textured = true;
uniform bool use_metalic_texture = true;
uniform bool use_normalmap_texture = true;
uniform bool use_roughness_texture = true;
uniform bool use_ao_texture = false;
uniform bool use_opacity_mask = false;
uniform bool ibl_on = true;
uniform bool gamma_correction = true;
uniform bool fadeAlpha = false;

//CSM + PCSS
uniform mat4 view;
uniform float far_plane;
uniform float farPlane;
uniform int cascadeCount;
uniform float[10] cascadePlaneDistances;
uniform float cascade_blend_distance = 0.5f;
uniform bool blendCascades = true;

uniform float max_penumbra = 10.f;
uniform float light_radius = 15.0f; // used for UV in pixels of shadow map
uniform float lightSize = 3.0f; // used for penumbra calculation, should be aligned with light_radius
uniform bool pcss_enabled = true;
uniform float max_shadow = 1.0f;
uniform int pcf_samples = 2; // from -pcf_samples to +pcf_samples

uniform float pcf_texture_sample_radius = 2.5f;
uniform bool pcf_texture_sample_enabled = true;
uniform vec4 pcf_OffsetTexSize; // (width, height, depth)

uniform float csm_base_slope_bias = 0.005f;
uniform float csm_base_cascade_plane_bias = 0.5f;

// Shadowing
uniform bool shadow_directional = true;
uniform bool use_csm_shadows = false;

// SSAO
uniform float ssao_threshold = 0.9f;
uniform float ssao_strength = 0.6f;

// Camera & Fog
uniform vec4 camPos;

struct FogInfo { float maxDist; float minDist; vec4 color; bool fog_on; float density; float gradient; };
uniform FogInfo Fog;
struct AreaLight { float intensity; vec4 color; vec4 points[4]; bool twoSided; };

const int max_lights = 8;

// Light & visibility
uniform int numberOfTilesX;
uniform int num_lights = 2;
uniform vec4 lightPositions[max_lights];
uniform vec4 lightDirections[max_lights];
uniform vec4 lightColors[max_lights];
uniform float constant[max_lights];
uniform float linear[max_lights];
uniform float quadratic[max_lights];
uniform float cutOff[max_lights];
uniform float outerCutOff[max_lights];
uniform bool flash[max_lights];
uniform bool isAreaLight[max_lights];
uniform float intensity[max_lights];
uniform mat4  points[max_lights];
uniform bool  twoSided[max_lights];
uniform float radius[max_lights];
uniform bool  isActive[max_lights];

uniform bool  forward_plus;
uniform float ibl_influance = 1.f;

// Instanced info pipeline
uniform bool  InstancedInfoRender = false;

// Textures
layout(binding=0)  uniform samplerCube  	depth_cube_map;
layout(binding=1)  uniform sampler2D    	depth_texture;
layout(binding=2)  uniform sampler2D    	albedoMap;
layout(binding=3)  uniform sampler2D    	metallicMap;
layout(binding=4)  uniform sampler2D    	normalMap;
layout(binding=5)  uniform sampler2D    	roughnessMap;
layout(binding=6)  uniform sampler2D    	emissionMap;
layout(binding=7)  uniform sampler2D 		LTC1; // for inverse M
layout(binding=8)  uniform sampler2D 		LTC2; // GGX norm, fresnel, 0(unused), sphere
layout(binding=9)  uniform samplerCube  	irradianceMap;
layout(binding=10) uniform samplerCube  	prefilterMap;
layout(binding=11) uniform sampler2D    	brdfLUT;
layout(binding=13) uniform sampler2DArray   texture_array_csm;
layout(binding=14) uniform sampler2D    	texture_ssao;
layout(binding=15) uniform sampler3D 		texture_pcf_offsets;
layout(binding=16) uniform sampler2D    	aoMap;
layout(binding=18) uniform sampler2D    	opacityMask;

layout (std140, binding = 0) uniform LightSpaceMatrices
{
    mat4 lightSpaceMatrices[16];
	mat4 lightProjection[16];
};

layout(std430, binding = 1) readonly buffer VisibleLightIndicesBuffer {
    int data[];
} visibleLightIndicesBuffer;

vec4 lightPos = lightPositions[0];

// Include dependencies
#include "CommonShadow.glsl"
#include "CommonAreaLight.glsl"
#include "Heraldry.glsl"

// Constants
const float PI = 3.14159265359;

// --- Utility Functions ---

bool isAnyNan(vec3 v) { return isnan(v.x) || isnan(v.y) || isnan(v.z); }

float GetAlpha() {
    float alpha = 1.0;
    if (textured) alpha = texture(albedoMap, Texcoord).a;
    return alpha;
}

vec3 GetAlbedo() {
    vec3 base = textured ? texture(albedoMap, Texcoord).rgb : albedo.rgb;

    // Apply heraldry overlay on top of base albedo (if enabled & inside shield rect)
    base = ApplyHeraldryToAlbedo(Texcoord, base);

    return gamma_correction ? ToLinear(base) : base;
}

float GetRoughness() {
    float r = use_roughness_texture ? texture(roughnessMap, Texcoord).r : roughness;
    return clamp(r, 0.14, 1.0);
}

float GetMetallic() {
    return use_metalic_texture ? texture(metallicMap, Texcoord).r : metallic;
}

float GetAO() {
    return use_ao_texture ? texture(aoMap, Texcoord).r : ao;
}

vec3 GetNormal() {
    if (!use_normalmap_texture) return normalize(theNormal);
    vec3 n = texture(normalMap, Texcoord).rgb * 2.0 - 1.0;
    n = normalize(TBN * n);
    return isAnyNan(n) ? normalize(theNormal) : n;
}

float GetOpacityMask() {
    return use_opacity_mask ? texture(opacityMask, Texcoord).r : opacity;
}

float GetSSAO() {
    ivec2 screenSize = textureSize(texture_ssao, 0);
    vec2 uv = gl_FragCoord.xy / vec2(screenSize);
    float ao_val = texture(texture_ssao, uv).r;
    return ao_val < ssao_threshold ? ao_val * ssao_strength : ao_val;
}

vec3 GetEmissive() {
    vec3 emissive = texture(emissionMap, Texcoord).rgb;
    return emissive * emission_color.rgb * emission_strength;
}

// Returns 1 at opaqueCoord, 0 at transparentCoord (works for either order).
float fade1D(float coord, float opaqueCoord, float transparentCoord)
{
    // k > 0 controls how quickly it drops early. Try 3..10.
    float k = 16.f;
    float t = (coord - opaqueCoord) / (transparentCoord - opaqueCoord);
    t = clamp(t, 0.0, 0.8);

    float e = 1.0 - exp(-k * t);
    e /= (1.0 - exp(-k)); // normalize so e(1)=1
    return 1.0 - e;       // 1 -> 0;
}

float DistributionGGX(vec3 N, vec3 H, float roughness);
float GeometrySchlickGGX(float NdotV, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);
vec3 fresnelSchlick(float cosTheta, vec3 F0);
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness);

// --- Main ---

void main() 
{
	if(InstancedInfoRender)
	{
	 FragColor = vec4(SolidColor, 1.0f);
	 return;
	}
	 
    float alpha = GetAlpha();
    if (alpha < 0.05) discard;
    if(fadeAlpha) alpha *= fade1D(Texcoord.y, 10.0, 0.0);

    vec3 albedo_f = GetAlbedo();
    float metallic_f = GetMetallic();
    float roughness_f = GetRoughness();
    float ao_f = GetAO();
    float opacity_f = GetOpacityMask();
    vec3 N = GetNormal();
    vec3 V = normalize(camPos.xyz - thePosition);
    vec3 R = reflect(-V, N);

    vec3 F0 = mix(vec3(0.04), albedo_f, metallic_f);
    float shadow = shadow_directional ?
                   (use_csm_shadows ? ShadowCalculationCSM(vec4(thePosition, 1.0), normalize(-lightDirections[0].xyz), N)
                                     : ShadowCalculation(LightSpacePos, normalize(-lightDirections[0].xyz), N))
                                     : ShadowCalculationCubeMap(thePosition, N, lightPositions[0].xyz);

    vec3 Lo = vec3(0.0);
    ivec2 tileID = ivec2(gl_FragCoord.xy) / ivec2(16);
    uint offset = uint(tileID.y * numberOfTilesX + tileID.x) * 1024u;

    for (int i = 0; i < num_lights; ++i) {
        if (!isActive[i]) continue;

        if (forward_plus) {
            bool found = false;
            for (int j = 0; j < 1024 && visibleLightIndicesBuffer.data[offset + j] != -1; ++j) {
                if (visibleLightIndicesBuffer.data[offset + j] == i) { found = true; break; }
            }
            if (!found) continue;
        }

        float distance = length(lightPositions[i].xyz - thePosition);

        if (isAreaLight[i] && distance < radius[i]) {
            vec4 lightTranslate = lightPositions[i];
			AreaLight areaLight;
			areaLight.points[0] = points[i][0];
			areaLight.points[1] = points[i][1];
			areaLight.points[2] = points[i][2];
			areaLight.points[3] = points[i][3];
			areaLight.intensity = intensity[i];
			areaLight.twoSided = twoSided[i];
			areaLight.color = lightColors[i];
            Lo += CalculateAreaLight(areaLight, lightTranslate, albedo_f, N, roughness_f).rgb;
            continue;
        }

        vec3 L = normalize(lightPositions[i].xyz - thePosition);
        vec3 H = normalize(V + L);
        float attenuation = 1.0 / (constant[i] + linear[i]*distance + quadratic[i]*distance*distance);
        vec3 radiance = lightColors[i].rgb * attenuation;

        float NDF = DistributionGGX(N, H, roughness_f);
        float G = GeometrySmith(N, V, L, roughness_f);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        vec3 specular = (NDF * G * F) / max(4.0 * max(dot(N,V),0.0) * max(dot(N,L),0.0), 0.001);
        vec3 kS = F;
        vec3 kD = (1.0 - kS) * (1.0 - metallic_f);
        float NdotL = max(dot(N, L), 0.0);
        vec3 lightResult = (kD * albedo_f / PI + specular) * radiance * NdotL;

        if (flash[i]) {
            float theta = dot(L, normalize(-lightDirections[i].xyz));
            float epsilon = cutOff[i] - outerCutOff[i];
            float spotIntensity = clamp((theta - outerCutOff[i]) / epsilon, 0.0, 1.0);
            lightResult *= spotIntensity;
        }

        Lo += lightResult;
    }

    vec3 indirectBounce = 0.1 * albedo_f;
	float shadowFactor = clamp(pow(shadow, 1.5), 0.0, 1.0); // boost contrast without going out of bounds
	vec3 color = Lo * shadowFactor + indirectBounce;

    if (ibl_on) {
        vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness_f);
        vec3 kS = F;
        vec3 kD = (1.0 - kS) * (1.0 - metallic_f);

        vec3 irradiance = texture(irradianceMap, N).rgb;
        vec3 diffuse = irradiance * albedo_f;

        vec3 prefilteredColor = textureLod(prefilterMap, R, roughness_f * 4.0).rgb;
        vec2 brdf = texture(brdfLUT, vec2(max(dot(N, V), 0.0), roughness_f)).rg;
        vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);

        color += (kD * diffuse + specular) * ao_f * ibl_influance;
    } else {
        color += vec3(0.05) * albedo_f * ao_f;
    }

    color += GetEmissive();
    float ao_ssao = GetSSAO();

    FragColor = vec4(color * ao_ssao, alpha * opacity_f);

    if (Fog.fog_on) {
        float dist = abs((view * vec4(thePosition, 1.0)).z);
        float fogFactor = clamp(exp(-pow(dist * Fog.density, Fog.gradient)), 0.0, 1.0);
        FragColor.rgb = mix(Fog.color.rgb, FragColor.rgb, fogFactor);
    }

    mask = 1.0 - roughness_f;
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
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
    return F0 + (1.0 - F0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
} 

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
