#version 430

in layout(location=0) vec3 position;
in layout(location=1) mat4 modelMatrix;

uniform mat4 MVP;
uniform float time;

out vec2 Texcoord;

void main()
{
	// Wind parameters
    float swayAmount = 0.1;       // Max displacement
    float frequency = 2.0;
    float phaseOffset = modelMatrix[3].x * 0.5; // vary per instance
	
	vec3 worldPos = vec3(modelMatrix * vec4(position, 1.0));
	// Animate vertex position with sine wave for wind
    worldPos += vec3(0.0, 1.0, 0.0) * swayAmount * sin(frequency * time + phaseOffset);
	
	if(position.xy == vec2(-0.5f, -0.5f))
		Texcoord = vec2(1.f , 1.f);
	else if(position.xy == vec2( 0.5f, -0.5f))
		Texcoord = vec2(1.f , 0.f);
	else if(position.xy == vec2(0.5f,  0.5f))
		Texcoord = vec2(0.f , 0.f);
	else if(position.xy == vec2(-0.5f,  0.5f))
		Texcoord = vec2(0.f , 1.f);
		
	gl_Position = MVP * vec4(worldPos,1.0);
}