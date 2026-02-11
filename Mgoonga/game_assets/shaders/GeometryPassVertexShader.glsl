#version 430 core

layout(location=0) in vec3 aPos;
layout(location=2) in vec3 aNormal;
layout(location=3) in vec2 aTexCoords;
layout(location=6) in ivec4 boneIDs;
layout(location=7) in vec4  weights;

out vec3 FragPos;   // view-space
out vec2 TexCoords;
out vec3 Normal;    // view-space

const int MAX_BONES = 100;

uniform bool invertedNormals = false;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 gBones[MAX_BONES];

void main()
{
    mat4 BoneTransform =
        gBones[boneIDs[0]] * weights[0] +
        gBones[boneIDs[1]] * weights[1] +
        gBones[boneIDs[2]] * weights[2] +
        gBones[boneIDs[3]] * weights[3];

    vec4 localPos = BoneTransform * vec4(aPos, 1.0);

    vec4 viewPos = view * model * localPos;
    FragPos  = viewPos.xyz;
    TexCoords = aTexCoords;

    mat3 normalMatrix = transpose(inverse(mat3(view * model)));
    vec3 skinnedNormal = normalize(mat3(BoneTransform) * aNormal);
    Normal = normalize(normalMatrix * (invertedNormals ? -skinnedNormal : skinnedNormal));

    gl_Position = projection * viewPos;
}
