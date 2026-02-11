#version 430

in vec2 vUV;
in vec2 vClipXY;
in vec4 vColor;

layout(binding=1) uniform sampler2D uTex1;  // MSDF atlas
out vec4 fragColor;

float median3(vec3 v) { return max(min(v.x,v.y), min(max(v.x,v.y), v.z)); }

uniform bool uDebugClip = false;

void main()
{
   if (uDebugClip)
   {
        // Map clip [-1,+1] → [0,1] → color
        vec2 t = vClipXY*0.5 + 0.5;
        //fragColor = vec4(t.x, t.y, 0.0, 1.0); // left→red, up→green
	//fragColor = vec4(vUV, 0.0, 1.0);
	vec3 s = texture(uTex1, vec2(vUV.x, 1.0 - vUV.y)).rgb;
	fragColor = vec4(s, 1.0); 
        return;
    }
    vec3 s  = texture(uTex1, vUV).rgb;
    float sd = median3(s) - 0.5;
    float w  = fwidth(sd);
    float cov = smoothstep(-w, w, sd);
    fragColor = vec4(vColor.rgb, vColor.a * cov);
}
