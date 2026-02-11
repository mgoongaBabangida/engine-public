#version 430
layout (local_size_x = 1, local_size_y = 1) in;

layout(binding = 0) uniform sampler2D luminanceTexture;  // mipmapped RED16F
layout(binding = 1, rgba16f) uniform image2D exposureOutput; // 1x1 texture holding previous exposure

uniform float targetLuminance = 0.4;
uniform float adaptationRate = 1.0;
uniform float deltaTime;

void main() {
    int maxMip = textureQueryLevels(luminanceTexture) - 1;
    float avgLum = texelFetch(luminanceTexture, ivec2(0), 0).r;	
    avgLum = max(avgLum, 1e-4); // protect against divide by zero

    float desiredExposure = clamp(targetLuminance / avgLum, 0.001, 10.0); // safe range

    vec4 prevExposure = imageLoad(exposureOutput, ivec2(0));
    float adapted = clamp(
        prevExposure.r + (desiredExposure - prevExposure.r) * (1.0 - exp(-adaptationRate * deltaTime)),
        0.01, 10.0
    );

    imageStore(exposureOutput, ivec2(0), vec4(adapted).rrrr); // ensure alpha = 1
}
