
float penumbraSize(float lightViewDistance, float avgBlockerDepth, float currentDepth)
{
    float penumbra =  (currentDepth - avgBlockerDepth) / avgBlockerDepth * lightViewDistance;
	// Clamp the penumbra size to a reasonable range
    penumbra = clamp(penumbra, 0.01, max_penumbra); // Adjust to a value that makes sense for your scene
	return penumbra;
}

float computeBias(vec3 normal, vec3 lightDir, float receiverDepth, float texelSize)
{
    //flaot csm_bias_scale = 2.0f;
    float cosTheta = max(dot(normal, lightDir), 0.0);
    float depthBias = csm_base_slope_bias * (1.0 - cosTheta);
    
    // Use depth gradient for perspective correction
    float biasGradient = length(vec2(dFdx(receiverDepth), dFdy(receiverDepth)));
    return depthBias + biasGradient * texelSize * 2.0f; // csm_bias_scale
}

float ShadowSampleCSM(int layer, float bias, vec3 projCoords, vec2 texelSize, float penumbra)
{	
	// PCF
	float shadow = 0.0;
	int sample_count = 0;
	float currentDepth = projCoords.z;
	
	if(pcf_texture_sample_enabled)
	{		 
		ivec3 offsetCoord;
		offsetCoord.xy = ivec2( mod( gl_FragCoord.xy, pcf_OffsetTexSize.xy ) );
		int samplesDiv2 = int(pcf_OffsetTexSize.z);
		for( int i = 0 ; i < 4; i++ )
		{
           offsetCoord.z = i;
           vec4 offsets = texelFetch(texture_pcf_offsets, offsetCoord, 0) * pcf_texture_sample_radius;

		   float pcfDepth = texture(
		   				texture_array_csm,
		   				vec3(projCoords.xy + vec2(offsets.x, offsets.y) * texelSize * penumbra,
		   				layer)
		   				).r; 
		   shadow += (currentDepth - bias) > pcfDepth ? 1.0 : 0.0;
		   ++sample_count;		
			
			pcfDepth = texture(
		   				texture_array_csm,
		   				vec3(projCoords.xy + vec2(offsets.z, offsets.w) * texelSize * penumbra,
		   				layer)
		   				).r; 
		   shadow += (currentDepth - bias) > pcfDepth ? 1.0 : 0.0;
		   ++sample_count;		
        }
		
		if( shadow/float(sample_count) != 1.0 && shadow/float(sample_count) != 0.0 )
		{
            for( int i = 4; i < samplesDiv2; i++ )
			{
                offsetCoord.z = i;
                vec4 offsets = texelFetch(texture_pcf_offsets, offsetCoord,0) * pcf_texture_sample_radius;

                float pcfDepth = texture(
		   				texture_array_csm,
		   				vec3(projCoords.xy + vec2(offsets.x, offsets.y) * texelSize * penumbra,
		   				layer)
		   				).r; 
				shadow += (currentDepth - bias) > pcfDepth ? 1.0 : 0.0;
				++sample_count;		
			
				pcfDepth = texture(
		   				texture_array_csm,
		   				vec3(projCoords.xy + vec2(offsets.z, offsets.w) * texelSize * penumbra,
		   				layer)
		   				).r; 
				shadow += (currentDepth - bias) > pcfDepth ? 1.0 : 0.0;
				++sample_count;		
            }
            shadow /= float(sample_count);
        }
	}
	else
	{
		for(int x = -pcf_samples; x <= pcf_samples; ++x)
		{
			for(int y = -pcf_samples; y <= pcf_samples; ++y)
			{
				float pcfDepth = texture(
							texture_array_csm,
							vec3(projCoords.xy + vec2(x, y) * texelSize * penumbra,
							layer)
							).r; 
				shadow += (currentDepth - bias) > pcfDepth ? 1.0 : 0.0;
				++sample_count;			
			}    
		}
		shadow /= float(sample_count);
	}
	return shadow;
}

float findBlocker(vec3 projCoords, float currentDepth, float diskRadius, int layer, vec2 texelSize)
{
    int numBlockers = 0;
    float avgBlockerDepth = 0.0;
	
    // Sample within the disk radius
    for(int x = -3; x <= 3; ++x)
    {
        for(int y = -3; y <= 3; ++y)
        {
            vec2 offset = vec2(x, y) * diskRadius * texelSize;
			float depth = texture(
						texture_array_csm,
						vec3(projCoords.xy + offset,
						layer)
						).r; 
			
			if (depth < 0.0 || depth > 1.0)
				continue;
	
            if(depth < currentDepth)
            {
				float z = depth * 2.0 - 1.0;

				vec4 clipSpacePosition = vec4((projCoords.xy + offset) * 2.0 - 1.0, z, 1.0);
				vec4 worldSpacePosition = inverse(lightSpaceMatrices[layer]) * clipSpacePosition;

				// Perspective division
				worldSpacePosition /= worldSpacePosition.w;
                avgBlockerDepth += length(worldSpacePosition.xyz - vec3(lightPos));
                numBlockers++;
            }
        }
    }

    if(numBlockers == 0)
        return -1.0;
    
    avgBlockerDepth /= numBlockers;
	avgBlockerDepth = max(avgBlockerDepth, 0.01);
    return avgBlockerDepth;
}

float ShadowCalculationCSM(vec4 fragPosWorldSpace, vec3 lightDir, vec3 normal)
{
	vec4 fragPosViewSpace = view * fragPosWorldSpace;
	float depthValue = abs(fragPosViewSpace.z);		
	int layer = -1;
	int layerBlend = -1;
	float blendCoef = 1.0f;
	for (int i = 0; i < cascadeCount; ++i)
	{
		if (depthValue < cascadePlaneDistances[i])
		{
			layer = i;
			
				if((cascadePlaneDistances[i] - depthValue)   < cascade_blend_distance)
				{
					float blendAreaStart = cascadePlaneDistances[i] - cascade_blend_distance;
					blendCoef = 1.0f - (((cascadePlaneDistances[i] - depthValue) / cascade_blend_distance + 1.0f) / 2.0f);
					layerBlend = i+1;
				}
				else if((depthValue - cascadePlaneDistances[i-1]) < cascade_blend_distance)
				{
					if(i > 0)
					{
					  float blendAreaEnd = cascadePlaneDistances[i-1] + cascade_blend_distance;
					  blendCoef = 1.0f - ((depthValue - cascadePlaneDistances[i-1])/(blendAreaEnd - cascadePlaneDistances[i-1])/ 2.0f + 0.5f);
					  layerBlend = i-1;
					}
				}				
			break;
		}
	}
	if(layer == 0)
	{
		blendCoef = 1.0f;
	}
	if (layer == -1)
	{
		layer = cascadeCount;
	}	
	
	vec4 fragPosLightProjSpace = lightSpaceMatrices[layer] * fragPosWorldSpace;	
	// perform perspective divide
	vec3 projCoords = fragPosLightProjSpace.xyz / fragPosLightProjSpace.w;
	// transform to [0,1] range
	projCoords = projCoords * 0.5 + 0.5;
	
	vec2 texelSize = 1.0f / vec2(textureSize(texture_array_csm, 0));
	
	// get depth of current fragment from light's perspective
	float currentDepth = projCoords.z;
	if (currentDepth  > 1.0)
		return 1.0;
	
	float penumbra = 1.0f;
	if(pcss_enabled)
	{
		float avgDistanceToBlocker = findBlocker(projCoords, currentDepth, light_radius, layer, texelSize);
	
		// No blockers found, return no shadow
		if(avgDistanceToBlocker == -1.0)
			return 1.0;
		
		// Penumbra size calculation
		float distanceToLight = length(vec3(fragPosWorldSpace) - vec3(lightPos));
		penumbra = penumbraSize(lightSize, avgDistanceToBlocker, distanceToLight);
	}
	
	// calculate bias (based on depth map resolution and slope)
	float cosTheta = max(dot(normal, lightDir), 0.01);
	float tanTheta = sqrt(1.0 - cosTheta * cosTheta) / cosTheta;
	float texelBias = texelSize.x * 2.5; // e.g. 1.5–2.5

	// Final bias = slope-based + texel-based
	float bias = tanTheta * csm_base_slope_bias + texelBias;

	//float bias = max(csm_base_slope_bias * (1.0 - dot(normal, lightDir)), csm_base_slope_bias * 0.1f);
	//float bias = computeBias(normal, lightDir, currentDepth, texelSize.x);
	if (layer == cascadeCount)
	{
		bias *= 1 / (farPlane * csm_base_cascade_plane_bias);
	}
	else
	{
		bias *= 1 / (cascadePlaneDistances[layer] * csm_base_cascade_plane_bias);
	}
	
	float shadow = 0.0f;
	shadow = ShadowSampleCSM(layer, bias, projCoords, texelSize, penumbra);
	
	if(blendCoef != 1.0f && blendCascades && layerBlend >= 0 && layerBlend < cascadeCount)
    {
     // Project into second cascade
     vec4 fragPosLightProjBlend = lightSpaceMatrices[layerBlend] * fragPosWorldSpace;
     vec3 projCoordsBlend = fragPosLightProjBlend.xyz / fragPosLightProjBlend.w;
     projCoordsBlend = projCoordsBlend * 0.5 + 0.5;
     
     // Clamp to avoid sampling outside shadow map
     projCoordsBlend = clamp(projCoordsBlend, vec3(0.001), vec3(0.999));
     
     float currentDepthBlend = projCoordsBlend.z;
     float biasBlend = max(csm_base_slope_bias * (1.0 - dot(normal, lightDir)), csm_base_slope_bias * 0.1f);
     //float biasBlend = computeBias(normal, lightDir, currentDepthBlend, texelSize.x);
     if (layerBlend == cascadeCount)
     {
         biasBlend *= 1 / (farPlane * csm_base_cascade_plane_bias);
     }
     else
     {
         biasBlend *= 1 / (cascadePlaneDistances[layerBlend] * csm_base_cascade_plane_bias);
     }
     
     float penumbraBlend = 1.0f;
     if(pcss_enabled)
     {
         float avgBlockerDepthBlend = findBlocker(projCoordsBlend, currentDepthBlend, light_radius, layerBlend, texelSize);
         if(avgBlockerDepthBlend != -1.0)
         {
             float distanceToLightBlend = length(vec3(fragPosWorldSpace) - vec3(lightPos));
             penumbraBlend = penumbraSize(lightSize, avgBlockerDepthBlend, distanceToLightBlend);
         }
     }
     
     float shadowBlend = ShadowSampleCSM(layerBlend, biasBlend, projCoordsBlend, texelSize, penumbraBlend);
     shadow = mix(shadow, shadowBlend, 1.0 - blendCoef);
    }
				
	return clamp((1.0f - shadow), 1.0f - max_shadow, 1.0f);
}

// Precomputed offset directions (sampling kernel for PCF)
vec3 gridSamplingDisk[20] = vec3[](
    vec3( 1,  1,  1), vec3(-1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1),
    vec3( 1,  1, -1), vec3(-1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1),
    vec3( 1,  0,  0), vec3(-1,  0,  0), vec3( 0,  1,  0), vec3( 0, -1,  0),
    vec3( 0,  0,  1), vec3( 0,  0, -1),
    vec3( 1,  1,  0), vec3(-1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0),
    vec3( 0,  1,  1), vec3( 0, -1, -1)
);

// todo better PCF
float ShadowCalculationCubeMap(vec3 fragPos, vec3 normal, vec3 lightPos)
{
	vec3 fragToLight = fragPos - lightPos;
    float currentDepth = length(fragToLight);
    vec3 lightDir = normalize(fragToLight);

    // Angle-dependent bias
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.0005);

    // PCF sampling
    float shadow = 0.0;
    int samples = 20;
    float diskRadius = 0.03; // Scene scale dependent!!!!!
	//float diskRadius = (1.0 + (currentDepth / far_plane)) * 0.05;
	
    for (int i = 0; i < samples; ++i)
    {
        vec3 sampleDir = fragToLight + gridSamplingDisk[i] * diskRadius;
        float sampleDepth = texture(depth_cube_map, sampleDir).r * far_plane;
        if (currentDepth - bias > sampleDepth)
            shadow += 1.0;
    }

    shadow /= float(samples);

    // Fade shadow with distance (optional, for atmospheric soft fade)
    float fade = clamp(1.0 - currentDepth / far_plane, 0.0, 1.0);
    shadow *= fade;

    // Return visibility: 1.0 = fully lit, 0.0 = full shadow
    return 1.0 - shadow;
} 

float ShadowCalculation(vec4 fragPosLightSpace, vec3 lightDir, vec3 normal )
{
  //perform perspective divide
  vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
  projCoords = projCoords * 0.5 + 0.5;
  float closestDepth = texture(depth_texture, projCoords.xy).r;
  float currentDepth = projCoords.z;
  float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.0005);
  
  float shadow = 0.0;
  vec2 texelSize = 1.0 / textureSize(depth_texture, 0);
  for(int x = -2; x <= 2; ++x)
  {
     for(int y = -2; y <= 2; ++y)
     {
         float pcfDepth = texture(depth_texture, projCoords.xy + vec2(x, y) * texelSize).r; 
         shadow += (currentDepth - bias) > pcfDepth ? 1.0 : 0.0;        
     }    
  }
  shadow /= 25.0;
  
  if(fragPosLightSpace.z > 1.0) //far plane issue!
     shadow = 0.0;
  return clamp((0.5f + (1.0f - shadow)), 0.0f, 1.0f);
} 
