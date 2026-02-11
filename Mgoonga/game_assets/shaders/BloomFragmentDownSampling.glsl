#version 430

// This shader performs downsampling on a texture,
// as taken from Call Of Duty method, presented at ACM Siggraph 2014.
// This particular method was customly designed to eliminate
// "pulsating artifacts and temporal stability issues".

// Remember to add bilinear minification filter for this texture!
// Remember to use a floating-point texture format (for HDR)!
// Remember to use edge clamping for this texture!

layout(binding=0)uniform sampler2D srcTexture;
uniform vec2 srcResolution;
uniform bool useKarisAverage = false;  // Toggle Karis on/off
uniform float karisStrength = 0.75f; // 0.0 = off, 1.0 = full Karis
uniform float karisThreshold = 0.1f;  // Luma threshold to start applying Karis (e.g., 1.0)

in vec2 TexCoords;
layout (location = 0) out vec3 downsample;

vec3 KarisAverage(vec3 color) {
    float luma = max(0.0001, dot(color, vec3(0.2126, 0.7152, 0.0722))); // Or vec3(0.2126, 0.7152, 0.0722) or vec3(0.299, 0.587, 0.114)
    
	// Compute dynamic strength based on luma
    //float strength = smoothstep(karisThreshold, karisThreshold * 4.0, luma);
    //strength *= karisStrength;
	
	vec3 karisColor =  color / (1.0 + luma);
	return mix(color, karisColor, karisStrength);
}

vec2 safeUV(vec2 uv) {
    return clamp(uv, vec2(0.0), vec2(1.0));
}

vec3 sampleTexture(vec2 coords) {
    vec3 color = texture(srcTexture, safeUV(coords)).rgb;
    return useKarisAverage ? KarisAverage(color) : color;
}

void main()
{
    vec2 srcTexelSize = 1.0 / srcResolution;
    float x = srcTexelSize.x;
    float y = srcTexelSize.y;

    // Take 13 samples around current texel:
    // a - b - c
    // - j - k -
    // d - e - f
    // - l - m -
    // g - h - i
    // === ('e' is the current texel) ===
    vec3 a = sampleTexture(vec2(TexCoords.x - 2*x, TexCoords.y + 2*y)).rgb;
    vec3 b = sampleTexture(vec2(TexCoords.x,       TexCoords.y + 2*y)).rgb;
    vec3 c = sampleTexture(vec2(TexCoords.x + 2*x, TexCoords.y + 2*y)).rgb;

    vec3 d = sampleTexture(vec2(TexCoords.x - 2*x, TexCoords.y)).rgb;
    vec3 e = sampleTexture(vec2(TexCoords.x,       TexCoords.y)).rgb;
    vec3 f = sampleTexture(vec2(TexCoords.x + 2*x, TexCoords.y)).rgb;

    vec3 g = sampleTexture(vec2(TexCoords.x - 2*x, TexCoords.y - 2*y)).rgb;
    vec3 h = sampleTexture(vec2(TexCoords.x,       TexCoords.y - 2*y)).rgb;
    vec3 i = sampleTexture(vec2(TexCoords.x + 2*x, TexCoords.y - 2*y)).rgb;

    vec3 j = sampleTexture(vec2(TexCoords.x - x, TexCoords.y + y)).rgb;
    vec3 k = sampleTexture(vec2(TexCoords.x + x, TexCoords.y + y)).rgb;
    vec3 l = sampleTexture(vec2(TexCoords.x - x, TexCoords.y - y)).rgb;
    vec3 m = sampleTexture(vec2(TexCoords.x + x, TexCoords.y - y)).rgb;

    // Apply weighted distribution:
    // 0.5 + 0.125 + 0.125 + 0.125 + 0.125 = 1
    // a,b,d,e * 0.125
    // b,c,e,f * 0.125
    // d,e,g,h * 0.125
    // e,f,h,i * 0.125
    // j,k,l,m * 0.5
    // This shows 5 square areas that are being sampled. But some of them overlap,
    // so to have an energy preserving downsample we need to make some adjustments.
    // The weights are the distributed, so that the sum of j,k,l,m (e.g.)
    // contribute 0.5 to the final color output. The code below is written
    // to effectively yield this sum. We get:
    // 0.125*5 + 0.03125*4 + 0.0625*4 = 1
    downsample = e*0.125;
    downsample += (a+c+g+i)*0.03125;
    downsample += (b+d+f+h)*0.0625;
    downsample += (j+k+l+m)*0.125;
	
	// === Border Fade ===
    // Fades bloom strength smoothly near edges
    vec2 border = smoothstep(0.0, 0.02, 1.0 - abs(TexCoords - 0.5) * 2.0);
	float borderFade = border.x * border.y;
	downsample *= borderFade;
}