#include "stdafx.h"
#include "SandBoxGameScript.h"

#include <base/Object.h>
#include <game_assets/MainContextBase.h>
#include <game_assets/ObjectFactory.h>
#include <game_assets/ModelManagerYAML.h>
#include <opengl_assets/Texture.h>

//------------------------------------------------------------
SandBoxGameScript::SandBoxGameScript(eMainContextBase* _game)
: m_game(_game)
{
}

//------------------------------------------------------
void SandBoxGameScript::Update(float _tick)
{
	static bool first_call = true;
	if (first_call)
	{
		//Material
		Material material{ glm::vec3(0.8f, 0.0f, 0.0f), 0.5f , 0.5f }; // -> move to base

		//MapleLeaves
		ObjectFactoryBase factory;
		shObject leaves = factory.CreateObject(m_game->GetModelManager()->Find("MapleLeaves"), eObject::RenderType::LEAF, "Maple Leaves", "Default", "", ColliderType::BOX);
		leaves->GetTransform()->setTranslation(glm::vec3(-7.f, -2.0f, -7.50f));
		leaves->GetTransform()->setScale(glm::vec3(0.5, 0.5, 0.5));

		const_cast<I3DMesh*>(leaves->GetModel()->Get3DMeshes()[0])->SetMaterial(material);
		Texture leaf_texture;
		leaf_texture.loadTextureFromFile("../game_assets/Resources/MapleTree/maple_leaf.png");
		Texture mask_texture;
		mask_texture.loadTextureFromFile("../game_assets/Resources/MapleTree/maple_leaf_Mask.jpg");
		auto leaf_material = leaves->GetModel()->Get3DMeshes()[0]->GetMaterial().value();
		leaf_material.textures[Material::TextureType::ALBEDO] = leaf_texture.m_id;
		leaf_material.used_textures.insert(Material::TextureType::ALBEDO);
		leaf_material.textures[Material::TextureType::OPACITY] = mask_texture.m_id;
		leaf_material.used_textures.insert(Material::TextureType::OPACITY);
		leaf_material.emission_strength = 0.;
		leaf_material.roughness = 1.;
		leaf_material.metallic = 0.;
		const_cast<I3DMesh*>(leaves->GetModel()->Get3DMeshes()[0])->SetMaterial(leaf_material);

		leaves->SetBackfaceCull(false);
		m_game->AddObject(leaves);

		shObject tree = factory.CreateObject(m_game->GetModelManager()->Find("MapleTree"), eObject::RenderType::PBR, "MapleTree", "Default", "", ColliderType::BOX);
		tree->GetTransform()->setTranslation(glm::vec3(-7.f, -2.0f, -7.50f));
		tree->GetTransform()->setScale(glm::vec3(0.5, 0.5, 0.5));

		const_cast<I3DMesh*>(tree->GetModel()->Get3DMeshes()[0])->SetMaterial(material);
		Texture bark_texture;
		bark_texture.loadTextureFromFile("../game_assets/Resources/MapleTree/maple_bark.png");
		auto tree_material = tree->GetModel()->Get3DMeshes()[0]->GetMaterial().value();
		tree_material.textures[Material::TextureType::ALBEDO] = bark_texture.m_id;
		tree_material.used_textures.insert(Material::TextureType::ALBEDO);
		tree_material.emission_strength = 0.;
		tree_material.roughness = 1.;
		tree_material.metallic = 0.;
		tree_material.ao = 0.2f;
		const_cast<I3DMesh*>(tree->GetModel()->Get3DMeshes()[0])->SetMaterial(tree_material);

		tree->SetBackfaceCull(false);
		m_game->AddObject(tree);
	}
	first_call = false;
}
