#version 430

layout(binding=1) uniform sampler2D originalTexture;

layout(location = 0) out vec4 out_colour;  // Color thresholded (for bloom)
layout(location = 1) out float out_brightness; // Luminance (for exposure)

in vec2 TexCoords;

uniform float amplifier = 1.0f;

void main(void)
{
	 vec4 color = texture(originalTexture, TexCoords);
	 float brightness = (color.r * 0.2126) + (color.g * 0.7152) + (color.b * 0.0722);
	 out_colour = color * brightness * amplifier;
	 out_brightness = brightness;
};