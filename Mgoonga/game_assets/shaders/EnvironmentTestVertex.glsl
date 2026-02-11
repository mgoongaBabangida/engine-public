#version 430

layout (location = 0) in vec3 position;
layout (location = 2) in vec3 aNormal;

out vec3 NormalWS;
out vec3 PositionWS;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
	vec4 v = vec4(position , 1.0);	
    gl_Position = projection * view * model * v;
	
    NormalWS = mat3(transpose(inverse(model))) * aNormal;
    PositionWS = vec3(model * v);
} 