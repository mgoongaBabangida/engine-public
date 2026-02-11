// Vertex shader for shadow map generation
 #version 430 core
 in layout (location = 0) vec3 position;

 uniform mat4 MVP;
 
 flat out int toDiscard;
 
 void main(void)
 { 
    vec4 v = vec4(position ,1.0);
    toDiscard = 0;
    gl_Position = MVP * v;
 };