#version 430

in vec2 TexCoords;

out vec4 color;

uniform bool frame = false;
uniform bool blend = false;
uniform bool kernel = false;

uniform float blurCoef = 1.0f;
uniform bool gamma_correction = true;
uniform float hdr_exposure = 1.0f;
uniform bool auto_exposure = true;
uniform int tone_mapping_type = 1; // 0 = none, 1 = Reinhard, 2 = ACES

uniform vec2 CursorPos;

uniform vec4 solidColor = vec4(0., 0., 0.5, 0.85);

uniform vec3 colorStart = vec3(1., 1., 0.);
uniform vec3 colorEnd = vec3(1., 0., 1.);
uniform vec2 bezierP1 = vec2(0.645, 0.045);
uniform vec2 bezierP2 = vec2(0.355, 1.0);

// Rounded corner parameters
uniform float cornerRadius = 0.0f;     // In UV units (0.0 - 0.5)
uniform float aspectRatio = 1.6f;      // width / height of screen/plane

const vec2 p0 = vec2(0.0, 0.0);
const vec2 p3 = vec2(1.0, 1.0);

layout(binding=1) uniform sampler2D screenTexture;
layout(binding=2) uniform sampler2D contrastTexture;
layout(binding=3) uniform sampler2D alphaMask;
layout(binding=4) uniform sampler2D exposureTex;

#include "GradientFunc.glsl"

subroutine vec4 ColorCalculationPtr();
subroutine uniform ColorCalculationPtr ColorFunction;

subroutine(ColorCalculationPtr) vec4 DefaultColor()
{
 return texture(screenTexture, TexCoords);
}

subroutine(ColorCalculationPtr) vec4 SolidColor()
{
  vec4 sampleColor = texture(screenTexture, TexCoords);
  if(sampleColor.a < 0.5f)
     return sampleColor;
  else
     return vec4(0., 0., 0.5, 0.85);
}

subroutine(ColorCalculationPtr) vec4 TestColor()
{
	float dist = distance(gl_FragCoord.xy, CursorPos);
	float alpha = texture(alphaMask, TexCoords).a;
	if(dist < 30.0f && alpha > 0.0f)
		return texture(screenTexture, TexCoords);
	else
		return texture(screenTexture, TexCoords) * 0.9f;
}	

subroutine(ColorCalculationPtr) vec4 GreyKernelColor()
{
	vec4 col = texture(screenTexture, TexCoords);
	float average = 0.2126 * col.r + 0.7152 * col.g + 0.0722 * col.b;
	return vec4(average, average, average, 1.0);
}

subroutine(ColorCalculationPtr) vec4 MaskBlendColor()
{
	vec4 mainCol     = texture(screenTexture, TexCoords);
	vec4 blurCol     = texture(contrastTexture, TexCoords); // blurred SSR
	float roughMask  = texture(alphaMask, TexCoords).r; // 1 = reflective material
	
	// Optionally smooth roughness to avoid hard edges
	roughMask = smoothstep(0.05, 1.0, roughMask);
			
	// Calculate final mask with softness: avoid harsh cutoffs
	float blendFactor = roughMask; //full blend only when both are 1
	
	// You can even curve it slightly if needed:
	blendFactor = pow(blendFactor, 0.75); // optional

	vec4 contrastCol = blurCol + mainCol;
	
	// Final result: mix blurred SSR with original screen using smooth mask
	return mix(mainCol, contrastCol, blendFactor);
}

subroutine(ColorCalculationPtr) vec4 GradientColor()
{
	float t = findTForX(TexCoords.y, bezierP1, bezierP2);
    float y = cubicBezier(p0, bezierP1, bezierP2, p3, t).y;
    vec3 colorMix = mix(colorStart, colorEnd, y);

    float alpha = cornerMask(TexCoords, 0.08, 0.5);
	if(alpha > 0.2)
		alpha = 1.0;

    return vec4(colorMix, alpha);
}

vec3 ReinhardTonemap(vec3 color)
{
    // exposure must be applied before this
    return color / (color + vec3(1.0));
}

vec3 ACESFittedTonemap(vec3 color)
{
    // This is a close approximation of ACES RRT+ODT fit used in Unreal Engine
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;

    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

//https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/ACES.hlsl
/*
=================================================================================================

  Baking Lab
  by MJP and David Neubelt
  http://mynameismjp.wordpress.com/

  All code licensed under the MIT license

=================================================================================================
 The code in this file was originally written by Stephen Hill (@self_shadow), who deserves all
 credit for coming up with this fit and implementing it. Buy him a beer next time you see him. :)
*/

// sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
mat3x3 ACESInputMat = mat3x3
(
	0.59719, 0.35458, 0.04823,
	0.07600, 0.90834, 0.01566,
	0.02840, 0.13383, 0.83777
);

// ODT_SAT => XYZ => D60_2_D65 => sRGB
mat3x3 ACESOutputMat = mat3x3
(
	 1.60475, -0.53108, -0.07367,
	-0.10208,  1.10813, -0.00605,
	-0.00327, -0.07276,  1.07602
);

vec3 RRTAndODTFit(vec3 v)
{
	vec3 a = v * (v + 0.0245786f) - 0.000090537f;
	vec3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
	return a / b;
}

vec3 ACESFitted(vec3 color)
{
	color = transpose(ACESInputMat) * color;
	// Apply RRT and ODT
	color = RRTAndODTFit(color);
	color = transpose(ACESOutputMat) * color;
	color = clamp(color, 0, 1);
	return color;
}

void main()
{
	vec4 col;
	vec4 contr;
	if( frame )
	 col = vec4(1.0, 1.0, 0.0, 1.0);
	else if( blend )
	{
	   col = texture(screenTexture, TexCoords);
	   contr = texture(contrastTexture, TexCoords);
		
	   col = col + (contr * clamp(blurCoef,0,1));
		
	   if(tone_mapping_type > 0)
	   {
		float exposure = hdr_exposure;
		if (auto_exposure)
		{
		  exposure = texture(exposureTex, vec2(0.5)).r;
		}
		// Apply tonemapping based on selected type
		if (tone_mapping_type == 1)
		{
			col.rgb = vec3(1.0) - exp(-col.rgb * exposure);
		}
		else if(tone_mapping_type == 2)
		{
			col.rgb *= exposure;
			col.rgb = ReinhardTonemap(col.rgb);
		}
		else if (tone_mapping_type == 3)
		{
			col.rgb *= exposure;
			col.rgb = ACESFitted(col.rgb);
		}
		else if(tone_mapping_type == 4)
		{
			col.rgb *= exposure;
			col.rgb = ACESFittedTonemap(col.rgb);
		}
	   }
	   if(gamma_correction)
		col.rgb = pow(col.rgb, vec3(1.0/2.2f));
	}
	else if( kernel )
	{
		col = texture(screenTexture, TexCoords);
		float average = 0.2126 * col.r + 0.7152 * col.g + 0.0722 * col.b;
		col = vec4(average, average, average, 1.0);
	}
    else
	{
		col = ColorFunction();
	}
   color = col;
}