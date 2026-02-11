#include "stdafx.h"
#include "SandBoxGame.h"

#include <base/InputController.h>

#include <math/BvhImporter.h>
#include <math/PhysicsSystem.h>

#include <opengl_assets/Sound.h>
#include <opengl_assets/TextureManager.h>
#include <opengl_assets/SoundManager.h>
#include <opengl_assets/GUI.h>

#include <game_assets/ModelManagerYAML.h>
#include <game_assets/AnimationManagerYAML.h>
#include <game_assets/GUIController.h>

#include <game_assets/CameraFreeController.h>
#include <game_assets/CameraRPGController.h>

#include <sdl_assets/ImGuiContext.h>

#include <game_assets/ShootScript.h>
#include <game_assets/PhysicsEngineTestScript.h>
#include <game_assets/InteractionData.h>

#include "SandBoxGameScript.h"
#include "RPGFactory.h"

//#include <sdl_assets/ImGuiWindowInternal.h>

#include <math/Hex.h>

//-------------------------------------------------------------------------
eSandBoxGame::eSandBoxGame(eInputController*  _input,
													 std::vector<IWindowImGui*>& _externalGui,
													 const std::string& _modelsPath,
													 const std::string& _assetsPath,
													 const std::string& _shadersPath,
													 const std::string& _scriptsPath,
													 int _width,
													 int _height)
: eMainContextBase(_input, _externalGui, _modelsPath, _assetsPath, _shadersPath, _scriptsPath, _width, _height), m_factory(this, m_asset_manager.m_animationManager.get())
{
	ObjectPicked.Subscribe([this](shObject _new_focused, bool _left)
		{
			if (_new_focused != m_scene.m_focused && _left)
			{
				FocusChanged.Occur(m_scene.m_focused, _new_focused);
				m_scene.m_focused = _new_focused;
				return true;
			}
			return false;
		});
}

//-------------------------------------------------------------------------
void eSandBoxGame::InitializeExternalGui()
{
	eMainContextBase::InitializeExternalGui();
	//@todo temp experiment with imgui internal menues
	/*externalGui.push_back(new eImGuiWindowInternal(this->texManager->Find("golden_frame")->id));
	m_input_controller->AddObserver(externalGui.back(), MONOPOLY);*/
}

//-------------------------------------------------------------------------
void eSandBoxGame::InitializeModels()
{
	eMainContextBase::InitializeModels();

	//DESERIALIZE ANIMATIONS
	math::eClock clock;
	clock.start();
	m_asset_manager.m_animationManager->Deserialize("AllFour.mgoongaAnimations");
	std::cout << "animationManager->Deserialize" << clock.newFrame() << std::endl;

	m_asset_manager.m_animationManager->LoadAnimation("Animations/Michelle/dae/Sitting.dae", "Sitting");
	m_asset_manager.m_animationManager->LoadAnimation("Animations/Michelle/dae/Taking Item.dae", "TakingItem");
	m_asset_manager.m_animationManager->LoadAnimation("Animations/Michelle/dae/Walking Backwards.dae", "WalkingBackwards");
	m_asset_manager.m_animationManager->LoadAnimation("Animations/Michelle/dae/Jumping Down.dae", "JumpingDown");
	m_asset_manager.m_animationManager->LoadAnimation("Animations/Michelle/dae/Throw.dae", "Throw");
	std::cout << "LoadAnimation " << clock.newFrame() << std::endl;

	//light
	GetMainLight().light_position = glm::vec4(0.f, 6.5f, 5.f , 1.f);
	GetMainLight().type = eLightType::CSM;

	ObjectFactoryBase factory(m_asset_manager.m_animationManager.get());
	shObject hdr_object = factory.CreateObject(m_asset_manager.m_modelManager->Find("white_quad"), eObject::RenderType::PHONG, "LightObject"); // or "white_quad"
	if (hdr_object->GetModel()->GetName() == "white_sphere")
		hdr_object->GetTransform()->setScale(glm::vec3(0.3f, 0.3f, 0.3f));
	hdr_object->GetTransform()->setTranslation(GetMainLight().light_position);

	GetMainCamera().setPosition({-1, 2, 10});
	GetMainCamera().setDirection({0, 0, -1.f});

	Material m{ glm::vec3{}, 0.0f, 0.0f };
	m.used_textures.insert(Material::TextureType::ALBEDO);
	m.opacity = 1.f;
	hdr_object->GetModel()->SetMaterial(m);
	AddObject(hdr_object);

	if (m_pipeline.IsMeasurementGridEnabled() != nullptr)
		*m_pipeline.IsMeasurementGridEnabled() = false;

	_InitializeScene();
	m_global_scripts.push_back(std::make_shared<SandBoxGameScript>(this));

	//GLOBAL SCRIPTS
	/*m_global_scripts.push_back(std::make_shared<PhysicsEngineTestScript>(this, m_externalGui[13]));
	m_input_controller->AddObserver(&*m_global_scripts.back(), WEAK);
	std::shared_ptr<GUIControllerBase> window_script = std::make_shared<GUIControllerBase>(this); // UI
	AddGlobalScript(window_script);*/

	if (m_factory.GetMainCharacterScript())
	{
		auto camera_controller = std::make_shared<CameraRPGController>(GetMainCamera());
		camera_controller->SetObject(m_factory.GetMainCharacterScript()->GetScriptObject().lock());
		m_global_scripts.push_back(camera_controller);
	}
	else
	{
		auto camera_controller = std::make_shared<CameraFreeController>(GetMainCamera());
		m_global_scripts.push_back(camera_controller);
	}

	AddInputObserver(this, WEAK);
	AddInputObserver(&*m_global_scripts.back(), WEAK);

	//@todo what initialize() exactly
	RunPhysics.Occur();
}

//-------------------------------------------------------------------------
void eSandBoxGame::InitializePipline()
{
	eMainContextBase::InitializePipline();
	m_pipeline.GetBlurCoefRef() = 1.0f;

	m_pipeline.GetFogInfo().fog_on = false;
	m_pipeline.GetSkyBoxOnRef() = true;
}

//-------------------------------------------------------------------------
void eSandBoxGame::_InitializeScene()
{
	//MODELS
	//modelManager->Add("MapleTree", (GLchar*)std::string(modelFolderPath + "MapleTree/MapleTree.obj").c_str());
	//modelManager->Add("Cottage", (GLchar*)std::string(modelFolderPath + "85-cottage_obj/cottage_obj.obj").c_str());

	Material material{ glm::vec3(0.8f, 0.0f, 0.0f), 0.5f , 0.5f }; // -> move to base
	material.textures[Material::TextureType::EMISSIVE] = Texture::GetTexture1x1(TColor::BLACK).m_id;

	//MATERIALS
	Material pbr1;
	pbr1.textures[Material::TextureType::ALBEDO] = m_asset_manager.m_texManager->Find("pbr1_basecolor")->m_id;
	pbr1.textures[Material::TextureType::METALLIC] = m_asset_manager.m_texManager->Find("pbr1_metallic")->m_id;
	pbr1.textures[Material::TextureType::NORMAL] = m_asset_manager.m_texManager->Find("pbr1_normal")->m_id;
	pbr1.textures[Material::TextureType::ROUGHNESS] = m_asset_manager.m_texManager->Find("pbr1_roughness")->m_id;
	//pbr1.ao_texture_id = texManager->Find("pbr1_ao")->m_id;
	pbr1.textures[Material::TextureType::EMISSIVE] = Texture::GetTexture1x1(BLACK).m_id;
	pbr1.used_textures.insert(Material::TextureType::ALBEDO);
	pbr1.used_textures.insert(Material::TextureType::METALLIC);
	pbr1.used_textures.insert(Material::TextureType::NORMAL);
	pbr1.used_textures.insert(Material::TextureType::ROUGHNESS);
	pbr1.used_textures.insert(Material::TextureType::EMISSIVE);
	pbr1.ao = 0.8f;

	Material gold;
	gold.textures[Material::TextureType::ALBEDO] = m_asset_manager.m_texManager->Find("pbr_gold_basecolor")->m_id;
	gold.textures[Material::TextureType::METALLIC] = m_asset_manager.m_texManager->Find("pbr_gold_metallic")->m_id;
	gold.textures[Material::TextureType::NORMAL] = m_asset_manager.m_texManager->Find("pbr_gold_normal")->m_id;
	gold.textures[Material::TextureType::ROUGHNESS] = m_asset_manager.m_texManager->Find("pbr_gold_roughness")->m_id;
	gold.textures[Material::TextureType::EMISSIVE] = Texture::GetTexture1x1(WHITE).m_id;
	gold.used_textures.insert(Material::TextureType::ALBEDO);
	gold.used_textures.insert(Material::TextureType::METALLIC);
	gold.used_textures.insert(Material::TextureType::NORMAL);
	gold.used_textures.insert(Material::TextureType::ROUGHNESS);
	gold.used_textures.insert(Material::TextureType::EMISSIVE);
	gold.ao = 0.9f;

	m_asset_manager.m_modelManager->Add("sphere_textured", Primitive::SPHERE, std::move(pbr1));
	m_asset_manager.m_modelManager->Add("sphere_gold", Primitive::SPHERE, std::move(gold));

	//OBJECTS
	ObjectFactoryBase factory(m_asset_manager.m_animationManager.get());

	shObject obj = factory.CreateObject(m_asset_manager.m_modelManager->Find("sphere_textured"), eObject::RenderType::PBR, "SpherePBR");
	obj->GetTransform()->setTranslation(glm::vec3(-2.0f, 3.5f, 1.5f));
	AddObject(obj);

	shObject goldsphere = factory.CreateObject(m_asset_manager.m_modelManager->Find("sphere_gold"), eObject::RenderType::PBR, "SpherePBRGold");
	goldsphere->GetTransform()->setTranslation(glm::vec3(-7.0f, 3.5f, 2.0f));
	AddObject(goldsphere);

	shObject wallCube = factory.CreateObject(m_asset_manager.m_modelManager->Find("wall_cube"), eObject::RenderType::PHONG, "WallCube");
	wallCube->GetTransform()->setTranslation(glm::vec3(3.0f, -1.0f, -5.0f));
	AddObject(wallCube);

	Texture t;

	shObject grassPlane = factory.CreateObject(m_asset_manager.m_modelManager->Find("grass_plane"), eObject::RenderType::PHONG, "GrassPlane");
	//grassPlane->GetModel()->SetMaterial(material); //!!! to test area light need roughness !!!
	grassPlane->GetTransform()->setTranslation(glm::vec3(0.0f, -2.0f, 0.0f));
	grassPlane->GetTransform()->setScale(glm::vec3(5.0f, 5.0f, 5.0f));
	AddObject(grassPlane);
	//physics
	grassPlane->GetCollider()->SetFrom(*grassPlane->GetTransform());
	grassPlane->GetRigidBody()->SetCollider(grassPlane->GetCollider());
	grassPlane->GetRigidBody()->SetKinematic();
	GetPhysicsSystem()->AddRigidbody(std::dynamic_pointer_cast<dbb::RigidBody>(grassPlane->GetRigidBody())); //@todo

	shObject wolf = factory.CreateObject(m_asset_manager.m_modelManager->Find("wolf"), eObject::RenderType::PHONG, "Wolf", "Default", "");
	wolf->GetTransform()->setRotation(glm::radians(-90.0f), 0.0f, 0.0f);
	wolf->GetTransform()->setTranslation(glm::vec3(3.0f, -2.0f, 0.0f));
	wolf->GetTransform()->setScale(glm::vec3(1.5f, 1.5f, 1.5f));
	AddObject(wolf);

	shObject character = m_factory.CreateObjectMain(m_asset_manager.m_modelManager->Find("Michelle"),
		eObject::RenderType::PHONG,
		"Michelle",
		/*"Default",*/ "MichelleStairs.mgoongaRigger",
		/*"",*/ "MishelleCollider.mgoongaBoxColliderDynamic",
		ColliderType::DYNAMIC_BOX); // dynamic collider

	character->GetTransform()->setTranslation(glm::vec3(11.6f, -2.0f, 1.8f));
	character->GetTransform()->setScale(glm::vec3(0.01f, 0.01f, 0.01f));
	character->GetTransform()->setRotation(0.0f, glm::radians(180.0f), 0.0f);
	character->GetCollider()->SetScale(character->GetTransform()->getScaleAsVector().x);

	AddObject(character);

	/*m_modelManager->Save(soldier->GetModel(), "Soldier.mgoongaObject3d");
	m_modelManager->Add("Soldier", "Soldier.mgoongaObject3d");*/

	if (true)
	{
		// GraveStone
		shObject gravestone = factory.CreateObject(m_asset_manager.m_modelManager->Find("Gravestone"), eObject::RenderType::PBR, "Gravestone");
		gravestone->GetTransform()->setTranslation(glm::vec3(0.5f, -2.0f, 4.0f));
		gravestone->GetTransform()->setRotation(0.0f, glm::radians(180.0f), 0.0f);
		gravestone->GetTransform()->setScale(glm::vec3(0.5f, 0.5f, 0.5f));
		const_cast<I3DMesh*>(gravestone->GetModel()->Get3DMeshes()[0])->SetMaterial(material);
		AddObject(gravestone);

		// TombeStone
		shObject tombstone = factory.CreateObject(m_asset_manager.m_modelManager->Find("Tombstone"), eObject::RenderType::PBR, "Tombstone");
		tombstone->GetTransform()->setTranslation(glm::vec3(-1.5f, -2.0f, 4.0f));
		tombstone->GetTransform()->setRotation(0.0f, glm::radians(180.0f), 0.0f);
		tombstone->GetTransform()->setScale(glm::vec3(0.5f, 0.5f, 0.5f));
		const_cast<I3DMesh*>(tombstone->GetModel()->Get3DMeshes()[0])->SetMaterial(material);
		AddObject(tombstone);

		//Chest
		shObject chest = m_factory.CreateObjectAnimationScript(m_asset_manager.m_modelManager->Find("Chest"), eObject::RenderType::PBR, "Chest");
		chest->GetTransform()->setTranslation(glm::vec3(-1.5f, -2.0f, 0.0f));
		chest->GetTransform()->setRotation(glm::radians(-90.0f), glm::radians(-90.0f), 0.0f);
		chest->GetTransform()->setScale(glm::vec3(0.5f, 0.5f, 0.5f));
		AddObject(chest);

		chest->GetRigger()->Apply("AnimStack::Chest_Top|Chest_open", false);
		chest->GetRigger()->UseFirstFrameAsIdle();

		Material chest_material = *(chest->GetModel()->GetMeshes()[0]->GetMaterial());
		chest_material.used_textures.insert(Material::TextureType::NORMAL);
		chest_material.used_textures.insert(Material::TextureType::METALLIC);
		chest_material.used_textures.insert(Material::TextureType::ROUGHNESS);
		const_cast<IMesh*>(chest->GetModel()->GetMeshes()[0])->SetMaterial(chest_material);

		// @todo should be assigned in blender
		t.loadTextureFromFile("../game_assets/Resources/homemade/chest/chest-diffuse.png");
		t.m_type = "texture_diffuse";
		const_cast<I3DMesh*>(chest->GetModel()->Get3DMeshes()[0])->AddTexture(&t);
		const_cast<I3DMesh*>(chest->GetModel()->Get3DMeshes()[1])->AddTexture(&t);
		t.loadTextureFromFile("../game_assets/Resources/homemade/chest/chest-normal.png");
		t.m_type = "texture_normal";
		const_cast<I3DMesh*>(chest->GetModel()->Get3DMeshes()[0])->AddTexture(&t);
		const_cast<I3DMesh*>(chest->GetModel()->Get3DMeshes()[1])->AddTexture(&t);
		t.loadTextureFromFile("../game_assets/Resources/homemade/chest/chest-metallic.png");
		t.m_type = "texture_specular";
		const_cast<I3DMesh*>(chest->GetModel()->Get3DMeshes()[0])->AddTexture(&t);
		const_cast<I3DMesh*>(chest->GetModel()->Get3DMeshes()[1])->AddTexture(&t);
		t.loadTextureFromFile("../game_assets/Resources/homemade/chest/chest-roughness.png");
		t.m_type = "texture_roughness";
		const_cast<I3DMesh*>(chest->GetModel()->Get3DMeshes()[0])->AddTexture(&t);
		const_cast<I3DMesh*>(chest->GetModel()->Get3DMeshes()[1])->AddTexture(&t);

		//Barrel
		shObject barrel = factory.CreateObject(m_asset_manager.m_modelManager->Find("Barrel"), eObject::RenderType::PBR, "Barrel");
		barrel->GetTransform()->setTranslation(glm::vec3(0.0f, -1.5f, 3.0f));
		barrel->GetTransform()->setScale(glm::vec3(0.5f, 0.5f, 0.5f));
		const_cast<I3DMesh*>(barrel->GetModel()->Get3DMeshes()[0])->SetMaterial(material);
		AddObject(barrel);

		//Beam
		shObject beam = factory.CreateObject(m_asset_manager.m_modelManager->Find("Beam"), eObject::RenderType::PBR, "Beam", "Default", "", ColliderType::MESH);
		beam->GetTransform()->setTranslation(glm::vec3(0.0f, -2.0f, 6.0f));
		beam->GetTransform()->setScale(glm::vec3(0.5f, 0.5f, 0.5f));
		const_cast<I3DMesh*>(beam->GetModel()->Get3DMeshes()[0])->SetMaterial(material);
		AddObject(beam);

		//Table1
		shObject table1 = factory.CreateObject(m_asset_manager.m_modelManager->Find("Table1"), eObject::RenderType::PBR, "Table1");
		table1->GetTransform()->setTranslation(glm::vec3(10.0f, -2.0f, 10.0f));
		table1->GetTransform()->setScale(glm::vec3(0.5f, 0.5f, 0.5f));
		const_cast<I3DMesh*>(table1->GetModel()->Get3DMeshes()[0])->SetMaterial(material);
		AddObject(table1);

		//Table2
		shObject table2 = factory.CreateObject(m_asset_manager.m_modelManager->Find("Table2"), eObject::RenderType::PBR, "Table2");
		table2->GetTransform()->setTranslation(glm::vec3(0.0f, -2.0f, 15.0f));
		table2->GetTransform()->setScale(glm::vec3(0.5f, 0.5f, 0.5f));
		const_cast<I3DMesh*>(table2->GetModel()->Get3DMeshes()[0])->SetMaterial(material);
		AddObject(table2);

		//Table3
		shObject table3 = factory.CreateObject(m_asset_manager.m_modelManager->Find("Table3"), eObject::RenderType::PBR, "Table3");
		table3->GetTransform()->setTranslation(glm::vec3(-8.0f, -2.0f, 7.0f));
		table3->GetTransform()->setScale(glm::vec3(0.5f, 0.5f, 0.5f));
		const_cast<I3DMesh*>(table3->GetModel()->Get3DMeshes()[0])->SetMaterial(material);
		AddObject(table3);

		// Bottle
		shObject bottle = factory.CreateObject(m_asset_manager.m_modelManager->Find("Bottle"), eObject::RenderType::PBR, "Bottle");
		bottle->GetTransform()->setTranslation(glm::vec3(-8.19f, -1.26f, 7.31f));
		bottle->GetTransform()->setScale(glm::vec3(0.115f, 0.115f, 0.115f));
		const_cast<I3DMesh*>(bottle->GetModel()->Get3DMeshes()[0])->SetMaterial(material);
		AddObject(bottle);

		// Bowl
		shObject bowl = factory.CreateObject(m_asset_manager.m_modelManager->Find("Bowl"), eObject::RenderType::PBR, "Bowl");
		bowl->GetTransform()->setTranslation(glm::vec3(-7.74f, -1.26f, 7.1f));
		bowl->GetTransform()->setScale(glm::vec3(0.115f, 0.115f, 0.115f));
		const_cast<I3DMesh*>(bowl->GetModel()->Get3DMeshes()[0])->SetMaterial(material);
		AddObject(bowl);

		// Jar
		shObject jar = m_factory.CreateObjectTakebleScript(m_asset_manager.m_modelManager->Find("Jar"), eObject::RenderType::PBR, "Jar");
		jar->GetTransform()->setTranslation(glm::vec3(-8.04f, -1.26f, 6.67f));
		jar->GetTransform()->setScale(glm::vec3(0.115f, 0.115f, 0.115f));
		const_cast<I3DMesh*>(jar->GetModel()->Get3DMeshes()[0])->SetMaterial(material);
		AddObject(jar);

		// House
		shObject house = factory.CreateObject(m_asset_manager.m_modelManager->Find("House"), eObject::RenderType::PBR, "House");
		house->GetTransform()->setTranslation(glm::vec3(-11.f, -2.0f, -1.00f));
		house->GetTransform()->setRotation(0.0f, glm::radians(-90.0f), 0.0f);
		house->GetTransform()->setScale(glm::vec3(0.85f, 0.85f, 0.85f));
		const_cast<I3DMesh*>(house->GetModel()->Get3DMeshes()[0])->SetMaterial(material);
		AddObject(house);

		// Stairs
		shObject stairs = factory.CreateObject(m_asset_manager.m_modelManager->Find("Stairs"), eObject::RenderType::PBR, "Stairs");
		stairs->GetTransform()->setTranslation(glm::vec3(-5.f, -2.0f, 5.00f));
		stairs->GetTransform()->setScale(glm::vec3(0.5f, 0.5f, 0.5f));
		const_cast<I3DMesh*>(stairs->GetModel()->Get3DMeshes()[0])->SetMaterial(material);
		AddObject(stairs);

		// Stairs2
		shObject stairs2 = factory.CreateObject(m_asset_manager.m_modelManager->Find("Stairs2"), eObject::RenderType::PBR, "Stairs2");
		stairs2->GetTransform()->setTranslation(glm::vec3(7.f, -2.0f, 5.00f));
		stairs2->GetTransform()->setScale(glm::vec3(1.5f, 1.5f, 1.5f));
		const_cast<I3DMesh*>(stairs2->GetModel()->Get3DMeshes()[0])->SetMaterial(material);
		AddObject(stairs2);

		// Steps
		shObject steps = m_factory.CreateObjectStairsScript(m_asset_manager.m_modelManager->Find("Steps"), eObject::RenderType::PBR, "Steps");
		steps->GetTransform()->setTranslation(glm::vec3(9.f, -2.0f, -1.00f));
		steps->GetTransform()->setScale(glm::vec3(1.5f, 1.5f, 1.5f));
		steps->GetTransform()->setRotation(0.0f, glm::radians(90.0f), 0.0f);
		const_cast<I3DMesh*>(steps->GetModel()->Get3DMeshes()[0])->SetMaterial(material);

		AddObject(steps);

		// Pipe
		shObject pipe = factory.CreateObject(m_asset_manager.m_modelManager->Find("Pipe"), eObject::RenderType::PBR, "Pipe");
		pipe->GetTransform()->setTranslation(glm::vec3(-20.f, -2.0f, -15.00f));
		pipe->GetTransform()->setScale(glm::vec3(0.5f, 0.5f, 0.5f));
		pipe->GetTransform()->setRotation(0.0f, glm::radians(90.0f), 0.0f);
		const_cast<I3DMesh*>(pipe->GetModel()->Get3DMeshes()[0])->SetMaterial(material);
		pipe->SetBackfaceCull(false);
		AddObject(pipe);

		// Fence
		shObject fence = factory.CreateObject(m_asset_manager.m_modelManager->Find("Fence"), eObject::RenderType::PBR, "Fence");
		fence->GetTransform()->setTranslation(glm::vec3(-5.f, -2.0f, -0.70f));
		fence->GetTransform()->setScale(glm::vec3(0.75f, 0.75f, 0.75f));
		fence->GetTransform()->setRotation(0.0f, glm::radians(90.0f), 0.0f);
		const_cast<I3DMesh*>(fence->GetModel()->Get3DMeshes()[0])->SetMaterial(material);
		AddObject(fence);

		// Crate
		shObject crate = factory.CreateObject(m_asset_manager.m_modelManager->Find("Crate"), eObject::RenderType::PBR, "Crate");
		crate->GetTransform()->setTranslation(glm::vec3(-15.0f, -1.0f, -15.0f));
		crate->GetTransform()->setScale(glm::vec3(1.f, 1.f, 1.f));
		crate->GetTransform()->setRotation(0.0f, 0.0f, 0.0f);
		const_cast<I3DMesh*>(crate->GetModel()->Get3DMeshes()[0])->SetMaterial(material);
		AddObject(crate);

		// Chair
		shObject chair = m_factory.CreateObjectChairScript(m_asset_manager.m_modelManager->Find("Chair"), eObject::RenderType::PBR, "Chair");
		chair->GetTransform()->setTranslation(glm::vec3(-8.2f, -2.0f, 8.3f));
		chair->GetTransform()->setScale(glm::vec3(.625f, .825f, .48f));
		chair->GetTransform()->setRotation(0.0f, glm::radians(180.0f), 0.0f);
		const_cast<I3DMesh*>(chair->GetModel()->Get3DMeshes()[0])->SetMaterial(material);
		AddObject(chair);

		// Bridge
		shObject bridge = factory.CreateObject(m_asset_manager.m_modelManager->Find("Bridge"), eObject::RenderType::PBR, "Bridge", "Default", "", ColliderType::MESH);
		bridge->GetTransform()->setTranslation(glm::vec3(-3.f, -2.0f, -7.00f));
		bridge->GetTransform()->setScale(glm::vec3(0.4f, 0.4f, 0.4f));
		bridge->GetTransform()->setRotation(0.0f, glm::radians(90.0f), 0.0f);
		const_cast<I3DMesh*>(bridge->GetModel()->Get3DMeshes()[0])->SetMaterial(material);
		AddObject(bridge);

		// Street Light
		shObject streetLight = factory.CreateObject(m_asset_manager.m_modelManager->Find("StreetLight"), eObject::RenderType::PBR, "Street Light");
		streetLight->GetTransform()->setTranslation(glm::vec3(10.f, -2.0f, 3.5f));
		streetLight->GetTransform()->setScale(glm::vec3(0.4f, 0.4f, 0.4f));
		auto streetLightMat = streetLight->GetModel()->Get3DMeshes()[0]->GetMaterial().value();
		streetLightMat.used_textures.insert(Material::TextureType::ALBEDO);
		const_cast<I3DMesh*>(streetLight->GetModel()->Get3DMeshes()[0])->SetMaterial(streetLightMat);
		streetLight->SetBackfaceCull(false);
		AddObject(streetLight);

		shObject areaLightSphere = factory.CreateObject(m_asset_manager.m_modelManager->Find("white_sphere"), eObject::RenderType::PBR, "AreaLightSphere");
		areaLightSphere->GetTransform()->setTranslation(glm::vec3(10.0f, 3.7f, 3.5f));
		areaLightSphere->GetTransform()->setScale(glm::vec3(0.2f, 0.2f, 0.2f));
		
		auto lightMaterial = areaLightSphere->GetModel()->GetMaterial().value();
		lightMaterial.textures[Material::TextureType::EMISSIVE] = Texture::GetTexture1x1(TColor::WHITE).m_id;
		lightMaterial.emission_strength = 15.f;
		lightMaterial.textures[Material::TextureType::ALBEDO] = Texture::GetTexture1x1(TColor::YELLOW).m_id;
		lightMaterial.emission_color = glm::vec3(0.5, 0.5, 0);
		areaLightSphere->GetModel()->SetMaterial(lightMaterial);
		areaLightSphere->SetBackfaceCull(true);
		AddObject(areaLightSphere);
	}

	shObject environement_cube = factory.CreateObject(m_asset_manager.m_modelManager->Find("wall_cube"), eObject::RenderType::ENVIRONMENT_PROBE, "Environement Cube");
	environement_cube->GetTransform()->setTranslation(glm::vec3(3.0f, -1.0f, -2.0f));
	environement_cube->SetVisible(false);
	AddObject(environement_cube);

	//GUI
	glm::vec2 icon_size = { 40.0f , 40.0f };
	float pos_x = 35.0f + 60.0f;
	float pos_y = 50.0f;

	std::shared_ptr<GUI> icon_1 = std::make_shared<GUIWithAlpha>((int)pos_x, (int)pos_y, (int)icon_size.x, (int)icon_size.y, Width(), Height());
	icon_1->SetRenderingFunc(GUI::RenderFunc::Default);
	icon_1->SetTransparent(false);
	const Texture* heart_icon = GetTexture("heart_icon");
	icon_1->SetTexture(*heart_icon, { 0,0 }, { heart_icon->m_width, heart_icon->m_height });
	icon_1->SetTakeMouseEvents(true);
	icon_1->SetHoverCommand(std::make_shared<GUICommand>([icon_1]()
		{
			icon_1->SetRotationAngle(icon_1->GetRotationAngle() + 1.f);
		}));
	AddGUI(icon_1);
	this->AddInputObserver(icon_1.get(), MONOPOLY);

	pos_x = 35.0f + 60.0f + 60.0f;

	std::shared_ptr<GUI> icon_2 = std::make_shared<GUIWithAlpha>((int)pos_x, (int)pos_y, (int)icon_size.x, (int)icon_size.y, Width(), Height());
	icon_2->SetRenderingFunc(GUI::RenderFunc::Default);
	icon_2->SetTransparent(false);
	const Texture* star_icon = GetTexture("star_icon");
	icon_2->SetTexture(*star_icon, { 0,0 }, { star_icon->m_width, star_icon->m_height });
	icon_2->SetTakeMouseEvents(true);
	icon_2->SetHoverCommand(std::make_shared<GUICommand>([icon_2]()
		{
			icon_2->SetRotationAngle(icon_2->GetRotationAngle() - 1.f);
		}));
	AddGUI(icon_2);
	this->AddInputObserver(icon_2.get(), MONOPOLY);

	//std::shared_ptr<GUI> test = std::make_shared<GUI>((int)pos_x, (int)pos_y + 100, 200, 400, Width(), Height());
	//test->SetRenderingFunc(GUI::RenderFunc::Gradient);
	//test->SetTexture(*star_icon, { 0,0 }, { star_icon->m_width, star_icon->m_height }); // any texture to have uv @todo
	//test->SetTransparent(true);
	//AddGUI(test);
	//this->AddInputObserver(test.get(), MONOPOLY);

	//Visualize OBBs
	auto objects = GetObjects();
	LineMesh* boxes_mesh = new LineMesh({}, {}, {});
	std::vector<glm::vec3> extrems_total;
	std::vector<unsigned int> indices_total;
	size_t i = 0;
	for (auto obj : objects)
	{
		if (InteractionScript* script = dynamic_cast<InteractionScript*>(obj->GetScript()); script)
		{ 
			dbb::OBB obb = script->GetInteractionVolume();
			if (obb.IsValid())
			{
				std::vector<dbb::point> extrems = obb.GetVertices();
				extrems_total.insert(extrems_total.end(), extrems.begin(), extrems.end());
				static const std::array<unsigned int, 24> boxEdges = obb.GetIndices();
				for (size_t j = 0; j < sizeof(boxEdges) / sizeof(boxEdges[0]); ++j)
					indices_total.push_back(i * 8 + boxEdges[j]);
				++i;
			}
		}
	}
	boxes_mesh->UpdateData(extrems_total, indices_total, { 0.0f, 0.0f, 1.0f, 1.0f });
	AddObject(factory.CreateObject(std::make_shared<SimpleModel>(boxes_mesh), eObject::RenderType::LINES, "OBB"));
}
