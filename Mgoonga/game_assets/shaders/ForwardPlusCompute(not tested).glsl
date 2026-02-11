#version 430

#define TILE_SIZE 16
#define MAX_LIGHTS_PER_TILE 128

layout (local_size_x = TILE_SIZE, local_size_y = TILE_SIZE) in;

struct Light {
    vec4 position;  position.xyz is the position, position.w is the radius
    vec4 color;     color.rgb is the color
};

layout (std430, binding = 0) buffer Lights {
    Light lights[];
};

layout (std430, binding = 1) buffer TileLights {
    uint lightIndices[];
    uvec2 tileLightCount[];
};

uniform int numLights;
uniform vec2 screenSize;
uniform mat4 projectionMatrix;

void main() {
    uint tileX = gl_GlobalInvocationID.x;
    uint tileY = gl_GlobalInvocationID.y;
    uint tileIndex = tileY  gl_NumWorkGroups.x + tileX;

    uvec2 tileCoord = uvec2(tileX, tileY);
    vec2 tileMin = vec2(tileCoord)  TILE_SIZE;
    vec2 tileMax = tileMin + vec2(TILE_SIZE);

    for (int i = 0; i  numLights; ++i) {
        Light light = lights[i];

         Transform light position to clip space
        vec4 clipSpacePos = projectionMatrix  vec4(light.position.xyz, 1.0);

         Perspective divide to get NDC
        vec3 ndcPos = clipSpacePos.xyz  clipSpacePos.w;

         Convert NDC to screen coordinates
        vec2 lightScreenPos = (ndcPos.xy  0.5 + 0.5)  screenSize;

         Calculate the light radius in screen space
        vec4 radiusClipPos = projectionMatrix  vec4(light.position.xyz + vec3(light.position.w, 0.0, 0.0), 1.0);
        vec3 radiusNdcPos = radiusClipPos.xyz  radiusClipPos.w;
        vec2 radiusScreenPos = (radiusNdcPos.xy  0.5 + 0.5)  screenSize;

        float lightRadiusScreenSpace = length(radiusScreenPos - lightScreenPos);

        vec2 lightMin = lightScreenPos - vec2(lightRadiusScreenSpace);
        vec2 lightMax = lightScreenPos + vec2(lightRadiusScreenSpace);

        if (lightMax.x  tileMin.x  lightMin.x  tileMax.x 
            lightMax.y  tileMin.y  lightMin.y  tileMax.y) {
            continue;
        }

        uint index = atomicAdd(tileLightCount[tileIndex].x, 1);
        if (index  MAX_LIGHTS_PER_TILE) {
            lightIndices[tileIndex  MAX_LIGHTS_PER_TILE + index] = i;
        }
    }
}
