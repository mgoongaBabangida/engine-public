#version 430
in vec2 vUV;
in vec4 vColor;

layout(binding=1) uniform sampler2D uTex1;   // atlas in texture unit 1
// (You can add uTex2/uTex3 for mask or effects later)

out vec4 fragColor;

void main(){
  vec4 tex = texture(uTex1, vUV);   // assume premultiplied or straight; you set blend func
  fragColor = tex * vColor;         // premult alpha path prefers ONE, ONE_MINUS_SRC_ALPHA
}
