#version 430

layout(location = 0) out vec4 out_colour;

in vec2 Texcoord;

layout(binding=2) uniform sampler2D albedoMap;
layout(binding=18) uniform sampler2D opacityMask;

void main(void)
{
	float alpha = texture(opacityMask, Texcoord).r;
	if (alpha < 0.9) 
		discard;
	vec4 albedo = texture(albedoMap, Texcoord);
	out_colour = vec4(albedo.r, albedo.g, albedo.b ,alpha);
};