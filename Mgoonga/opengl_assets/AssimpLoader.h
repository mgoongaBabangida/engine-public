#pragma once

#include "opengl_assets.h"
#include "texture_type.h"

#include <base/base.h>

#include <math/SkeletalAnimation.h>

class eModel;

#define NUM_BONES_PER_VEREX 4

struct aiNode;
struct aiMesh;
struct aiScene;
struct aiAnimation;
struct aiMaterial;

namespace Assimp {
	class Importer;
}

//--------------------------------------------------------------------
class DLL_OPENGL_ASSETS AssimpLoader
{
public:
	AssimpLoader();
	~AssimpLoader();

	eModel* LoadModel(char* path,
									const std::string& _name,
									bool _m_invert_y_uv = false);

	SkeletalAnimation					ImportAnimation(const std::string& path);
	SkeletalAnimation					ImportAnimationFbx(const std::string& path);
protected:
	void											_ProcessNode(aiNode* node, const aiScene* scene);
	void											_ProcessMesh(aiMesh* mesh, aiNode* _ownerNode, const aiScene* scene);
	void											_LoadNodesToBone(aiNode* node);
	void											_LoadBoneChildren(aiNode* node);
	SkeletalAnimation					_ProccessAnimation(const aiAnimation* anim);
	std::vector<TextureInfo>	_LoadMaterialTextures(aiMaterial* mat, opengl_assets::aiTextureType type, std::string typeName);
	Material									_ProcessMaterial(aiMesh* mesh, std::vector<TextureInfo>& textures);

	void RebuildLocalBindFromOffsets();
	void LogMeshOffsetsVsComputedInverseBind(float posEps = 1e-4f, float matEps = 1e-3f) const;
	void ComputePerMeshBindCorrections(bool preferPelvis =true);
	void SetBindCorrectionByMeshName(eModel* model, const std::string& meshName, const glm::mat4& C);

	void RebuildBindLocalsFromBakedOffsets_ModelSpace();
	void RebuildBindLocalsFromOffsets_SceneSpace();

	SkeletalAnimation _ProccessAnimationSimple(const aiAnimation* _anim);
	void							PreRegisterSkinnedBones(const aiScene* scene);

protected:
	std::unique_ptr<Assimp::Importer>				m_import;
	std::unique_ptr<aiScene>								m_scene;
	eModel*																	m_model = nullptr;
	bool																		m_invert_y_uv;

private:
	std::unordered_map<std::string, glm::mat4> m_meshBindCorrection; // key: mesh name
	bool m_sceneHasSkin = false;
};