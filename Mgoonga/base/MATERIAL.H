#pragma once

#include <glm\glm\glm.hpp>

#include <unordered_map>
#include <unordered_set>

//------------------------------------------------------------
struct Material
{
	glm::vec3 albedo = glm::vec3(1.0f);
	float metallic = 0.0f;
	float roughness = 0.5f;
	float ao = 1.0f;
	glm::vec3 emission_color = glm::vec3(0.0f);
	float emission_strength = 0.0f;
	float opacity = 1.0f;

	float transmission = 0.0f;
	float clearcoat = 0.0f;
	float clearcoat_roughness = 0.0f;
	glm::vec3 sheen_color = glm::vec3(0.0f);
	float sheen_roughness = 0.0f;
	float anisotropy = 0.0f;
	float anisotropy_rotation = 0.0f;
	float ior = 1.45f;

	enum class TextureType {
		ALBEDO, METALLIC, ROUGHNESS, NORMAL, AO, EMISSIVE, DISPLACEMENT,
		OPACITY, TRANSMISSION, CLEARCOAT, SHEEN_COLOR, IOR, ANISOTROPY, ATLAS_0
	};
	std::unordered_map<TextureType, uint32_t> textures;
	std::unordered_set<TextureType> used_textures;

	glm::vec2 uv_scale = glm::vec2(1.0f);
	glm::vec2 uv_offset = glm::vec2(0.0f);
	uint32_t uv_set_index = 0;

	enum class AlphaMode { MAT_OPAQUE, MAT_MASK, MAT_BLEND };
	AlphaMode alpha_mode = AlphaMode::MAT_OPAQUE;
	float alpha_cutoff = 0.5f;

	enum class ShadingModel { PBR, PHONG, UNLIT };
	ShadingModel shading_model = ShadingModel::PBR;
	bool double_sided = false;

	Material(glm::vec3 _albedo = glm::vec3(1.0f), float _metallic = 0.0f, float _roughness = 0.5f)
		: albedo(_albedo), metallic(_metallic), roughness(_roughness)
	{}
};