#version 430

in layout(location=0) vec3 position;
in layout(location=1) vec3 vertexColor;
in layout(location=2) vec3 normal;
in layout(location=3) vec2 texcoord;
in layout(location=4) vec4 tangent;
in layout(location=5) vec3 bitangent;
in layout(location=6) ivec4 boneIDs;
in layout(location=7) vec4 weights;

const int MAX_BONES = 100;

layout(std430, binding = 5) buffer ModelToProj {
    mat4 modelToProjectionMatrix[];
};

layout(std430, binding = 6) buffer ModelToWorld {
    mat4 modelToWorldMatrix[];
};

layout(std430, binding = 7) buffer InstancedData {
    mat4 instancedData[];
};

layout(std430, binding = 9) buffer BonesPacked {
    mat4 bonesPacked[];
};

layout(std430, binding = 10) buffer BoneBaseIndex {
    uint boneBaseIndex[];
};

uniform mat4 shadowMatrix;
uniform mat4 gBones[MAX_BONES];

// outline path stays, but only for non-instanced rendering
uniform bool outline = false;
uniform int outline_bone = MAX_BONES;

// enables SSBO palette fetch in instanced draws
uniform bool useInstancedSkinning = false;

uniform vec4 clip_plane;

uniform bool isInstanced = false;
uniform int instanceIndex = 0; // only used if !isInstanced
uniform mat4 meshOffset = mat4(1.0);

out vec3 thePosition;
out vec3 theNormal;
out vec3 theTangent;
out vec2 Texcoord;
out vec4 LocalSpacePos;
out vec4 LightSpacePos;
out vec3 LocalSpaceNormal;
out mat3 TBN;
out vec4 debug;
out vec3 vertColor;

flat out vec3 SolidColor;
flat out int toDiscard;
flat out int InstanceID;

// --- helper to read bone matrices from the correct source ---
mat4 ReadBone(int effectiveInstance, int boneIndex)
{
    // Instanced path: palette comes from SSBO pool
    if (isInstanced && useInstancedSkinning)
    {
        int base = int(boneBaseIndex[effectiveInstance]); // base offset in mat4 elements
        return bonesPacked[base + boneIndex];
    }

    // Non-instanced path: palette is uniform array
    return gBones[boneIndex];
}

void main()
{
    // Outline selection is only supported for the non-instanced old path
    if (!isInstanced)
    {
        toDiscard = (outline &&
                     boneIDs[0] != outline_bone &&
                     boneIDs[1] != outline_bone &&
                     boneIDs[2] != outline_bone &&
                     boneIDs[3] != outline_bone) ? 1 : 0;
    }
    else
    {
        toDiscard = 0;
    }

    int effectiveInstance = isInstanced ? gl_InstanceID : instanceIndex;

    // Fetch bones (uniform or SSBO depending on flags)
    mat4 B0 = ReadBone(effectiveInstance, boneIDs[0]);
    mat4 B1 = ReadBone(effectiveInstance, boneIDs[1]);
    mat4 B2 = ReadBone(effectiveInstance, boneIDs[2]);
    mat4 B3 = ReadBone(effectiveInstance, boneIDs[3]);

    mat4 BoneTransform = B0 * weights[0] +
                         B1 * weights[1] +
                         B2 * weights[2] +
                         B3 * weights[3];

    vec4 v = BoneTransform * meshOffset * vec4(position, 1.0);

    gl_Position = modelToProjectionMatrix[effectiveInstance] * v;
    mat4 modelWorld = modelToWorldMatrix[effectiveInstance];

    LightSpacePos = shadowMatrix * modelWorld * v;
    LocalSpacePos = v;
    Texcoord = texcoord;

    vec3 skinnedNormal = normalize(mat3(BoneTransform) * mat3(meshOffset) * normal);
    vec3 worldNormal   = normalize(mat3(modelWorld) * skinnedNormal);
    theNormal = worldNormal;
    LocalSpaceNormal = skinnedNormal;

    vec4 WorldPosV = modelWorld * v;
    gl_ClipDistance[0] = dot(WorldPosV, clip_plane);
    thePosition = vec3(WorldPosV);

    float handedness = tangent.w;
    vec3 skinnedTangent = normalize(mat3(BoneTransform) * mat3(meshOffset) * tangent.xyz);
    vec3 T = normalize(vec3(modelWorld * vec4(skinnedTangent, 0.0)));
    vec3 N = worldNormal;
    vec3 B = normalize(cross(N, T) * handedness);

    if (dot(cross(N, T), B) < 0.0)
        T = -T;

    TBN = mat3(T, B, N);

    float weightSum = weights[0] + weights[1] + weights[2] + weights[3];
    debug = vec4(weightSum, weightSum, weightSum, 1.0);

    vec3 worldTangent = normalize(mat3(modelWorld) * skinnedTangent);
    theTangent = worldTangent;

    SolidColor = instancedData[effectiveInstance][0].xyz;
    InstanceID = effectiveInstance;
}
