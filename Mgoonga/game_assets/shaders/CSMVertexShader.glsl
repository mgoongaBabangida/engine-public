#version 460 core
layout (location = 0) in vec3 position;

in layout(location=6) ivec4 boneIDs;
in layout(location=7) vec4 weights;
 
const int MAX_BONES = 100;

layout (std140, binding = 0) uniform LightSpaceMatrices
{
    mat4 lightSpaceMatrices[16];
};

layout(std430, binding = 6) buffer ModelToWorld {
    mat4 modelToWorldMatrix[];
};

uniform bool isInstanced = false;
uniform int instanceIndex = 0; // only used if !isInstanced
uniform int cascadeIndex = 0;

uniform mat4 gBones[MAX_BONES];
 
void main()
{
  mat4 BoneTransform      = gBones[boneIDs[0]] * weights[0];
       BoneTransform     += gBones[boneIDs[1]] * weights[1];
       BoneTransform     += gBones[boneIDs[2]] * weights[2];
       BoneTransform     += gBones[boneIDs[3]] * weights[3];
	 
	vec4 v =  BoneTransform * vec4(position ,1.0);
	int effectiveInstance = isInstanced ? gl_InstanceID : instanceIndex;
  gl_Position = lightSpaceMatrices[cascadeIndex] * modelToWorldMatrix[effectiveInstance] * v;
}