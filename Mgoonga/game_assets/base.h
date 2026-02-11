#pragma once

#include "game_assets.h"

#include <base/interfaces.h>

class eTextureManager;
class ModelManagerYAML;
class AnimationManagerYAML;
class eSoundManager;
class ScriptManager;
class eMainContextBase;

//--------------------------------------------
enum class GizmoType // to be compatible with ImGuizmo
{
	TRANSLATE = 7,
	ROTATE = 120,
	SCALE = 896
};

//--------------------------------------------
enum class FramedChoice
{
	WITH_LEFT,
	WITH_RIGHT,
	DISABLED
};

//--------------------------------------------
struct AssetManagement
{
	AssetManagement(const AssetManagement&) = delete;
	AssetManagement& operator=(const AssetManagement&) = delete;

	AssetManagement(AssetManagement&&) noexcept = default;
	AssetManagement& operator=(AssetManagement&&) noexcept = default;

	AssetManagement(eMainContextBase* _context,
		const std::string& _modelsPath,
		const std::string& _assetsPath,
		const std::string& _shadersPath,
		const std::string& _scriptsPath);

	std::unique_ptr<eTextureManager>					m_texManager;
	std::unique_ptr<ModelManagerYAML>					m_modelManager;
	std::unique_ptr<AnimationManagerYAML>			m_animationManager;
	std::unique_ptr<eSoundManager>						m_soundManager;
	std::unique_ptr<ScriptManager>						m_scriptManager;

	std::string																m_modelFolderPath;
	std::string																m_assetsFolderPath;
	std::string																m_shadersFolderPath;
	std::string																m_scriptsFolderPath;

	bool																			m_load_model_multithreading = true;
};