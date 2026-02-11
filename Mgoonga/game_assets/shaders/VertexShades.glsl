// Vertex shader for shadow map generation
 #version 430 core
 
 in layout (location = 0) vec3 position;
 in layout(location=6) ivec4 boneIDs;
 in layout(location=7) vec4 weights;
 
 const int MAX_BONES = 100;
 
layout(std430, binding = 5) buffer ModelToProj {
    mat4 modelToProjectionMatrix[];
};
 uniform mat4 MVP;
 uniform bool isInstanced = false;
 uniform mat4 gBones[MAX_BONES];

 void main(void)
 { 
  mat4 BoneTransform      = gBones[boneIDs[0]] * weights[0];
       BoneTransform     += gBones[boneIDs[1]] * weights[1];
       BoneTransform     += gBones[boneIDs[2]] * weights[2];
       BoneTransform     += gBones[boneIDs[3]] * weights[3];
	
	mat4 mvp = isInstanced ? modelToProjectionMatrix[gl_InstanceID] : MVP;
	vec4 v = BoneTransform *vec4(position ,1.0);
    	gl_Position = mvp * v;
 };