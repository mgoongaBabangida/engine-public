#include "stdafx.h"
#include "AmericanTreasureGame.h"
#include "GameController.h"

#include <base/InputController.h>

#include <math/ParticleSystem.h>
#include <math/BaseCollider.h>

#include <opengl_assets/Texture.h>
#include <opengl_assets/RenderManager.h>
#include <opengl_assets/TextureManager.h>
#include <opengl_assets/SoundManager.h>

#include <game_assets/ModelManagerYAML.h>
#include <game_assets/AnimationManagerYAML.h>
#include <game_assets/GUIController.h>
#include <game_assets/CameraFreeController.h>

#include <sdl_assets/ImGuiContext.h>

#include <game_assets/ObjectFactory.h>
#include <game_assets/BezierCurveUIController.h>

//---------------------------------------------------------------------------
AmericanTreasureGame::AmericanTreasureGame(eInputController* _input,
                                           std::vector<IWindowImGui*>& _externalGui,
                                           const std::string& _modelsPath,
                                           const std::string& _assetsPath,
                                           const std::string& _shadersPath,
                                           const std::string& _scripts,
                                           int _width,
                                           int _height)
  : eMainContextBase(_input, _externalGui, _modelsPath, _assetsPath, _shadersPath, _scripts, _width, _height)
{
}

//--------------------------------------------------------------------------
AmericanTreasureGame::~AmericanTreasureGame() {}

//-----------------------------------------------------------------------------
void AmericanTreasureGame::InitializeModels()
{
  eMainContextBase::InitializeModels();
  
  //MODELS
  Material pbr1;
  pbr1.textures[Material::TextureType::ALBEDO] = m_asset_manager.m_texManager->Find("pbr1_basecolor")->m_id;
  pbr1.textures[Material::TextureType::METALLIC] = m_asset_manager.m_texManager->Find("pbr1_metallic")->m_id;
  pbr1.textures[Material::TextureType::NORMAL] = m_asset_manager.m_texManager->Find("pbr1_normal")->m_id;
  pbr1.textures[Material::TextureType::ROUGHNESS] = m_asset_manager.m_texManager->Find("pbr1_roughness")->m_id;

  pbr1.textures[Material::TextureType::EMISSIVE] = Texture::GetTexture1x1(BLACK).m_id;
  pbr1.used_textures.insert(Material::TextureType::ALBEDO);
  pbr1.used_textures.insert(Material::TextureType::METALLIC);
  pbr1.used_textures.insert(Material::TextureType::NORMAL);
  pbr1.used_textures.insert(Material::TextureType::ROUGHNESS);
  pbr1.used_textures.insert(Material::TextureType::EMISSIVE);
  pbr1.ao = 0.8f;

  Material red;
  red.albedo = glm::vec3(0.9f, 0.0f, 0.0f);
  red.ao = 1.0f;
  red.roughness = 0.5;
  red.metallic = 0.5;

  m_asset_manager.m_modelManager->Add("sphere_textured", Primitive::SPHERE, std::move(pbr1));
  m_asset_manager.m_modelManager->Add("sphere_red", Primitive::SPHERE, std::move(red));

  //GLOBAL CONTROLLERS 
  IWindowImGui* debug_window = this->m_externalGui.size() > 10 ? m_externalGui[10] : nullptr;
  m_global_scripts.push_back(std::make_shared<GameController>(this, m_asset_manager.m_modelManager.get(), m_asset_manager.m_texManager.get(), m_asset_manager.m_soundManager.get(), m_pipeline, GetMainCamera(), debug_window));
  m_global_scripts.push_back(std::make_shared<GUIControllerMenuWithButtons>(this, m_pipeline, m_asset_manager.m_soundManager->GetSound("page_sound")));
  m_global_scripts.push_back(std::make_shared<CameraFreeController>(GetMainCamera() /*,true*/));

  m_input_controller->AddObserver(this, WEAK);
  m_input_controller->AddObserver(&*m_global_scripts.back(), WEAK);
}
