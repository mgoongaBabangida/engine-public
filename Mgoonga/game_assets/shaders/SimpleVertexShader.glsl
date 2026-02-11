 #version 430
  
 layout (location = 0) in vec3 position;
 layout (location = 1) in vec2 aTexCoords;
 
 uniform mat2 rotationMatrix;
 uniform vec2 center;   
 uniform float aspectRatio = 1.6f;

 out vec2 TexCoords;
 flat out int toDiscard;

 void main(void)
 { 
    toDiscard = 0;
    TexCoords = aTexCoords;
    vec2 centeredPosition = vec2(position) - center;
    vec2 scaledPosition = vec2(centeredPosition.x, centeredPosition.y / aspectRatio); 
    vec2 rotatedPosition = rotationMatrix * scaledPosition;
    vec2 finalPosition = vec2(rotatedPosition.x, rotatedPosition.y * aspectRatio) + center;
    gl_Position = vec4(finalPosition, position.z, 1.0f);
 };