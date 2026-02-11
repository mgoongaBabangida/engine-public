// DebugRig.h
#pragma once
#include <iostream>
#define DLOG(...) do { std::cout << __VA_ARGS__ << std::endl; } while(0)
#include <string>
#include <vector>
#include <unordered_map>
#include <glm/glm/glm.hpp>

class Bone;
class eModel;

struct DebugMeshInfo {
  std::string meshName;
  std::string attachedNodeName;    // node that holds this mesh
  size_t numVertices = 0;
  size_t numBones = 0;             // mNumBones from importer
  bool   isSkinned = false;        // numBones > 0
};

struct BoneMapEntry {
  std::string name;
  int index = -1;
};

struct DebugVertexInfluence {
  uint32_t vi;
  unsigned idx[4];
  float    w[4];
};

struct DebugDiff {
  float maxAbs = 0.f;
  glm::mat4 A, B;
};

// Engine-facing hooks you can fill in if names differ:
int  GetBoneIndexByName(const std::vector<Bone>& bones, const std::string& name);
glm::mat4 GetInverseBindByName(const std::vector<Bone>& bones, const std::string& name);
glm::mat4 GetGlobalAnimatedByName(const std::vector<Bone>& bones, const std::string& name);

// These you implement per your Mesh/Model format:
DebugMeshInfo GatherMeshInfo(const eModel& model, int meshId);
std::vector<std::string> MeshBoneNames(const eModel& model, int meshId); // from weights
std::vector<DebugVertexInfluence> MeshInfluences(const eModel& model, int meshId);
std::string NodeNameOwningMesh(const eModel& model, int meshId);

// Utility:
DebugDiff MatDiff(const glm::mat4& a, const glm::mat4& b);
void PrintMat(const char* tag, const glm::mat4& m);

// High-level debuggers:
void DebugMeshBinding(const eModel& model, const std::vector<Bone>& bones, int meshId);
void DebugBoneInfluenceForMesh(const eModel& model, int meshId, const std::string& boneName, float wEps = 1e-5f);
void DebugOffsetsVsImporter(const std::vector<Bone>& bones,
  const std::unordered_map<std::string, glm::mat4>& importerOffsets);
glm::vec4 CpuSkinOneVertex(const glm::vec3& pos, const glm::uvec4& idx, const glm::vec4& w,
  const std::vector<glm::mat4>& palette);

