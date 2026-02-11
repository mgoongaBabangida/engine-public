#version 430

out vec4 FragColor;
in vec3 WorldPos;

layout(binding=2) uniform sampler2D equirectangularMap;

const vec2 invAtan = vec2(0.1591, 0.3183);

vec2 SampleSphericalMap(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main()
{		
    vec2 uv = SampleSphericalMap(normalize(WorldPos));
    vec3 color = texture(equirectangularMap, uv).rgb;

    //Tone-map to control overly bright areas (especially sun/hot spots)
    color = color / (color + vec3(1.0)); // Reinhard tone mapping
    // Optional gamma correction for previewing:
    // color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}
