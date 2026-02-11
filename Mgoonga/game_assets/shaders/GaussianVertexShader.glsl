#version 430

subroutine vec2 Blur(int i, vec2 centerTexCoords, float _pixelSize);
subroutine uniform Blur BlurFunction;

in layout(location=0) vec2 position;
in layout(location=1) vec2 texcoord;

uniform float pixelSize = 0.01f;
uniform int kernelSize = 5; // Use this to specify the kernel size: 5 for 11x11, 6 for 13x13, 7 for 15x15

out vec2 blurTextureCoords[15]; // Maximum size needed for 15x15 kernel

subroutine(Blur)
vec2 HorizontalBlur(int i, vec2 centerTexCoords, float _pixelSize) {
    return centerTexCoords + vec2(_pixelSize * i, 0.0f);
}

subroutine(Blur)
vec2 VerticalBlur(int i, vec2 centerTexCoords, float _pixelSize) {
    return centerTexCoords + vec2(0.0f, _pixelSize * i);
}

void main()
{
    gl_Position = vec4(position.x, position.y, 0.0f, 1.0f);
    vec2 centerTexCoords = vec2(position.xy) * 0.5 + 0.5;

    int offset = kernelSize;
    int size = 2 * kernelSize + 1;

    for (int i = -offset; i <= offset; ++i) {
        blurTextureCoords[i + offset] = BlurFunction(i, centerTexCoords, pixelSize);
    }
}