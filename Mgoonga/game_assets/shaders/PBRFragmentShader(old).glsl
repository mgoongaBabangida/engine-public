#version 460 core

layout(location = 0) out vec4 FragColor;
layout(location = 1) out float mask;

in vec2 Texcoord;
in vec3 thePosition; //WorldPos
in vec3 theNormal;   //theNormal
in mat3 TBN;
in vec4 LightSpacePos; //shadows

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
uniform bool isActive[max_lights];

uniform bool  forward_plus;
uniform float ibl_influance = 1.f;

struct FogInfo{float maxDist;float minDist;vec4 color;bool fog_on;float density;float gradient;};
uniform FogInfo Fog;

// Textures
layout(binding=0)  uniform samplerCube  depth_cube_map;
layout(binding=1)  uniform sampler2D    depth_texture;
layout(binding=2)  uniform sampler2D    albedoMap;
layout(binding=3)  uniform sampler2D    metallicMap;
layout(binding=4)  uniform sampler2D    normalMap;
layout(binding=5)  uniform sampler2D    roughnessMap;
layout(binding=6)  uniform sampler2D    emissionMap;
layout(binding=7) uniform sampler2D LTC1; // for inverse M
layout(binding=8) uniform sampler2D LTC2; // GGX norm, fresnel, 0(unused), sphere
layout(binding=9)  uniform samplerCube irradianceMap;
layout(binding=10) uniform samplerCube prefilterMap;
layout(binding=11) uniform sampler2D   brdfLUT;
layout(binding=13) uniform sampler2DArray  texture_array_csm;
layout(binding=14) uniform sampler2D    texture_ssao;
layout(binding=15) uniform sampler3D 	texture_pcf_offsets;
layout(binding=16) uniform sampler2D    aoMap;
layout(binding=18) uniform sampler2D    opacityMask;

struct PointLight {
	vec4 color;
	vec4 position;
	vec4 paddingAndRadius;
};

struct VisibleIndex {
	int index;
};

layout(std430, binding = 1) readonly buffer VisibleLightIndicesBuffer {
	VisibleIndex data[];
} visibleLightIndicesBuffer;

struct AreaLight
{
    float intensity;
    vec4 color;
    vec4 points[4];
    bool twoSided;
};

const float PI = 3.14159265359;
  
float DistributionGGX(vec3 N, vec3 H, float roughness);
float GeometrySchlickGGX(float NdotV, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);
vec3 fresnelSchlick(float cosTheta, vec3 F0);
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness);

layout (std140, binding = 0) uniform LightSpaceMatrices
{
    mat4 lightSpaceMatrices[16];
	mat4 lightProjection[16];
};

vec4 lightPos = lightPositions[0];

#include "CommonShadow.glsl"
#include "CommonAreaLight.glsl"

bool isAnyNan(vec3 v) { return isnan(v.x) || isnan(v.y) || isnan(v.z);}

void main()
{	  
   // Determine which tile this pixel belongs to
   ivec2 location = ivec2(gl_FragCoord.xy);
   ivec2 tileID = location / ivec2(16, 16);
   uint index = tileID.y * numberOfTilesX + tileID.x;
   uint offset = index * 1024;
   
   vec3 albedo_f;
   vec3 theNormal_f;
   float metallic_f;
   float roughness_f;
   float ao_f;
   float alpha = 1.;
   
   if(textured)
   {
   	 vec4 albedo = texture(albedoMap, Texcoord);
     if(gamma_correction)
       albedo_f    = pow(albedo.rgb, vec3(2.2));
     else
	    albedo_f    = albedo.rgb;
	 alpha = albedo.a;
   }
   else
	  albedo_f = albedo.xyz;
   
   if(alpha < 0.05f)
	discard;	
	
   if(use_metalic_texture)
    metallic_f  = texture(metallicMap, Texcoord).r;
   else
    metallic_f = metallic;

    if(use_normalmap_texture)
    {
      theNormal_f = texture(normalMap, Texcoord).rgb;
	  // Transform normal vector to range [-1,1]
	  theNormal_f = normalize(theNormal_f * 2.0 - 1.0);
	  theNormal_f = TBN * theNormal_f;
	  if(isAnyNan(theNormal_f))
		theNormal_f = theNormal;
    }
    else
      theNormal_f = theNormal;

   if(use_roughness_texture)
    roughness_f  =  texture(roughnessMap, Texcoord).r;
   else
    roughness_f = roughness;

   roughness_f = clamp(roughness_f, 0.14f, 1.0f);

   if(use_ao_texture)
    ao_f  =  texture(aoMap, Texcoord).r;
   else
    ao_f = ao;
	
	vec3 N = normalize(theNormal_f);
	vec3 V = normalize(camPos.xyz - thePosition);
	vec3 R = reflect(-V, N);
	
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo_f, metallic_f);
	 
	float shadow;
	vec3 lightVector = -normalize(vec3(lightDirections[0]));	 // @should be for every light
	if(shadow_directional && !use_csm_shadows)
	  shadow =  ShadowCalculation(LightSpacePos, lightVector, N);
	else if(shadow_directional && use_csm_shadows)
	  shadow = ShadowCalculationCSM(vec4(thePosition, 1.0f), lightVector, N);
	else
	  shadow =  ShadowCalculationCubeMap(thePosition, N, lightPositions[0].xyz);
	 
    // reflectance equation
    vec3 Lo = vec3(0.0);
    for(int i = 0; i < num_lights; ++i)
    {
	  if(!isActive[i])
		continue;
	
	 if(forward_plus)
	 {
	   //check if the light is concerned
	   bool isConcerned = false;
	   for(int j = 0; j < 1024 && visibleLightIndicesBuffer.data[offset + j].index != -1; ++j)
	   {
		 if(visibleLightIndicesBuffer.data[offset + j].index == i)
			isConcerned = true;
	   }
	   
	   if(!isConcerned)
		continue;
	 }
	  
	  float distance    = length(lightPositions[i].xyz - thePosition);
	  if(isAreaLight[i])
	  {
		if(distance < radius[i])
		{
		  AreaLight areaLight;
		  areaLight.points[0] = points[i][0];
		  areaLight.points[1] = points[i][1];
		  areaLight.points[2] = points[i][2];
		  areaLight.points[3] = points[i][3];
		  areaLight.intensity = intensity[i];
		  areaLight.twoSided = twoSided[i];
		  areaLight.color = lightColors[i];
		  Lo += vec3(CalculateAreaLight(areaLight, lightPositions[i], albedo_f, N, roughness_f));
		}
	  }
	  else
	  {
		// calculate per-light radiance
		vec3 L = normalize(lightPositions[i].xyz - thePosition);
		vec3 H = normalize(V + L);
		float attenuation = 1.0f /(constant[i] + linear[i] * distance + quadratic[i] * (distance * distance));
		vec3 radiance     = lightColors[i].xyz * attenuation;
		
		// cook-torrance brdf
		float NDF = DistributionGGX(N, H, roughness_f);
		float G   = GeometrySmith(N, V, L, roughness_f);
		vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);
	
		vec3 numerator    = NDF * G * F;
		float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
		vec3 specular     = numerator / max(denominator, 0.001); 
	
		vec3 kS = F;
		vec3 kD = vec3(1.0) - kS;
		kD *= 1.0 - metallic_f;
			
		// add to outgoing radiance Lo
		float NdotL = max(dot(N, L), 0.0);
		vec3 outgoing_radiance = (kD * albedo_f / PI + specular) * radiance * NdotL;
		if(flash[i])
		{
		  float theta     = dot(L, normalize(-lightDirections[i].xyz));
		  float epsilon   = cutOff[i] - outerCutOff[i];
		  float intensity = clamp((theta - outerCutOff[i]) / epsilon, 0.0, 1.0);
		  outgoing_radiance *= intensity;
		}
		Lo += outgoing_radiance;
	  }
    }
		
  float shadowStrength = 4.;
  vec3 indirectBounce = 0.1 * albedo_f;
  vec3 color = Lo * mix(1.0, shadow, shadowStrength) + indirectBounce;
  if(ibl_on)
  {
  	// ambient lighting (we now use IBL as the ambient term)
    vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness_f);
	vec3 kS = F;
	vec3 kD = 1.0 - kS;
	kD *= 1.0 - metallic_f;

  	vec3 irradiance = texture(irradianceMap, N).rgb;
	vec3 diffuse    = irradiance * albedo_f;

	  // sample both the pre-filter map and the BRDF lut and combine them together as per the Split-Sum approximation to get the IBL specular part.
    const float MAX_REFLECTION_LOD = 4.0;
    vec3 prefilteredColor = textureLod(prefilterMap, R,  roughness_f * MAX_REFLECTION_LOD).rgb;
    vec2 brdf  = texture(brdfLUT, vec2(max(dot(N, V), 0.0), roughness_f)).rg;
    vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);
	  
	vec3 ambient = (kD * diffuse + specular) * ao_f * ibl_influance;
    color = color + ambient;
  }
  else
  {
   vec3 ambient = vec3(0.05) * albedo_f * ao_f;
   color = color + ambient;
  }
	
   vec3 emissive_color = vec3(emission_color) * texture(emissionMap, Texcoord).r; // using texture_emissionl as mask 
   color.rgb += (emissive_color * emission_strength);

  //Ambient Occlusion
  ivec2 screenSize = textureSize(texture_ssao, 0);
  vec2 buf_relative_tex_coord = vec2(gl_FragCoord[0]/ screenSize.x, gl_FragCoord[1] / screenSize.y);
  float ambient_occlusion = texture(texture_ssao, buf_relative_tex_coord).r;
  
  if(ambient_occlusion < ssao_threshold)
    ambient_occlusion = ambient_occlusion * ssao_strength;

  float opacity_f = opacity;
  if(use_opacity_mask)
	opacity_f = texture(opacityMask, Texcoord).r;
  FragColor = vec4(color * ambient_occlusion, alpha * opacity_f);

  if(Fog.fog_on)
  {
   float dist = abs(vec4(view * vec4(thePosition, 1.0f)).z);
   float fogFactor = exp(-pow(dist * Fog.density, Fog.gradient));
   fogFactor = clamp( fogFactor, 0.0f, 1.0f );
   FragColor.rgb = mix(Fog.color.rgb, FragColor.rgb, fogFactor);
  }

  mask = 1.0f - roughness_f;
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
