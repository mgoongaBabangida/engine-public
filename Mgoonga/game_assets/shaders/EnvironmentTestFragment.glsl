#version 430 core

out vec4 FragColor;

// Inputs from vertex shader
in vec3 NormalWS;
in vec3 PositionWS;

uniform mat4 WorldToLocal;       	// World to local transformation matrix
uniform vec4 CameraWS; 				// Camera position in world space
uniform vec4 CubemapPositionWS;  	// Position of the cubemap in world space

layout(binding=5) uniform samplerCube envMap;

void main()
{             
    vec3 DirectionWS = normalize(PositionWS - vec3(CameraWS));
	
    vec3 ReflDirectionWS = reflect(DirectionWS, normalize(NormalWS));
    
	FragColor = vec4(texture(envMap, ReflDirectionWS).rgb, 1.0);
}