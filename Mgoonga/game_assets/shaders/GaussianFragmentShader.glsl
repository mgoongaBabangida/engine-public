#version 430

in vec2 blurTextureCoords[15]; // Match the maximum size from the vertex shader

layout(binding=1) uniform sampler2D originalTexture;

uniform int kernelSize = 5; // Same as in the vertex shader

const float weights11[11] = float[](0.0093f, 0.028002f, 0.065984f, 0.121703f, 0.175713f, 0.198596f, 0.175713f, 0.121703f, 0.065984f, 0.028002f, 0.0093f);
const float weights13[13] = float[](0.015, 0.034, 0.067, 0.113, 0.164, 0.204, 0.227, 0.204, 0.164, 0.113, 0.067, 0.034, 0.015);
const float weights15[15] = float[](0.011, 0.023, 0.045, 0.080, 0.127, 0.169, 0.198, 0.210, 0.198, 0.169, 0.127, 0.080, 0.045, 0.023, 0.011);

out vec4 fragColor;

void main() {
    vec4 color = vec4(0.0);

    int offset = kernelSize;
    int size = 2 * kernelSize + 1;

    if (kernelSize == 5) {
        for (int i = 0; i < size; ++i) {
            color += texture(originalTexture, blurTextureCoords[i]) * weights11[i];
        }
    } else if (kernelSize == 6) {
        for (int i = 0; i < size; ++i) {
            color += texture(originalTexture, blurTextureCoords[i]) * weights13[i];
        }
    } else  { // if (kernelSize == 7)
        for (int i = 0; i < size; ++i) {
            color += texture(originalTexture, blurTextureCoords[i]) * weights15[i];
        }
    }
    fragColor = color;
}