#version 330 core
layout(location=0) in vec2  aPos;     // virtual px
layout(location=1) in vec2  aUV;      // normalized
layout(location=2) in uint  aColor;   // packed 0xRRGGBBAA (integer attrib via glVertexAttribIPointer)
layout(location=3) in vec2  aAux;     // reserved

uniform vec4 uVirtualToClip;          // (Sx, Sy, Ox, Oy)
uniform bool invert_y = false;

out vec2 vUV;
out vec4 vColor;
out vec2 vClipXY;                     // optional debug

vec4 unpackRGBA8(uint c) {
    float r = float((c >> 24u) & 0xFFu) / 255.0;
    float g = float((c >> 16u) & 0xFFu) / 255.0;
    float b = float((c >>  8u) & 0xFFu) / 255.0;
    float a = float((c       ) & 0xFFu) / 255.0;
    return vec4(r,g,b,a);
}

void main()
{
    float x = aPos.x * uVirtualToClip.x + uVirtualToClip.z;
    float y = aPos.y * uVirtualToClip.y + uVirtualToClip.w;
    gl_Position = vec4(x, y, 0.0, 1.0);

    vUV    = aUV;
    if(invert_y)
      vUV.y = 1. - vUV.y;
    vColor = unpackRGBA8(aColor);
    vClipXY = vec2(x, y);
}

