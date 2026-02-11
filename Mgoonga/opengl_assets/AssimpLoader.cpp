#include "stdafx.h"
#include "AssimpLoader.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/material.h>

#include "Model.h"

#include <base/Log.h>
#include <math/Transform.h>

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

// ============================================================
// Build switches (set to 1 locally when you want noise)
// ============================================================

#ifndef ASSIMPLOADER_DEBUG
#define ASSIMPLOADER_DEBUG 0
#endif

#ifndef ASSIMPLOADER_ENABLE_MESH_BIND_CORRECTION
#define ASSIMPLOADER_ENABLE_MESH_BIND_CORRECTION 0
#endif

#if ASSIMPLOADER_DEBUG
#define ADBG(x) do { std::cout << x << std::endl; } while(0)
#else
#define ADBG(x) do {} while(0)
#endif

// ============================================================
// Forward decls (debug helpers live at bottom)
// ============================================================

static glm::mat4  AiToGlm(const aiMatrix4x4& m);
static glm::mat4  CalcNodeGlobal(const aiNode* n);
static glm::mat3  NormalMatrixFrom(const glm::mat4& M);
static std::string NormalizeAssimpName(std::string s);
static int FindAttachBoneIndexForNode(aiNode* node, const std::map<std::string, int>& boneMap);
static std::string NormalizeTexturePath(const std::string& modelDir, const std::string& rel, const std::string& assetPrefix);
static void FixQuaternionContinuity(std::vector<Frame>&frames);
static glm::quat OrthonormalQuatFromMat4(const glm::mat4 & M);

// Optional debug-only dumps
#if ASSIMPLOADER_DEBUG
static void Debug_DumpHierarchy(const aiNode* n, int depth = 0);
static void Debug_DumpMeshesAndBones(const aiScene* s);
static void Debug_DumpAnimationDebug(const aiScene* s);
static void Debug_PrintTRS(const char* tag, const glm::mat4& M);
#endif

// ============================================================
// AssimpLoader
// ============================================================

AssimpLoader::AssimpLoader()
{
  m_import.reset(new Assimp::Importer());
  m_import->SetPropertyInteger(AI_CONFIG_PP_LBW_MAX_WEIGHTS, 4);
}

AssimpLoader::~AssimpLoader()
{
  if (m_scene.get())
    m_scene.release();
}

eModel* AssimpLoader::LoadModel(char* path, const std::string& name, bool invertYuv)
{
  base::Log({ "Start Loading AssimpLoader::LoadModel::" + std::string(path) });

  if (!m_import)
    return nullptr;

  m_import->SetPropertyBool(AI_CONFIG_IMPORT_FBX_READ_ALL_GEOMETRY_LAYERS, true);
  m_import->SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
  m_import->SetPropertyBool(AI_CONFIG_IMPORT_FBX_STRICT_MODE, false);
  m_import->SetPropertyInteger(AI_CONFIG_PP_LBW_MAX_WEIGHTS, 4);

  const aiScene* scene = m_import->ReadFile(path,
    aiProcess_Triangulate |
    aiProcess_GenSmoothNormals |
    aiProcess_CalcTangentSpace |
    aiProcess_LimitBoneWeights);

  if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode)
  {
    std::cout << "ERROR::ASSIMP::" << m_import->GetErrorString() << std::endl;
    return nullptr;
  }

  m_scene.release();
  m_scene.reset((aiScene*)scene);

  m_invert_y_uv = invertYuv;
  m_model = new eModel(path, name);

  // ------------------------------------------------------------
  //   Detect whether the scene is "skinned context"
  // - if false, we must keep baking static meshes (shield case)
  // ------------------------------------------------------------
  m_sceneHasSkin = (scene->mNumAnimations > 0);
  for (unsigned mi = 0; mi < scene->mNumMeshes && !m_sceneHasSkin; ++mi)
  {
    const aiMesh* m = scene->mMeshes[mi];
    if (m && m->HasBones()) m_sceneHasSkin = true;
  }

  PreRegisterSkinnedBones(scene);

#if ASSIMPLOADER_DEBUG
  std::cout << "\n=== Scene Hierarchy ===\n";
  Debug_DumpHierarchy(scene->mRootNode);
  std::cout << "\n=== Mesh/Bone Summary ===\n";
  Debug_DumpMeshesAndBones(scene);
#endif

  // Meshes (collect explicit offsets, vertices, materials)
  _ProcessNode(m_scene->mRootNode, m_scene.get());

  // Root inverse (scene -> model space)
  m_model->m_GlobalInverseTransform = glm::inverse(AiToGlm(m_scene->mRootNode->mTransformation));

  // Nodes as bones + children
  _LoadNodesToBone(m_scene->mRootNode);
  _LoadBoneChildren(m_scene->mRootNode);

  // Root bone pointer
  {
    const std::string rootName = NormalizeAssimpName(m_scene->mRootNode->mName.C_Str());
    auto it = m_model->m_BoneMapping.find(rootName);
    if (it != m_model->m_BoneMapping.end())
      m_model->m_root_bone = &m_model->m_bones[it->second];
    else if (!m_model->m_bones.empty())
      m_model->m_root_bone = &m_model->m_bones.front();
  }

  // Bind pose consistency
  RebuildBindLocalsFromOffsets_SceneSpace();

#if ASSIMPLOADER_ENABLE_MESH_BIND_CORRECTION
  // Keep if you still use it; IMPORTANT: must not overwrite per-mesh corrections already set in _ProcessMesh
  ComputePerMeshBindCorrections(true);
#endif

  if (m_model->m_no_real_bones)
    m_model->mapMehsesToNodes();

  // Animations
  for (uint32_t i = 0; i < m_scene->mNumAnimations; ++i)
    m_model->m_animations.push_back(_ProccessAnimation(m_scene->mAnimations[i]));

#if ASSIMPLOADER_DEBUG
  Debug_DumpAnimationDebug(scene);
#endif

  return m_model;
}

void AssimpLoader::_ProcessNode(aiNode* node, const aiScene* scene)
{
  for (GLuint i = 0; i < node->mNumMeshes; i++)
  {
    aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
    const std::string meshName = mesh->mName.C_Str();

    // prevent duplicates by name
    auto it = std::find_if(m_model->m_meshes.begin(), m_model->m_meshes.end(),
      [&](const eMesh& m) { return m.Name() == meshName; });

    if (it == m_model->m_meshes.end())
      _ProcessMesh(mesh, node, scene);
  }

  for (GLuint i = 0; i < node->mNumChildren; i++)
    _ProcessNode(node->mChildren[i], scene);
}

void AssimpLoader::SetBindCorrectionByMeshName(eModel* model, const std::string& meshName, const glm::mat4& C)
{
  if (!model) return;
  auto it = std::find_if(model->m_meshes.begin(), model->m_meshes.end(),
    [&](eMesh& m) { return m.Name() == meshName; });
  if (it != model->m_meshes.end())
    it->SetBindCorrection(C);
}

void AssimpLoader::_ProcessMesh(aiMesh* mesh, aiNode* ownerNode, const aiScene* scene)
{
  std::vector<Vertex> vertices;
  std::vector<GLuint> indices;
  std::vector<TextureInfo> textures;
  std::vector<eModel::VertexBoneData> boneData;

  // --- UV channel selection ---
  int uvChan = 0;
  if (mesh->mMaterialIndex >= 0)
  {
    aiMaterial* mat = m_scene->mMaterials[mesh->mMaterialIndex];
    int uvSrc = 0;
    if (AI_SUCCESS == aiGetMaterialInteger(mat, AI_MATKEY_UVWSRC(aiTextureType_DIFFUSE, 0), &uvSrc))
      uvChan = uvSrc;
  }
  if (!mesh->HasTextureCoords(uvChan))
    uvChan = 0;

  const bool hasUV = mesh->HasTextureCoords(uvChan);

  // --- Root (scene) and owner global (scene) ---
  const glm::mat4 R = AiToGlm(scene->mRootNode->mTransformation);
  const glm::mat4 Rinv = glm::inverse(R);
  const glm::mat4 Mowner = CalcNodeGlobal(ownerNode);

  // model-space transform that would bake this mesh into root-removed model space:
  const glm::mat4 meshBindModel = Rinv * Mowner;

  // ------------------------------------------------------------
  //  - If this mesh has bones: KEEP baking (your current skin pipeline expects it)
  //  - If this mesh has no bones:
  //      * in skinned scene -> DO NOT bake, use BindCorrection instead
  //      * in static scene  -> bake (shield FBX stays correct)
  // ------------------------------------------------------------
  const bool hasBones = mesh->HasBones();
  const bool useRigidCorrection = (!hasBones && m_sceneHasSkin);

  const bool bakeVertices = (hasBones || !useRigidCorrection);

  const glm::mat4 B = bakeVertices ? meshBindModel : glm::mat4(1.0f);
  const glm::mat3 Nmat = bakeVertices ? NormalMatrixFrom(B) : glm::mat3(1.0f);

  // --- vertices ---
  vertices.reserve(mesh->mNumVertices);
  for (GLuint i = 0; i < mesh->mNumVertices; i++)
  {
    Vertex v{};

    // pos
    glm::vec4 p = B * glm::vec4(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z, 1.0f);
    v.Position = glm::vec3(p);

    // normal
    glm::vec3 n = Nmat * glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
    v.Normal = glm::normalize(n);

    // tangent/bitangent
    if (mesh->mTangents)
    {
      glm::vec3 t = Nmat * glm::vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
      v.tangent = glm::vec4(glm::normalize(t), 1.0f);

      glm::vec3 b = Nmat * glm::vec3(mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z);
      v.bitangent = glm::normalize(b);
    }

    // uv
    if (hasUV)
    {
      const aiVector3D aUV = mesh->mTextureCoords[uvChan][i];
      v.TexCoords.x = aUV.x;
      v.TexCoords.y = m_invert_y_uv ? (1.0f - aUV.y) : aUV.y;
    }
    else
    {
      v.TexCoords = glm::vec2(0.0f);
    }

    vertices.push_back(v);
  }

  // --- Bones / weights ---
  if (hasBones)
  {
    boneData.resize(vertices.size());

    for (uint32_t bi = 0; bi < mesh->mNumBones; ++bi)
    {
      const aiBone* aBone = mesh->mBones[bi];
      const std::string boneName = NormalizeAssimpName(aBone->mName.C_Str());

      // Offset rebased into MODEL space:
      // Omodel = O * inverse(Mowner) * R
      const glm::mat4 O = AiToGlm(aBone->mOffsetMatrix);
      const glm::mat4 Omodel = O * glm::inverse(Mowner) * R;

      int boneIndex = 0;
      auto it = m_model->m_BoneMapping.find(boneName);
      if (it == m_model->m_BoneMapping.end())
      {
        boneIndex = (int)m_model->m_NumBones++;
        Bone newBone(boneIndex, boneName, UNIT_MATRIX, /*real=*/true);
        newBone.SetInverseBindTransform(Omodel);
        m_model->m_bones.push_back(newBone);
        m_model->m_BoneMapping[boneName] = boneIndex;
      }
      else
      {
        boneIndex = it->second;
        Bone& Bn = m_model->m_bones[boneIndex];
        if (!Bn.HasExplicitInverseBind())
          Bn.SetInverseBindTransform(Omodel);
      }

      for (uint32_t w = 0; w < aBone->mNumWeights; w++)
      {
        const int   vid = aBone->mWeights[w].mVertexId;
        const float wt = aBone->mWeights[w].mWeight;
        if (vid >= 0 && vid < (int)boneData.size())
          boneData[vid].AddBoneData(boneIndex, wt);
      }
    }

    for (int i = 0; i < (int)boneData.size(); ++i)
    {
      vertices[i].boneIDs = glm::vec4(
        boneData[i].IDs[0], boneData[i].IDs[1], boneData[i].IDs[2], boneData[i].IDs[3]);
      vertices[i].weights = glm::vec4(
        boneData[i].Weights[0], boneData[i].Weights[1], boneData[i].Weights[2], boneData[i].Weights[3]);
    }
  }
  else
  {
    // Only mark "no real bones" when we are in a skinned context and doing rigid attach.
    if (useRigidCorrection)
      m_model->m_no_real_bones = true;

    if (useRigidCorrection)
    {
      int attachIndex = FindAttachBoneIndexForNode(ownerNode, m_model->m_BoneMapping);
      if (attachIndex < 0) attachIndex = 0;

#if ASSIMPLOADER_DEBUG
      {
        std::string attachedName = "<unknown>";
        for (auto& kv : m_model->m_BoneMapping)
          if (kv.second == attachIndex) { attachedName = kv.first; break; }

        std::cout << "[RigidAttach] mesh='" << mesh->mName.C_Str()
          << "' node='" << ownerNode->mName.C_Str()
          << "' -> boneIndex=" << attachIndex
          << " boneName='" << attachedName << "'\n";
      }
#endif

      for (auto& v : vertices)
      {
        v.boneIDs = glm::vec4((float)attachIndex, 0.f, 0.f, 0.f);
        v.weights = glm::vec4(1.f, 0.f, 0.f, 0.f);
      }
    }
    else
    {
      // Static scene: leave IDs/weights as-is (or set zeros if your vertex format needs it).
      // Most static shaders ignore them anyway.
    }
  }

  // --- Indices ---
  indices.reserve(mesh->mNumFaces * 3);
  for (GLuint i = 0; i < mesh->mNumFaces; i++)
  {
    const aiFace& face = mesh->mFaces[i];
    for (GLuint j = 0; j < face.mNumIndices; j++)
      indices.push_back(face.mIndices[j]);
  }

  // --- Material ---
  Material mat = _ProcessMaterial(mesh, textures);

  const std::string meshName = mesh->mName.C_Str();
  m_model->AddMesh(vertices, indices, textures, mat, meshName.c_str(), mesh->mTangents == NULL);

  // ------------------------------------------------------------
  // Apply BindCorrection only for rigid meshes in skinned scenes
  // (Do NOT apply to static scenes — static render path may ignore it)
  // ------------------------------------------------------------
  if (useRigidCorrection)
  {
    SetBindCorrectionByMeshName(m_model, meshName, meshBindModel);
    m_meshBindCorrection[meshName] = meshBindModel;
  }
}

void AssimpLoader::_LoadNodesToBone(aiNode* node)
{
  for (uint32_t i = 0; i < node->mNumChildren; ++i)
    _LoadNodesToBone(node->mChildren[i]);

  const std::string name = NormalizeAssimpName(node->mName.C_Str());
  const glm::mat4 nodeLocal = AiToGlm(node->mTransformation);

  auto it = std::find_if(m_model->m_bones.begin(), m_model->m_bones.end(),
    [&](const Bone& b) { return b.GetName() == name; });

  if (it == m_model->m_bones.end())
  {
    m_model->m_bones.push_back(Bone(m_model->m_NumBones, name, nodeLocal, false));
    Bone& B = m_model->m_bones.back();
    B.SetMTransform(nodeLocal);
    m_model->m_BoneMapping[name] = (int)m_model->m_NumBones;
    m_model->m_NumBones++;
  }
  else
  {
    it->SetLocalBindTransform(nodeLocal);
    it->SetMTransform(nodeLocal);
  }
}

void AssimpLoader::_LoadBoneChildren(aiNode* node)
{
  for (uint32_t i = 0; i < node->mNumChildren; ++i)
    _LoadBoneChildren(node->mChildren[i]);

  const std::string me = NormalizeAssimpName(node->mName.C_Str());

  auto curIt = std::find_if(m_model->m_bones.begin(), m_model->m_bones.end(),
    [&](const Bone& b) { return b.GetName() == me; });

  if (curIt == m_model->m_bones.end())
    return;

  for (uint32_t i = 0; i < node->mNumChildren; ++i)
  {
    const std::string childName = NormalizeAssimpName(node->mChildren[i]->mName.C_Str());
    auto mapIt = m_model->m_BoneMapping.find(childName);
    if (mapIt != m_model->m_BoneMapping.end())
      curIt->AddChild(&m_model->m_bones[mapIt->second]);
  }
}

// ============================================================
// Animations
// ============================================================

static Transform ConvertAssimpTR(const aiVector3D& pos, const aiQuaternion& rot)
{
  Transform t;
  t.setTranslation(glm::vec3(pos.x, pos.y, pos.z));
  t.setRotation(glm::normalize(glm::quat(rot.w, rot.x, rot.y, rot.z)));
  return t;
}

//----------------------------------------------------------------
SkeletalAnimation AssimpLoader::_ProccessAnimation(const aiAnimation* A)
{
  const double tps = (A->mTicksPerSecond > 0.0) ? A->mTicksPerSecond : 30.0;
  const double seconds = (tps > 0.0) ? (A->mDuration / tps) : 0.0;

  // Bind defaults (translation + rotation)
  std::unordered_map<std::string, std::pair<glm::vec3, glm::quat>> bindTR;
  bindTR.reserve(m_model->m_bones.size());
  for (const Bone& b : m_model->m_bones)
  {
    const glm::mat4 L = b.GetLocalBindTransform();
    glm::vec3 t(L[3][0], L[3][1], L[3][2]);
    glm::quat r = OrthonormalQuatFromMat4(L);
    bindTR[b.GetName()] = { t, r };
  }

  unsigned maxKeys = 0;
  for (unsigned c = 0; c < A->mNumChannels; ++c)
    maxKeys = std::max(maxKeys, std::max(A->mChannels[c]->mNumPositionKeys,
      A->mChannels[c]->mNumRotationKeys));
  if (maxKeys == 0) maxKeys = 1;

  const int durationMsec = (int)(seconds * 1'000.0);

  std::vector<Frame> frames(maxKeys);
  for (unsigned i = 0; i < maxKeys; ++i)
  {
    double t = (seconds * (double)i) / (double)std::max(1u, maxKeys - 1);
    frames[i].AddTimeStamp((int64_t)(t * 1'000.0));
  }

  for (unsigned c = 0; c < A->mNumChannels; ++c)
  {
    const aiNodeAnim* ch = A->mChannels[c];
    std::string name = NormalizeAssimpName(ch->mNodeName.C_Str());

    glm::vec3 bindT(0.0f);
    glm::quat bindR(1, 0, 0, 0);
    if (auto it = bindTR.find(name); it != bindTR.end())
    {
      bindT = it->second.first;
      bindR = it->second.second;
    }

    for (unsigned i = 0; i < maxKeys; ++i)
    {
      const double t = (seconds * (double)i) / (double)std::max(1u, maxKeys - 1);
      const double tick = t * tps;

      aiVector3D pos(bindT.x, bindT.y, bindT.z);
      aiQuaternion rot(bindR.w, bindR.x, bindR.y, bindR.z);

      if (ch->mNumPositionKeys)
      {
        unsigned k = 0;
        while (k + 1 < ch->mNumPositionKeys && ch->mPositionKeys[k + 1].mTime <= tick) ++k;
        pos = ch->mPositionKeys[k].mValue;
      }

      if (ch->mNumRotationKeys)
      {
        unsigned k = 0;
        while (k + 1 < ch->mNumRotationKeys && ch->mRotationKeys[k + 1].mTime <= tick) ++k;
        rot = ch->mRotationKeys[k].mValue;
      }

      frames[i].AddTrnasform(name, ConvertAssimpTR(pos, rot));
    }
  }
  FixQuaternionContinuity(frames);
  return SkeletalAnimation(durationMsec, frames, A->mName.C_Str());
}

// You said you still need this for animation-only files:
SkeletalAnimation AssimpLoader::_ProccessAnimationSimple(const aiAnimation* anim)
{
  int durationMsc = (int)(anim->mDuration / (anim->mTicksPerSecond > 0.0 ? anim->mTicksPerSecond : 30.0) * 1000);
  int qNodes = (int)anim->mNumChannels;
  int qFrames = (qNodes > 0) ? (int)anim->mChannels[0]->mNumPositionKeys : 0;
  if (qFrames <= 0) qFrames = 1;

  std::vector<Frame> frames(qFrames);
  for (int i = 0; i < qFrames; ++i)
    frames[i].AddTimeStamp((durationMsc / qFrames) * i);

  for (int c = 0; c < qNodes; ++c)
  {
    const aiNodeAnim* ch = anim->mChannels[c];
    std::string name = NormalizeAssimpName(ch->mNodeName.C_Str());

    for (int f = 0; f < qFrames; ++f)
    {
      aiVector3D pos(0, 0, 0);
      aiQuaternion rot(1, 0, 0, 0);

      if (ch->mNumPositionKeys) {
        int idx = std::min(f, (int)ch->mNumPositionKeys - 1);
        pos = ch->mPositionKeys[idx].mValue;
      }
      if (ch->mNumRotationKeys) {
        int idx = std::min(f, (int)ch->mNumRotationKeys - 1);
        rot = ch->mRotationKeys[idx].mValue;
      }

      frames[f].AddTrnasform(name, ConvertAssimpTR(pos, rot));
    }
  }
  FixQuaternionContinuity(frames);
  return SkeletalAnimation(durationMsc, frames, anim->mName.C_Str());
}

// Pre-register all deform bones from all meshes so rigid meshes can attach
void AssimpLoader::PreRegisterSkinnedBones(const aiScene* scene)
{
  if (!scene || !m_model) return;

  for (unsigned mi = 0; mi < scene->mNumMeshes; ++mi)
  {
    const aiMesh* mesh = scene->mMeshes[mi];
    if (!mesh || mesh->mNumBones == 0) continue;

    for (unsigned bi = 0; bi < mesh->mNumBones; ++bi)
    {
      const aiBone* ab = mesh->mBones[bi];
      if (!ab) continue;

      std::string boneName = NormalizeAssimpName(ab->mName.C_Str());

      if (m_model->m_BoneMapping.find(boneName) == m_model->m_BoneMapping.end())
      {
        const int id = (int)m_model->m_NumBones++;
        Bone b(id, boneName, UNIT_MATRIX, /*real=*/true);
        // DO NOT set inverse bind here (we don't know the correct rebasing yet).
        // It will be set later in _ProcessMesh when we have owner node.
        m_model->m_bones.push_back(b);
        m_model->m_BoneMapping[boneName] = id;
      }
    }
  }
}

SkeletalAnimation AssimpLoader::ImportAnimation(const std::string& path)
{
  Assimp::Importer importer;
  const aiScene* scene = importer.ReadFile(path, aiProcessPreset_TargetRealtime_Fast);

  if (!scene || !scene->HasAnimations())
    throw std::runtime_error("File has no animations.");

  return _ProccessAnimationSimple(scene->mAnimations[0]);
}

SkeletalAnimation AssimpLoader::ImportAnimationFbx(const std::string& path)
{
  // Keeping this because it’s in the header.
  // If you still rely on it, leave it. Otherwise later we’ll delete it + header entry.
  Assimp::Importer importer;
  const aiScene* scene = importer.ReadFile(path, aiProcessPreset_TargetRealtime_Fast);

  if (!scene || !scene->HasAnimations())
    throw std::runtime_error("File has no animations.");

  // Old behavior preserved: just return simple sampling.
  // (Your previous “bindOffsets injection” code was large and brittle.)
  return _ProccessAnimationSimple(scene->mAnimations[0]);
}

// ============================================================
// Materials
// ============================================================

std::vector<TextureInfo> AssimpLoader::_LoadMaterialTextures(aiMaterial* mat,
  opengl_assets::aiTextureType type,
  std::string typeName)
{
  std::vector<TextureInfo> textures;

  for (GLuint i = 0; i < mat->GetTextureCount((aiTextureType)type); ++i)
  {
    aiString str;
    mat->GetTexture((aiTextureType)type, i, &str);

    const std::string raw = str.C_Str();
    if (!raw.empty() && raw[0] == '*')
    {
      textures.emplace_back(typeName, raw);
      continue;
    }

    const std::string fixed = NormalizeTexturePath(m_model->m_directory, raw, m_model->m_directory);
    textures.emplace_back(typeName, fixed);
  }

  return textures;
}

Material AssimpLoader::_ProcessMaterial(aiMesh* mesh, std::vector<TextureInfo>& textures)
{
  Material mat;

  if (mesh->mMaterialIndex >= 0)
  {
    aiMaterial* material = m_scene->mMaterials[mesh->mMaterialIndex];

    auto diffuseMaps = _LoadMaterialTextures(material, opengl_assets::aiTextureType_DIFFUSE, "texture_diffuse");
    if (!diffuseMaps.empty())
    {
      textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
      mat.used_textures.insert(Material::TextureType::ALBEDO);
    }

    auto specularMaps = _LoadMaterialTextures(material, opengl_assets::aiTextureType_SPECULAR, "texture_specular");
    if (!specularMaps.empty())
    {
      textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
      mat.used_textures.insert(Material::TextureType::METALLIC);
    }

    auto normalMaps = _LoadMaterialTextures(material, opengl_assets::aiTextureType_NORMALS, "texture_normal");
    if (!normalMaps.empty())
    {
      textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
      mat.used_textures.insert(Material::TextureType::NORMAL);
    }

    auto emissionMaps = _LoadMaterialTextures(material, opengl_assets::aiTextureType_EMISSIVE, "texture_emission");
    if (!emissionMaps.empty())
    {
      textures.insert(textures.end(), emissionMaps.begin(), emissionMaps.end());
      mat.used_textures.insert(Material::TextureType::EMISSIVE);
    }

    // NOTE: “shininess” is not roughness; but you kept it this way, so I keep it.
    auto roughnessMaps = _LoadMaterialTextures(material, opengl_assets::aiTextureType_SHININESS, "texture_roughness");
    if (!roughnessMaps.empty())
    {
      textures.insert(textures.end(), roughnessMaps.begin(), roughnessMaps.end());
      mat.used_textures.insert(Material::TextureType::ROUGHNESS);
    }
  }

  return mat;
}

// ============================================================
// Bind rebuild utilities (declared in header)
// ============================================================

void AssimpLoader::RebuildBindLocalsFromOffsets_SceneSpace()
{
  if (!m_model || !m_scene || !m_model->m_root_bone)
    return;

  const glm::mat4 R = AiToGlm(m_scene->mRootNode->mTransformation);
  const glm::mat4 Rinv = glm::inverse(R);

  std::function<void(Bone&, const glm::mat4&)> walk =
    [&](Bone& b, const glm::mat4& parentGlobalScene)
    {
      glm::mat4 globalSceneBind(1.f);

      if (b.HasExplicitInverseBind())
      {
        // offsetModel == inverse(globalModelBind)
        const glm::mat4 offsetModel = b.GetInverseBindTransform();
        const glm::mat4 globalModelBind = glm::inverse(offsetModel);

        // convert model-bind global -> scene space
        globalSceneBind = R * globalModelBind;
      }
      else
      {
        // no explicit: use authored node local (stored in MTransform)
        const glm::mat4 localScene = b.GetMTransform();
        globalSceneBind = parentGlobalScene * localScene;
      }

      // local scene bind
      const glm::mat4 localBindScene = glm::inverse(parentGlobalScene) * globalSceneBind;
      b.SetLocalBindTransform(localBindScene);
      b.SetMTransform(localBindScene);

      // ensure every bone has a usable inverse bind in MODEL space
      if (!b.HasExplicitInverseBind())
      {
        const glm::mat4 globalModelBind = Rinv * globalSceneBind;
        b.SetInverseBindTransform(glm::inverse(globalModelBind));
      }

      for (Bone* c : b.GetChildren())
        if (c) walk(*c, globalSceneBind);
    };

  walk(*m_model->m_root_bone, glm::mat4(1.f));
}

void AssimpLoader::RebuildBindLocalsFromBakedOffsets_ModelSpace()
{
  // Kept only because it’s in the header; not used in the main pipeline.
  // If you later remove it from the header, you can delete this.
  if (!m_model || !m_model->m_root_bone)
    return;

  std::function<void(Bone&, const glm::mat4&)> walk =
    [&](Bone& b, const glm::mat4& parentGlobalModelBind)
    {
      glm::mat4 globalModelBind(1.f);

      if (b.HasExplicitInverseBind())
      {
        globalModelBind = glm::inverse(b.GetInverseBindTransform());
        glm::mat4 localBind = glm::inverse(parentGlobalModelBind) * globalModelBind;
        b.SetLocalBindTransform(localBind);
        b.SetMTransform(localBind);
      }
      else
      {
        glm::mat4 localBind = b.GetLocalBindTransform();
        globalModelBind = parentGlobalModelBind * localBind;
      }

      for (Bone* c : b.GetChildren())
        if (c) walk(*c, globalModelBind);
    };

  walk(*m_model->m_root_bone, glm::mat4(1.f));
}

void AssimpLoader::RebuildLocalBindFromOffsets()
{
  // Legacy path; kept only because header declares it.
  // Prefer RebuildBindLocalsFromOffsets_SceneSpace().
  if (!m_scene || !m_model || !m_model->m_root_bone)
    return;

  RebuildBindLocalsFromOffsets_SceneSpace();
}

void AssimpLoader::LogMeshOffsetsVsComputedInverseBind(float /*posEps*/, float /*matEps*/) const
{
  // Header-declared debug helper. Keep quiet unless debugging.
#if ASSIMPLOADER_DEBUG
  base::Log("LogMeshOffsetsVsComputedInverseBind: (debug stub in refactor)");
#endif
}

void AssimpLoader::ComputePerMeshBindCorrections(bool /*preferPelvis*/)
{
#if !ASSIMPLOADER_ENABLE_MESH_BIND_CORRECTION
  return;
#else
  // IMPORTANT:
  // - Do NOT clear m_meshBindCorrection here.
  // - Preserve corrections already set (rigid attachments).
  // - Fill missing ones with identity.

  if (!m_model) return;

  for (auto& meshObj : m_model->m_meshes)
  {
    auto it = m_meshBindCorrection.find(meshObj.Name());
    if (it != m_meshBindCorrection.end())
    {
      meshObj.SetBindCorrection(it->second);
    }
    else
    {
      meshObj.SetBindCorrection(glm::mat4(1.f));
      // Optionally track it:
      // m_meshBindCorrection[meshObj.Name()] = glm::mat4(1.f);
    }
  }
#endif
}

// ============================================================
// STATIC HELPERS + DEBUG AT BOTTOM (as requested)
// ============================================================

static glm::mat4 AiToGlm(const aiMatrix4x4& from)
{
  glm::mat4 to;
  to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
  to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
  to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
  to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
  return to;
}

static std::string NormalizeAssimpName(std::string s)
{
  if (auto p = s.find("_$AssimpFbx$_"); p != std::string::npos) s.erase(p);
  std::replace(s.begin(), s.end(), ':', '_');
  return s;
}

static glm::mat4 CalcNodeGlobal(const aiNode* n)
{
  glm::mat4 G(1.f);
  std::vector<const aiNode*> chain;
  while (n) { chain.push_back(n); n = n->mParent; }
  for (auto it = chain.rbegin(); it != chain.rend(); ++it)
    G = G * AiToGlm((*it)->mTransformation);
  return G;
}

static glm::mat3 NormalMatrixFrom(const glm::mat4& M)
{
  return glm::transpose(glm::inverse(glm::mat3(M)));
}

static bool EndsWithDotDigits(const std::string& s, size_t dotPos)
{
  if (dotPos == std::string::npos || dotPos + 1 >= s.size()) return false;
  for (size_t i = dotPos + 1; i < s.size(); ++i)
    if (!std::isdigit((unsigned char)s[i])) return false;
  return true;
}

static int FindAttachBoneIndexForNode(aiNode* node, const std::map<std::string, int>& boneMap)
{
  aiNode* cur = node;
  while (cur)
  {
    // 1) normalized lookup
    std::string key = NormalizeAssimpName(cur->mName.C_Str());
    auto it = boneMap.find(key);
    if (it != boneMap.end()) return it->second;

    // 2) Blender duplicate objects often have ".001" suffix — strip it and try again
    // (only if suffix is numeric)
    size_t dot = key.rfind('.');
    if (EndsWithDotDigits(key, dot))
    {
      std::string stripped = key.substr(0, dot);
      auto it2 = boneMap.find(stripped);
      if (it2 != boneMap.end()) return it2->second;
    }

    cur = cur->mParent;
  }
  return -1;
}

static std::string NormalizeTexturePath(const std::string& modelDir,
  const std::string& rel,
  const std::string& /*assetPrefix*/)
{
  // Embedded textures ("*0", "*1", ...)
  if (!rel.empty() && rel[0] == '*')
    return rel;

  if (rel.empty())
    return {};

  std::string r = rel;
  std::replace(r.begin(), r.end(), '\\', '/');

  fs::path p(r);

  // IMPORTANT:
  // - Do NOT collapse absolute paths to filename (that loses "Knight.fbm/...")
  // - If path is relative, resolve it against modelDir (folder containing the model)
  fs::path resolved = p.is_absolute() ? p : (fs::path(modelDir) / p);
  resolved = resolved.lexically_normal();

  return resolved.generic_string();
}

//------------------------------------------------------------------
static void FixQuaternionContinuity(std::vector<Frame>& frames)
{
  if (frames.size() < 2) return;

  // Collect all joint names that appear in any frame
  std::vector<std::string> joints;
  {
    std::unordered_set<std::string> set;
    for (auto& f : frames)
      for (auto& kv : f.m_pose)
        set.insert(kv.first);
    joints.assign(set.begin(), set.end());
  }

  for (const std::string& joint : joints)
  {
    bool havePrev = false;
    glm::quat prev(1, 0, 0, 0);

    for (auto& f : frames)
    {
      auto it = f.m_pose.find(joint);
      if (it == f.m_pose.end())
        continue;

      Transform& tr = it->second;

      glm::quat q = glm::normalize(tr.getRotation());
      if (havePrev)
      {
        // Hemisphere fix: keep shortest-path continuity
        if (glm::dot(prev, q) < 0.0f)
          q = -q;
      }

      tr.setRotation(q); // updates matrix too
      prev = q;
      havePrev = true;
    }
  }
}

//---------------------------------------------------------------------------
static glm::quat OrthonormalQuatFromMat4(const glm::mat4& M)
{
  glm::vec3 c0 = glm::vec3(M[0]);
  glm::vec3 c1 = glm::vec3(M[1]);
  glm::vec3 c2 = glm::vec3(M[2]);

  float sx = glm::length(c0); c0 = (sx > 0) ? (c0 / sx) : glm::vec3(1, 0, 0);
  float sy = glm::length(c1); c1 = (sy > 0) ? (c1 / sy) : glm::vec3(0, 1, 0);
  float sz = glm::length(c2); c2 = (sz > 0) ? (c2 / sz) : glm::vec3(0, 0, 1);

  glm::mat3 R;
  R[0] = c0; R[1] = c1; R[2] = c2;
  return glm::normalize(glm::quat_cast(R));
}

#if ASSIMPLOADER_DEBUG
static glm::vec3 Debug_ExtractT(const glm::mat4& M) { return glm::vec3(M[3]); }
static glm::vec3 Debug_ExtractS(const glm::mat4& M)
{
  return glm::vec3(glm::length(glm::vec3(M[0])),
    glm::length(glm::vec3(M[1])),
    glm::length(glm::vec3(M[2])));
}
static void Debug_PrintTRS(const char* tag, const glm::mat4& M)
{
  glm::vec3 t = Debug_ExtractT(M);
  glm::vec3 s = Debug_ExtractS(M);
  std::printf("%s  T=(%.6f %.6f %.6f)  S=(%.6f %.6f %.6f)\n",
    tag, t.x, t.y, t.z, s.x, s.y, s.z);
}
static void Debug_DumpHierarchy(const aiNode* n, int depth)
{
  std::string indent(depth * 2, ' ');
  std::cout << indent << "- " << n->mName.C_Str()
    << "  (meshes: " << n->mNumMeshes
    << ", children: " << n->mNumChildren << ")\n";
  for (unsigned i = 0; i < n->mNumChildren; ++i)
    Debug_DumpHierarchy(n->mChildren[i], depth + 1);
}
static void Debug_DumpMeshesAndBones(const aiScene* s)
{
  std::cout << "[Meshes: " << s->mNumMeshes << "]\n";
  for (unsigned m = 0; m < s->mNumMeshes; ++m)
  {
    const aiMesh* mesh = s->mMeshes[m];
    std::cout << "  * Mesh[" << m << "] '" << mesh->mName.C_Str()
      << "' vtx=" << mesh->mNumVertices
      << " faces=" << mesh->mNumFaces
      << " bones=" << mesh->mNumBones << "\n";
    for (unsigned b = 0; b < mesh->mNumBones; ++b)
    {
      const aiBone* bone = mesh->mBones[b];
      std::cout << "      - Bone[" << b << "] " << bone->mName.C_Str()
        << " weights=" << bone->mNumWeights << "\n";
    }
  }
}
static void Debug_DumpAnimationDebug(const aiScene* s)
{
  std::cout << "\n=== Animations: " << s->mNumAnimations << " ===\n";
  for (unsigned a = 0; a < s->mNumAnimations; ++a)
  {
    const aiAnimation* A = s->mAnimations[a];
    double tps = (A->mTicksPerSecond > 0.0) ? A->mTicksPerSecond : 30.0;
    double seconds = (tps > 0.0) ? (A->mDuration / tps) : 0.0;

    std::cout << "  * Anim[" << a << "] '"
      << (A->mName.length ? A->mName.C_Str() : "<unnamed>") << "'"
      << " channels=" << A->mNumChannels
      << " durationTicks=" << A->mDuration
      << " tps=" << tps
      << " seconds=" << seconds << "\n";
  }
}
#endif
