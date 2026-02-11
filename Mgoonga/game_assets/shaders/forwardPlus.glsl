#version 430

struct PointLight {
	vec4 color;
	vec4 position;
	vec4 paddingAndRadius;
};

struct VisibleIndex {
	int index;
};

// Shader storage buffer objects
layout(std430, binding = 0) readonly buffer LightBuffer {
	PointLight data[];
} lightBuffer;

layout(std430, binding = 1) writeonly buffer VisibleLightIndicesBuffer {
	VisibleIndex data[];
} visibleLightIndicesBuffer;

// Uniforms
layout(binding=17) uniform sampler2D depthMap;

uniform mat4 view;
uniform mat4 projection;
uniform ivec2 screenSize;
uniform int lightCount;
uniform float near = 0.01f;
uniform float far = 40.f;

// Shared values between all the threads in the group
shared uint minDepthInt;
shared uint maxDepthInt;
shared uint visibleLightCount;
shared vec4 frustumPlanes[6];
// Shared local storage for visible indices, will be written out to the global buffer at the end
shared int visibleLightIndices[1024];
shared mat4 viewProjection;

// Took some light culling guidance from Dice's deferred renderer
// http://www.dice.se/news/directx-11-rendering-battlefield-3/

#define TILE_SIZE 16
layout(local_size_x = TILE_SIZE, local_size_y = TILE_SIZE, local_size_z = 1) in;

// Helper function: create a plane from 3 points in CCW order (normal pointing outward)
vec4 createPlane(vec3 a, vec3 b, vec3 c) {
    vec3 normal = normalize(cross(b - a, c - a));
    float d = dot(normal, a);
    return vec4(normal, -d);
}

void main() {
	ivec2 location = ivec2(gl_GlobalInvocationID.xy);
	ivec2 itemID = ivec2(gl_LocalInvocationID.xy);
	ivec2 tileID = ivec2(gl_WorkGroupID.xy);
	ivec2 tileNumber = ivec2(gl_NumWorkGroups.xy);
	uint index = tileID.y * tileNumber.x + tileID.x;

	// Initialize shared global values for depth and light count
	if (gl_LocalInvocationIndex == 0) {
		minDepthInt = 0xFFFFFFFF;
		maxDepthInt = 0;
		visibleLightCount = 0;
		viewProjection = projection * view;
	}

	barrier();

	// Step 1: Calculate the minimum and maximum depth values (from the depth buffer) for this group's tile
	float maxDepth, minDepth;
	vec2 text = vec2(location) / vec2(screenSize);
	float depth = texture(depthMap, text).r;
	
	// Linearize the depth value from depth buffer (must do this because we created it using projection)
	//depth = (0.5 * projection[3][2]) / (depth + 0.5 * projection[2][2] - 0.5);

	// Convert depth to NDC depth
	float ndcDepth = depth * 2.0 - 1.0;

	// Reconstruct inverse of projection matrix
    mat4 invProj = inverse(projection);
	// Reconstruct view-space Z by unprojecting (0,0,ndcDepth)
	vec4 viewPos = invProj * vec4(0.0, 0.0, ndcDepth, 1.0);
	float viewZ = viewPos.z / viewPos.w;  // This is the view-space Z

	// Convert depth to uint so we can do atomic min and max comparisons between the threads
	uint depthInt = floatBitsToUint(viewZ);
	atomicMin(minDepthInt, depthInt);
	atomicMax(maxDepthInt, depthInt);

	barrier();

	// Step 2: One thread should calculate the frustum planes to be used for this tile
	if (gl_LocalInvocationIndex == 0) {
      maxDepth = uintBitsToFloat(minDepthInt);
      minDepth = uintBitsToFloat(maxDepthInt);
	  
	  float A = projection[2][2];
	  float B = projection[3][2];	  
      float ndcMinDepth = (A * minDepth + B) / -minDepth;
	  float ndcMaxDepth = (A * maxDepth + B) / -maxDepth;
	  
	  //float ndcMinDepth = minDepth * 2.0 - 1.0;
	  //float ndcMaxDepth = maxDepth * 2.0 - 1.0;

      // Compute screen space corners of the tile
      vec2 tileMin = vec2(tileID) * TILE_SIZE;
      vec2 tileMax = tileMin + TILE_SIZE;

      vec2 ndcMin = (tileMin / vec2(screenSize)) * 2.0 - 1.0;
      vec2 ndcMax = (tileMax / vec2(screenSize)) * 2.0 - 1.0;

      // Prepare NDC corner positions with min/max depth
      vec3 ndcCorners[8];
      ndcCorners[0] = vec3(ndcMin.x, ndcMin.y, ndcMinDepth); // Near Lower Left
      ndcCorners[1] = vec3(ndcMax.x, ndcMin.y, ndcMinDepth); // Near Lower Right
      ndcCorners[2] = vec3(ndcMax.x, ndcMax.y, ndcMinDepth); // Near Upper Right
      ndcCorners[3] = vec3(ndcMin.x, ndcMax.y, ndcMinDepth); // Near Upper Left

      ndcCorners[4] = vec3(ndcMin.x, ndcMin.y, ndcMaxDepth); // Far Lower Left
      ndcCorners[5] = vec3(ndcMax.x, ndcMin.y, ndcMaxDepth); // Far Lower Right
      ndcCorners[6] = vec3(ndcMax.x, ndcMax.y, ndcMaxDepth); // Far Upper Right
      ndcCorners[7] = vec3(ndcMin.x, ndcMax.y, ndcMaxDepth); // Far Upper Left

      // Transform NDC corners to world space
      vec3 viewCorners[8];
      for (int i = 0; i < 8; ++i) {
          vec4 corner = invProj * vec4(ndcCorners[i], 1.0);
          viewCorners[i] = corner.xyz / corner.w;
      }

      // Create planes using three points CCW to ensure normal faces outward
      frustumPlanes[0] = createPlane(viewCorners[0], viewCorners[3], viewCorners[7]); // Left
      frustumPlanes[1] = createPlane(viewCorners[2], viewCorners[1], viewCorners[5]); // Right
      frustumPlanes[2] = createPlane(viewCorners[1], viewCorners[0], viewCorners[4]); // Bottom
      frustumPlanes[3] = createPlane(viewCorners[3], viewCorners[2], viewCorners[6]); // Top
      frustumPlanes[4] = createPlane(viewCorners[0], viewCorners[1], viewCorners[2]); // Near
      frustumPlanes[5] = createPlane(viewCorners[5], viewCorners[4], viewCorners[7]); // Far
	}

	barrier();


	// Step 3: Cull lights.
	// Parallelize the threads against the lights now.
	// Can handle 256 simultaniously. Anymore lights than that and additional passes are performed
	
	uint threadIndex = gl_LocalInvocationIndex;	
	uint totalThreads = TILE_SIZE * TILE_SIZE;
	uint passCount = (lightCount + totalThreads - 1) / totalThreads;
	
    for (uint pass = 0; pass < passCount; ++pass) {
        uint lightIndex = pass * totalThreads + threadIndex;
        if (lightIndex < uint(lightCount)) {
            PointLight light = lightBuffer.data[lightIndex];
            vec3 pos = light.position.xyz;         // in view space
            float radius = light.paddingAndRadius.w;
    			
            // Sphere-frustum intersection test: 
            // light is visible if it is inside or intersects all 6 planes
            bool visible = true;
            for (int p = 0; p < 6; ++p) {
                float dist = dot(frustumPlanes[p].xyz, pos) + frustumPlanes[p].w;
                if (dist < -radius) {
                    visible = false;
                    break;
                }
            }
			
            if (visible) {
                // Atomically reserve a slot in the visible light indices array
                uint visibleIdx = atomicAdd(visibleLightCount, 1);
                // Prevent overflow of shared array
                if (visibleIdx < 1024) {
                    visibleLightIndices[visibleIdx] = int(lightIndex);
                }
            }
        }
		    barrier();  // sync all threads before next pass
    }
	
		   
	// One thread should fill the global light buffer
	if (gl_LocalInvocationIndex == 0) {
		uint offset = index * 1024; // Determine bosition in global buffer
		for (uint i = 0; i < visibleLightCount; i++) {
			visibleLightIndicesBuffer.data[offset + i].index = visibleLightIndices[i];
		}

		if (visibleLightCount != 1024) {
			// Unless we have totally filled the entire array, mark it's end with -1
			// Final shader step will use this to determine where to stop (without having to pass the light count)
			visibleLightIndicesBuffer.data[offset + visibleLightCount].index = -1;
		}
	}
}