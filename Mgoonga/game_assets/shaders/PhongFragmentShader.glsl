#version 460 core

layout (location = 0) out vec4 outColor;
layout (location = 1) out float mask;

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

in vec3 theColor;
in vec3 theNormal;
in vec3 thePosition;
in vec2 Texcoord;

in vec4 LightSpacePos;
in vec4 LocalSpacePos;
in vec3 LocalSpaceNormal;
in mat3 TBN;
in vec4 debug;

uniform Light light;

struct FogInfo
{
  float maxDist;
  float minDist;
  vec4 color;
  bool fog_on;
  float density;
  float gradient;
};
uniform FogInfo Fog;

subroutine vec3 LightingPtr(Light light, vec3 normal, vec3 thePosition, vec3 diffuseTexture, vec2 Texcoords);
subroutine uniform LightingPtr LightingFunction;

layout(binding=0) uniform samplerCube 	   depth_cube_map;// Shadow point
layout(binding=1) uniform sampler2D	   	   depth_texture; // Shadow dir

layout(binding=2) uniform sampler2D        texture_diffuse1;
layout(binding=3) uniform sampler2D        texture_specular1;
layout(binding=4) uniform sampler2D        texture_normal1;
layout(binding=5) uniform sampler2D        texture_depth1;
layout(binding=6) uniform sampler2D        texture_emissionl;

layout(binding=12) uniform sampler2DArray  texture_array_albedo;
layout(binding=13) uniform sampler2DArray  texture_array_csm;

layout(binding=14) uniform sampler2D       texture_ssao;
layout(binding=15) uniform sampler3D 	   texture_pcf_offsets;

uniform vec4 eyePositionWorld;
uniform bool normalMapping = true;
uniform bool shadow_directional = true;
uniform bool use_csm_shadows = false;

uniform float shininess = 32.0f;
uniform float ssao_threshold = 0.9f;
uniform float ssao_strength = 0.6f;

uniform bool gamma_correction = true;
uniform float emission_strength = 1.0f;

uniform bool debug_white_color = false;
uniform bool debug_white_texcoords = false;

//terrain
const int max_texture_array_size = 8;
const float epsilon = 0.001f;
uniform bool texture_blending = false;
uniform float min_height = 0.0f;
uniform float max_height = 1.0f;
uniform float base_start_heights[max_texture_array_size];
uniform int color_count = 4;
uniform float textureScale[max_texture_array_size];

//csm + pcss
uniform mat4 view;
uniform float far_plane;
uniform float farPlane;
uniform int cascadeCount;
uniform float[10] cascadePlaneDistances;
uniform float cascade_blend_distance = 0.5f;
uniform bool blendCascades = true;

uniform float max_penumbra = 3.f;
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

layout (std140, binding = 0) uniform LightSpaceMatrices
{
    mat4 lightSpaceMatrices[16];
	mat4 lightProjection[16];
};

vec4 lightPos = light.position;

#include "CommonShadow.glsl"

vec2  ParallaxMapping(vec2 texCoords, vec3 viewDir);

float inverseLerp(float a, float b, float value)
{
	return clamp((value-a)/(b-a), 0.0f, 1.0f);
}

vec3 triplaner(float layer, vec3 blendAxes)
{  
	int index = int(layer);
	vec3 scaledWorldPos = vec3(LocalSpacePos) / textureScale[index];
	if(gamma_correction)
	{
	  vec3 xProjection = vec3(pow(texture(texture_array_albedo, vec3(scaledWorldPos.yz, layer)).rgb, vec3(2.2f))) * abs(blendAxes.x);
	  vec3 yProjection = vec3(pow(texture(texture_array_albedo, vec3(scaledWorldPos.xz, layer)).rgb, vec3(2.2f))) * abs(blendAxes.y);
	  vec3 zProjection = vec3(pow(texture(texture_array_albedo, vec3(scaledWorldPos.xy, layer)).rgb, vec3(2.2f))) * abs(blendAxes.z);
	  //return xProjection + zProjection;
	  return xProjection + yProjection + zProjection;
	}
	else
	{
		vec3 xProjection = vec3(texture(texture_array_albedo, vec3(scaledWorldPos.yz, layer))) * blendAxes.x;
		vec3 yProjection = vec3(texture(texture_array_albedo, vec3(scaledWorldPos.xz, layer))) * blendAxes.y;
		vec3 zProjection = vec3(texture(texture_array_albedo, vec3(scaledWorldPos.xy, layer))) * blendAxes.z;
		return xProjection + yProjection + zProjection;
	}
	//return vec3(pow(texture(texture_array_albedo, vec3(scaledWorldPos.yz, layer)).rgb, vec3(2.2f)));		
}

vec4 SampleAlbedoTexture(vec2 TexCoords)
{
	if(!texture_blending)
	{
	 vec4 color = texture(texture_diffuse1, TexCoords);
	  if(gamma_correction)
		return vec4(pow(texture(texture_diffuse1, TexCoords).rgb, vec3(2.2f)), color.a);
	  else
		return color;
	}
	else
	{
		vec3 colorAlbedo;
		float heightPercent = inverseLerp(min_height, max_height, LocalSpacePos.y);
		
		vec3 blendAxes = LocalSpaceNormal;
		blendAxes = normalize(blendAxes);
		
		for(int i = 0; i < color_count; ++i)
		{
			//float drawStrength = inverseLerp(-base_blends[i]/2 - epsilon, base_blends[i]/2, heightPercent-base_start_heights[i]);
			//vec3 texture_color = triplaner(i, blendAxes);
			//colorAlbedo = colorAlbedo * (1-drawStrength) + texture_color * drawStrength;
			
			if(heightPercent >= base_start_heights[i] && heightPercent <= base_start_heights[i+1])
			{
				vec3 colorMain = triplaner(i, blendAxes);
				if(base_start_heights[i+1] - heightPercent < 0.05f)
				{
					float mixing = 1.0f / 0.05f;
					vec3 colorMix = triplaner(i+1, blendAxes);
					colorAlbedo = mix(colorMix, colorMain, (base_start_heights[i+1] - heightPercent) * mixing);
				}
				else
					colorAlbedo = colorMain;
			}
		}
		return vec4(colorAlbedo ,1.0f); // no transparensy
	}
}

vec3 CalculateAlbedo(vec3  lightVector, vec3 normal, vec3 diffuseTexture)
{
	float Brightness   = clamp(dot(lightVector, normal), 0, 1);
	if(!texture_blending)
	{
		return vec3(light.diffuse.xyz * Brightness * diffuseTexture);
	}
	else
	{
		return vec3(light.diffuse.xyz * Brightness * diffuseTexture);
	}
}

vec3 SampleSpecularTexture(vec2 TexCoords)
{
	if(!texture_blending)
	{
		// no gamma_correction for specular ?
		return vec3(texture(texture_specular1, TexCoords));
	}
	else
	{
		return vec3(SampleAlbedoTexture(TexCoords)); // ?
	}
}

subroutine(LightingPtr) vec3 calculatePhongPointSpecDif(Light light, vec3 normal, vec3 thePosition, vec3 diffuseTexture, vec2 Texcoords)
{
  //Diffuse
  vec3  lightVector  = normalize(vec3(light.position) - thePosition);
  vec3  diffuseLight = CalculateAlbedo(lightVector, normal, diffuseTexture);

  //Specular
  vec3 Reflaction = reflect(-lightVector,normal);
  vec3 eyeVector  = normalize(eyePositionWorld.xyz - thePosition);
  float spec      = clamp(dot(Reflaction,eyeVector), 0, 1);

  spec = pow(spec, shininess);
  vec3 specularLight = vec3(light.specular.xyz * spec * SampleSpecularTexture(Texcoords));
  specularLight=clamp(specularLight,0,1);

  // Attenuation
  float distance    = length(vec3(light.position) - thePosition);
  float attenuation = 1.0f /(light.constant + light.linear * distance + light.quadratic * (distance * distance));
 
  diffuseLight  *= attenuation;
  specularLight *= attenuation;

  return diffuseLight + specularLight;
}

subroutine(LightingPtr) vec3 calculatePhongDirectionalSpecDif(Light light, vec3 normal, vec3 thePosition, vec3 diffuseTexture, vec2 Texcoords)
{
    vec3 lightVector = -normalize(vec3(light.direction));
    // diffuse shadingfloat 
	float Brightness  = clamp(dot(lightVector, normal), 0, 1);
	vec3 diffuseLight = CalculateAlbedo(lightVector, normal, diffuseTexture);
	
   // specular shading
    vec3 Reflaction = reflect(-lightVector, normal);
    vec3 eyeVector = normalize(eyePositionWorld.xyz - thePosition);
	float spec      = clamp(dot(Reflaction,eyeVector), 0, 1);	
    
	spec = pow(spec, shininess);   
	vec3 specularLight = light.specular.xyz * spec * SampleSpecularTexture(Texcoords);
	specularLight=clamp(specularLight,0,1);
	
    return diffuseLight + specularLight;
}

subroutine(LightingPtr) vec3 calculatePhongFlashSpecDif(Light light, vec3 normal, vec3 thePosition, vec3 diffuseTexture, vec2 Texcoords)
{
	vec3 lightDir= normalize(vec3(light.position)-thePosition);
	float theta     = dot(lightDir, normalize(-light.direction.xyz));
	float epsilon   = light.cutOff - light.outerCutOff;
	float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);

	if(theta > light.cutOff) 
	{
		vec3 ret = calculatePhongPointSpecDif(light, normal, thePosition, diffuseTexture, Texcoords);
		return ret *= intensity;
	}
	else
	 return vec3(0.0f,0.0f,0.0f);
}

subroutine(LightingPtr) vec3 calculateBlinnPhongPointSpecDif (Light light, vec3 normal, vec3 thePosition, vec3 diffuseTexture, vec2 Texcoords)
{
  //Diffuse
  vec3 lightVector  = normalize(vec3(light.position)-thePosition);
  float Brightness  = clamp(dot(lightVector, normal),0,1);
  vec3 diffuseLight = CalculateAlbedo(lightVector, normal, diffuseTexture);
  
  //Specular
  vec3 eyeVector= normalize(eyePositionWorld.xyz - thePosition);
  vec3 halfvector = normalize(eyeVector+lightVector);
  float spec=clamp(dot(normal, halfvector), 0, 1);
  spec=pow(spec, shininess);
  vec3 specularLight=vec3(light.specular.xyz *spec * SampleSpecularTexture(Texcoords));//* material.specular
  specularLight=clamp(specularLight,0,1);

  // Attenuation
  float distance    = length(vec3(light.position) - thePosition);
  float attenuation = 1.0f /(light.constant + light.linear * distance + light.quadratic * (distance * distance));
 
  diffuseLight  *= attenuation;
  specularLight *= attenuation;

  return diffuseLight + specularLight;
}

subroutine(LightingPtr) vec3 calculateBlinnPhongDirectionalSpecDif(Light light, vec3 normal, vec3 thePosition, vec3 diffuseTexture, vec2 Texcoords)
{
    vec3 lightVector = -normalize(vec3(light.direction));
    // diffuse shadingfloat 
	float Brightness  = clamp(dot(lightVector, normal), 0, 1);
	vec3 diffuseLight = CalculateAlbedo(lightVector, normal, diffuseTexture);
	
   //Specular
    vec3 eyeVector= normalize(eyePositionWorld.xyz - thePosition); 
    vec3 halfvector = normalize(eyeVector+lightVector);
    float spec = clamp(dot(normal, halfvector), 0, 1);
    spec=pow(spec, shininess);
    vec3 specularLight = vec3(light.specular.xyz * spec * SampleSpecularTexture(Texcoords));//* material.specular
    specularLight = clamp(specularLight,0,1);	
	
    return diffuseLight + specularLight;
}

subroutine(LightingPtr) vec3 calculateBlinnPhongFlashSpecDif(Light light, vec3 normal, vec3 thePosition, vec3 diffuseTexture, vec2 Texcoords)
{
	vec3 lightDir   = normalize(vec3(light.position)-thePosition);
	float theta     = dot(lightDir, normalize(-light.direction.xyz));
	float epsilon   = light.cutOff - light.outerCutOff;
	float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);

	vec3 ret = calculateBlinnPhongPointSpecDif(light, normal, thePosition, diffuseTexture, Texcoords);
	return ret *= intensity;
}

void main()
{   
  if(debug_white_texcoords)
	outColor = vec4(emission_strength, emission_strength, emission_strength, emission_strength); 
  else if(debug_white_color)
  {
	outColor = vec4(1.0f,1.0f,1.0f, 1.0f);
  }
  else
  {
	// Paralax mapping
	//vec3 tangetnViewDir   = normalize(TBN * eyePositionWorld.xyz - TBN * thePosition);
	//vec2 fTexCoords = ParallaxMapping(Texcoord,  tangetnViewDir);
	//if(fTexCoords.x > 1.0 || fTexCoords.y > 1.0 || fTexCoords.x < 0.0 || fTexCoords.y < 0.0)
		//discard;
	
	vec3 bNormal;
	if(normalMapping)
	{
		// Obtain normal from normal map in range [0,1]
		bNormal = texture(texture_normal1, Texcoord).rgb;
		// Transform normal vector to range [-1,1]
		bNormal = normalize(bNormal * 2.0 - 1.0);   
		bNormal = normalize(TBN * bNormal);
	}
	else
		bNormal = theNormal;

  //Ambient Occlusion
  ivec2 screenSize = textureSize(texture_ssao, 0);
  vec2 buf_relative_tex_coord = vec2(gl_FragCoord[0]/ screenSize.x, gl_FragCoord[1] / screenSize.y);
  float ambient_occlusion = texture(texture_ssao, buf_relative_tex_coord).r;
  
  if(ambient_occlusion < ssao_threshold)
    ambient_occlusion = ambient_occlusion * ssao_strength;
	
  vec4 dif_texture = SampleAlbedoTexture(Texcoord);
  if(dif_texture.a < 0.05f)
	discard;	
  vec3 ambientLight = light.ambient.xyz * vec3(dif_texture) * ambient_occlusion; 

  float shadow;
	 vec3 lightVector = -normalize(vec3(light.direction));
     if(shadow_directional && !use_csm_shadows)
		shadow =  ShadowCalculation(LightSpacePos, lightVector, bNormal);
     else if(shadow_directional && use_csm_shadows)
		shadow = ShadowCalculationCSM(vec4(thePosition, 1.0f), lightVector, bNormal);
     else
		shadow =  ShadowCalculationCubeMap(thePosition, bNormal, light.position.xyz);

  //Diffuse and Specular
  vec3 difspec = LightingFunction(light, bNormal, thePosition, vec3(dif_texture), Texcoord);

	outColor = vec4(ambientLight + difspec * shadow, 1.0);
	vec3 emissive_color = vec3(outColor) * vec3(texture(texture_emissionl, Texcoord)).r; // using texture_emissionl as mask
	outColor.rgb += (emissive_color * emission_strength);

	if(Fog.fog_on)
	{
		float dist = abs(vec4(view * vec4(thePosition, 1.0f)).z);
		float fogFactor = exp(-pow(dist * Fog.density, Fog.gradient));
		fogFactor = clamp( fogFactor, 0.0f, 1.0f );
		outColor.rgb = mix(Fog.color.rgb, outColor.rgb, fogFactor);
		outColor.a = dif_texture.a;
	}
	  
  }
  mask = 0.0f;
  
};

vec2 ParallaxMapping(vec2 texCoords, vec3 viewDir)
{
	float height_scale =0.1f;
	// number of depth layers
    const float numLayers = 10;
    // calculate the size of each layer
    float layerDepth = 1.0 / numLayers;
    // depth of current layer
    float currentLayerDepth = 0.0;
    // the amount to shift the texture coordinates per layer (from vector P)
    vec2 P = viewDir.xy * height_scale; 
    vec2 deltaTexCoords = P / numLayers;

	// get initial values
	vec2 currentTexCoords     = texCoords;
	float currentDepthMapValue = texture(texture_depth1, currentTexCoords).r;
  
	while(currentLayerDepth < currentDepthMapValue)
	{
		// shift texture coordinates along direction of P
		currentTexCoords -= deltaTexCoords;
		// get depthmap value at current texture coordinates
		currentDepthMapValue = texture(texture_depth1, currentTexCoords).r;  
		// get depth of next layer
		currentLayerDepth += layerDepth;  
	}
	return currentTexCoords;
}