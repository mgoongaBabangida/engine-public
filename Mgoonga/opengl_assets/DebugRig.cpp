// DebugRig.cpp
#include "DebugRig.h"
#include <algorithm>
#include <cmath>

#include <math/Bone.h>
#include "Model.h"

int GetBoneIndexByName(const std::vector<Bone>& bones, const std::string& n) {
  auto it = std::find_if(bones.begin(), bones.end(),
    [&](const Bone& b) { return b.GetName() == n; });
  return (it == bones.end()) ? -1 : int(it - bones.begin());
}

glm::mat4 GetInverseBindByName(const std::vector<Bone>& bones, const std::string& n) {
  auto it = std::find_if(bones.begin(), bones.end(),
    [&](const Bone& b) { return b.GetName() == n; });
  return (it == bones.end()) ? glm::mat4(1.f) : it->GetInverseBindTransform();
}

glm::mat4 GetGlobalAnimatedByName(const std::vector<Bone>& bones, const std::string& n) {
  auto it = std::find_if(bones.begin(), bones.end(),
    [&](const Bone& b) { return b.GetName() == n; });
  return (it == bones.end()) ? glm::mat4(1.f) : it->GetGlobalTransform();
}

DebugDiff MatDiff(const glm::mat4& a, const glm::mat4& b) {
  DebugDiff d; d.A = a; d.B = b; d.maxAbs = 0.f;
  for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c)
    d.maxAbs = std::max(d.maxAbs, std::fabs(a[r][c] - b[r][c]));
  return d;
}

void PrintMat(const char* tag, const glm::mat4& m) {
  DLOG(tag << "\n"
    << m[0][0] << " " << m[0][1] << " " << m[0][2] << " " << m[0][3] << "\n"
    << m[1][0] << " " << m[1][1] << " " << m[1][2] << " " << m[1][3] << "\n"
    << m[2][0] << " " << m[2][1] << " " << m[2][2] << " " << m[2][3] << "\n"
    << m[3][0] << " " << m[3][1] << " " << m[3][2] << " " << m[3][3] << "\n");
}

void DebugMeshBinding(const eModel& model, const std::vector<Bone>& bones, int meshId) {
  auto info = GatherMeshInfo(model, meshId);
  DLOG("=== DebugMeshBinding ===");
  DLOG("Mesh: " << info.meshName << "  verts=" << info.numVertices
    << "  bones=" << info.numBones << "  skinned=" << (info.isSkinned ? "yes" : "no"));
  DLOG("Attached node: " << info.attachedNodeName);

  auto names = MeshBoneNames(model, meshId);
  if (names.empty()) {
    DLOG("This mesh has NO bone weights. It must follow the NODE transform chain.");
  }
  else {
    DLOG("Bone names used by this mesh:");
    for (auto& n : names) {
      int idx = GetBoneIndexByName(bones, n);
      if (idx < 0) DLOG("  ! MISSING in skeleton: " << n);
      else         DLOG("    " << n << "  → boneIndex=" << idx);
    }
  }
}

void DebugBoneInfluenceForMesh(const eModel& model, int meshId, const std::string& boneName, float wEps) {
  auto inf = MeshInfluences(model, meshId);
  // You must provide a way to convert boneName->index used in mesh indices:
  // If your mesh indices refer directly to skeleton order, reuse GetBoneIndexByName().
  // If they use a mesh-local bone table, adapt this to your data.
  int target = -1; /* TODO: set this from your mesh's bone table */
  DLOG("=== DebugBoneInfluenceForMesh ===");
  DLOG("MeshId=" << meshId << "  Bone='" << boneName << "'  meshBoneIndex=" << target);

  size_t count = 0;
  for (auto& v : inf) {
    if ((v.idx[0] == (unsigned)target && v.w[0] > wEps) ||
      (v.idx[1] == (unsigned)target && v.w[1] > wEps) ||
      (v.idx[2] == (unsigned)target && v.w[2] > wEps) ||
      (v.idx[3] == (unsigned)target && v.w[3] > wEps)) {
      ++count;
    }
  }
  DLOG("Vertices influenced (> " << wEps << "): " << count);
  if (count == 0) {
    DLOG("No vertices are weighted to that bone on this mesh. It’s probably a rigid node-driven mesh.");
  }
}

void DebugOffsetsVsImporter(const std::vector<Bone>& bones,
  const std::unordered_map<std::string, glm::mat4>& importerOffsets) {
  DLOG("=== DebugOffsetsVsImporter ===");
  for (auto& [name, off] : importerOffsets) {
    glm::mat4 mine = GetInverseBindByName(bones, name);
    auto diff = MatDiff(mine, off);
    if (diff.maxAbs > 1e-3f) {
      DLOG("Offset mismatch bone='" << name << "' maxAbs=" << diff.maxAbs);
      PrintMat("mine(invBind):", mine);
      PrintMat("importer(Offset):", off);
    }
  }
}

glm::vec4 CpuSkinOneVertex(const glm::vec3& pos, const glm::uvec4& idx, const glm::vec4& w,
  const std::vector<glm::mat4>& palette) {
  glm::vec4 p(0, 0, 0, 0);
  if (w.x > 0) p += w.x * (palette[idx.x] * glm::vec4(pos, 1));
  if (w.y > 0) p += w.y * (palette[idx.y] * glm::vec4(pos, 1));
  if (w.z > 0) p += w.z * (palette[idx.z] * glm::vec4(pos, 1));
  if (w.w > 0) p += w.w * (palette[idx.w] * glm::vec4(pos, 1));
  return p;
}

DebugMeshInfo GatherMeshInfo(const eModel& model, int meshId)
{
  DebugMeshInfo info;
  if (model.GetMeshes().size() > meshId)
  {
    //const I3DMesh* mesh = model.Get3DMeshes()[meshId];
    for (int k = 0; k < model.Get3DMeshes().size(); ++k)
    {
      const I3DMesh* mesh = model.Get3DMeshes()[k];
      if (mesh->Name() != "Horse_armor")
        continue;
      std::cout << "------------ " << mesh->Name() << "------------ " << std::endl;
      for (int i = 0; i < mesh->GetVertexs().size(); ++i)
      {
        std::cout << "boneIDs " + std::to_string(i) + " " << mesh->GetVertexs()[i].boneIDs[0] << " "
          << mesh->GetVertexs()[i].boneIDs[1] << " "
          << mesh->GetVertexs()[i].boneIDs[2] << " "
          << mesh->GetVertexs()[i].boneIDs[3] << std::endl;
        std::cout << "weights " + std::to_string(i) + " " << mesh->GetVertexs()[i].weights[0] << " "
          << mesh->GetVertexs()[i].weights[1] << " "
          << mesh->GetVertexs()[i].weights[2] << " "
          << mesh->GetVertexs()[i].weights[3] << std::endl;
      }
    }
  }
  return info;
}

std::vector<std::string> MeshBoneNames(const eModel& model, int meshId)
{
  //@todo
  return std::vector<std::string>();
}

std::vector<DebugVertexInfluence> MeshInfluences(const eModel& model, int meshId)
{
  //@todo
  return std::vector<DebugVertexInfluence>();
}

std::string NodeNameOwningMesh(const eModel& model, int meshId)
{
  //@todo
  return std::string();
}
