#include "base.h"
#include "SceneSerializerYAML.h"
#include "ModelManagerYAML.h"
#include "AnimationManagerYAML.h"
#include "ScriptManager.h"
#include "MainContextBase.h"
#include <opengl_assets/TextureManager.h>
#include <opengl_assets/SoundManager.h>

//------------------------------------------------------------------------
AssetManagement::AssetManagement(eMainContextBase* _context,
																 const std::string& _modelsPath,
																 const std::string& _assetsPath,
																 const std::string& _shadersPath,
																 const std::string& _scriptsPath)
	: m_modelFolderPath(_modelsPath)
	, m_assetsFolderPath(_assetsPath)
	, m_shadersFolderPath(_shadersPath)
	, m_scriptsFolderPath(_scriptsPath)
	, m_texManager(new eTextureManager)
	, m_modelManager(new ModelManagerYAML)
	, m_animationManager(new AnimationManagerYAML(_modelsPath))
	, m_soundManager(new eSoundManager(_assetsPath))
	, m_scriptManager(new ScriptManager(_context))
{}
