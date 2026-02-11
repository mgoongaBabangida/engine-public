#include "stdafx.h"
#include "RPGFactory.h"

#include <game_assets/MainContextBase.h>
#include <game_assets/GUIController.h>
#include <game_assets/ScriptManager.h>
#include <math/PhysicsSystem.h>

#include "StairsScript.h"

//--------------------------------------------------
RPGFactory::RPGFactory(eMainContextBase* _game, AnimationManagerYAML* _animationManager)
	: ObjectFactoryBase(_animationManager), m_game(_game)
{
}

//-------------------------------------------------------------------------------------------------------------------------------------
std::shared_ptr<eObject> RPGFactory::CreateObjectMain(std::shared_ptr<IModel> _model, eObject::RenderType _render_type, const std::string& _name, const std::string& _rigger_path, const std::string& _collider_path, ColliderType _colliderType)
{
	std::shared_ptr<eObject> character = CreateObject(_model, _render_type, _name, _rigger_path, _collider_path, _colliderType);
	
	character->GetRigidBody()->SetKinematic();
	//m_game->GetPhysicsSystem()->AddRigidbody(std::dynamic_pointer_cast<dbb::RigidBody>(character->GetRigidBody())); //@todo

	//factory.SaveDynamicCollider(character, "Michelle4Anim.mgoongaBoxColliderDynamic");

	ScriptManager* scriptManager = m_game->GetScriptManager();

	if (auto script = scriptManager->createScriptByName("CharacterScript"); script.first)
	{
		scriptManager->DLLToBeUnloaded.Subscribe([this, character](const std::string& _dllName)
			{
				if (_dllName == "CharacterScript")
				{
					m_game->DeleteInputObserver(m_crt_script);
					character->SetScript(nullptr);
				}
			});
		scriptManager->DLLUnloaded.Subscribe([this, character](const std::string& _dllName)
			{
				if (_dllName == "CharacterScript")
				{
					auto script = m_game->GetScriptManager()->createScriptByName("CharacterScript");
					character->SetScript(script.first, script.second);
					m_crt_script = character->GetScript();
					m_game->AddInputObserver(character->GetScript(), WEAK);
					m_crt_script->Initialize();
				}
			});
		character->SetScript(script.first, script.second);
		m_crt_script = character->GetScript();
		m_game->AddInputObserver(m_crt_script, WEAK);
	}
	return character;
}

//------------------------------------------------------------------------------------------------------------------------
std::shared_ptr<eObject> RPGFactory::CreateObjectStairsScript(std::shared_ptr<IModel> _model, eObject::RenderType _render_type, const std::string& _name)
{
	std::shared_ptr<eObject> steps = CreateObject(_model, _render_type, _name);
	if (m_crt_script)
	{
		auto steps_script = new StairsScript(m_game);
		std::shared_ptr<GUIControllerMenuForStairsScript> window_script = std::make_shared<GUIControllerMenuForStairsScript>(m_game); //GUIControllerMenuForStairsScript or GUIControllerBase
		steps_script->SetDebugWindow(window_script.get());
		/*steps_script->Start.Subscribe([this]() {m_crt_script->Start(); });
		steps_script->Reset.Subscribe([this]() {m_crt_script->Reset(); });*/
		steps->SetScript(steps_script);
		m_game->AddGlobalScript(window_script); //@should not be here
	}
	return steps;
}

//-----------------------------------------------------------------------------
std::shared_ptr<eObject> RPGFactory::CreateObjectChairScript(std::shared_ptr<IModel> _model, eObject::RenderType _render_type, const std::string& _name)
{
	std::shared_ptr<eObject> chair = CreateObject(_model, _render_type, _name);
	InteractionData data;
	data.m_name = "chair";
	dbb::OBB volume;
	volume.origin = { -8.2f, -2.0f, 8.3f };
	volume.size = { 0.5f, 0.5f , 0.5f };
	volume.orientation = { {1,0,0}, {0,1,0}, {0,0,1} };
	chair->SetScript(new InteractionScript(m_game, data, volume));
	return chair;
}

//------------------------------------------------------------------------------------------------
std::shared_ptr<eObject> RPGFactory::CreateObjectTakebleScript(std::shared_ptr<IModel> _model, eObject::RenderType _render_type, const std::string& _name)
{
	std::shared_ptr<eObject> takable = CreateObject(_model, _render_type, _name);
	InteractionData data;
	data.m_name = "taking";
	data.m_object = takable;

	//Custom !!!
	Transform transform;     // s -> r -> tr
	glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(270.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	transform.setTranslation(glm::vec3(-21.78 / 100, 2.35 / 100, -16.3 / 100));
	transform.setRotation(transform.getRotation() * glm::quat(rotation));
	transform.setScale(glm::vec3(0.115f, 0.115f, 0.115f)); // child scale
	data.m_pretransform = transform.getModelMatrix();

	dbb::OBB volume;
	volume.origin = { -8.04f, -2.0f, 6.67f }; // external !!!
	volume.size = { 0.75f, 0.75f , 0.75f };
	volume.orientation = { {1,0,0}, {0,1,0}, {0,0,1} };
	takable->SetScript(new InteractionScript(m_game, data, volume));
	return takable;
}

//-----------------------------------------------------------------------------
std::shared_ptr<eObject> RPGFactory::CreateObjectAnimationScript(std::shared_ptr<IModel> _model, eObject::RenderType _render_type, const std::string& _name)
{
	std::shared_ptr<eObject> animated = CreateObject(_model, _render_type, _name, "Default", "", ColliderType::DYNAMIC_BOX);
	InteractionData data;
	data.m_name = "animated";
	data.m_object = animated;

	dbb::OBB volume;
	volume.origin = { -1.5f, -2.0f, 0.0f }; // external !!!
	volume.size = { 1.25f,  1.25f ,  1.25f };
	volume.orientation = { {1,0,0}, {0,1,0}, {0,0,1} };
	animated->SetScript(new InteractionScript(m_game, data, volume));
	return animated;
}

//---------------------------------------------------------
IScript* RPGFactory::GetMainCharacterScript()
{
	return m_crt_script;
}
