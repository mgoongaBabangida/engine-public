#version  440 core

flat in int toDiscard;

out vec4 outColor;

uniform vec4 color = vec4(1.0, 1.0, 0.0, 1.0);

void main()
{
 if(toDiscard == 1)
	discard;
 
 outColor = color;
}
