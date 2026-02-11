#include "stdafx.h"

#include "MainContextBase.h"
#include "ObjectFactory.h"

#include "SceneSerializerYAML.h"
#include "ModelManagerYAML.h"
#include "AnimationManagerYAML.h"
#include "ScriptManager.h"

#include "DynamicBoundingBoxColliderSerializerYAML.h"

#include "ParticleSystemToolController.h"
#include "PhysicsSystemController.h"
#include "TerrainGeneratorTool.h"
#include "VolumetricCloudsTool.h"
#include "CalculusTool.h"
#include "BezierCurveUIController.h"
#include "CameraSecondScript.h"
#include "EngineBaseVisualsScript.h"
#include "LightsController.h"
#include "SettingsLoadingService.h"

#include <base/InputController.h>
#include <base/Log.h>

#include <tcp_lib/Network.h>
#include <tcp_lib/Server.h>
#include <tcp_lib/Client.h>

#include <opengl_assets/TextureManager.h>
#include <opengl_assets/SoundManager.h>
#include <opengl_assets/Sound.h>
#include <opengl_assets/GUI.h>

#ifndef STANDALONE
#include <sdl_assets/ImGuiContext.h>
#endif

#define RIGGER_WND_USE

#include <math/Clock.h>
#include <math/ParticleSystem.h>
#include <math/PhysicsSystem.h>
#include <math/RigAnimator.h>
#include <math/Utils.h>
#include <math/BoxColliderDynamic.h>

#include <sstream>

//-----------------------------------------------------------------
eMainContextBase::eMainContextBase(eInputController* _input,
	                               std::vector<IWindowImGui*>& _externalGui,
	                               const std::string& _modelsPath,
	                               const std::string& _assetsPath,
	                               const std::string& _shadersPath,
																 const std::string& _scriptsPath,
																 int _width,
																 int _height)
: m_input_controller(_input)
, m_pipeline(_width, _height)
, m_physics_system(new dbb::PhysicsSystem)
, m_externalGui(_externalGui)
, m_asset_manager(this, _modelsPath, _assetsPath, _shadersPath, _scriptsPath)
{
	m_scene.m_cameras.reserve(8); //@todo redesign
}

//-------------------------------------------------------------------------
eMainContextBase::~eMainContextBase()
{
	StopPhysics.Occur();
	m_global_scripts.clear();
	m_scene.m_objects.clear();
}

//-------------------------------------------------------------------------
uint32_t eMainContextBase::Width() const { return m_pipeline.Width(); }
//-------------------------------------------------------------------------
uint32_t eMainContextBase::Height()  const { return m_pipeline.Height(); }


//*********************InputObserver*********************************
//--------------------------------------------------------------------------
bool eMainContextBase::OnKeyJustPressed(uint32_t _asci, KeyModifiers _modifier)
{
	switch (_asci)
	{
		case ASCII_Q:
		{
			if (m_use_guizmo == false)
			{
				m_use_guizmo = true;
				m_gizmo_type = GizmoType::TRANSLATE;
			}
			else if (m_gizmo_type == GizmoType::TRANSLATE)
				m_gizmo_type = GizmoType::ROTATE;
			else if (m_gizmo_type == GizmoType::ROTATE)
				m_gizmo_type = GizmoType::SCALE;
			else if (m_gizmo_type == GizmoType::SCALE)
				m_use_guizmo = false;
			return true;
		}
	default: return false;
	}
}

//--------------------------------------------------------------------------
eTextureManager* eMainContextBase::GetTextureManager() const
{
	return m_asset_manager.m_texManager.get();
}

//----------------------------------------------------------------------------------
std::shared_ptr<TerrainGeneratorTool> eMainContextBase::CreateTerrainGeneratorTool()
{
	auto tool = std::make_shared<TerrainGeneratorTool>(this, m_asset_manager.m_modelManager.get(), m_asset_manager.m_texManager.get(), m_pipeline, m_externalGui.empty() ? nullptr : m_externalGui[ExternalWindow::TERRAIN_GENERATOR_WND]);
	m_global_scripts.push_back(tool);
	m_global_scripts.back()->Initialize();
	return tool;
}

//------------------------------------------------------------------------
void eMainContextBase::PrintScreen()
{
	m_pipeline.GetDefaultBufferTexture().saveToFile("PrintScreen.png");
}

//--------------------------------------------------------------------------
bool eMainContextBase::OnMousePress(int32_t x, int32_t y, bool _left, KeyModifiers _modifier)
{
	if (_modifier == KeyModifiers::SHIFT)
		return true;

	if (m_scene.m_framed)
		m_scene.m_framed->clear();

	//Get Visible and Children
	std::vector<shObject> visible_objects;
	for (auto obj : m_scene.m_objects)
		if (obj->IsVisible())
			visible_objects.push_back(obj);

	auto [picked, intersaction] = GetMainCamera().getCameraRay().CalculateIntersaction(visible_objects, static_cast<float>(x), static_cast<float>(y));
	if (picked != m_scene.m_focused)
		ObjectPicked.Occur(picked, _left);

	if (m_input_strategy)
		m_input_strategy->OnMousePress(x, y, _left);

	return false;
}

//--------------------------------------------------------------------------
bool eMainContextBase::OnMouseMove(int32_t x, int32_t y, KeyModifiers _modifier)
{
	if (m_scene.m_update_hovered)
	{
		auto [picked, intersaction] = GetMainCamera().getCameraRay().CalculateIntersaction(m_scene.m_objects, static_cast<float>(x), static_cast<float>(y));
		m_scene.m_hovered = picked;
	}

	if (GetMainCamera().getCameraRay().IsPressedWithLeft() && m_framed_choice_enabled == FramedChoice::WITH_LEFT
		|| GetMainCamera().getCameraRay().IsPressedWithRight() && m_framed_choice_enabled == FramedChoice::WITH_RIGHT)
	{
		if ((!m_input_strategy || (m_input_strategy && !m_input_strategy->OnMouseMove(x, y))))
		{
			m_scene.m_framed = std::make_shared<std::vector<shObject>>(GetMainCamera().getCameraRay().OnMove(m_scene.m_objects, static_cast<float>(x), static_cast<float>(y))); 	//to draw a frame
			return true;
		}
	}
	else
	{
		if (m_input_strategy)
			m_input_strategy->OnMouseMove(x, y);
	}
	return false;
}

//---------------------------------------------------------------------------------
bool eMainContextBase::OnMouseRelease(KeyModifiers _modifier)
{
	if (m_input_strategy)
		m_input_strategy->OnMouseRelease();
	return false;
}

//--------------------------------------------------------------------------
bool eMainContextBase::OnDropEvent(uint32_t x, uint32_t y, const std::string& _file_path)
{
	OnMousePress(x, y, true, KeyModifiers::NONE); // update m_scene.m_focused
	// load file in gl-thread and assign to m_scene.m_focused albedo
	m_scene.m_drop_path = _file_path;
	return false;
}

//-------------------------------------------------------------------------------
void eMainContextBase::PaintGL()
{
	int64_t tick = m_global_clock.newFrame();
	float msc = static_cast<float>(tick);

	if (m_gameState == IGame::GameState::LOADED)
	{
		if (!m_scene.m_texts.empty() && m_pipeline.ShowFPS() && tick > 0)
			m_scene.m_texts[0]->content = { "FPS " + std::to_string(1000 / tick) };

		_AcceptDrop();
		m_asset_manager.m_scriptManager->checkForScriptChanges();

		for (auto& script : m_global_scripts) script->Update(msc);

		// --- Build RenderBuckets ---
		RenderBuckets buckets;
		// (no need to prefill; default-constructed vectors are fine)
		auto pushByType = [&](const shObject& obj) {
			const auto rt = obj->GetRenderType();
			const auto idx = to_index(rt);
			buckets[idx].push_back(obj);
		};

		// Source list (avoid copies when not culling)
		const auto& srcObjects = m_scene.m_objects;
		std::vector<shObject> culled;
		const std::vector<shObject>* allObjectsPtr = &srcObjects;

		if (m_pipeline.IsFrustumCull())
		{
			culled = GetMainCamera().getCameraRay().FrustumCull(srcObjects);
			allObjectsPtr = &culled;
		}
		const auto& allObjects = *allObjectsPtr;

		for (const shObject& object : allObjects)
		{
			if (!object || !object->IsVisible()) continue;

			if (auto* s = object->GetScript()) s->Update(msc);
			for (const shObject& child : object->GetChildrenObjects())
				if(child->GetScript()) child->GetScript()->Update(msc);

			pushByType(object);
			for (const shObject& child : object->GetChildrenObjects())
				if (child && child->IsVisible())
				{
					pushByType(child);
					for (const shObject& child2 : child->GetChildrenObjects()) //@todo need real recusrsion
						if (child2 && child2->IsVisible())
							pushByType(child2);
				}
		}

		if (m_input_strategy) m_input_strategy->UpdateInRenderThread();

		if (GetMainLight().type == eLightType::DIRECTION || GetMainLight().type == eLightType::CSM)
			GetMainLight().light_direction = -GetMainLight().light_position;

		// Focused/outlined bucket (OUTLINED index)
		{
			auto& outlined = buckets[to_index(eObject::RenderType::OUTLINED)];
			if (m_scene.m_framed && !m_scene.m_framed->empty())
				outlined.insert(outlined.end(), m_scene.m_framed->begin(), m_scene.m_framed->end());
			else if (m_scene.m_focused)
				outlined.push_back(m_scene.m_focused);
		}

		m_pipeline.UpdateSharedUniforms();
		m_pipeline.RenderFrame(m_scene, buckets, msc);
	}
	else if (m_gameState == IGame::GameState::MODEL_RELOAD)
	{
		m_asset_manager.m_modelManager->ReloadTextures();
		if (m_scene.m_welcome) DeleteGUI(m_scene.m_welcome);
		m_gameState = IGame::GameState::LOADED;
	}
	else if (m_gameState == IGame::GameState::LOADING)
	{
		if (m_scene.m_guis.empty())
		{
			m_scene.m_welcome = std::make_shared<GUI>(Width() / 4, Height() / 4, Width() / 2, Height() / 2, Width(), Height());
			const Texture* t = GetTexture("Twelcome");
			m_scene.m_welcome->SetTexture(*t, { 0,0 }, { t->m_width, t->m_height });
			AddGUI(m_scene.m_welcome);
		}

		RenderBuckets emptyBuckets; // all vectors empty
		m_pipeline.UpdateSharedUniforms();
		m_pipeline.RenderFrame(m_scene, emptyBuckets, msc);
	}
}

//******************************Get Set Add Delete*********************************
//--------------------------------------------------------------------------------
uint32_t eMainContextBase::GetFinalImageId()
{
	if (m_pipeline.GetEnabledCameraInterpolationRef())
		return m_asset_manager.m_texManager->Find("computeImageRWCameraInterpolation")->m_id;
	else
		return m_pipeline.GetDefaultBufferTexture().m_id;
}

//--------------------------------------------------------------------------------
uint32_t eMainContextBase::GetUIlessImageId()
{
	return m_pipeline.GetUIlessTexture().m_id;
}

//---------------------------------------------------------------------------------
std::shared_ptr<eObject> eMainContextBase::GetFocusedObject()
{
	return m_scene.m_focused;
}

//--------------------------------------------------------------------------------
std::shared_ptr<eObject> eMainContextBase::GetHoveredObject()
{
	return m_scene.m_hovered;
}

//--------------------------------------------------------------------------------
void eMainContextBase::AddObject(std::shared_ptr<eObject> _object)
{
	ObjectBeingAddedToScene.Occur(_object);
	m_scene.m_objects.push_back(_object);
}

//--------------------------------------------------------------------------------
void eMainContextBase::DeleteObject(std::shared_ptr<eObject> _object)
{
	if (auto it = std::remove(m_scene.m_objects.begin(), m_scene.m_objects.end(), _object); it != m_scene.m_objects.end())
	{
		ObjectBeingDeletedFromScene.Occur(_object);
		m_scene.m_objects.erase(it);
	}
}

//--------------------------------------------------------------------------------
void eMainContextBase::SetFocused(std::shared_ptr<eObject> _newFocused)
{
	auto old_focused = m_scene.m_focused;
	m_scene.m_focused = _newFocused;
	FocusChanged.Occur(old_focused, _newFocused);
}

//--------------------------------------------------------------------------------
void eMainContextBase::SetFocused(const eObject* _newFocused)
{
	for (shObject object : m_scene.m_objects)
	{
		if (object.get() == _newFocused)
		{
			auto old_focused = m_scene.m_focused;
			m_scene.m_focused = object;
			FocusChanged.Occur(old_focused, object);
		}
	}
}

//--------------------------------------------------------------------------------
void eMainContextBase::SetFramed(const std::vector<shObject>& _framed)
{
	m_scene.m_framed = std::make_shared<std::vector<shObject>>(_framed);
}

//--------------------------------------------------------------------------------
void eMainContextBase::AddInputObserver(IInputObserver* _observer, ePriority _priority)
{
	m_input_controller->AddObserver(_observer, _priority);
}

//-------------------------------------------------------------------------------
void eMainContextBase::DeleteInputObserver(IInputObserver* _observer)
{
	m_input_controller->DeleteObserver(_observer);
}

//--------------------------------------------------------------------------------
std::vector<std::shared_ptr<IParticleSystem>> eMainContextBase::GetParticleSystems()
{
	return m_pipeline.GetParticleSystems();
}

//--------------------------------------------------------------------------------
const Texture* eMainContextBase::GetTexture(const std::string& _name) const
{
	return m_asset_manager.m_texManager->Find(_name);
}

//--------------------------------------------------------------------------------
glm::mat4 eMainContextBase::GetMainCameraViewMatrix()
{
	return m_scene.m_cameras[m_scene.m_cur_camera].getWorldToViewMatrix();
}

//--------------------------------------------------------------------------------
glm::mat4 eMainContextBase::GetMainCameraProjectionMatrix()
{
	return m_scene.m_cameras[m_scene.m_cur_camera].getProjectionMatrix();
}

//--------------------------------------------------------------------------------
glm::vec3 eMainContextBase::GetMainCameraPosition() const
{
	return m_scene.m_cameras[m_scene.m_cur_camera].getPosition();
}

//-------------------------------------------------------------------------------
glm::vec3 eMainContextBase::GetMainCameraDirection() const
{
	return m_scene.m_cameras[m_scene.m_cur_camera].getDirection();
}

//------------------------------------------------------------
void eMainContextBase::_PreInitModelManager()
{
	m_asset_manager.m_modelManager->InitializePrimitives();
	//PRIMITIVES
	m_asset_manager.m_modelManager->Add("wall_cube",
		std::make_shared<MyModel>(m_asset_manager.m_modelManager->FindBrush("cube"),
															"wall_cube",
															m_asset_manager.m_texManager->Find("Tbrickwall0_d"),
															m_asset_manager.m_texManager->Find("Tbrickwall0_d"),
															m_asset_manager.m_texManager->Find("Tbrickwall0_n"),
															&Texture::GetTexture1x1(BLACK)));
	m_asset_manager.m_modelManager->Add("container_cube",
		std::make_shared<MyModel>(m_asset_manager.m_modelManager->FindBrush("cube"),
															"container_cube",
															m_asset_manager.m_texManager->Find("Tcontainer0_d"),
															m_asset_manager.m_texManager->Find("Tcontainer0_s"),
															&Texture::GetTexture1x1(BLUE),
															&Texture::GetTexture1x1(BLACK)));
	m_asset_manager.m_modelManager->Add("arrow",
		std::make_shared<MyModel>(m_asset_manager.m_modelManager->FindBrush("arrow"),
															"arrow",
															m_asset_manager.m_texManager->Find("Tcontainer0_d"),
															m_asset_manager.m_texManager->Find("Tcontainer0_s"),
															&Texture::GetTexture1x1(BLUE),
															&Texture::GetTexture1x1(BLACK)));
	m_asset_manager.m_modelManager->Add("grass_plane",
		std::make_shared<MyModel>(m_asset_manager.m_modelManager->FindBrush("plane"),
															"grass_plane",
															m_asset_manager.m_texManager->Find("Tgrass0_d"),
															m_asset_manager.m_texManager->Find("Tgrass0_d"),
															&Texture::GetTexture1x1(BLUE),
															&Texture::GetTexture1x1(BLACK)));
	m_asset_manager.m_modelManager->Add("white_cube",
		std::make_shared<MyModel>(m_asset_manager.m_modelManager->FindBrush("cube"),
															"white_cube",
															&Texture::GetTexture1x1(WHITE)));
	m_asset_manager.m_modelManager->Add("brick_square",
		std::make_shared<MyModel>(m_asset_manager.m_modelManager->FindBrush("square"),
															"brick_square",
															m_asset_manager.m_texManager->Find("Tbricks0_d"),
															m_asset_manager.m_texManager->Find("Tbricks0_d"),
															&Texture::GetTexture1x1(BLUE),
															&Texture::GetTexture1x1(BLACK)));
	m_asset_manager.m_modelManager->Add("brick_cube",
		std::make_shared<MyModel>(m_asset_manager.m_modelManager->FindBrush("cube"),
															"brick_cube",
															m_asset_manager.m_texManager->Find("Tbricks2_d"),
															m_asset_manager.m_texManager->Find("Tbricks2_d"),
															m_asset_manager.m_texManager->Find("Tbricks2_n"),
															m_asset_manager.m_texManager->Find("Tbricks2_dp")));
	m_asset_manager.m_modelManager->Add("pbr_cube",
		std::make_shared<MyModel>(m_asset_manager.m_modelManager->FindBrush("cube"),
														"pbr_cube",
														m_asset_manager.m_texManager->Find("pbr1_basecolor"),
														m_asset_manager.m_texManager->Find("pbr1_metallic"),
														m_asset_manager.m_texManager->Find("pbr1_normal"),
														m_asset_manager.m_texManager->Find("pbr1_roughness")));
	m_asset_manager.m_modelManager->Add("white_sphere",
		std::make_shared<MyModel>(m_asset_manager.m_modelManager->FindBrush("sphere"),
														"white_sphere",
														&Texture::GetTexture1x1(WHITE)));
	m_asset_manager.m_modelManager->Add("white_quad",
		std::make_shared<MyModel>(m_asset_manager.m_modelManager->FindBrush("quad"),
															"white_quad",
															&Texture::GetTexture1x1(WHITE)));
	m_asset_manager.m_modelManager->Add("white_ellipse",
		std::make_shared<MyModel>(m_asset_manager.m_modelManager->FindBrush("ellipse"),
															"white_ellipse",
															&Texture::GetTexture1x1(WHITE)));
	m_asset_manager.m_modelManager->Add("white_hex",
															std::make_shared<MyModel>(m_asset_manager.m_modelManager->FindBrush("hex"),
															"white_hex",
															&Texture::GetTexture1x1(WHITE)));
}

//----------------------------------------------------
void eMainContextBase::_AcceptDrop()
{
#ifndef STANDALONE
	if (!m_scene.m_drop_path.empty() && m_scene.m_focused)
	{
		Texture drop;
		drop.loadTextureFromFile(m_scene.m_drop_path, GL_RGBA, GL_REPEAT);
		if (drop.m_id == Texture::GetDefaultTextureId())
			return;

		if (m_scene.m_focused->GetModel()->Get3DMeshes()[0]->GetMaterial().has_value())
		{
			Material m = m_scene.m_focused->GetModel()->Get3DMeshes()[0]->GetMaterial().value();
			m.textures[Material::TextureType::ALBEDO] = drop.m_id;
			m.used_textures.insert(Material::TextureType::ALBEDO);
			const_cast<I3DMesh*>(m_scene.m_focused->GetModel()->Get3DMeshes()[0])->SetMaterial(m);
		}
		else if (m_scene.m_focused->GetModel()->GetMaterial().has_value())
		{
			
			Material m = m_scene.m_focused->GetModel()->GetMaterial().value();
			m.textures[Material::TextureType::ALBEDO] = drop.m_id;
			m.used_textures.insert(Material::TextureType::ALBEDO);
			m_scene.m_focused->GetModel()->SetMaterial(m);
		}
	}
	m_scene.m_drop_path = "";
#endif
}

//------------------------------------------------------------
Light& eMainContextBase::GetMainLight()
{
	if (m_scene.m_lights.empty())
		throw std::logic_error("main light was deleted!");

	return m_scene.m_lights[0];
}

//------------------------------------------------------------------
Camera& eMainContextBase::GetMainCamera()
{
	if (m_scene.m_cameras.empty())
		throw std::logic_error("main camera was deleted!");

	return m_scene.m_cameras[m_scene.m_cur_camera];
}

//----------------------------------------------------------------
Camera& eMainContextBase::GetCamera(size_t index)
{
	if (m_scene.m_cameras.size() == index)
		m_scene.m_cameras.push_back(m_scene.m_cameras[0]);
	else if(m_scene.m_cameras.size() < index)
		throw std::logic_error("rying to access non-existing camera!");

	return m_scene.m_cameras[index];
}

//--------------------------------------------------------------
void eMainContextBase::SetMainCameraIndex(size_t index)
{
	m_scene.m_cur_camera = index;
	m_pipeline.SetCurrentCamera(m_scene.m_cur_camera);
	MainCameraIndexChanged.Occur(m_scene.m_cur_camera);
}

//----------------------------------------------------------------
void eMainContextBase::AddGUI(const std::shared_ptr<GUI>& _gui)
{
	m_scene.m_guis.push_back(_gui);
}

//----------------------------------------------------------------
void eMainContextBase::DeleteGUI(const std::shared_ptr<GUI>& _gui)
{
	m_scene.m_guis.erase(std::remove(m_scene.m_guis.begin(), m_scene.m_guis.end(), (_gui)));
}

//----------------------------------------------------------------
void eMainContextBase::AddText(std::shared_ptr<Text> _text)
{
	m_scene.m_texts.push_back(_text);
}

//----------------------------------------------------------------
std::vector<std::shared_ptr<Text>>& eMainContextBase::GetTexts()
{
	return m_scene.m_texts;
}

//--------------------------------------------------------------
void eMainContextBase::AddDecal(const Decal& _decal)
{
	m_scene.m_decals.push_back(_decal);
}

//----------------------------------------------------------------
void eMainContextBase::EnableHovered(bool _hover) { m_scene.m_update_hovered = _hover; }

//-----------------------------------------------------------------------------
void eMainContextBase::EnableFrameChoice(bool _enable, bool _with_left)
{
	if (!_enable)
		m_framed_choice_enabled = FramedChoice::DISABLED;
	else if(_with_left)
		m_framed_choice_enabled = FramedChoice::WITH_LEFT;
	else
		m_framed_choice_enabled = FramedChoice::WITH_RIGHT;
}

//-----------------------------------------------------------------------------
ModelManagerYAML* eMainContextBase::GetModelManager() const
{
	return m_asset_manager.m_modelManager.get();
}

//-----------------------------------------------------------------------------
ScriptManager* eMainContextBase::GetScriptManager() const
{
	return m_asset_manager.m_scriptManager.get();
}

//*********************Initialize*********************************
//--------------------------------------------------------------------------
void eMainContextBase::InitializeGL()
{
	m_scene.InitalizeMainLightAndCamera(m_pipeline.Width(), m_pipeline.Height());

#ifndef STANDALONE
	for (auto& gui : m_externalGui)
		m_input_controller->AddObserver(gui, MONOPOLY);
#endif

	m_global_clock.start();
	InitializeTextures();
	base::Log("InitializeTextures(): " + std::to_string(m_global_clock.newFrame()));

	InitializeBuffers();
	base::Log("InitializeBuffers(): " + std::to_string(m_global_clock.newFrame()));

	InitializeSounds();
	base::Log("InitializeSounds(): " + std::to_string(m_global_clock.newFrame()));

	_PreInitModelManager();
	base::Log("_PreInitModelManager(): " + std::to_string(m_global_clock.newFrame()));

	InitializeRenders();
	base::Log("InitializeRenders(): " + std::to_string(m_global_clock.newFrame()));

	InitializePipline();
	base::Log("InitializePipline(): " + std::to_string(m_global_clock.newFrame()));

#ifndef STANDALONE
	InitializeExternalGui();
	base::Log("InitializeExternalGui(): " + std::to_string(m_global_clock.newFrame()));
#endif

	m_gameState = IGame::GameState::LOADING;
}

//-------------------------------------------------------------------------------
void eMainContextBase::InitializeScene() // Non-GL thread
{
	math::eClock model_loading_clock;
	model_loading_clock.start();

	m_asset_manager.m_scriptManager->loadScriptsFromFolder(m_asset_manager.m_scriptsFolderPath); // @todo maybe move somwhere

	InitializeModels();
	base::Log("InitializeModels(): " + std::to_string(model_loading_clock.newFrame()));

	InitializeScripts();
	m_gameState = IGame::GameState::MODEL_RELOAD;
}

//--------------------------------------------------------------------------------
void eMainContextBase::InitializePipline()
{
	m_pipeline.Initialize();
	SettingsLoadingService::LoadPipelineSettings("pipeline.ini", this, m_pipeline);
}

//--------------------------------------------------------------------------------
void eMainContextBase::InitializeBuffers()
{
	m_pipeline.InitializeBuffers(
		FboBits::Default |
		FboBits::Screen |
		FboBits::ScreenWithSSR |
		FboBits::MTS |
		FboBits::Reflection |
		FboBits::Refraction |
		FboBits::SSR |
		FboBits::SSRBlur |
		FboBits::ShadowDir |
		FboBits::Depth |
		FboBits::ShadowCube |
		FboBits::ShadowCSM |
		FboBits::Square |
		FboBits::BrightFilter |
		FboBits::Gaussian1 |
		FboBits::Gaussian2 |
		FboBits::Deferred |
		FboBits::SSAO |
		FboBits::SSAOBlur |
		FboBits::IBLCubemap |
		FboBits::IBLCubemapIrr |
		FboBits::EnvironmentCubemap |
		FboBits::Bloom |

		// Custom buffers
		FboBits::CameraInterpolationBuffer |
		FboBits::ComputeParticleBuffer |
		FboBits::UIlessBuffer,

		SsboBits::ModelToProjection |
		SsboBits::ModelToWorld |
		SsboBits::InstancedInfo |
		SsboBits::HeraldryInfo |
		SsboBits::BoneBaseIndexes |
		SsboBits::BonesPacked
	);
}

//--------------------------------------------------------------------------------
void eMainContextBase::InitializeModels()
{
	// Camera should be here in editor mode @todo ifdef EDITOR
	m_asset_manager.m_modelManager->Add("Camera", (GLchar*)std::string(m_asset_manager.m_modelFolderPath + "Camera_v2/Camera_v2.obj").c_str());
	SettingsLoadingService::LoadModels("models.ini", m_asset_manager.m_modelFolderPath, m_asset_manager.m_modelManager.get(), m_asset_manager.m_load_model_multithreading);
	SettingsLoadingService::LoadModelTextures("model_textures.ini", m_asset_manager.m_modelManager.get());

	Material material{ glm::vec3(0.8f, 0.0f, 0.0f), 0.5f , 0.5f }; // -> move to base
	material.textures[Material::TextureType::EMISSIVE] = Texture::GetTexture1x1(TColor::BLACK).m_id;
	material.emission_strength = 5.0f;
	m_asset_manager.m_modelManager->Add("sphere_red", Primitive::SPHERE, std::move(material));

	Material material_blue{ glm::vec3(0.0f, 0.0f, 0.8f), 0.5f , 0.5f }; // -> move to base
	material_blue.textures[Material::TextureType::EMISSIVE] = Texture::GetTexture1x1(TColor::BLACK).m_id;
	material_blue.emission_strength = 5.0f;
	m_asset_manager.m_modelManager->Add("sphere_blue", Primitive::SPHERE, std::move(material_blue));

	Material material_green{ glm::vec3(0.0f, 0.8f, 0.0f), 0.5f , 0.5f }; // -> move to base
	material_green.textures[Material::TextureType::EMISSIVE] = Texture::GetTexture1x1(TColor::BLACK).m_id;
	material_green.emission_strength = 5.0f;
	m_asset_manager.m_modelManager->Add("sphere_green", Primitive::SPHERE, std::move(material_green));
}

//--------------------------------------------------------------------------------
void eMainContextBase::InitializeRenders()
{
	m_pipeline.InitializeRenders(*m_asset_manager.m_modelManager.get(), *m_asset_manager.m_texManager.get(), m_asset_manager.m_shadersFolderPath);
	// set uniforms
	// exposure, shininess etc. @todo dont change every frame in render
}

//--------------------------------------------------------------------------------
void eMainContextBase::InitializeTextures()
{
	m_asset_manager.m_texManager->InitContext(m_asset_manager.m_assetsFolderPath);
	m_asset_manager.m_texManager->Initialize();
}

//--------------------------------------------------------------------------------
void eMainContextBase::InitializeScripts()
{
	//@todo why is it here ?
	std::shared_ptr<Text> t = std::make_shared<Text>();
	t->font = "ARIALN";
	t->pos_x = 25.0f;
	t->pos_y = 25.0f;
	t->scale = 1.0f;
	t->color = glm::vec3(0.8, 0.8f, 0.0f);
	t->mvp = glm::ortho(0.0f, (float)m_pipeline.Width(), 0.0f, (float)m_pipeline.Height());
	m_scene.m_texts.push_back(t);

	for (auto& script : m_global_scripts)
	{
		script->Initialize();
	}
	for (auto& object : m_scene.m_objects)
	{
		if (object->GetScript())
			object->GetScript()->Initialize();
	}
}

//--------------------------------------------------------------------------------
void eMainContextBase::InitializeExternalGui()
{
#ifndef STANDALONE

	//Global script
	auto visual = std::make_shared<EngineBaseVisualsScript>(this);
	m_pipeline.IsMeasurementGridEnabled() = &visual->GetVisibility();

	m_global_scripts.push_back(visual);
	std::shared_ptr<LightsController> lights_controller = std::make_shared<LightsController>
		(this, m_asset_manager.m_modelManager.get(), m_asset_manager.m_texManager.get(), m_asset_manager.m_soundManager.get(), m_pipeline, GetMainCamera());
	ObjectBeingAddedToScene.Subscribe([lights_controller](shObject obj) { lights_controller->OnObjectAddedToScene(obj); });
	m_global_scripts.push_back(lights_controller);

	// Lights & Cameras
	static std::function<void(int, int*&)> change_ibl_callback = [this](int _ibl, int*& _val)
	{
		if (m_asset_manager.m_texManager->GetIBLIds().size() > _ibl)
		{
			auto [irr, prefilter] = m_asset_manager.m_texManager->GetIBLIds()[_ibl];
			m_pipeline.SetSkyIBL(irr, prefilter);
		}
	};
	m_externalGui[ExternalWindow::LIGHT_CAMERA_WND]->Add(CHECKBOX, "Use IBL", &m_pipeline.IBL());
	m_externalGui[ExternalWindow::LIGHT_CAMERA_WND]->Add(SLIDER_FLOAT_NERROW, "IBL Influance", &m_pipeline.IBLInfluance());
	m_externalGui[ExternalWindow::LIGHT_CAMERA_WND]->Add(SPIN_BOX, "IBL Map", (void*)&change_ibl_callback);

	m_externalGui[ExternalWindow::LIGHT_CAMERA_WND]->Add(TEXT, "Light", nullptr);
	std::function<void()> add_light_callback = [this]()
	{
		m_scene.m_lights.push_back({});
		m_scene.m_lights.back().light_position = glm::vec4(0.5f, 2.0f, -4.0f, 1.0f);
		m_scene.m_lights.back().light_direction = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		m_scene.m_lights.back().intensity = glm::vec4{ 10.0f, 10.0f ,10.0f, 1.0f };
		m_scene.m_lights.back().type = eLightType::DIRECTION;
		m_scene.m_lights.back().constant = 0.9f;
		m_scene.m_lights.back().linear = 0.5f;
		m_scene.m_lights.back().quadratic = 0.03f;

		std::array<glm::vec4, 4> points = { // for area light
		glm::vec4(-0.33f, -0.33f, 0.0f, 1.0f),
		glm::vec4(-0.33f, 0.33f, 0.0f, 1.0f),
		glm::vec4(0.33f, 0.33f, 0.0f, 1.0f),
		glm::vec4(0.33f, -0.33f, 0.0f, 1.0f) };

		m_scene.m_lights.back().points = points;
	};
	m_externalGui[ExternalWindow::LIGHT_CAMERA_WND]->Add(BUTTON, "Add Light", &add_light_callback);
	m_externalGui[ExternalWindow::LIGHT_CAMERA_WND]->Add(GAME, "Game", (void*)&(*this));

	static std::function<void(int, int*&)> light_index_callback = [this, lights_controller](int _index, int*& _val)
	{

		static bool first_call = true;
		if (first_call)
		{
			*_val = 0;
			first_call = false;
			return;
		}
		lights_controller->OnCurrentLightChanged(_index);
	};
	m_externalGui[ExternalWindow::LIGHT_CAMERA_WND]->Add(SPIN_BOX, "Current light Index", (void*)&light_index_callback);

	std::function<size_t(size_t)> light_type_callback = [this, lights_controller](size_t _index)
	{
		size_t prev = (size_t)lights_controller->GetCurrentLight().type;
		if (_index == 0)
			lights_controller->GetCurrentLight().type = eLightType::POINT;
		else if (_index == 1)
			lights_controller->GetCurrentLight().type = eLightType::DIRECTION;
		else if (_index == 2)
			lights_controller->GetCurrentLight().type = eLightType::SPOT;
		else if (_index == 3)
			lights_controller->GetCurrentLight().type = eLightType::CSM;
		return prev;
	};
	static eVectorStringsCallback light_types{ {"point", "directional", "cut-off", "csm"}, light_type_callback };

	m_externalGui[ExternalWindow::LIGHT_CAMERA_WND]->Add(COMBO_BOX, "Light type.", &light_types);
	m_externalGui[ExternalWindow::LIGHT_CAMERA_WND]->Add(LIGHT_TYPE_VISUAL, "Light object.", m_asset_manager.m_modelManager.get());
	m_externalGui[ExternalWindow::LIGHT_CAMERA_WND]->Add(CHECKBOX, "Light active", &lights_controller->GetCurrentLight().active);
	m_externalGui[ExternalWindow::LIGHT_CAMERA_WND]->Add(SLIDER_FLOAT_3, "Light position.", &lights_controller->GetCurrentLight().light_position);
	m_externalGui[ExternalWindow::LIGHT_CAMERA_WND]->Add(SLIDER_FLOAT_3, "Light direction.", &lights_controller->GetCurrentLight().light_direction);
	m_externalGui[ExternalWindow::LIGHT_CAMERA_WND]->Add(SLIDER_FLOAT_3_LARGE, "Light intensity.", &lights_controller->GetCurrentLight().intensity);
	m_externalGui[ExternalWindow::LIGHT_CAMERA_WND]->Add(SLIDER_FLOAT_3, "Light ambient.", &lights_controller->GetCurrentLight().ambient);
	m_externalGui[ExternalWindow::LIGHT_CAMERA_WND]->Add(SLIDER_FLOAT_3, "Light diffuse.", &lights_controller->GetCurrentLight().diffuse);
	m_externalGui[ExternalWindow::LIGHT_CAMERA_WND]->Add(SLIDER_FLOAT_3, "Light specular.", &lights_controller->GetCurrentLight().specular);

	m_externalGui[ExternalWindow::LIGHT_CAMERA_WND]->Add(TEXT, "Light constant", nullptr);
	m_externalGui[ExternalWindow::LIGHT_CAMERA_WND]->Add(SLIDER_FLOAT_NERROW, "Constant", &lights_controller->GetCurrentLight().constant);
	m_externalGui[ExternalWindow::LIGHT_CAMERA_WND]->Add(TEXT, "Light linear", nullptr);
	m_externalGui[ExternalWindow::LIGHT_CAMERA_WND]->Add(SLIDER_FLOAT_NERROW, "Linear", &lights_controller->GetCurrentLight().linear);
	m_externalGui[ExternalWindow::LIGHT_CAMERA_WND]->Add(TEXT, "Light quadratic", nullptr);
	m_externalGui[ExternalWindow::LIGHT_CAMERA_WND]->Add(SLIDER_FLOAT_NERROW, "Quadratic", &lights_controller->GetCurrentLight().quadratic);
	m_externalGui[ExternalWindow::LIGHT_CAMERA_WND]->Add(TEXT, "Light cut off", nullptr);
	m_externalGui[ExternalWindow::LIGHT_CAMERA_WND]->Add(SLIDER_FLOAT_NERROW, "Cut off", &lights_controller->GetCurrentLight().cutOff);
	m_externalGui[ExternalWindow::LIGHT_CAMERA_WND]->Add(TEXT, "Light outer cut off", nullptr);
	m_externalGui[ExternalWindow::LIGHT_CAMERA_WND]->Add(SLIDER_FLOAT_NERROW, "Outer cut off", &lights_controller->GetCurrentLight().outerCutOff);
	m_externalGui[ExternalWindow::LIGHT_CAMERA_WND]->Add(TEXT, "Light Radius", nullptr);
	m_externalGui[ExternalWindow::LIGHT_CAMERA_WND]->Add(SLIDER_FLOAT, "Radius ", &lights_controller->GetCurrentLight().radius);

	//Camera
	m_externalGui[ExternalWindow::LIGHT_CAMERA_WND]->Add(TEXT, "Camera", nullptr);
	m_externalGui[ExternalWindow::LIGHT_CAMERA_WND]->Add(SLIDER_FLOAT_3, "position", &GetMainCamera().PositionRef());
	m_externalGui[ExternalWindow::LIGHT_CAMERA_WND]->Add(SLIDER_FLOAT_3, "direction", &GetMainCamera().ViewDirectionRef());

	std::function<void()> add_camera_callback = [this]()
	{
		m_scene.m_cameras.emplace_back(m_pipeline.Width(), m_pipeline.Height(), 0.1f, 10.0f);
		m_scene.m_cameras.back().SetVisualiseFrustum(true);
		m_externalGui[LIGHT_CAMERA_WND]->Add(CAMERA, "Camera second", &m_scene.m_cameras.back());
		std::function<void()> switch_camera_callback = [this]()
		{
			m_scene.m_cameras[m_scene.m_cur_camera].SetVisualiseFrustum(true);
			if (m_scene.m_cur_camera == 0)
				m_scene.m_cur_camera = 1;
			else
				m_scene.m_cur_camera = 0;

			m_scene.m_cameras[m_scene.m_cur_camera].SetVisualiseFrustum(false);
			m_pipeline.SetCurrentCamera(m_scene.m_cur_camera);
		};
		m_externalGui[ExternalWindow::LIGHT_CAMERA_WND]->Add(BUTTON, "Switch Camera", &switch_camera_callback);

		Material material{ glm::vec3(0.8f, 0.0f, 0.0f), 0.5 , 0.5 };
		material.ao = 1.0f;
		material.textures[Material::TextureType::EMISSIVE] = Texture::GetTexture1x1(TColor::BLACK).m_id;
		material.used_textures.insert(Material::TextureType::ALBEDO);
		material.used_textures.insert(Material::TextureType::METALLIC);
		material.used_textures.insert(Material::TextureType::ROUGHNESS);
		material.used_textures.insert(Material::TextureType::NORMAL);

		ObjectFactoryBase factory(m_asset_manager.m_animationManager.get());
		m_scene.m_camera_obj = factory.CreateObject(m_asset_manager.m_modelManager->Find("Camera"), eObject::RenderType::PBR, "Camera1");
		m_scene.m_camera_obj->SetScript(new CameraSecondScript(&m_scene.m_cameras.back(), this));
		m_scene.m_camera_obj->GetTransform()->setScale(glm::vec3{ 0.01f, 0.01f, 0.01f });
		for (auto& mesh : m_scene.m_camera_obj->GetModel()->Get3DMeshes())
			const_cast<I3DMesh*>(mesh)->SetMaterial(material);
		AddObject(m_scene.m_camera_obj);
	};

	m_externalGui[ExternalWindow::LIGHT_CAMERA_WND]->Add(BUTTON, "Add camera", &add_camera_callback);

	//Pipeline
	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(TEXT_INT32, "Draw calls ", &m_pipeline.GetDrawCalls());
	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(TEXT_INT, "Draw triangles ", &m_pipeline.GetDrawTriangles());
	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(CHECKBOX, "Show bounding boxes", &m_pipeline.GetBoundingBoxBoolRef());
	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(CHECKBOX, "Use Multi sampling", &m_pipeline.GetMultiSamplingBoolRef());
	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(CHECKBOX, "Use forward plus pipeline", &m_pipeline.ForwardPlusPipeline());
	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(CHECKBOX, "Use Z-pre pass", &m_pipeline.ZPrePassPipeline());
	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(CHECKBOX, "Frustun cull", &m_pipeline.IsFrustumCull());
	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(CHECKBOX, "Show measurement grid", &visual->GetVisibility());

	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(CHECKBOX, "Sky box on", &m_pipeline.GetSkyBoxOnRef());
	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(CHECKBOX, "Rotate Sky box", &m_pipeline.GetRotateSkyBoxRef());
	static std::function<void(int, int*&)> change_skybox_callback = [this](int _skybox, int*& _val)
	{
		static bool first_call = true;
		if (first_call)
		{
			const Texture* skybox = m_asset_manager.m_texManager->FindByID(m_asset_manager.m_texManager->GetCubeMapIds()[1]);
			if (skybox)
				m_pipeline.SetSkyBoxTexture(skybox);
			*_val = 1;
			first_call = false;
			return;
		}

		if (m_asset_manager.m_texManager->GetCubeMapIds().size() > _skybox)
		{
			const Texture* skybox = m_asset_manager.m_texManager->FindByID(m_asset_manager.m_texManager->GetCubeMapIds()[_skybox]);
			m_pipeline.SetSkyBoxTexture(skybox);
		}
	};
	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(SPIN_BOX, "Sky box", (void*)&change_skybox_callback);

	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(CHECKBOX, "Water", &m_pipeline.GetWaterOnRef());
	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(CHECKBOX, "Hex", &m_pipeline.GetGeometryOnRef());
	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(CHECKBOX, "Mesh line draw", &m_pipeline.GetMeshLineOn());
	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(CHECKBOX, "Kernel", &m_pipeline.GetKernelOnRef());
	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(CHECKBOX, "Sky noise", &m_pipeline.GetSkyNoiseOnRef());
	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(CHECKBOX, "Use gizmo", &m_use_guizmo);
	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(CHECKBOX, "Outline focused", &m_pipeline.GetOutlineFocusedRef());
	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(CHECKBOX, "Environment capture", &m_pipeline.EnvironmentMap());

	std::function<size_t(size_t)> frame_choice_callback = [this](size_t _index)
	{
		if (_index == 0)
			m_framed_choice_enabled = FramedChoice::DISABLED;
		else if (_index == 1)
			m_framed_choice_enabled = FramedChoice::WITH_LEFT;
		else if (_index == 2)
			m_framed_choice_enabled = FramedChoice::WITH_RIGHT;
		return 0;
	};
	static eVectorStringsCallback frame_types{ {"None", "Left", "Right" }, frame_choice_callback };
	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(RADIO_BUTTON, "Frame choice", &frame_types);

	//
	m_externalGui[ExternalWindow::HDR_BLOOM_WND]->Add(CHECKBOX, "Gamma Correction", &m_pipeline.GetGammaCorrectionRef());
	std::function<size_t(size_t)> tonemapping_choice_callback = [this](size_t _index)
	{
		m_pipeline.GetToneMappingRef() = _index;
		return 0;
	};
	static eVectorStringsCallback tonemapping_types{ {"None", "Default", "Reinhard", "ACES", "ACES-base"}, tonemapping_choice_callback };
	m_externalGui[ExternalWindow::HDR_BLOOM_WND]->Add(RADIO_BUTTON, "Gamma Tone Mapping", &tonemapping_types);
	m_externalGui[ExternalWindow::HDR_BLOOM_WND]->Add(SLIDER_FLOAT, "Gamma Exposure", &m_pipeline.GetExposureRef());
	m_externalGui[ExternalWindow::HDR_BLOOM_WND]->Add(CHECKBOX, "Auto Exposure", &m_pipeline.GetAutoExposure());
	static std::function<void(int, int*&)> target_lum_callback = [this](int _blur_coef, int*& _val)
	{
		static bool first_call = true;
		if (first_call)
		{
			*_val = m_pipeline.GetTargetLuminance() * 100.f;
			first_call = false;
			return;
		}
		m_pipeline.GetTargetLuminance() = _blur_coef / 100.f;
	};
	m_externalGui[ExternalWindow::HDR_BLOOM_WND]->Add(SPIN_BOX, "Target Luminance", (void*)&target_lum_callback);
	m_externalGui[ExternalWindow::HDR_BLOOM_WND]->Add(SLIDER_FLOAT, "Adaption rate", &m_pipeline.GetAdaptionRate());

	static std::function<void(int, int*&)> blur_coef_callback = [this](int _blur_coef, int*& _val)
	{
		static bool first_call = true;
		if (first_call)
		{
			*_val = m_pipeline.GetBlurCoefRef() * 100.f;
			first_call = false;
			return;
		}
		m_pipeline.GetBlurCoefRef() = _blur_coef / 100.f;
	};
	m_externalGui[ExternalWindow::HDR_BLOOM_WND]->Add(SPIN_BOX, "Blur coefficients", (void*)&blur_coef_callback);
	static std::function<void(int, int*&)> brightness_threshold_callback = [this](int _threshold, int*& _val)
	{
		static bool first_call = true;
		if (first_call)
		{
			*_val = m_pipeline.BrightnessAmplifier() * 100.f;
			first_call = false;
			return;
		}
		m_pipeline.BrightnessAmplifier() = _threshold / 100.f;
	};
	m_externalGui[ExternalWindow::HDR_BLOOM_WND]->Add(SPIN_BOX, "Brightness Threshold", (void*)&brightness_threshold_callback);

	m_externalGui[ExternalWindow::HDR_BLOOM_WND]->Add(SLIDER_FLOAT, "Emission Phong", &m_pipeline.GetEmissionStrengthRef());

	static std::function<void(int, int*&)> kernel_size_callback = [this](int _kernel_size, int*& _val)
	{
		static bool first_call = true;
		if (first_call)
		{
			*_val = m_pipeline.KernelSize();
			first_call = false;
			return;
		}
		m_pipeline.KernelSize() = _kernel_size;
	};
	m_externalGui[ExternalWindow::HDR_BLOOM_WND]->Add(SPIN_BOX, "Kernel Size", (void*)&kernel_size_callback);

	m_externalGui[ExternalWindow::HDR_BLOOM_WND]->Add(SLIDER_FLOAT, "Sample Size", &m_pipeline.SampleSize());
	//SSAO
	m_externalGui[ExternalWindow::DEFERRED_WND]->Add(CHECKBOX, "SSAO", &m_pipeline.GetSSAOEnabledRef());
	m_externalGui[ExternalWindow::DEFERRED_WND]->Add(SLIDER_FLOAT, "SSAO Threshold", &m_pipeline.GetSaoThresholdRef());
	m_externalGui[ExternalWindow::DEFERRED_WND]->Add(SLIDER_FLOAT, "SSAO Strength", &m_pipeline.GetSaoStrengthRef());
	//Fog
	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(CHECKBOX, "Fog", &m_pipeline.GetFogInfo().fog_on);
	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(SLIDER_FLOAT_3, "Fog color.", &m_pipeline.GetFogInfo().color);
	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(SLIDER_FLOAT_NERROW, "Fog density", &m_pipeline.GetFogInfo().density);
	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(SLIDER_FLOAT, "Fog gradient", &m_pipeline.GetFogInfo().gradient);
	//SSR
	m_externalGui[ExternalWindow::DEFERRED_WND]->Add(CHECKBOX, "SSR", &m_pipeline.GetSSREnabledRef());
	m_externalGui[ExternalWindow::DEFERRED_WND]->Add(SLIDER_FLOAT, "Step", &m_pipeline.Step());
	m_externalGui[ExternalWindow::DEFERRED_WND]->Add(SLIDER_FLOAT, "MinRayStep", &m_pipeline.MinRayStep());
	m_externalGui[ExternalWindow::DEFERRED_WND]->Add(SLIDER_FLOAT, "Metallic", &m_pipeline.Metallic());
	m_externalGui[ExternalWindow::DEFERRED_WND]->Add(SLIDER_INT_NERROW, "NumBinarySearchSteps", &m_pipeline.NumBinarySearchSteps());
	m_externalGui[ExternalWindow::DEFERRED_WND]->Add(SLIDER_FLOAT, "ReflectionSpecularFalloffExponent", &m_pipeline.ReflectionSpecularFalloffExponent());
	m_externalGui[ExternalWindow::DEFERRED_WND]->Add(SLIDER_FLOAT_LARGE, "K", &m_pipeline.K());
	//Camera Interpolation
	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(CHECKBOX, "CameraInterpolation", &m_pipeline.GetEnabledCameraInterpolationRef());
	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(TEXTURE, "Compute Shader buffer", (void*)m_pipeline.GetComputeParticleSystem().m_id);

	if (auto* image = m_asset_manager.m_texManager->Find("computeImageRW"); image != nullptr)
	{
		m_externalGui[ExternalWindow::PIPELINE_WND]->Add(CHECKBOX, "Compute shader", &m_pipeline.GetComputeShaderRef());
		uint32_t id = image->m_id;
		m_externalGui[ExternalWindow::PIPELINE_WND]->Add(TEXTURE, "Compute image", (void*)(id));
	}

	m_externalGui[ExternalWindow::GAME_DEBUG_WND]->Add(SLIDER_FLOAT_3, "Second Camera Position", &m_pipeline.GetSecondCameraPositionRef());
	m_externalGui[ExternalWindow::GAME_DEBUG_WND]->Add(SLIDER_FLOAT, "Displacement", &m_pipeline.GetDisplacementRef());

	if (auto* image = m_asset_manager.m_texManager->Find("computeImageRWCameraInterpolation"); image != nullptr)
	{
		uint32_t id = image->m_id;
		m_externalGui[ExternalWindow::GAME_DEBUG_WND]->Add(TEXTURE, "Camera Interpolation image", (void*)(id));

		m_externalGui[ExternalWindow::GAME_DEBUG_WND]->Add(MATRIX, "Look At Matrix", (void*)&m_pipeline.GetLookAtMatrix());
		m_externalGui[ExternalWindow::GAME_DEBUG_WND]->Add(MATRIX, "Projection Matrix", (void*)&m_pipeline.GetProjectionMatrix());
		m_externalGui[ExternalWindow::GAME_DEBUG_WND]->Add(MATRIX, "Look At Projected Matrix", (void*)&m_pipeline.GetLookAtProjectedMatrix());
	}

	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(CHECKBOX, "Shadows", &m_pipeline.ShadowingRef());

	std::function<void()> emit_partilces_callback = [this]()
	{
		auto sys = std::make_shared<ParticleSystem>(50, 0, 0, 10'000, glm::vec3(0.0f, 3.0f, -2.5f),
			m_asset_manager.m_texManager->Find("Tatlas2"),
			m_asset_manager.m_soundManager->GetSound("shot_sound"),
			m_asset_manager.m_texManager->Find("Tatlas2")->m_num_rows);
		sys->Start();
		m_pipeline.AddParticleSystem(sys);
	};
	std::function<void()> emit_partilces_gpu_callback = [this]()
	{
		m_pipeline.AddParticleSystemGPU(glm::vec3(0.5f, 3.0f, -2.5f), m_asset_manager.m_texManager->Find("Tatlas2"));
	};
	std::function<void()> update_uniforms_callback = [this]()
	{
		m_pipeline.UpdateShadersInfo();
	};

	std::function<void()> split_current_model_callback = [this]()
	{
		auto new_models = m_asset_manager.m_modelManager->Split(m_scene.m_focused->GetModel()->GetName());
		for (auto model : new_models)
		{
			ObjectFactoryBase factory;
			shObject obj = factory.CreateObject(model, m_scene.m_focused->GetRenderType(), model->GetName());
			obj->GetTransform()->setTranslation(m_scene.m_focused->GetTransform()->getTranslation());
			obj->GetTransform()->setScale(m_scene.m_focused->GetTransform()->getScaleAsVector());
			obj->GetTransform()->setRotation(m_scene.m_focused->GetTransform()->getRotation());
			AddObject(obj);
		}
		shObject focused = m_scene.m_focused;
		SetFocused(nullptr);
		DeleteObject(focused);
	};

	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(BUTTON, "Emit particle system", (void*)&emit_partilces_callback);
	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(BUTTON, "Emit particle system gpu", (void*)&emit_partilces_gpu_callback);
	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(BUTTON, "Split current model", (void*)&split_current_model_callback);
	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(CHECKBOX, "Debug white", &m_pipeline.GetDebugWhite());
	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(CHECKBOX, "Debug Tex Coords", &m_pipeline.GetDebugTexCoords());

	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(SLIDER_FLOAT, "PBR debug dist", (void*)&m_pipeline.debug_float[0]);
	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(SLIDER_FLOAT, "PBR debug intensity", (void*)&m_pipeline.debug_float[1]);
	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(SLIDER_FLOAT, "PBR debug shininess", (void*)&m_pipeline.debug_float[2]);
	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(SLIDER_FLOAT, "PBR debug ao", (void*)&m_pipeline.debug_float[3]);

	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(TEXTURE, "Reflection buffer", (void*)m_pipeline.GetReflectionBufferTexture().m_id);
	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(TEXTURE, "Refraction buffer", (void*)m_pipeline.GetRefractionBufferTexture().m_id);
	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(TEXTURE, "LUT", (void*)m_pipeline.GetLUT().m_id);

	m_externalGui[ExternalWindow::HDR_BLOOM_WND]->Add(TEXTURE, "Gaussian buffer", (void*)m_pipeline.GetGausian2BufferTexture().m_id);
	m_externalGui[ExternalWindow::HDR_BLOOM_WND]->Add(TEXTURE, "Luminance", (void*)m_pipeline.GetLuminanceTexture().m_id);
	m_externalGui[ExternalWindow::HDR_BLOOM_WND]->Add(TEXTURE, "Bright filter buffer", (void*)m_pipeline.GetBrightFilter().m_id);

	m_externalGui[ExternalWindow::DEFERRED_WND]->Add(TEXTURE, "SSAO buffer", (void*)m_pipeline.GetSSAO().m_id);
	m_externalGui[ExternalWindow::DEFERRED_WND]->Add(TEXTURE, "SSR buffer", (void*)m_pipeline.GetSSRTexture().m_id);
	m_externalGui[ExternalWindow::DEFERRED_WND]->Add(TEXTURE, "SSR buffer blur", (void*)m_pipeline.GetSSRWithScreenTexture().m_id);
	m_externalGui[ExternalWindow::DEFERRED_WND]->Add(TEXTURE, "SSR Mask", (void*)m_pipeline.GetSSRTextureScreenMask().m_id);
	m_externalGui[ExternalWindow::DEFERRED_WND]->Add(TEXTURE, "Deffered Pos", (void*)m_pipeline.GetDefferedOne().m_id);
	m_externalGui[ExternalWindow::DEFERRED_WND]->Add(TEXTURE, "Deffered Norm", (void*)m_pipeline.GetDefferedTwo().m_id);

	if (GetMainLight().type == eLightType::DIRECTION)
		m_externalGui[ExternalWindow::SHADOWS_WND]->Add(TEXTURE, "Shadow buffer directional", (void*)m_pipeline.GetShadowBufferTexture().m_id);
	else
		m_externalGui[ExternalWindow::SHADOWS_WND]->Add(TEXTURE, "Shadow buffer point", (void*)m_pipeline.GetShadowBufferTexture().m_id);

	m_externalGui[ExternalWindow::SHADOWS_WND]->Add(TEXTURE, "Depth buffer", (void*)m_pipeline.GetDepthBufferTexture().m_id);

	/*if (m_debug_csm)
	{*/
	//pipeline.DumpCSMTextures();
	/*m_externalGui[ExternalWindow::SHADOWS_WND]->Add(TEXTURE, "CSM 1", (void*)pipeline.GetCSMMapLayer1().m_id);
	m_externalGui[ExternalWindow::SHADOWS_WND]->Add(TEXTURE, "CSM 2", (void*)pipeline.GetCSMMapLayer2().m_id);
	m_externalGui[ExternalWindow::SHADOWS_WND]->Add(TEXTURE, "CSM 3", (void*)pipeline.GetCSMMapLayer3().m_id);
	m_externalGui[ExternalWindow::SHADOWS_WND]->Add(TEXTURE, "CSM 4", (void*)pipeline.GetCSMMapLayer4().m_id);
	m_externalGui[ExternalWindow::SHADOWS_WND]->Add(TEXTURE, "CSM 5", (void*)pipeline.GetCSMMapLayer5().m_id);*/
	/*}*/

	m_externalGui[ExternalWindow::SHADOWS_WND]->Add(CHECKBOX, "PCSS enabled", &m_pipeline.GetPCSSEnabledRef());
	static std::function<void(int, int*&)> change_pcf_callback = [this](int _pcf, int*& _val)
	{
		static bool first_call = true;
		if (first_call)
		{
			*_val = m_pipeline.GetPcfSamples();
			first_call = false;
			return;
		}
		m_pipeline.GetPcfSamples() = _pcf;
	};
	m_externalGui[ExternalWindow::SHADOWS_WND]->Add(SPIN_BOX, "PCF samples", (void*)&change_pcf_callback);
	m_externalGui[ExternalWindow::SHADOWS_WND]->Add(SLIDER_FLOAT, "Z mult", &m_pipeline.ZMult());
	m_externalGui[ExternalWindow::SHADOWS_WND]->Add(SLIDER_FLOAT, "First Cascade Plane Distance", &m_pipeline.GetFirstCascadePlaneDistance());
	m_externalGui[ExternalWindow::SHADOWS_WND]->Add(SLIDER_FLOAT, "Light Placement Coef", &m_pipeline.GetLightPlacementCoef());
	m_externalGui[ExternalWindow::SHADOWS_WND]->Add(SLIDER_FLOAT, "Max Penumbra", &m_pipeline.GetPenumbraRef());
	m_externalGui[ExternalWindow::SHADOWS_WND]->Add(SLIDER_FLOAT, "Light Radius", &m_pipeline.GetLightRadiusRef());
	m_externalGui[ExternalWindow::SHADOWS_WND]->Add(SLIDER_FLOAT, "Light Size", &m_pipeline.GetLightSizeRef());
	m_externalGui[ExternalWindow::SHADOWS_WND]->Add(SLIDER_FLOAT, "PCF Sample Radius", &m_pipeline.PcfTextureSampleRadiusRef());
	m_externalGui[ExternalWindow::SHADOWS_WND]->Add(CHECKBOX, "PCF Texture Enabled", &m_pipeline.PcfTextureSampleEnabledRef());
	m_externalGui[ExternalWindow::SHADOWS_WND]->Add(SLIDER_FLOAT_NERROW, "Cascade blend dist", &m_pipeline.GetCascadeBlendDistance());
	m_externalGui[ExternalWindow::SHADOWS_WND]->Add(CHECKBOX, "Blend cascades", &m_pipeline.BlendCascades());
	m_externalGui[ExternalWindow::SHADOWS_WND]->Add(SLIDER_FLOAT_NERROW, "Max Shadow", &m_pipeline.GetMaxShadow());

	static std::function<void(int, int*&)> slope_bias_callback = [this](int _input, int*& _out)
	{
		static bool first_call = true;
		if (first_call)
		{
			*_out = m_pipeline.GetCsmBaseSlopeBias() * 1000.0f;
			first_call = false;
			return;
		}
		m_pipeline.GetCsmBaseSlopeBias() = (float)_input / 1000.0f;
	};
	m_externalGui[ExternalWindow::SHADOWS_WND]->Add(SPIN_BOX, "Slope bias", (void*)&slope_bias_callback);

	static std::function<void(int, int*&)> cascade_bias_callback = [this](int _input, int*& _out)
	{
		static bool first_call = true;
		if (first_call)
		{
			*_out = m_pipeline.GetCsmBaseCascadePlaneBias() * 100.0f;
			first_call = false;
			return;
		}
		m_pipeline.GetCsmBaseCascadePlaneBias() = (float)_input / 100.0f;
	};
	m_externalGui[ExternalWindow::SHADOWS_WND]->Add(SPIN_BOX, "Cascade bias", (void*)&cascade_bias_callback);

	m_externalGui[ExternalWindow::SHADOWS_WND]->Add(SLIDER_FLOAT, "Polygon Offset", &m_pipeline.GetCsmPolygonOffset());
	m_externalGui[ExternalWindow::SHADOWS_WND]->Add(CHECKBOX, "Face cull", &m_pipeline.GetCSMCullingEnabled());
	m_externalGui[ExternalWindow::SHADOWS_WND]->Add(CHECKBOX, "Front face", &m_pipeline.GetCSMCullingFront());

	m_externalGui[ExternalWindow::HDR_BLOOM_WND]->Add(CHECKBOX, "Physicly Based Bloom", &m_pipeline.PBBloomRef());
	m_externalGui[ExternalWindow::HDR_BLOOM_WND]->Add(CHECKBOX, "Bloom Threshold", &m_pipeline.GetBloomThreshold());
	m_externalGui[ExternalWindow::HDR_BLOOM_WND]->Add(TEXTURE, "Bloom", (void*)m_pipeline.GetBloomTexture().m_id);

	//Objects transform
	m_externalGui[ExternalWindow::TRANSFORM_WND]->Add(OBJECT_REF_TRANSFORM, "Transform", (void*)&m_scene.m_focused);

	//Shaders
	m_externalGui[ExternalWindow::SHADER_WND]->Add(BUTTON, "Update shaders", (void*)&update_uniforms_callback);
	m_externalGui[ExternalWindow::SHADER_WND]->Add(SHADER, "Shaders", (void*)&m_pipeline.GetShaderInfos());

	//Main Menu
	static std::function<void(const std::string&)> add_model_callback = [this](const std::string& _path)
	{
		m_asset_manager.m_modelManager->Add(dbb::ExtractFilename(_path), (GLchar*)_path.c_str());
	};
	m_externalGui[ExternalWindow::MAIN_MENU_WND]->Add(MENU_OPEN, "Add model", reinterpret_cast<void*>(&add_model_callback));

	static std::function<void(const std::string&)> serealize_scene_callback = [this](const std::string& _path)
	{
		SceneSerializerYAML serealizer(GetObjects(), *m_asset_manager.m_modelManager.get(), *m_asset_manager.m_animationManager.get());
		serealizer.Serialize(_path);
	};
	m_externalGui[ExternalWindow::MAIN_MENU_WND]->Add(MENU_SAVE_SCENE, "Serealize scene", reinterpret_cast<void*>(&serealize_scene_callback));

	static std::function<void(const std::string&)> deserealize_scene_callback = [this](const std::string& _path)
	{
		m_scene.m_objects.clear();
		SceneSerializerYAML serealizer(GetObjects(), *m_asset_manager.m_modelManager.get(), *m_asset_manager.m_animationManager.get());
		m_scene.m_objects = serealizer.Deserialize(_path);
	};
	m_externalGui[ExternalWindow::MAIN_MENU_WND]->Add(MENU_OPEN_SCENE, "Deserealize scene", reinterpret_cast<void*>(&deserealize_scene_callback));

	// Tools Menu
	static std::function<void()> terrain_tool_callback = [this]()
	{
		m_global_scripts.push_back(std::make_shared<TerrainGeneratorTool>(this, m_asset_manager.m_modelManager.get(), m_asset_manager.m_texManager.get(), m_pipeline, m_externalGui[ExternalWindow::TERRAIN_GENERATOR_WND]));
		m_global_scripts.back()->Initialize();
	};
	m_externalGui[ExternalWindow::MAIN_MENU_WND]->Add(MENU, "Terrain Tool", reinterpret_cast<void*>(&terrain_tool_callback));

	// Tools Menu
	static std::function<void()> clouds_tool_callback = [this]()
	{
		m_pipeline.GetComputeShaderRef() = true;
		m_global_scripts.push_back(std::make_shared<VolumetricCloudsTool>(this, m_asset_manager.m_modelManager.get(), m_asset_manager.m_texManager.get(), m_pipeline, m_externalGui[ExternalWindow::CLOUDS_WND]));
		m_global_scripts.back()->Initialize();
	};
	m_externalGui[ExternalWindow::MAIN_MENU_WND]->Add(MENU, "Volumetric Clouds Tool", reinterpret_cast<void*>(&clouds_tool_callback));

	// Tools Menu
	static std::function<void()> calculus_tool_callback = [this]()
	{
		m_global_scripts.push_back(std::make_shared<CalculusTool>(this, m_asset_manager.m_modelManager.get(), m_asset_manager.m_texManager.get(), m_pipeline, m_externalGui[ExternalWindow::CALCULUS_WND]));
		m_global_scripts.back()->Initialize();
	};
	m_externalGui[ExternalWindow::MAIN_MENU_WND]->Add(MENU, "Calculus Tool", reinterpret_cast<void*>(&calculus_tool_callback));

	//Create CREATE_WND
	std::function<void()> create_cube_callbaack = [this]()
	{
		ObjectFactoryBase factory;
		shObject cube = factory.CreateObject(std::make_shared<MyModel>(m_asset_manager.m_modelManager->FindBrush("cube"), "default_cube"), eObject::RenderType::PHONG, "DefaultCube");
		AddObject(cube);
	};
	m_externalGui[ExternalWindow::CREATE_WND]->Add(BUTTON, "Cube", (void*)&create_cube_callbaack);

	std::function<void()> create_sphere_callbaack = [this]()
	{
		ObjectFactoryBase factory;
		shObject sphere = factory.CreateObject(std::make_shared<MyModel>(m_asset_manager.m_modelManager->FindBrush("sphere"), "default_sphere"), eObject::RenderType::PHONG, "DefaultSphere");
		AddObject(sphere);
	};
	m_externalGui[ExternalWindow::CREATE_WND]->Add(BUTTON, "Sphere", (void*)&create_sphere_callbaack);

	std::function<void()> create_plane_callbaack = [this]()
	{
		ObjectFactoryBase factory;
		shObject plane = factory.CreateObject(std::make_shared<MyModel>(m_asset_manager.m_modelManager->FindBrush("plane"), "default_plane"), eObject::RenderType::PHONG, "DefaultPlane");
		AddObject(plane);
	};
	m_externalGui[ExternalWindow::CREATE_WND]->Add(BUTTON, "Plane", (void*)&create_plane_callbaack);

	std::function<void()> create_ellipse_callback = [this]()
	{
		ObjectFactoryBase factory;
		shObject ellipse = factory.CreateObject(std::make_shared<MyModel>(m_asset_manager.m_modelManager->FindBrush("ellipse"), "default_ellipse"), eObject::RenderType::PHONG, "DefaultEllipse");
		ellipse->SetBackfaceCull(false);
		AddObject(ellipse);
	};
	m_externalGui[ExternalWindow::CREATE_WND]->Add(BUTTON, "Ellipse", (void*)&create_ellipse_callback);

	std::function<void()> create_capsule_callback = [this]()
	{
		ObjectFactoryBase factory;
		shObject capsule = factory.CreateObject(std::make_shared<MyModel>(m_asset_manager.m_modelManager->FindBrush("capsule"), "default_capsule"), eObject::RenderType::PHONG, "DefaultCapsule");
		capsule->SetBackfaceCull(false);
		AddObject(capsule);
	};
	m_externalGui[ExternalWindow::CREATE_WND]->Add(BUTTON, "Capsule", (void*)&create_capsule_callback);

	//bezier 2d
	std::function<void()> create_bezier_callback = [this]()
	{
		dbb::Bezier bezier;
		bezier.p0 = { -0.85f, -0.75f, 0.0f };
		bezier.p1 = { -0.45f, -0.33f, 0.0f };
		bezier.p2 = { 0.17f,  0.31f, 0.0f };
		bezier.p3 = { 0.55f,  0.71f, 0.0f };

		ObjectFactoryBase factory;
		shObject bezier_model = factory.CreateObject(std::make_shared<BezierCurveModel>(std::vector<BezierCurveMesh*>{new BezierCurveMesh(bezier, /*2d*/true)}), eObject::RenderType::BEZIER_CURVE);
		AddObject(bezier_model);

		for (int i = 0; i < 4; ++i)
		{
			shObject pbr_sphere = factory.CreateObject(m_asset_manager.m_modelManager->Find("sphere_red"), eObject::RenderType::PBR, "SphereBezierPBR " + std::to_string(i));
			bezier_model->GetChildrenObjects().push_back(pbr_sphere);
			pbr_sphere->Set2DScreenSpace(true);
		}
		bezier_model->SetScript(new BezierCurveUIController(this, bezier_model, 0.02f, m_asset_manager.m_texManager->Find("pseudo_imgui")));
	};
	m_externalGui[ExternalWindow::CREATE_WND]->Add(BUTTON, "Bezier Curve 2D", (void*)&create_bezier_callback);

	//bezier 3d
	std::function<void()> create_bezier_callbaack_3d = [this]()
	{
		dbb::Bezier bezier;
		bezier.p0 = { 1.0f, 3.0f, 0.0f };
		bezier.p1 = { 3.0f, 3.0f, 3.0f };
		bezier.p2 = { 4.2f, 3.0f, -2.5f };
		bezier.p3 = { 8.0f, 3.0f, 1.0f };

		ObjectFactoryBase factory;
		shObject bezier_model = factory.CreateObject(std::make_shared<BezierCurveModel>(std::vector<BezierCurveMesh*>{new BezierCurveMesh(bezier, /*2d*/false)}), eObject::RenderType::BEZIER_CURVE);
		AddObject(bezier_model);

		for (int i = 0; i < 4; ++i)
		{
			shObject pbr_sphere = factory.CreateObject(m_asset_manager.m_modelManager->Find("sphere_red"), eObject::RenderType::PBR, "SphereBezierPBR " + std::to_string(i));
			bezier_model->GetChildrenObjects().push_back(pbr_sphere);
		}
		bezier_model->SetScript(new BezierCurveUIController(this, bezier_model, 0.1f));
	};
	m_externalGui[ExternalWindow::CREATE_WND]->Add(BUTTON, "Bezier Curve 3D", (void*)&create_bezier_callbaack_3d);

	//Create from Models
	static size_t g_model_index = 0;
	static bool g_sale_100 = false;
	static std::function<size_t(size_t)> objects_callback = [](size_t _index)
	{
		size_t prev = g_model_index;
		if (_index != MAXSIZE_T)
			g_model_index = _index;
		return prev;
	};
	static eVectorStringsCallback watcher{ {}, objects_callback };
	m_asset_manager.m_modelManager->ModelsUpdated.Subscribe([](const std::map<std::string, std::shared_ptr<IModel> >& _models)
		{
			watcher.data.clear();
			for (const auto model : _models)
				watcher.data.push_back(model.first);
		});
	m_externalGui[ExternalWindow::CREATE_WND]->Add(COMBO_BOX, "Models", &watcher);
	std::function<void()> create_from_callback = [this]()
	{
		ObjectFactoryBase factory;
		shObject obj = factory.CreateObject(m_asset_manager.m_modelManager->Find(watcher.data[g_model_index]), eObject::RenderType::PHONG, watcher.data[g_model_index]);
		if (g_sale_100)
			obj->GetTransform()->setScale(glm::vec3(0.01f, 0.01f, 0.01f));
		AddObject(obj);
	};
	m_externalGui[ExternalWindow::CREATE_WND]->Add(BUTTON, "Create from", (void*)&create_from_callback);
	m_externalGui[ExternalWindow::CREATE_WND]->Add(CHECKBOX, "Scale m to sm", (void*)&g_sale_100);

	//Object List
	m_externalGui[ExternalWindow::OBJECTS_WND]->Add(OBJECT_LIST, "Objects List", (void*)this);

	//Objects material
	m_externalGui[ExternalWindow::MATERIAL_WND]->Add(OBJECT_REF_MATERIAL, "Material", (void*)&m_scene.m_focused);

#ifdef RIGGER_WND_USE

	//Objects rigger
	m_externalGui[ExternalWindow::RIGGER_WND]->Add(GAME, "Game", (void*)&(*this));
	m_externalGui[ExternalWindow::RIGGER_WND]->Add(OBJECT_REF_RIGGER, "Rigger", (void*)&m_scene.m_focused);

	static std::function<void(shObject, const std::string&)> load_rigger = [this](shObject obj, const std::string& _path)
	{
		IRigger* rigger = m_asset_manager.m_animationManager->DeserializeRigger(_path);
		obj->SetRigger(rigger);
	};
	m_externalGui[ExternalWindow::RIGGER_WND]->Add(ADD_CALLBACK, "Load Rigger", reinterpret_cast<void*>(&load_rigger));

	static std::function<void(shObject, const std::string&)> save_rigger = [this](shObject obj, const std::string& _path)
	{
		IRigger* rigger = obj->GetRigger();
		m_asset_manager.m_animationManager->SerializeRigger(dynamic_cast<const RigAnimator*>(rigger), _path);
	};
	m_externalGui[ExternalWindow::RIGGER_WND]->Add(ADD_CALLBACK, "Save Rigger", reinterpret_cast<void*>(&save_rigger));

	static std::function<void(shObject, const std::string&)> save_animations = [this](shObject obj, const std::string& _path)
	{
		IRigger* rigger = obj->GetRigger();
		/*for(int i = 0; i < rigger->GetAnimationCount(); ++i)*/
		SkeletalAnimation* cur_animation = dynamic_cast<SkeletalAnimation*>(rigger->GetCurrentAnimation());
		if (cur_animation)
			m_asset_manager.m_animationManager->AddAnimation(*cur_animation);

		m_asset_manager.m_animationManager->Serialize(_path);
	};
	m_externalGui[ExternalWindow::RIGGER_WND]->Add(ADD_CALLBACK, "Save Animations", reinterpret_cast<void*>(&save_animations));

	static std::function<void(shObject, const std::string&)> load_collider = [this](shObject obj, const std::string& _path)
	{
		IRigger* rigger = m_asset_manager.m_animationManager->DeserializeRigger(_path);
		obj->SetRigger(rigger);
	};
	m_externalGui[ExternalWindow::RIGGER_WND]->Add(ADD_CALLBACK, "Load Dynamic Collider from", reinterpret_cast<void*>(&load_collider));

	static std::function<void(shObject, const std::string&)> save_collider = [this](shObject obj, const std::string& _path)
	{
		DynamicBoundingBoxColliderSerializerYAML boxSerializer;
		boxSerializer.Serialize(dynamic_cast<BoxColliderDynamic*>(obj->GetCollider()), _path);
	};
	m_externalGui[ExternalWindow::RIGGER_WND]->Add(ADD_CALLBACK, "Save Dynamic Collider", reinterpret_cast<void*>(&save_collider));

#endif

	//Console
	static std::function<void(const std::string&)> console_plane_callbaack = [this](const std::string& _commandLine)
	{
		std::cout << _commandLine << std::endl;
		// Parse _commandLine and call the function (Set uniform, set script, set material etc.)
		std::string parsed, input = _commandLine;
		std::stringstream input_stringstream(input);
		std::vector<std::string> res;
		while (std::getline(input_stringstream, parsed, ' '))
		{
			res.push_back(parsed);
		}
		if (res.size() == 4)
			m_pipeline.SetUniformData(res[0] + " " + res[1], res[2], std::stof(res[3]));
		else if (res.size() == 5)
			m_pipeline.SetUniformData(res[0] + " " + res[1], res[2], glm::vec2(std::stof(res[3]), std::stof(res[4])));
		else if (res.size() == 6)
			m_pipeline.SetUniformData(res[0] + " " + res[1], res[2], glm::vec3(std::stof(res[3]), std::stof(res[4]), std::stof(res[5])));
	};
	m_externalGui[ExternalWindow::CONSOLE_WND]->Add(CONSOLE, "Console", reinterpret_cast<void*>(&console_plane_callbaack));

	//Global Scrips
	m_global_scripts.push_back(std::make_shared<ParticleSystemToolController>(this, m_externalGui[ExternalWindow::PARTICLE_SYSTEM_WND], m_asset_manager.m_modelManager.get(), m_asset_manager.m_texManager.get(), m_asset_manager.m_soundManager.get(), m_pipeline));

	auto physics = std::make_shared<PhysicsSystemController>(this);
	m_global_scripts.push_back(physics);

	std::function<void()> run_physics = [physics]() { physics->RunPhysics(); };
	std::function<void()> stop_physics = [physics]() { physics->StopPhysics(); };
	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(BUTTON, "Run Physics", &run_physics);
	m_externalGui[ExternalWindow::PIPELINE_WND]->Add(BUTTON, "Stop Physics", &stop_physics);

	RunPhysics.Subscribe(run_physics);
	StopPhysics.Subscribe(stop_physics);
#endif
}

