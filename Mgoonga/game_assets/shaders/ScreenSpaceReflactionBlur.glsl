#version 430

out vec4 FragColor;
  
in vec2 TexCoords;

layout(binding=2) uniform sampler2D tPos;
layout(binding=3) uniform sampler2D ssrInput;
layout(binding=4) uniform sampler2D ssrMask;

const float depthSigma = 0.8;

void main() 
{
    vec2 texelSize = 1.0 / vec2(textureSize(ssrInput, 0));
	float centerDepth = texture(tPos, TexCoords).z;
    float centerMask  = texture(ssrMask, TexCoords).r;
	
    vec4 result = vec4(0.0);
	float totalWeight = 0.0;
	
    for (int x = -2; x <= 2; ++x) 
    {
        for (int y = -2; y <= 2; ++y) 
        {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
			vec2 sampleUV = TexCoords + offset;
            
			float sampleDepth = texture(tPos, sampleUV).z;
			
			// Depth difference in view space
            float dz = abs(sampleDepth - centerDepth);
            float weight = exp(-dz * dz / (depthSigma * depthSigma));
			
			result += texture(ssrInput, sampleUV).rgba * weight;
			totalWeight += weight;
        }
    }
    FragColor = result / totalWeight;
}  