#pragma once

#include "game_assets.h"
#include "base.h"

#include <base/interfaces.h>
#include <base/Event.h>

#include <math/Clock.h>
#include <math/Decal.h>

#include <sdl_assets/sdl_assets.h>

#include <opengl_assets/OpenGlRenderPipeline.h>

#include "InputStrategy.h"

class eInputController;
class IWindowImGui;
class ITcpAgent;
class TerrainGeneratorTool;
class eTextureManager;

namespace dbb { class PhysicsSystem; }

//-------------------------------------------------------
class DLL_GAME_ASSETS eMainContextBase : public IGame, public IInputObserver
{
	friend class LightsController;
public:
	eMainContextBase(eInputController* _input,
									 std::vector<IWindowImGui*>& _externalGui,
									 const std::string& _modelsPath,
									 const std::string& _assetsPath,
									 const std::string& _shadersPath,
									 const std::string& _scriptsPath,
									 int _width,
									 int _height);

	virtual ~eMainContextBase();

	base::Event<std::function<void(shObject, bool _left)>>					ObjectPicked;
	base::Event<std::function<void(shObject _new, shObject _old)>>	FocusChanged;
	base::Event<std::function<void(shObject)>>											ObjectBeingAddedToScene;
	base::Event<std::function<void(shObject)>>											ObjectBeingDeletedFromScene;

	base::Event<std::function<void(void)>>													RunPhysics;
	base::Event<std::function<void(void)>>													StopPhysics;

	base::Event<std::function<void(size_t)>>												MainCameraIndexChanged;
	//IInputObserver
	virtual bool	OnKeyJustPressed(uint32_t asci, KeyModifiers _modifier)	override;

	virtual bool	OnMouseMove(int32_t x, int32_t y, KeyModifiers _modifier) override;
	virtual bool	OnMousePress(int32_t x, int32_t y, bool left, KeyModifiers _modifier) override;
	virtual bool	OnMouseRelease(KeyModifiers _modifier) override;
	virtual bool	OnDropEvent(uint32_t x, uint32_t y, const std::string& _file_path) override;

	//IGame
	void							InitializeGL() final;
	virtual void			InitializeScene() final;

	virtual void	PaintGL() override;

	virtual void	AddObject(std::shared_ptr<eObject> _object) override;
	virtual void	DeleteObject(std::shared_ptr<eObject> _object) override;

	virtual void	SetFocused(std::shared_ptr<eObject>) override;
	void					SetFocused(const eObject* _newFocused);
	virtual void	SetFramed(const std::vector<shObject>&) override;

	virtual void AddInputObserver(IInputObserver* _observer, ePriority _priority) override;
	virtual void DeleteInputObserver(IInputObserver* _observer) override;

	virtual void AddGlobalScript(std::shared_ptr<IScript> _script) override { m_global_scripts.push_back(_script); }

	virtual uint32_t																			GetFinalImageId() override;
				  uint32_t																			GetUIlessImageId();
	virtual GameState																			GetState() const { return m_gameState; }

	virtual std::shared_ptr<eObject>											GetFocusedObject() override;
	std::shared_ptr<eObject>															GetHoveredObject();
	virtual const std::vector<std::shared_ptr<eObject>>&	GetObjects() const override { return m_scene.m_objects; }

	virtual std::vector<std::shared_ptr<IParticleSystem> >  GetParticleSystems() override;

	virtual const Texture* GetTexture(const std::string& _name) const override;

	virtual Light& GetMainLight() override;

	Camera& GetMainCamera();
	Camera& GetCamera(size_t i);
	void SetMainCameraIndex(size_t i);

	virtual glm::mat4 GetMainCameraViewMatrix() override;
	virtual glm::mat4 GetMainCameraProjectionMatrix() override;
	virtual glm::vec3 GetMainCameraPosition() const override;
	virtual glm::vec3 GetMainCameraDirection() const override;

	virtual bool&			UseGizmo() override { return m_use_guizmo; }
	virtual uint32_t	CurGizmoType() override { return (uint32_t)m_gizmo_type; }

	virtual bool& ShowFPS() override { return m_pipeline.ShowFPS(); }
	virtual bool& ShowMeasurementGrid() override { /* if(m_pipeline.IsMeasurementGridEnabled())*/ //@todo !!!
																								 return *m_pipeline.IsMeasurementGridEnabled(); }

	virtual uint32_t			Width() const override;
	virtual uint32_t			Height()  const override;

	virtual void AddGUI(const std::shared_ptr<GUI>&);
	virtual void DeleteGUI(const std::shared_ptr<GUI>&);

	virtual void AddText(std::shared_ptr<Text>);
	virtual std::vector<std::shared_ptr<Text>>& GetTexts();

	void AddDecal(const Decal& );

	void EnableHovered(bool _hover);
	void EnableFrameChoice(bool _enable, bool _with_left = true);

	void SetInputStrategy(InputStrategy* _input_strategy) 
	{
		m_input_strategy.reset(_input_strategy);
	}

	ModelManagerYAML*				GetModelManager() const;
	ScriptManager*					GetScriptManager() const;
	dbb::PhysicsSystem*			GetPhysicsSystem() const { return m_physics_system.get(); }
	AnimationManagerYAML*		GetAnimationManager() const { return m_asset_manager.m_animationManager.get(); }
	eTextureManager*				GetTextureManager() const; // open gl class, hide

	std::shared_ptr<TerrainGeneratorTool> CreateTerrainGeneratorTool();

	void PrintScreen();
protected:
	virtual void		InitializePipline();
	virtual void		InitializeBuffers();
	virtual void		InitializeModels();
	virtual void		InitializeRenders();
	virtual void		InitializeTextures();
	virtual void		InitializeSounds()  {}
	virtual void		InitializeScripts();
	virtual void		InitializeExternalGui();

	virtual void		Pipeline()			{}

	void						_PreInitModelManager();
	void						_AcceptDrop();

protected:
	Scene																			m_scene;
	eInputController*													m_input_controller;

	AssetManagement														m_asset_manager;
	eOpenGlRenderPipeline											m_pipeline;
	std::unique_ptr<dbb::PhysicsSystem>				m_physics_system;
	std::vector <std::shared_ptr<IScript>>		m_global_scripts;

	GameState																	m_gameState = IGame::GameState::UNINITIALIZED;
	std::unique_ptr<InputStrategy>						m_input_strategy;
	math::eClock															m_global_clock;

	std::vector<IWindowImGui*>&								m_externalGui;

	bool																			m_l_pressed = false;
	FramedChoice															m_framed_choice_enabled = FramedChoice::DISABLED;

	bool																			m_use_guizmo = true;
	GizmoType																	m_gizmo_type = GizmoType::TRANSLATE;
};
