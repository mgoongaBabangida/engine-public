#include "stdafx.h"
#include <base/Log.h>

#include "TerrainGeneratorTool.h"
#include "ObjectFactory.h"
#include "MainContextBase.h"

#include <sdl_assets/ImGuiContext.h>
#include <opengl_assets/ModelManager.h>
#include <opengl_assets/TextureManager.h>
#include <opengl_assets/openglrenderpipeline.h>

#include <math/Random.h>
#include <math/Colliders.h>

#include "BezierCurveUIController.h"

float InverseLerp(float xx, float yy, float value)
{
	return (value - xx) / (yy - xx);
}

namespace
{
	// Raw world-aligned fBm for a single chunk.
	// - Writes raw (unnormalized) heights into outNoise
	// - Returns localMin/localMax of the raw sum
	//
	// NOTE: This is intentionally free of TerrainGeneratorTool state so that
	//       we can later call it from per-chunk worker threads.
	void GenerateWorldAlignedNoiseRaw(
		int width, int height,
		float scale, int octaves, float persistence, float lacunarity,
		glm::vec2 chunk_center, glm::vec2 chunk_scale_xz,
		float S0x, float S0z,
		glm::vec2 world_noise_offset, uint32_t seed,
		std::vector<float>& outNoise,
		float& outLocalMin,
		float& outLocalMax)
	{
		if (width <= 0 || height <= 0) {
			outNoise.clear();
			outLocalMin = 0.0f;
			outLocalMax = 0.0f;
			return;
		}

		if (scale <= 0.0f)     scale = 0.0001f;
		if (octaves <= 0)      octaves = 1;
		if (lacunarity < 1.0f) lacunarity = 1.0f;

		const size_t N = size_t(width) * size_t(height);
		outNoise.assign(N, 0.0f);

		// Local per-octave buffers (no shared state, thread-safe)
		std::vector<std::vector<float>> octavesBuf;
		octavesBuf.resize(size_t(octaves));
		for (auto& buf : octavesBuf)
			buf.assign(N, 0.0f);

		// Stable per-octave world offsets
		std::vector<glm::vec2> octave_offsets;
		octave_offsets.resize(static_cast<size_t>(octaves));
		for (int i = 0; i < octaves; ++i) {
			float ox = math::Random::RandomFloat(-10000.0f, 10000.0f, seed + uint32_t(i))
				+ world_noise_offset.x;
			float oz = math::Random::RandomFloat(-10000.0f, 10000.0f, seed + uint32_t(i) * 31u)
				+ world_noise_offset.y;
			octave_offsets[size_t(i)] = { ox, oz };
		}

		// Launch one async task per octave (unchanged behavior, but local data)
		std::vector<std::future<void>> tasks;
		tasks.reserve(size_t(octaves));

		float amplitude = 1.0f;
		float frequency = 1.0f;

		for (int octave = 0; octave < octaves; ++octave) {
			const float amp = amplitude;
			const float freq = frequency;
			glm::vec2 off = octave_offsets[size_t(octave)];

			tasks.emplace_back(std::async(
				std::launch::async,
				[&, octave, amp, freq, off]()
				{
					for (int y = 0; y < height; ++y) {
						const float v = (height > 1) ? float(y) / float(height - 1) : 0.0f;
						const float localZ = (v - 0.5f) * S0z;

						for (int x = 0; x < width; ++x) {
							const float u = (width > 1) ? float(x) / float(width - 1) : 0.0f;
							const float localX = (u - 0.5f) * S0x;

							// Local -> World (same as TES / FS)
							const float worldX = localX * chunk_scale_xz.x + chunk_center.x;
							const float worldZ = localZ * chunk_scale_xz.y + chunk_center.y;

							// World-space Perlin sampling for this octave
							const float sx = (worldX + off.x) / scale * 50.0f * freq; // 50 for backward compat
							const float sz = (worldZ + off.y) / scale * 50.0f * freq;

							const float p = glm::perlin(glm::vec2(sx, sz)) * 2.0f - 1.0f;
							octavesBuf[size_t(octave)][size_t(y) * size_t(width) + size_t(x)] = p * amp;
						}
					}
				}
			));

			amplitude *= persistence;
			frequency *= lacunarity;
		}

		for (auto& fut : tasks)
			fut.get();

		// Sum octaves and compute local min/max
		outLocalMin = std::numeric_limits<float>::max();
		outLocalMax = std::numeric_limits<float>::lowest();

		for (size_t i = 0; i < N; ++i) {
			float acc = 0.0f;
			for (int o = 0; o < octaves; ++o)
				acc += octavesBuf[size_t(o)][i];

			outNoise[i] = acc;
			if (acc < outLocalMin) outLocalMin = acc;
			if (acc > outLocalMax) outLocalMax = acc;
		}

		// Preserve your old “duplicate last row/col” fix (still in raw space)
		if (height >= 2) {
			for (int x = 0; x < width; ++x)
				outNoise[size_t(height - 1) * size_t(width) + size_t(x)] =
				outNoise[size_t(height - 2) * size_t(width) + size_t(x)];
		}
		if (width >= 2) {
			for (int y = 0; y < height; ++y)
				outNoise[size_t(y) * size_t(width) + size_t(width - 1)] =
				outNoise[size_t(y) * size_t(width) + size_t(width - 2)];
		}
	}
}

namespace {

	static inline glm::ivec3 AxialRound_ToCube(glm::vec2 qr) {
		float qf = qr.x, rf = qr.y, sf = -qf - rf;
		int qi = int(std::round(qf));
		int ri = int(std::round(rf));
		int si = int(std::round(sf));
		float dq = fabsf(qi - qf), dr = fabsf(ri - rf), ds = fabsf(si - sf);
		if (dq > dr && dq > ds)       qi = -ri - si;
		else if (dr > ds)             ri = -qi - si;
		else                          si = -qi - ri;
		return { qi, ri, si };
	}

	// === Biome shaping (base in [0,1] -> shaped [0,1]) ===
	enum class Biome : uint8_t { Plain, Hill, Mountain, Water };

	struct BiomeParams {
		float amp;    // amplitude multiplier
		float gamma;  // pow() curve ( <1 boosts peaks, >1 flattens )
		float sea;    // sea level clamp (only for Water)
	};

	static inline float ShapeHeight(float base, Biome b)
	{
		// Tunables (good starting points)
		switch (b) {
		case Biome::Mountain: {
			const BiomeParams P{ 1.00f, 0.65f, 0.0f };
			float h = powf(glm::clamp(base, 0.f, 1.f), P.gamma) * P.amp;
			// (optional) tiny ridge accent without new octaves:
			// h = glm::clamp(0.85f*h + 0.15f*(1.0f - fabsf(2.0f*base - 1.0f)), 0.0f, 1.0f);
			return glm::clamp(h, 0.0f, 1.0f);
		}
		case Biome::Hill: {
			const BiomeParams P{ 0.60f, 1.20f, 0.0f };
			float h = powf(glm::clamp(base, 0.f, 1.f), P.gamma) * P.amp;
			return glm::clamp(h, 0.0f, 1.0f);
		}
		case Biome::Plain: {
			const BiomeParams P{ 0.25f, 1.80f, 0.0f };
			float h = powf(glm::clamp(base, 0.f, 1.f), P.gamma) * P.amp;
			return glm::clamp(h, 0.0f, 1.0f);
		}
		case Biome::Water: {
			const BiomeParams P{ 0.15f, 2.00f, 0.30f }; // sea ~= 0.30
			float h = powf(glm::clamp(base, 0.f, 1.f), P.gamma) * P.amp;
			return glm::min(h, P.sea);
		}
		}
		return base;
	}

	//----------------------------------------------------------------------
	static inline extremDots ComputeChunkAABB_CPU(
		float S0x, float S0z,
		const glm::vec2& chunk_scale_xz,
		const glm::vec2& chunk_offset_xz,
		float min_height,         // TES uniform min_height
		float max_height,         // TES uniform max_height
		float heightMapResolution // TES uniform heightMapResolution (e.g. 1024)
	)
	{
		extremDots e;

		// --- 1) Match TES half-texel UV inset exactly ---
		// TES maps: local = (uv - 0.5) * S0
		// and clamps uv to [pad, 1-pad], pad = 0.5 / res
		const float texel = 1.0f / glm::max(1.0f, heightMapResolution);
		const float pad = 0.5f * texel; // half-texel

		const float localX_min = (-0.5f + pad) * S0x;
		const float localX_max = (0.5f - pad) * S0x;
		const float localZ_min = (-0.5f + pad) * S0z;
		const float localZ_max = (0.5f - pad) * S0z;

		// --- 2) Apply the same non-uniform scale + offset as TES ---
		const float worldX0 = localX_min * chunk_scale_xz.x + chunk_offset_xz.x;
		const float worldX1 = localX_max * chunk_scale_xz.x + chunk_offset_xz.x;
		const float worldZ0 = localZ_min * chunk_scale_xz.y + chunk_offset_xz.y;
		const float worldZ1 = localZ_max * chunk_scale_xz.y + chunk_offset_xz.y;

		e.MinX = glm::min(worldX0, worldX1);
		e.MaxX = glm::max(worldX0, worldX1);
		e.MinZ = glm::min(worldZ0, worldZ1);
		e.MaxZ = glm::max(worldZ0, worldZ1);

		// --- 3) Match TES vertical clamp exactly ---
		// TES: Height = texture(...) * height_scale; Height = clamp(Height, min_height, max_height); p.y += Height;
		// So final Y is strictly within [min_height, max_height].
		e.MinY = glm::min(min_height, max_height);
		e.MaxY = glm::max(min_height, max_height);

		// --- 4) Small conservative pad to kill borderline negatives due to FP drift ---
		const float padXZ = 0.0025f * glm::max(e.MaxX - e.MinX, e.MaxZ - e.MinZ) + 1e-5f;
		const float padY = 0.01f * (e.MaxY - e.MinY) + 1e-4f;

		e.MinX -= padXZ; e.MaxX += padXZ;
		e.MinZ -= padXZ; e.MaxZ += padXZ;
		e.MinY -= padY;  e.MaxY += padY;

		return e;
	}

	//---------------------------------------------------------------------
	glm::vec2 HalfCellNudge(int totalQ, int totalR, float R)
	{
		const float dq = (totalQ % 2 == 0) ? 0.5f : 0.0f;   // half a q-cell
		const float dr = (totalR % 2 == 0) ? 0.5f : 0.0f;   // half an r-cell

		const float X = 1.5f * R * dq;
		const float Z = sqrtf(3.0f) * R * (dr + 0.5f * dq);
		return { X, Z };
	}

}

//------------------------------------------------------------------------------------------------
TerrainGeneratorTool::TerrainGeneratorTool(eMainContextBase* _game,
																					 eModelManager* _modelManager,
																					 eTextureManager* _texManager,
																					 eOpenGlRenderPipeline& _pipeline,
																					 IWindowImGui* _imgui)
	: m_game(_game)
	, m_modelManager(_modelManager)
	, m_texture_manager(_texManager)
	, m_pipeline(_pipeline)
	, m_imgui(_imgui)
{
}

//------------------------------------------------------------------------------------------------
void TerrainGeneratorTool::Initialize()
{
	m_terrain_types.insert({ "water",		0.0f, 0.3f, {0.0f, 0.0f, 0.8f} }); //@todo design
	m_terrain_types.insert({ "grass",		0.3f,	0.5f, {0.0f, 1.0f, 0.0f} });
	m_terrain_types.insert({ "mounten", 0.5f, 0.8f, {0.5f, 0.5f, 0.0f} });
	m_terrain_types.insert({ "snow",		0.8f, 1.0f, {1.0f, 1.0f, 1.0f} });

	m_texture_scale[0] = 1.0f;
	m_texture_scale[1] = 1.5f;
	m_texture_scale[2] = 0.4f;
	m_texture_scale[3] = 0.3f;

	//@todo needs to be regenerated when resized
	m_falloff_map.resize(m_width * m_height);
	_GenerateFallOffMap();

	m_noise_map.resize(m_width * m_height);
	m_octaves_buffer.resize(m_octaves);
	for (auto& octave_buffer : m_octaves_buffer)
		octave_buffer.resize(m_width * m_height);

	_GenerateNoiseMap(m_width, m_height, m_scale, m_octaves, m_persistance, m_lacunarity, m_noise_offset, m_seed);
	m_noise_texture.TextureFromBuffer<GLfloat>(&m_noise_map[0], m_width, m_height, GL_RED);

	m_color_map.resize(m_width * m_height);
	_GenerateColorMap();
	m_color_texture.TextureFromBuffer<GLfloat>(&m_color_map[0].x, m_width, m_height, GL_RGBA);

	//TERRAIN
	std::unique_ptr<TerrainModel> terrainModel = m_modelManager->CloneTerrain("simple");

	//OBJECTS
	ObjectFactoryBase factory;
	m_terrain_pointer = terrainModel.get();

	m_terrain = factory.CreateObject(std::shared_ptr<IModel>(terrainModel.release()), eObject::RenderType::PHONG, "Terrain");
	m_terrain->SetCollider(new dbb::OBBCollider({}));
	m_terrain->SetName("Procedural Terrain");
	m_terrain->SetTextureBlending(true);
	m_terrain_pointer->setAlbedoTextureArray(m_texture_manager->Find("terrain_albedo_array_0"));
	m_terrain_pointer->setNormalTextureArray(m_texture_manager->Find("terrain_normal_array_0"));
	m_terrain_pointer->setMetallicTextureArray(m_texture_manager->Find("terrain_metallic_array_0"));
	m_terrain_pointer->setRoughnessTextureArray(m_texture_manager->Find("terrain_roughness_array_0"));
	m_terrain_pointer->setAOTextureArray(m_texture_manager->Find("terrain_ao_array_0"));
	m_terrain_pointer->SetTessellationInfoUpdater(m_pipeline.get().GetTessellationInfoUpdater());
	m_terrain->SetPickable(true);
	m_terrain_pointer->SetCamera(&m_game->GetMainCamera());
	m_game->MainCameraIndexChanged.Subscribe([this](size_t _index) { m_terrain_pointer->SetCamera(&m_game->GetCamera(_index)); });
	m_game->AddObject(m_terrain);

	_UpdateShaderUniforms();

	std::function<void()> switch_lod__callback = [this]()
	{
		for (auto& mesh : m_terrain_pointer->getMeshes())
		{
			if (mesh->LODInUse() == 1)
				mesh->SwitchLOD(2);
			else if (mesh->LODInUse() == 2)
				mesh->SwitchLOD(3);
			else
				mesh->SwitchLOD(1);
		}
	};

	std::function<void()> render_mode__callback = [this]()
	{
		for (auto& mesh : m_terrain_pointer->getMeshes())
		{
			if (mesh->GetRenderMode() == Brush::RenderMode::DEFAULT)
				mesh->SetRenderMode(Brush::RenderMode::WIREFRAME);
			else
				mesh->SetRenderMode(Brush::RenderMode::DEFAULT);
		}
	};

	std::function<void()> texturing__callback = [this]()
	{
		m_terrain->SetTextureBlending(!m_terrain->IsTextureBlending());
	};

	std::function<void()> tessellation__callback = [this]()
	{
		if (m_terrain->GetRenderType() != eObject::RenderType::TERRAIN_TESSELLATION)
		{
			EnableTesselation();
		}
		else
		{
			DisableTesselation();
		}
	};

	std::function<void()> albedo_texture__callback = [this]()
	{
		static bool lague = false;
		if (lague == true)
		{
			m_terrain_pointer->setAlbedoTextureArray(m_texture_manager->Find("terrain_albedo_array_0"));
			lague = false;
		}
		else
		{
			m_terrain_pointer->setAlbedoTextureArray(m_texture_manager->Find("terrain_albedo_array_lague"));
			lague = true;
		}
	};

	std::function<void()> pbr_renderer__callback = [this]()
	{
		static bool pbr_renderer = false;
		if (pbr_renderer == true)
		{
			m_pipeline.get().SetUniformData("class eTerrainTessellatedRender", "pbr_renderer", false);
			pbr_renderer = false;
		}
		else
		{
			m_pipeline.get().SetUniformData("class eTerrainTessellatedRender", "pbr_renderer", true);
			pbr_renderer = true;
		}
	};

	std::function<void()> update__callback = [this]()
	{
		m_update_textures = true;
	};

	std::function<void()> add_plane__callback = [this]()
	{
		m_update_textures = true;
		m_generate_plane = true;
	};

	// spin box callbacks
	static std::function<void(int, int*&)> posX__callback = [this](int _new_value, int*& _data)
	{
		static bool first_call = true;
		if (first_call)
		{
			*_data = m_cur_pos_X;
			first_call = false;
			return;
		}
			m_cur_pos_X = _new_value;
	};

	static std::function<void(int, int*&)> posY__callback = [this](int _new_value, int*& _data)
	{
		static bool first_call = true;
		if (first_call)
		{
			*_data = m_cur_pos_Y;
			first_call = false;
			return;
		}
			m_cur_pos_Y = _new_value;
	};

	static std::function<void(int, int*&)> offsetX__callback = [this](int _new_value, int*& _data)
	{
		static bool first_call = true;
		if (first_call)
		{
			*_data = m_noise_offset[0];
			first_call = false;
			return;
		}
			m_noise_offset[0] = _new_value;
	};

	static std::function<void(int, int*&)> offsetY__callback = [this](int _new_value, int*& _data)
	{
		static bool first_call = true;
		if (first_call)
		{
			*_data = m_noise_offset[1];
			first_call = false;
			return;
		}
			m_noise_offset[1] = _new_value;
	};

	static std::function<void(int, int*&)> noise_scale__callback = [this](int _new_value, int*& _data)
	{
		static bool first_call = true;
		if (first_call)
		{
			*_data = m_scale;
			first_call = false;
			return;
		}
			m_scale = _new_value;
	};

	static std::function<void(int, int*&)> snowness__callback = [this](int _new_value, int*& _data)
	{
		static bool first_call = true;
		if (first_call)
		{
			*_data = m_snowness;
			first_call = false;
			return;
		}
			m_snowness = _new_value;
	};

	static std::function<void(int, int*&)> normal_sharpness__callback = [this](int _new_value, int*& _data)
	{
		static bool first_call = true;
		if (first_call)
		{
			*_data = m_normal_sharpness;
			first_call = false;
			return;
		}
		m_normal_sharpness = _new_value;
	};

	m_imgui->Add(TEXTURE, "Noise texture", (void*)m_noise_texture.m_id);
	m_imgui->Add(SPIN_BOX, "Position X", (void*)&posX__callback);
	m_imgui->Add(SPIN_BOX, "Position Y", (void*)&posY__callback);
	m_imgui->Add(BUTTON, "Update", (void*)&update__callback);
	m_imgui->Add(BUTTON, "Add Plane", (void*)&add_plane__callback);
	m_imgui->Add(SLIDER_INT, "Noise width", &m_width);
	m_imgui->Add(SLIDER_INT, "Noise height", &m_height);
	m_imgui->Add(SPIN_BOX, "Scale", (void*)&noise_scale__callback);
	m_imgui->Add(SLIDER_INT_NERROW, "Octaves", &m_octaves);
	m_imgui->Add(SLIDER_FLOAT_SPIN, "Persistance", &m_persistance);
	m_imgui->Add(SLIDER_FLOAT_SPIN, "Lacunarity", &m_lacunarity);
	m_imgui->Add(SPIN_BOX, "Normal sharpness", (void*)&normal_sharpness__callback);
	m_imgui->Add(SPIN_BOX, "Offset X", (void*)&offsetX__callback);
	m_imgui->Add(SPIN_BOX, "Offset Y", (void*)&offsetY__callback);
	m_imgui->Add(SLIDER_INT, "Seed", &m_seed);
	m_imgui->Add(SLIDER_FLOAT, "Height Scale", &m_height_scale);
	m_imgui->Add(SLIDER_FLOAT, "Min height", &m_min_height);
	int counter = 0;
	for (const auto& type : m_terrain_types)
	{
		m_imgui->Add(SLIDER_FLOAT, "Start height "+ std::to_string(counter), (void*)&type.threshold_start);
		++counter;
	}
	m_imgui->Add(SLIDER_FLOAT, "Texture Scale 0", &m_texture_scale[0]);
	m_imgui->Add(SLIDER_FLOAT, "Texture Scale 1", &m_texture_scale[1]);
	m_imgui->Add(SLIDER_FLOAT, "Texture Scale 2", &m_texture_scale[2]);
	m_imgui->Add(SLIDER_FLOAT, "Texture Scale 3", &m_texture_scale[3]);
	m_imgui->Add(SPIN_BOX, "Snowness", (void*)&snowness__callback);
	m_imgui->Add(SLIDER_FLOAT, "Min Tes Distance", &m_min_tessellation_distance);
	m_imgui->Add(SLIDER_FLOAT, "Max Tes Distance", &m_max_tessellation_distance);
	m_imgui->Add(CHECKBOX, "Use Fall Off", (void*)&m_apply_falloff);
	//m_imgui->Add(SLIDER_FLOAT, "Adjust fallof A", (void*)&m_fall_off_a);
	//m_imgui->Add(SLIDER_FLOAT, "Adjust fallof B", (void*)&m_fall_off_b);
	m_imgui->Add(SLIDER_FLOAT_NERROW, "Adjust fallof k", (void*)&m_fall_off_k);
	m_imgui->Add(SLIDER_FLOAT_LARGE, "Adjust fallof T", (void*)&m_fall_off_T);
	m_imgui->Add(CHECKBOX, "Apply noise blur", (void*)&m_apply_blur);
	m_imgui->Add(SLIDER_FLOAT, "Blur Sigma", (void*)&m_blur_sigma);
	m_imgui->Add(SLIDER_INT, "Kernel size", &m_blur_kernel_size);
	m_imgui->Add(BUTTON, "Switch LOD", (void*)&switch_lod__callback);
	m_imgui->Add(BUTTON, "Render mode", (void*)&render_mode__callback);
	m_imgui->Add(BUTTON, "Texturing", (void*)&texturing__callback);
	m_imgui->Add(BUTTON, "Tessellation", (void*)&tessellation__callback);
	m_imgui->Add(BUTTON, "Albedo texture", (void*)&albedo_texture__callback);
	m_imgui->Add(BUTTON, "PBR Model", (void*)&pbr_renderer__callback);
	m_imgui->Add(CHECKBOX, "PBR use normal map", (void*)&m_use_normal_texture_pbr);
	m_imgui->Add(CHECKBOX, "PBR use roughness map", (void*)&m_use_roughness_texture_pbr);
	m_imgui->Add(CHECKBOX, "PBR use metalic map", (void*)&m_use_metalic_texture_pbr);
	m_imgui->Add(CHECKBOX, "PBR use ao map", (void*)&m_use_ao_texture_pbr);
	m_imgui->Add(TEXTURE, "Color texture", (void*)m_color_texture.m_id);
	m_imgui->Add(SLIDER_FLOAT_NERROW, "PBR normal map strength", (void*)&m_normal_mapping_strength);

	dbb::Bezier bezier;
	bezier.p0 = { -0.95f, -0.95f, 0.0f };
	bezier.p1 = { -0.45f, -0.33f, 0.0f };
	bezier.p2 = { 0.17f,  0.31f, 0.0f };
	bezier.p3 = { 0.98f,  0.95f, 0.0f };
	m_interpolation_curve = bezier;

	std::function<void()> create_bezier_callback = [this]()
	{
		ObjectFactoryBase factory;

		dbb::Bezier curve = m_interpolation_curve;
		// map window from NDC to screen restriction
		glm::vec2 x_restriction = { -0.88f, 0.95f }; // NDC space (y-inverted) of the screen where we can move objects with mouse
		glm::vec2 y_restriction = { -0.8f, 0.73f };
		curve.p0.x = dbb::MapValueLinear(m_interpolation_curve.p0.x, -1.f, 1.f, x_restriction.x, x_restriction.y);
		curve.p0.y = dbb::MapValueLinear(m_interpolation_curve.p0.y, -1.f, 1.f, y_restriction.x, y_restriction.y);
		curve.p1.x = dbb::MapValueLinear(m_interpolation_curve.p1.x, -1.f, 1.f, x_restriction.x, x_restriction.y);
		curve.p1.y = dbb::MapValueLinear(m_interpolation_curve.p1.y, -1.f, 1.f, y_restriction.x, y_restriction.y);
		curve.p2.x = dbb::MapValueLinear(m_interpolation_curve.p2.x, -1.f, 1.f, x_restriction.x, x_restriction.y);
		curve.p2.y = dbb::MapValueLinear(m_interpolation_curve.p2.y, -1.f, 1.f, y_restriction.x, y_restriction.y);
		curve.p3.x = dbb::MapValueLinear(m_interpolation_curve.p3.x, -1.f, 1.f, x_restriction.x, x_restriction.y);
		curve.p3.y = dbb::MapValueLinear(m_interpolation_curve.p3.y, -1.f, 1.f, y_restriction.x, y_restriction.y);

		shObject bezier_model = factory.CreateObject(std::make_shared<BezierCurveModel>(std::vector<BezierCurveMesh*>{new BezierCurveMesh(curve, /*2d*/true)}), eObject::RenderType::BEZIER_CURVE);
		m_game->AddObject(bezier_model);

		for (int i = 0; i < 4; ++i)
		{
			shObject pbr_sphere = factory.CreateObject(m_modelManager->Find("sphere_red"), eObject::RenderType::PBR, "SphereBezierPBR " + std::to_string(i));
			bezier_model->GetChildrenObjects().push_back(pbr_sphere);
			pbr_sphere->Set2DScreenSpace(true);
		}
		auto* script = new BezierCurveUIController(m_game, bezier_model, 0.02f, m_texture_manager->Find("pseudo_imgui"));
		script->ToolFinished.Subscribe([this](const dbb::Bezier& _bezier) { m_interpolation_curve = _bezier; Update(0); });
		bezier_model->SetScript(script);
	};

	m_imgui->Add(CHECKBOX, "Use Curve", (void*)&m_use_curve);
	m_imgui->Add(BUTTON, "Interpolation Curve", (void*)&create_bezier_callback);

	// river
	std::function<void()> add_river__callback = [this]()
	{
		ObjectFactoryBase factory;
		extremDots extrems = m_terrain_pointer->GetExtremDotsOfMeshes()[0];
		dbb::Bezier curve{ glm::vec3{extrems.MaxX, m_terrain_height, extrems.MaxZ},
											 glm::vec3{extrems.MinX + (extrems.MaxX- extrems.MinX) * 0.33f ,m_terrain_height, extrems.MinZ + (extrems.MaxZ - extrems.MinZ) * 0.33f},
											 glm::vec3{extrems.MinX + (extrems.MaxX - extrems.MinX) * 0.66f , m_terrain_height, extrems.MinZ + (extrems.MaxZ - extrems.MinZ) * 0.66f},
											 glm::vec3{extrems.MinX, m_terrain_height, extrems.MinZ } };
		shObject bezier_model = factory.CreateObject(std::make_shared<BezierCurveModel>(std::vector<BezierCurveMesh*>{new BezierCurveMesh(curve, /*2d*/false)}), eObject::RenderType::BEZIER_CURVE);
		m_game->AddObject(bezier_model);

		for (int i = 0; i < 4; ++i)
		{
			shObject pbr_sphere = factory.CreateObject(m_modelManager->Find("sphere_red"), eObject::RenderType::PBR, "SphereBezierPBR " + std::to_string(i));
			bezier_model->GetChildrenObjects().push_back(pbr_sphere);
		}
		auto* script = new BezierCurveUIController(m_game, bezier_model, 0.1f, nullptr, true);
		script->ToolFinished.Subscribe([this](const dbb::Bezier& _bezier) { m_river_curve = _bezier; Update(0); });
		script->CurveChanged.Subscribe([this](const dbb::Bezier& _bezier) { m_river_curve = _bezier; 	m_update_textures = true; m_river_info.m_update = true; Update(0); });
		bezier_model->SetScript(script);
		m_game->AddInputObserver(script, ALWAYS);
	};
	m_imgui->Add(BUTTON, "Add River", (void*)&add_river__callback);

	m_initialized = true;

	if (true)
	{
		_UpdateCurrentMesh();
		_CacheChunkStride();
		TessellationRenderingInfo info = _BuildInfo();
		m_terrain_pointer->SetTessellationInfo(glm::ivec2(m_cur_pos_X, m_cur_pos_Y), info);
	}
	else
	{
		m_auto_update = true;
		for (int x = -1; x <= 1; ++x)
			for (int y = -1; y <= 1; ++y)
		{
				m_cur_pos_X = x;
				m_cur_pos_Y = y;
				m_noise_offset.x = x * ((int)m_width);
				m_noise_offset.y = y * ((int)m_height);
				Update(0);
		}
		m_auto_update = false;
	}

	m_imgui->Add(TEXTURE, "Normal map", (void*)m_terrain_pointer->GetMaterial()->textures[Material::TextureType::NORMAL]);
	m_imgui->Add(CHECKBOX, "Patch", (void*)&m_patch);

	//_InitDebugMountain();
	_InitDebugHill();
}

//-----------------------------------------------------------------------------
void TerrainGeneratorTool::_InitDebugMountain()
{
	m_imgui->Add(SLIDER_FLOAT_NERROW, "Shoulder Start", (void*)&m_tunables.mountain.SHOULDER_START);
	m_imgui->Add(SLIDER_FLOAT, "Sharpness", (void*)&m_tunables.mountain.SHARPNESS);
	m_imgui->Add(SLIDER_FLOAT_NERROW, "round k", (void*)&m_tunables.mountain.ROUND_K);
	m_imgui->Add(SLIDER_FLOAT_NERROW, "circle radius fr", (void*)&m_tunables.mountain.CIRCLE_RADIUS_FR);

	// spin box callbacks
	static std::function<void(int, int*&)> JITTER_AMP_MIN__callback = [this](int _new_value, int*& _data)
	{
		static bool first_call = true;
		if (first_call)
		{
			*_data = m_tunables.mountain.JITTER_AMP_MIN * 100;
			first_call = false;
			return;
		}
		m_tunables.mountain.JITTER_AMP_MIN = float(_new_value) / 100.f;
	};

	static std::function<void(int, int*&)> JITTER_AMP_MAX__callback = [this](int _new_value, int*& _data)
	{
		static bool first_call = true;
		if (first_call)
		{
			*_data = m_tunables.mountain.JITTER_AMP_MAX * 100;
			first_call = false;
			return;
		}
		m_tunables.mountain.JITTER_AMP_MAX = float(_new_value)/ 100.f;
	};

	m_imgui->Add(SPIN_BOX, "JITTER_AMP_MIN", (void*)&JITTER_AMP_MIN__callback);
	m_imgui->Add(SPIN_BOX, "JITTER_AMP_MAX", (void*)&JITTER_AMP_MAX__callback);

	m_imgui->Add(SLIDER_FLOAT, "lobes mon", (void*)&m_tunables.mountain.LOBES_MIN);
	m_imgui->Add(SLIDER_FLOAT, "lobes max", (void*)&m_tunables.mountain.LOBES_MAX);

	static std::function<void(int, int*&)> MARGIN_callback = [this](int _new_value, int*& _data)
	{
		static bool first_call = true;
		if (first_call)
		{
			*_data = m_tunables.mountain.MARGIN * 100;
			first_call = false;
			return;
		}
		m_tunables.mountain.MARGIN = float(_new_value) / 100.f;
	};

	static std::function<void(int, int*&)> LIFT_CENTER__callback = [this](int _new_value, int*& _data)
	{
		static bool first_call = true;
		if (first_call)
		{
			*_data = m_tunables.mountain.LIFT_CENTER * 100;
			first_call = false;
			return;
		}
		m_tunables.mountain.LIFT_CENTER = float(_new_value) / 100.f;
	};
	m_imgui->Add(SPIN_BOX, "MARGIN", (void*)&MARGIN_callback);
	m_imgui->Add(SPIN_BOX, "LIFT_CENTER", (void*)&LIFT_CENTER__callback);
	m_imgui->Add(SLIDER_FLOAT, "lift border", (void*)&m_tunables.mountain.LIFT_BORDER);

	static std::function<void(int, int*&)> WARP_AMP_FRR_callback = [this](int _new_value, int*& _data)
	{
		static bool first_call = true;
		if (first_call)
		{
			*_data = m_tunables.mountain.WARP_AMP_FRR * 100;
			first_call = false;
			return;
		}
		m_tunables.mountain.WARP_AMP_FRR = float(_new_value) / 100.f;
	};
	m_imgui->Add(SPIN_BOX, "WARP_AMP_FRR", (void*)&WARP_AMP_FRR_callback);
	m_imgui->Add(SLIDER_FLOAT_NERROW, "WARP_FREQ_OVER_R", (void*)&m_tunables.mountain.WARP_FREQ_OVER_R);

	//meso octavs
	m_imgui->Add(SLIDER_FLOAT, "MESO_FREQ_OVER_R", (void*)&m_tunables.mountain.MESO_FREQ_OVER_R);
	static std::function<void(int, int*&)> MESO_OCTAVES_callback = [this](int _new_value, int*& _data)
	{
		static bool first_call = true;
		if (first_call)
		{
			*_data = m_tunables.mountain.MESO_OCTAVES;
			first_call = false;
			return;
		}
		m_tunables.mountain.MESO_OCTAVES = _new_value;
	};
	m_imgui->Add(SPIN_BOX, "MESO_OCTAVES", (void*)&MESO_OCTAVES_callback);
	m_imgui->Add(SLIDER_FLOAT_NERROW, "MESO_AMP", (void*)&m_tunables.mountain.MESO_AMP);

	m_imgui->Add(SLIDER_FLOAT, "MICRO_FREQ_OVER_R", (void*)&m_tunables.mountain.MICRO_FREQ_OVER_R);
	static std::function<void(int, int*&)> MICRO_OCTAVES_callback = [this](int _new_value, int*& _data)
	{
		static bool first_call = true;
		if (first_call)
		{
			*_data = m_tunables.mountain.MICRO_OCTAVES;
			first_call = false;
			return;
		}
		m_tunables.mountain.MICRO_OCTAVES = _new_value;
	};
	m_imgui->Add(SPIN_BOX, "MICRO_OCTAVES", (void*)&MICRO_OCTAVES_callback);
	m_imgui->Add(SLIDER_FLOAT_NERROW, "MICRO_AMP", (void*)&m_tunables.mountain.MICRO_AMP);
	m_imgui->Add(SLIDER_FLOAT, "LACUNARITY", (void*)&m_tunables.mountain.LACUNARITY);

	m_imgui->Add(SLIDER_FLOAT, "GAIN", (void*)&m_tunables.mountain.GAIN);
	m_imgui->Add(SLIDER_FLOAT, "SPIKE_POWER", (void*)&m_tunables.mountain.SPIKE_POWER);
	m_imgui->Add(SLIDER_FLOAT_NERROW, "SPIKE_AMP", (void*)&m_tunables.mountain.SPIKE_AMP);
	m_imgui->Add(SLIDER_FLOAT, "HIGH_GAIN_CENTER", (void*)&m_tunables.mountain.HIGH_GAIN_CENTER);
	m_imgui->Add(SLIDER_FLOAT, "HIGH_GAIN_BORDER", (void*)&m_tunables.mountain.HIGH_GAIN_BORDER);

	m_imgui->Add(SLIDER_FLOAT, "FOOTPRINT_FR", (void*)&m_tunables.mountain.FOOTPRINT_FR);
}

//-----------------------------------------------------------------------------
void TerrainGeneratorTool::_InitDebugHill()
{
	m_imgui->Add(SLIDER_FLOAT_NERROW, "H_HEIGHT_SCALE", (void*)&m_tunables.hill.H_HEIGHT_SCALE);
	m_imgui->Add(SLIDER_FLOAT_NERROW, "H_BLEND_START", (void*)&m_tunables.hill.H_BLEND_START);
	m_imgui->Add(SLIDER_FLOAT_NERROW, "H_BLEND_END", (void*)&m_tunables.hill.H_BLEND_END);

	m_imgui->Add(SLIDER_FLOAT_NERROW, "H_HILL_RADIUS_FR", (void*)&m_tunables.hill.H_HILL_RADIUS_FR);
	m_imgui->Add(SLIDER_FLOAT_NERROW, "H_SHOULDER_START", (void*)&m_tunables.hill.H_SHOULDER_START);
	m_imgui->Add(SLIDER_FLOAT, "H_SHARPNESS", (void*)&m_tunables.hill.H_SHARPNESS);

	m_imgui->Add(SLIDER_FLOAT_NERROW, "H_LIFT_CENTER_BASE", (void*)&m_tunables.hill.H_LIFT_CENTER_BASE);
	m_imgui->Add(SLIDER_FLOAT_NERROW, "H_LIFT_CENTER_GAIN", (void*)&m_tunables.hill.H_LIFT_CENTER_GAIN);
	m_imgui->Add(SLIDER_FLOAT_NERROW, "H_TOP_SUPPRESS", (void*)&m_tunables.hill.H_TOP_SUPPRESS);
	m_imgui->Add(SLIDER_FLOAT_NERROW, "H_DOME_AMP", (void*)&m_tunables.hill.H_DOME_AMP);
	m_imgui->Add(SLIDER_FLOAT_NERROW, "H_DOME_POW", (void*)&m_tunables.hill.H_DOME_POW);
	
	m_imgui->Add(SLIDER_FLOAT, "taper_exp", (void*)&m_tunables.hill.taper_exp);

	m_imgui->Add(SLIDER_FLOAT_NERROW, "H_PEAK_OFFSET_MIN_FR", (void*)&m_tunables.hill.H_PEAK_OFFSET_MIN_FR);
	m_imgui->Add(SLIDER_FLOAT_NERROW, "H_PEAK_OFFSET_MAX_FR", (void*)&m_tunables.hill.H_PEAK_OFFSET_MAX_FR);
	m_imgui->Add(SLIDER_FLOAT, "H_PEAK_POW", (void*)&m_tunables.hill.H_PEAK_POW);
	
	m_imgui->Add(SLIDER_FLOAT_NERROW, "H_BASEMENT_PREBLEND_START", (void*)&m_tunables.hill.H_BASEMENT_PREBLEND_START);
	m_imgui->Add(SLIDER_FLOAT_NERROW, "H_BASEMENT_PREBLEND_STRENGTH", (void*)&m_tunables.hill.H_BASEMENT_PREBLEND_STRENGTH);
	m_imgui->Add(SLIDER_FLOAT, "H_BASEMENT_PREBLEND_EXP", (void*)&m_tunables.hill.H_BASEMENT_PREBLEND_EXP);
}

//-----------------------------------------------------------------------------
struct ChunkJob
{
	int chunkX;
	int chunkZ;
	std::vector<float> noise;
	float localMin = 0.0f;
	float localMax = 0.0f;
};

//-----------------------------------------------------------------------------
TerrainModel* TerrainGeneratorTool::BuildFromSnapshot(
	const logic::WorldSnapshot& ws,
	const terrain::ChunkConfig& cfg,
	bool patch)
{
	if (!m_initialized || !m_terrain_pointer) return nullptr;

	// --- Chunk layout (12x12 flat-top) ---
	const int Nx = cfg.tilesPerChunkX;
	const int Ny = cfg.tilesPerChunkZ;

	// Local (pre-scale) rect the tess patch expects
	const float S0x = 6.3f;	// should be aligned with TerrainMesh::MakePlaneVerts
	const float S0z = 6.3f;

	// Hex radius picked so one chunk spans exactly Nx tiles in X
	const float R = S0x / (1.5f * Nx + 0.5f);
	math::Hex::SetOuterRadius(R);

	// Whole-world axial size (Q×R = width×height in hexes)
	const int Q_world = ws.width;   // 96
	const int R_world = ws.height;  // 64

	// How many chunks cover the world
	const int chunksX = (Q_world + Nx - 1) / Nx;   // ceil
	const int chunksY = (R_world + Ny - 1) / Ny;   // ceil

	// Center the entire world at (0,0) in world XZ space.
	auto MapCenter_Flat = [&](int Q, int Rhex)->glm::vec2 {
		const float Xc = 0.75f * R * float(Q - 1);
		const float Zc = 0.5f * std::sqrt(3.0f) * R *
			(float(Rhex - 1) + 0.5f * float(Q - 1));
		return { Xc, Zc };
		};

	glm::vec2 worldCenterShift = -MapCenter_Flat(Q_world, R_world);
	worldCenterShift -= HalfCellNudge(Q_world, R_world, R);

	printf("BuildFromSnapshot: world q=[%d..%d] (size=%d), r=[%d..%d] (size=%d)\n",
		-Q_world / 2, Q_world / 2 - 1, Q_world,
		-R_world / 2, R_world / 2 - 1, R_world);
	printf("BuildFromSnapshot: worldCenterShift=(%.3f, %.3f)\n",
		worldCenterShift.x, worldCenterShift.y);

	// Per-chunk scale is constant for all chunks with this Nx×Ny + (S0x,S0z)
	const glm::vec2 chunkScale = terrain::ChunkScaleFromS0(Nx, Ny, R, S0x, S0z);
	printf("BuildFromSnapshot: chunkScale=(%.4f, %.4f)\n",
		chunkScale.x, chunkScale.y);

	// Tessellation/material base info
	TessellationRenderingInfo baseInfo;
	baseInfo.min_height = m_min_height;
	baseInfo.max_height = m_height_scale * m_max_height_coef;
	baseInfo.height_scale = m_height_scale;

	baseInfo.base_start_heights.clear();
	for (const auto& t : m_terrain_types)
		baseInfo.base_start_heights.push_back(t.threshold_start);

	baseInfo.texture_scale.clear();
	for (int i = 0; i < 4; ++i)
		baseInfo.texture_scale.push_back(m_texture_scale[i]);

	// --- Per-chunk job description ---
	struct Job
	{
		glm::ivec2         chunkIdx{};
		glm::vec2          centerWorld{};
		glm::vec2          chunkScale{};
		std::vector<float> noise;      // raw -> normalized -> biome-shaped
		float              localMin{ 0.0f };
		float              localMax{ 0.0f };
	};

	std::vector<Job> jobs;
	jobs.reserve(chunksX * chunksY);

	// Build job list
	for (int cy = 0; cy < chunksY; ++cy)
	{
		for (int cx = 0; cx < chunksX; ++cx)
		{
			Job job;
			job.chunkIdx = { cx, cy };

			const glm::vec2 centerBase =
				terrain::ChunkCenter_Flat(Nx, Ny, R, job.chunkIdx);

			job.centerWorld = centerBase + worldCenterShift;
			job.chunkScale = chunkScale;

			jobs.push_back(std::move(job));
		}
	}

	if (jobs.empty())
		return m_terrain_pointer;

	// Ensure we recompute global noise normalization
	m_has_global_range = false;

	// -------------------------
	// Phase A: parallel raw noise generation
	// -------------------------
	{
		std::vector<std::thread> workers;
		workers.reserve(jobs.size());

		for (Job& job : jobs)
		{
			workers.emplace_back(
				[this, &job, S0x, S0z, chunkScale]()
				{
					_GenerateNoiseMap_WorldAlignedJob(
						/*heightmap size*/ m_width, m_height,
						m_scale, m_octaves, m_persistance, m_lacunarity,
						/*chunk center*/ job.centerWorld,
						/*chunk scale */ chunkScale,
						/*S0*/ S0x, S0z,
						/*world_noise_offset*/ glm::vec2(0.0f),
						/*seed*/ m_seed,
						/*out*/ job.noise,
						job.localMin,
						job.localMax
					);
				});
		}

		for (auto& t : workers)
			t.join();
	}

	// -------------------------
	// Compute global min/max
	// -------------------------
	float globalMin = jobs[0].localMin;
	float globalMax = jobs[0].localMax;

	for (const Job& job : jobs)
	{
		globalMin = std::min(globalMin, job.localMin);
		globalMax = std::max(globalMax, job.localMax);
	}

	const float span = std::max(1e-6f, globalMax - globalMin);
	const float pad = span * m_global_margin_frac;

	m_global_min_raw = globalMin - pad;
	m_global_max_raw = globalMax + pad;
	m_has_global_range = true;

	const float gSpan = std::max(1e-6f, m_global_max_raw - m_global_min_raw);

	// -------------------------
	// Phase B: parallel normalize + biome painting
	// -------------------------
	if (patch)
	{
		std::vector<std::thread> workers;
		workers.reserve(jobs.size());

		for (Job& job : jobs)
		{
			workers.emplace_back(
				[this, &job, &ws, Nx, Ny, R, S0x, S0z, gSpan]()
				{
					// Normalize using global range
					for (float& h : job.noise)
					{
						float v = (h - m_global_min_raw) / gSpan;
						h = glm::clamp(v, 0.0f, 1.0f);
					}

					// Apply biomes on this job's local heightmap
					_ApplySnapshotBiomesToHeightmap(
						ws, Nx, Ny, job.chunkIdx,
						R, S0x, S0z,
						/*scale*/  job.chunkScale,
						/*offset*/ job.centerWorld,
						/*feather_world*/ 0.0f,
						/*heightmap*/ job.noise
					);
				});
		}

		for (auto& t : workers)
			t.join();
	}
	else
	{
		// No biomes: just parallel normalization
		std::vector<std::thread> workers;
		workers.reserve(jobs.size());

		for (Job& job : jobs)
		{
			workers.emplace_back(
				[this, &job, gSpan]()
				{
					for (float& h : job.noise)
					{
						float v = (h - m_global_min_raw) / gSpan;
						h = glm::clamp(v, 0.0f, 1.0f);
					}
				});
		}

		for (auto& t : workers)
			t.join();
	}

	// -------------------------
	// Phase C: main-thread GL upload + terrain chunks
	// -------------------------
	for (Job& job : jobs)
	{
		const int cx = job.chunkIdx.x;
		const int cy = job.chunkIdx.y;

		const glm::vec2 centerBase =
			terrain::ChunkCenter_Flat(Nx, Ny, R, job.chunkIdx);

		printf("Chunk grid (%d,%d): centerBase=(%.3f,%.3f) centerWorld=(%.3f,%.3f)\n",
			cx, cy, centerBase.x, centerBase.y,
			job.centerWorld.x, job.centerWorld.y);

		// Copy job.noise into m_noise_map so all existing code still uses it
		m_noise_map = job.noise;

		// Upload height & color
		if (!glIsTexture(m_noise_texture.m_id)) {
			fprintf(stderr, "[HeightTex] id=%u is NOT a texture. Creating now...\n",
				m_noise_texture.m_id);
		}

		m_noise_texture.TextureFromBuffer<GLfloat>(
			m_noise_map.data(),
			m_width,
			m_height,
			GL_RED);

		_GenerateColorMap();

		// Per-chunk uniforms (scale + placement)
		TessellationRenderingInfo info = baseInfo;
		info.chunk_scale_xz = job.chunkScale;
		info.chunk_offset_xz = job.centerWorld;

		// Add/Update this chunk
		m_terrain_pointer->AddOrUpdate(
			/*_pos*/                 job.chunkIdx,
			/*per-mesh info*/        info,
			/*diffuse*/              &m_color_texture,
			/*height*/               &m_noise_texture,
			/*spread tex*/           true,
			/*normal sharpness*/     m_normal_sharpness,
			/*apply_normal_blur*/    false,
			/*tessellation_coef*/    16,
			/*cpu_lods*/             1
		);

		// Compute world-space AABB
		extremDots aabb = ComputeChunkAABB_CPU(
			S0x, S0z,
			info.chunk_scale_xz,
			info.chunk_offset_xz,
			info.min_height,
			info.max_height,
			float(m_noise_texture.m_width)
		);

		m_terrain_pointer->SetAABBtoChunk(aabb, job.chunkIdx);
	}

	// TES derivative step: pass local patch width
	m_pipeline.get().SetUniformData(
		"class eTerrainTessellatedRender", "terrainSizeXZ", S0x);
	m_pipeline.get().SetUniformData(
		"class eTerrainTessellatedRender", "heightMapResolution",
		float(m_noise_texture.m_width));

	return m_terrain_pointer;
}

//---------------------------------------------------------------------------
void TerrainGeneratorTool::EnableTesselation()
{
	m_terrain_pointer->EnableTessellation(true);
	m_terrain->SetRenderType(eObject::RenderType::TERRAIN_TESSELLATION);
}

//----------------------------------------------------------------------------
void TerrainGeneratorTool::DisableTesselation()
{
	m_terrain_pointer->EnableTessellation(false);
	m_terrain->SetRenderType(eObject::RenderType::PHONG);
}

//--------------------------------------------------------------------------
void TerrainGeneratorTool::SetBPRRenderer(bool _pbr)
{
	m_pipeline.get().SetUniformData("class eTerrainTessellatedRender", "pbr_renderer", _pbr);
}

//--------------------------------------------------------------------------
void TerrainGeneratorTool::SetNormalMapping(bool _normalmapp)
{
	m_use_normal_texture_pbr = _normalmapp;
}

//-----------------------------------------------------------------------------
TerrainGeneratorTool::~TerrainGeneratorTool()
{
	m_color_texture.freeTexture();
	m_noise_texture.freeTexture();
}

//-----------------------------------------------------------------------------
void TerrainGeneratorTool::Update(float _tick)
{
	if (m_initialized)
	{
		static int last_scale = m_scale;
		static float last_persistance = m_persistance;
		static float last_lacunarirty = m_lacunarity;
		static glm::ivec2 noise_offset = m_noise_offset;
		static GLuint octaves = m_octaves;
		static GLuint seed = m_seed;
		static float last_height_scale = m_height_scale;
		static float last_min_height = m_min_height;
		static bool use_falloff = false;
		static bool use_curve = false;

		static float last_fall_off_a = m_fall_off_a;
		static float last_fall_off_b = m_fall_off_b;
		static float last_fall_off_k = m_fall_off_k;
		static float last_fall_off_T = m_fall_off_T;

		static float last_sigma = m_blur_sigma;
		static float last_min_tes_dist = m_min_tessellation_distance;
		static float last_max_tes_dist = m_max_tessellation_distance;
		static int32_t last_normal_sharpness = m_normal_sharpness;

		static float last_texture_scales0 = m_texture_scale[0];
		static float last_texture_scales1 = m_texture_scale[1];
		static float last_texture_scales2 = m_texture_scale[2];
		static float last_texture_scales3 = m_texture_scale[3];

		static float last_finish_height0 = m_terrain_types.find({ "water",		0.0f, 0.3f, {0.0f, 0.0f, 0.8f} })->threshold_start; //@todo find with numbers bad design
		static float last_finish_height1 = m_terrain_types.find({ "grass",		0.3f,	0.5f, {0.0f, 1.0f, 0.0f} })->threshold_start;
		static float last_finish_height2 = m_terrain_types.find({ "mounten", 0.5f, 0.8f, {0.5f, 0.5f, 0.0f} })->threshold_start;
		static float last_finish_height3 = m_terrain_types.find({ "snow",		0.8f, 1.0f, {1.0f, 1.0f, 1.0f} })->threshold_start;
		static int last_snowness = 0;

		m_pipeline.get().SetUniformData("class eTerrainTessellatedRender", "use_normal_texture_pbr", m_use_normal_texture_pbr);
		m_pipeline.get().SetUniformData("class eTerrainTessellatedRender", "use_roughness_texture_pbr", m_use_roughness_texture_pbr);
		m_pipeline.get().SetUniformData("class eTerrainTessellatedRender", "use_metalic_texture_pbr", m_use_metalic_texture_pbr);
		m_pipeline.get().SetUniformData("class eTerrainTessellatedRender", "use_ao_texture_pbr", m_use_ao_texture_pbr);
		m_pipeline.get().SetUniformData("class eTerrainTessellatedRender", "normal_detail_strength", m_normal_mapping_strength);

		auto water = std::find_if(m_terrain_types.begin(), m_terrain_types.end(), [](const TerrainType& _other) { return _other.name == "water"; });
		auto grass = std::find_if(m_terrain_types.begin(), m_terrain_types.end(), [](const TerrainType& _other) { return _other.name == "grass"; });
		auto mounten = std::find_if(m_terrain_types.begin(), m_terrain_types.end(), [](const TerrainType& _other) { return _other.name == "mounten"; });
		auto snow = std::find_if(m_terrain_types.begin(), m_terrain_types.end(), [](const TerrainType& _other) { return _other.name == "snow"; });

		//@todo  check if start is higher then finish and roll back if it is

		if (water == m_terrain_types.end()
			|| grass == m_terrain_types.end()
			|| mounten == m_terrain_types.end()
			|| snow == m_terrain_types.end())
			return;

		if (last_texture_scales0 != m_texture_scale[0] ||
				last_texture_scales1 != m_texture_scale[1] ||
				last_texture_scales2 != m_texture_scale[2] ||
				last_texture_scales3 != m_texture_scale[3] ||
				last_height_scale		 != m_height_scale		 ||
				last_min_height			 != m_min_height			 ||
				last_min_tes_dist != m_min_tessellation_distance ||
				last_max_tes_dist != m_max_tessellation_distance ||
				last_finish_height0	 != water->threshold_start ||
				last_finish_height1	 != grass->threshold_start ||
				last_finish_height2	 != mounten->threshold_start ||
				last_finish_height3	 != snow->threshold_start ||
			  last_snowness != m_snowness
			)
		{
			_UpdateShaderUniforms();
			TessellationRenderingInfo info = _BuildInfo();
			m_terrain_pointer->SetTessellationInfo(glm::ivec2(m_cur_pos_X, m_cur_pos_Y), info);
		}

		if (m_update_textures)
		{
			if (m_noise_texture.m_width != m_width ||
				  m_noise_texture.m_height != m_height ||
				  last_scale != m_scale ||
				  last_persistance != m_persistance ||
				  last_lacunarirty != m_lacunarity ||
				  noise_offset != m_noise_offset ||
				  octaves != m_octaves ||
				  seed != m_seed ||
				  last_height_scale != m_height_scale ||
				  use_falloff != m_apply_falloff ||
				  use_curve != m_use_curve ||
				  last_fall_off_a != m_fall_off_a ||
				  last_fall_off_b != m_fall_off_b ||
					last_fall_off_k != m_fall_off_k ||
					last_fall_off_T != m_fall_off_T ||
				  last_sigma != m_blur_sigma ||
				  last_normal_sharpness != m_normal_sharpness ||
				  m_generate_plane ||
				  m_river_info.m_update)
			{
				m_noise_map.resize(m_width * m_height);
				m_octaves_buffer.resize(m_octaves);
				for (auto& octave_buffer : m_octaves_buffer)
					octave_buffer.resize(m_width * m_height);

				if (m_fall_off_k != m_fall_off_k || m_fall_off_T != m_fall_off_T)
					_GenerateFallOffMapWithFunction();

				if (m_generate_plane)
					_GeneratePlaneNoiseMap();
				else if (m_river_info.m_update)
					_UpdateRiverNoiseMap();
				else
				_GenerateNoiseMap(m_width, m_height, m_scale, m_octaves, m_persistance, m_lacunarity, m_noise_offset, m_seed);

				m_generate_plane = false;

				//update noise texture
				m_noise_texture.m_width = m_width;
				m_noise_texture.m_height = m_height;
				m_noise_texture.m_channels = 1;
				glBindTexture(GL_TEXTURE_2D, m_noise_texture.m_id);
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, GL_RED, GL_FLOAT, &m_noise_map[0]);
				glBindTexture(GL_TEXTURE_2D, 0);

				//m_noise_texture.saveToFile("noise_terrain.jpg", GL_TEXTURE_2D, GL_RED, GL_FLOAT);
				//update color texture

				_GenerateColorMap();
				m_color_texture.TextureFromBuffer<GLfloat>(&m_color_map[0].x, m_width, m_height, GL_RGBA);

				_AddCurrentMesh();

				last_scale = m_scale;
				last_persistance = m_persistance;
				last_lacunarirty = m_lacunarity;
				noise_offset = m_noise_offset;
				octaves = m_octaves;
				seed = m_seed;
				last_min_tes_dist = m_min_tessellation_distance;
				last_max_tes_dist = m_max_tessellation_distance;
				use_falloff = m_apply_falloff;
				use_curve = m_use_curve;
				last_fall_off_a = m_fall_off_a;
				last_fall_off_b = m_fall_off_b;
				last_fall_off_k = m_fall_off_k;
				last_fall_off_T = m_fall_off_T;
				last_sigma = m_blur_sigma;
				last_normal_sharpness = m_normal_sharpness;
				use_curve = m_use_curve;

				m_update_textures = false;
			}
		}

		last_height_scale = m_height_scale;
		last_min_height = m_min_height;
		last_texture_scales0 = m_texture_scale[0];
		last_texture_scales1 = m_texture_scale[1];
		last_texture_scales2 = m_texture_scale[2];
		last_texture_scales3 = m_texture_scale[3];
		last_finish_height0 = water->threshold_start;
		last_finish_height1 = grass->threshold_start;
		last_finish_height2 = mounten->threshold_start;
		last_finish_height3 = snow->threshold_start;
		last_snowness = m_snowness;
	}
}

//-----------------------------------------------------------------------------
void TerrainGeneratorTool::_GenerateNoiseMap(GLuint _width, GLuint _height, float _scale, GLuint _octaves,
																						 float _persistance, float _lacunarity, glm::vec2 _offset, GLuint _seed)
{
	float maxPossibleHeight = 0;

	std::vector<glm::vec2> octaveOffsets(_octaves);
	float gamplitude = 1.f;
	for (uint32_t i = 0; i < _octaves; ++i)
	{
		float offsetX = math::Random::RandomFloat(-10'000.0f, 10'000.0f, _seed + i) + _offset.x;
		float offsetY = math::Random::RandomFloat(-10'000.0f, 10'000.0f, _seed + i) + _offset.y;
		octaveOffsets[i] = { offsetX , offsetY };

		maxPossibleHeight += gamplitude;
		gamplitude *= _persistance;
	}

	if (_scale <= 0.0f)
		_scale = 0.0001f;
	if (_width < 1)
		_width = 1;
	if (_height < 1)
		_height = 1;
	if (_lacunarity < 1.0f)
		_lacunarity = 1.0f;
	if (_octaves < 1)
		_octaves = 1;

	// for seemless tiles need the same max min height interpretation
	/*static*/ float minHeight = 100'000.0f; // float max
	/*static*/ float maxHeight = -100'000.0f; // float min
	static bool update_max_heights = true;

	// launch master thread not to block the main thread
	/*std::function<bool()> main_func = [this, _octaves, _persistance, _lacunarity, _height, _width, _scale, 
																		 octaveOffsets]()->bool
	{*/
		float halfWidth = _width / 2;
		float halfHeight = _height / 2;

		float amplitude = 1.0f;
		float frequency = 1.0f;

		std::vector<std::future<bool>> m_tasks;
		// launch async task for every octave
		for (uint32_t octave = 0; octave < _octaves; ++octave)
		{
			std::function<bool()> func = [this, amplitude, frequency, halfWidth, halfHeight,
				octave, _height, _width, _scale, octaveOffsets]()->bool
			{
				for (uint32_t row = 0; row < _height; ++row)
				{
					for (uint32_t col = 0; col < _width; ++col)
					{
						float sampleX = (col - halfWidth + octaveOffsets[octave].x) / _scale * frequency;
						float sampleY = (row - halfHeight + octaveOffsets[octave].y) / _scale * frequency;
						float perlinValue = glm::perlin(glm::vec2{ sampleX, sampleY }) * 2.0f - 1.0f;
						m_octaves_buffer[octave][row * _width + col] = perlinValue * amplitude;
					}
				}
				return true;
			};
			m_tasks.emplace_back(std::async(func));
			amplitude *= _persistance;
			frequency *= _lacunarity;
		}

		//wait for the tasks
		for (auto& fut : m_tasks)
		{
			fut.get();
		}

		//sum up all the octaves
		for (uint32_t i = 0; i < _octaves; ++i)
		{
			for (uint32_t row = 0; row < _height; ++row)
				for (uint32_t col = 0; col < _width; ++col)
				{
					if(i!=0)
						m_noise_map[row * _width + col] += m_octaves_buffer[i][row * _width + col];
					else
						m_noise_map[row * _width + col] = m_octaves_buffer[i][row * _width + col];
				}
		}

		if (update_max_heights)
		{
			for (uint32_t i = 0; i < m_noise_map.size(); ++i)
			{
				if (m_noise_map[i] > maxHeight)
					maxHeight = m_noise_map[i];
				if (m_noise_map[i] < minHeight)
					minHeight = m_noise_map[i];
			}
		}

		dbb::Bezier interpolation_curve_normalized; // ( +1) /2 from -1 1 to 0 1 (bezier 2d tool returns bezier in -1 to 1 space) -> we need 0 to 1
		interpolation_curve_normalized.p0 = { (m_interpolation_curve.p0.x + 1.0f) / 2.0f, (m_interpolation_curve.p0.y + 1.0f) / 2.0f, 0.0f };
		interpolation_curve_normalized.p1 = { (m_interpolation_curve.p1.x + 1.0f) / 2.0f, (m_interpolation_curve.p1.y + 1.0f) / 2.0f, 0.0f };
		interpolation_curve_normalized.p2 = { (m_interpolation_curve.p2.x + 1.0f) / 2.0f, (m_interpolation_curve.p2.y + 1.0f) / 2.0f, 0.0f };
		interpolation_curve_normalized.p3 = { (m_interpolation_curve.p3.x + 1.0f) / 2.0f, (m_interpolation_curve.p3.y + 1.0f) / 2.0f, 0.0f };
		
		//noise values should be normalized
		for (auto& val : m_noise_map)
		{
			val = InverseLerp(maxHeight, minHeight, val); //noise local space - > [0,1] space
			if (m_use_curve)
			{
				val = dbb::GetPoint(interpolation_curve_normalized, val).y; // apply bezier curve to correct heights
			}
		}

		//blur the noise
		if (m_apply_blur)
			_ApplyGaussianBlur();

		if (m_apply_falloff)
		{
			for (int i = 0; i < m_noise_map.size(); ++i)
				m_noise_map[i] = glm::clamp(m_noise_map[i] * m_falloff_map[i], 0.f, 1.f); // * or -  !!!
		}

		//update_max_heights = false;
		std::cout <<"Noise Space: " << "Min noise height= " << minHeight << " Max noise height= " << maxHeight << std::endl;
	/*	return true;
	};

	m_generat_noise_task = std::async(main_func);*/
}

//-------------------------------------------------------------------------------
void TerrainGeneratorTool::_GenerateNoiseMap_WorldAligned(
	int width, int height,
	float scale, int octaves, float persistence, float lacunarity,
	glm::vec2 chunk_center, glm::vec2 chunk_scale_xz,
	float S0x, float S0z,
	glm::vec2 world_noise_offset, uint32_t seed)
{
	if (width == 0 || height == 0) return;
	if (scale <= 0.0f)     scale = 0.0001f;
	if (octaves == 0)      octaves = 1;
	if (lacunarity < 1.0f) lacunarity = 1.0f;

	const size_t N = size_t(width) * size_t(height);

	// 1) Raw fBm for this chunk (no normalization yet)
	float localMin = 0.0f;
	float localMax = 0.0f;

	GenerateWorldAlignedNoiseRaw(
		width, height,
		scale, octaves, persistence, lacunarity,
		chunk_center, chunk_scale_xz,
		S0x, S0z,
		world_noise_offset, seed,
		m_noise_map,
		localMin, localMax);

	if (m_noise_map.size() != N) {
		// Just in case; should never happen
		m_noise_map.resize(N, 0.0f);
	}

	// 2) Initialize global range once (first chunk) – same logic as before
	if (!m_has_global_range) {
		float span = std::max(1e-6f, localMax - localMin);
		float pad = span * m_global_margin_frac; // e.g. 0.80f => ±10% margin
		m_global_min_raw = localMin - pad;
		m_global_max_raw = localMax + pad;
		m_has_global_range = true;

		// Optional log:
		// std::cout << "[NoiseCalib] local=[" << localMin << "," << localMax
		//           << "] -> global=[" << m_global_min_raw << "," << m_global_max_raw << "]\n";
	}

	// 3) Normalize to [0,1] using the *global* range (avoid seams)
	float gSpan = std::max(1e-6f, m_global_max_raw - m_global_min_raw);
	for (size_t i = 0; i < N; ++i) {
		float h = (m_noise_map[i] - m_global_min_raw) / gSpan;
		m_noise_map[i] = glm::clamp(h, 0.0f, 1.0f);
	}
}

//----------------------------------------------------------------
void TerrainGeneratorTool::_GenerateNoiseMap_WorldAlignedJob(
	int width, int height,
	float scale, int octaves, float persistence, float lacunarity,
	glm::vec2 chunk_center, glm::vec2 chunk_scale_xz,
	float S0x, float S0z,
	glm::vec2 world_noise_offset, uint32_t seed,
	std::vector<float>& out_noise,
	float& out_local_min,
	float& out_local_max)
{
	if (width == 0 || height == 0) {
		out_noise.clear();
		out_local_min = out_local_max = 0.0f;
		return;
	}
	if (scale <= 0.0f)     scale = 0.0001f;
	if (octaves == 0)      octaves = 1;
	if (lacunarity < 1.0f) lacunarity = 1.0f;

	const size_t N = size_t(width) * size_t(height);
	out_noise.assign(N, 0.0f);

	float localMin = 0.0f;
	float localMax = 0.0f;

	GenerateWorldAlignedNoiseRaw(
		width, height,
		scale, octaves, persistence, lacunarity,
		chunk_center, chunk_scale_xz,
		S0x, S0z,
		world_noise_offset, seed,
		out_noise,
		localMin, localMax);

	out_local_min = localMin;
	out_local_max = localMax;
}

//-----------------------------------------------------------------------------
void TerrainGeneratorTool::_GeneratePlaneNoiseMap()
{
	for (auto& val : m_noise_map)
	{
		val = m_terrain_height;
	}
}

//-----------------------------------------------------------------------------
void TerrainGeneratorTool::_UpdateRiverNoiseMap()
{
	extremDots extrems = m_terrain_pointer->GetExtremDotsOfMeshes()[0]; //@todo cur
	for (unsigned int index = 0;  index < m_width * m_height; ++index)
	{
		// Compute grid indices from 1D index
		int i = index / m_width;
		int j = index % m_height;
		float z = extrems.MinX + (extrems.MaxX - extrems.MinX) * ((float)i / (float)m_width);
		float x = extrems.MinZ + (extrems.MaxZ - extrems.MinZ) * ((float)j / (float)m_height);

		glm::vec3 pos(x, m_terrain_height , z); // y = base height
		float distance_to_bezier = glm::length(pos - dbb::ClosestPointOnBezier(m_river_curve, pos, 0.f, 1.f, 40));
		if (distance_to_bezier < m_river_info.m_radius)
		{
			float value = pow(dbb::MapValueLinear(distance_to_bezier, 0, m_river_info.m_radius, 0.f, 1.f), 2); // map to 0-1 and func
			m_noise_map[index] = dbb::MapValueLinear(value, 0, 1, 0, m_terrain_height); // map to terrain height
		}
		else
		{
			m_noise_map[index] = m_terrain_height;
		}
	}
	m_river_info.m_update = false;
}

//-----------------------------------------------------------------------------
void TerrainGeneratorTool::_GenerateColorMap()
{
	m_color_map.resize(m_noise_map.size());
	for (uint32_t row = 0; row < m_height; ++row)
	{
		for (uint32_t col = 0; col < m_width; ++col)
		{
			float height = m_noise_map[row * m_width + col];
			for (const auto& type : m_terrain_types)
			{
				if (height <= type.threshold_finish) // @todo should check if it corelates with start
				{
					m_color_map[row * m_width + col] = glm::vec4{ type.color, 1.0f };
					break;
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
void TerrainGeneratorTool::_UpdateCurrentMesh()
{
	if (m_terrain_pointer)
	{
		Texture blue = Texture::GetTexture1x1(BLUE);
		m_terrain_pointer->Initialize(&m_color_texture,
																	&m_color_texture,
																	&blue,
																	&m_noise_texture,
																	true,
																	m_height_scale,
																	m_height_scale * m_max_height_coef,
																	m_min_height,
																	m_normal_sharpness);
		//BuildFromSnapshot(logic::WorldSnapshot{}, terrain::ChunkConfig{}, m_patch);
	}
}

//-----------------------------------------------------------------------------
void TerrainGeneratorTool::_AddCurrentMesh()
{
	if (m_terrain_pointer)
	{
		TessellationRenderingInfo info = _BuildInfo();
		m_terrain_pointer->AddOrUpdate(glm::ivec2(m_cur_pos_X, m_cur_pos_Y),
																	 info,
																	 &m_color_texture,
																	 &m_noise_texture,
																	 true,
																	 m_normal_sharpness);
		//BuildFromSnapshot(logic::WorldSnapshot{}, terrain::ChunkConfig{}, m_patch);
		m_imgui->Add(TEXTURE, "Normal map", (void*)m_terrain_pointer->GetMaterial()->textures[Material::TextureType::NORMAL]);
	}
}

//-----------------------------------------------------------------------------
void TerrainGeneratorTool::_GenerateFallOffMap()
{
	for (uint32_t row = 0; row < m_height; ++row)
	{
		for (uint32_t col = 0; col < m_width; ++col)
		{
			float x = row / (float)m_height * 2 - 1;
			float y = col / (float)m_width * 2 - 1;
			float value = glm::max(glm::abs(x), glm::abs(y));

			// function adjustment
			value = glm::pow(value, m_fall_off_a) /
							(glm::pow(value, m_fall_off_a) + glm::pow(m_fall_off_b - m_fall_off_b * value, m_fall_off_a) );
			m_falloff_map[row * m_width + col] = value;
		}
	}
}

//----------------------------------------------------------
void TerrainGeneratorTool::_GenerateFallOffMapWithFunction()
{
	for (uint32_t row = 0; row < m_height; ++row)
	{
		for (uint32_t col = 0; col < m_width; ++col)
		{
			// Compute distance from the nearest edge
			float d_x = glm::min(float(row), float(m_height - row));
			float d_y = glm::min(float(col), float(m_width - col));
			float d = glm::min(d_x, d_y);
			// Sigmoid
			float k = 0.1f;
			auto falloffFactor = [this](float d, float T) -> float { return 1.0f / (1.0f + exp(-m_fall_off_k * (d - T))); };

			// Apply falloff factor
			float factor = glm::clamp(falloffFactor(d, m_fall_off_T), 0.0f, 1.0f);
			m_falloff_map[row * m_width + col] = factor;
		}
	}
}

//----------------------------------------------------------
void TerrainGeneratorTool::_UpdateShaderUniforms()
{
	int counter = 0;
	for (const auto& type : m_terrain_types) {
		m_pipeline.get().SetUniformData("class ePhongRender",
			"base_start_heights[" + std::to_string(counter) + "]", type.threshold_start);
		m_pipeline.get().SetUniformData("class ePhongRender",
			"textureScale[" + std::to_string(counter) + "]", m_texture_scale[counter]);
		++counter;
	}
	m_pipeline.get().SetUniformData("class ePhongRender", "base_start_heights[" + std::to_string(counter) + "]", 1.0f);

	// Tess renderer
	counter = 0;
	for (const auto& type : m_terrain_types) {
		m_pipeline.get().SetUniformData("class eTerrainTessellatedRender",
			"base_start_heights[" + std::to_string(counter) + "]", type.threshold_start);
		m_pipeline.get().SetUniformData("class eTerrainTessellatedRender",
			"textureScale[" + std::to_string(counter) + "]", m_texture_scale[counter]);
		++counter;
	}
	m_pipeline.get().SetUniformData("class eTerrainTessellatedRender", "base_start_heights[" + std::to_string(counter) + "]", 1.0f);

	// Shared scalars
	const float maxH = m_height_scale * m_max_height_coef;
	m_pipeline.get().SetUniformData("class ePhongRender", "min_height", m_min_height);
	m_pipeline.get().SetUniformData("class ePhongRender", "max_height", maxH);
	m_pipeline.get().SetUniformData("class ePhongRender", "height_scale", m_height_scale);

	m_pipeline.get().SetUniformData("class eTerrainTessellatedRender", "min_height", m_min_height);
	m_pipeline.get().SetUniformData("class eTerrainTessellatedRender", "max_height", maxH);      // <-- fixed
	m_pipeline.get().SetUniformData("class eTerrainTessellatedRender", "height_scale", m_height_scale);

	m_pipeline.get().SetUniformData("class eTerrainTessellatedRender", "min_distance", m_min_tessellation_distance);
	m_pipeline.get().SetUniformData("class eTerrainTessellatedRender", "max_distance", m_max_tessellation_distance);

	// snow controls
	m_pipeline.get().SetUniformData("class eTerrainTessellatedRender", "snowness", float(m_snowness) / 100.f);
	m_pipeline.get().SetUniformData("class eTerrainTessellatedRender", "color_count", int(m_terrain_types.size()));
	const int snow_idx = int(std::distance(m_terrain_types.begin(),
		std::find_if(m_terrain_types.begin(), m_terrain_types.end(),
			[](const TerrainType& t) { return t.name == "snow"; })));
	m_pipeline.get().SetUniformData("class eTerrainTessellatedRender", "snow_color", snow_idx);
}

//------------------------------------------------------------------------
void TerrainGeneratorTool::_ApplyGaussianBlur()
{
	// Define Gaussian kernel size (e.g., 5x5)

	// Calculate half kernel size
	int32_t halfKernel = m_blur_kernel_size / 2;

	// Generate Gaussian kernel
	std::vector<float> kernel(size_t(m_blur_kernel_size * m_blur_kernel_size));
	float sum = 0.0f;
	for (int32_t i = -halfKernel; i <= halfKernel; ++i)
	{
		for (int32_t j = -halfKernel; j <= halfKernel; ++j)
		{
			int index = (i + halfKernel) * m_blur_kernel_size + (j + halfKernel);
			float weight = exp(-(i * i + j * j) / (2 * m_blur_sigma * m_blur_sigma));
			if (index < kernel.size())
			{
				kernel[index] = weight;
				sum += weight;
			}
		}
	}

	// Normalize the kernel
	for (int i = 0; i < m_blur_kernel_size * m_blur_kernel_size; ++i) {
		kernel[i] /= sum;
	}

	// Apply the Gaussian blur
	std::vector<float> blurredHeightmap(m_width * m_height);
	for (int y = 0; y < m_height; ++y)
	{
		for (int x = 0; x < m_width; ++x)
		{
			float sum = 0.0f;
			for (int ky = -halfKernel; ky <= halfKernel; ++ky)
			{
				for (int kx = -halfKernel; kx <= halfKernel; ++kx)
				{
					int offsetX = x + kx;
					int offsetY = y + ky;
					if (offsetX >= 0 && offsetX < m_width && offsetY >= 0 && offsetY < m_height)
					{
						int index = offsetY * m_width + offsetX;
						int kernelIndex = (ky + halfKernel) * m_blur_kernel_size + (kx + halfKernel);
						sum += m_noise_map[index] * kernel[kernelIndex];
					}
				}
			}
			int index = y * m_width + x;
			blurredHeightmap[index] = sum;
		}
	}

	// Copy the blurred heightmap back to the original heightmap
	std::swap(blurredHeightmap, m_noise_map);
	blurredHeightmap.clear();
	kernel.clear();
}

//-----------------------------------------------------------------------------------
TessellationRenderingInfo TerrainGeneratorTool::_BuildInfo() const
{
	TessellationRenderingInfo info;
	// ranges
	info.min_height = m_min_height;
	info.max_height = m_height_scale * m_max_height_coef; // <-- consistent everywhere
	info.height_scale = m_height_scale;

	// layers
	for (const auto& type : m_terrain_types) info.base_start_heights.push_back(type.threshold_start);
	for (auto s : m_texture_scale)            info.texture_scale.push_back(s);
	info.color_count = (int)info.base_start_heights.size();

	// snow – if you always keep it as last layer, use that index; otherwise search by name
	auto snowIt = std::find_if(m_terrain_types.begin(), m_terrain_types.end(),
		[](const TerrainType& t) { return t.name == "snow"; });
	info.snow_color = snowIt == m_terrain_types.end() ? -1
		: int(std::distance(m_terrain_types.begin(), snowIt));
	info.snowness = m_snowness / 100.0f;

	// PBR toggles (mirrors the pipeline globals you already set)
	info.pbr_renderer = true; // or your UI toggle
	info.use_normal_texture_pbr = m_use_normal_texture_pbr;
	info.use_roughness_texture_pbr = m_use_roughness_texture_pbr;
	info.use_metalic_texture_pbr = m_use_metalic_texture_pbr;
	info.use_ao_texture_pbr = m_use_ao_texture_pbr;
	info.normal_mapping_strength = m_normal_mapping_strength;
	
	info.gamma_correction = true; // your default
	info.normal_detail_strength = 0.6f; // tweak in UI if desired
	info.normal_y_flip = false;

	// tess distance
	info.tess_min_distance = m_min_tessellation_distance;
	info.tess_max_distance = m_max_tessellation_distance;

	// resolution (TES uses this)
	info.heightmap_resolution = glm::vec2(float(m_width), float(m_height));

	// world_offset is provided by the mesh each draw (C), so we don’t set it here.
	return info;
}

//-----------------------------------------------------------------
void TerrainGeneratorTool::_CacheChunkStride()
{
	auto all = m_terrain_pointer->GetExtremDotsOfMeshes();
	if (all.empty()) return;
	extremDots e = all[0];
	m_chunkStrideX = e.MaxX - e.MinX; // ? (rows-1)/devisor
	m_chunkStrideZ = e.MaxZ - e.MinZ; // ? (cols-1)/devisor
}



// ---- Small utilities (keep static inline in cpp) ----
static inline glm::vec2 WorldFromTexel(
	int x, int y, int W, int H,
	float S0x, float S0z,
	glm::vec2 scale, glm::vec2 offset)
{
	const float u = (W > 1) ? float(x) / float(W - 1) : 0.0f;
	const float v = (H > 1) ? float(y) / float(H - 1) : 0.0f;
	const float localX = (u - 0.5f) * S0x;
	const float localZ = (v - 0.5f) * S0z;
	return { localX * scale.x + offset.x,
					 localZ * scale.y + offset.y };
}

static inline glm::vec2 AxialFromWorld_Flat(float x, float z, float R) {
	const float invR = 1.0f / R;
	float qf = (2.0f / 3.0f) * x * invR;
	float rf = (z / (std::sqrt(3.0f) * R)) - (x * invR * (1.0f / 3.0f));
	return { qf, rf };
}
static inline glm::ivec3 CubeRound(glm::vec2 qr) {
	float qf = qr.x, rf = qr.y, sf = -qf - rf;
	int qi = int(std::round(qf));
	int ri = int(std::round(rf));
	int si = int(std::round(sf));
	float dq = std::fabs(qi - qf), dr = std::fabs(ri - rf), ds = std::fabs(si - sf);
	if (dq > dr && dq > ds)       qi = -ri - si;
	else if (dr > ds)             ri = -qi - si;
	else                          si = -qi - ri;
	return { qi, ri, si }; // q=c.x, r=c.y
}
static inline glm::vec2 AxialToWorld_Flat(int q, int r, float R) {
	float X = 1.5f * R * float(q);
	float Z = std::sqrt(3.0f) * R * (float(r) + 0.5f * float(q));
	return { X, Z };
}

// Simple hash for per-hex jitter
static inline float Hash01(int a, int b) {
	float h = std::sin(glm::dot(glm::vec2(float(a), float(b)), glm::vec2(12.9898f, 78.233f))) * 43758.5453f;
	return glm::fract(h);
}

// Ridged fBm in [0..1]
static inline float FBM_Ridged01(glm::vec2 p, int oct, float lac, float gain) {
	float a = 1.0f, sum = 0.0f, norm = 0.0f;
	for (int i = 0; i < oct; ++i) {
		float n = 1.0f - std::fabs(glm::perlin(p));
		n *= n;
		sum += n * a;
		norm += a;
		p *= lac;
		a *= gain;
	}
	return (norm > 0.0f) ? glm::clamp(sum / norm, 0.0f, 1.0f) : 0.0f;
}

static inline glm::vec2 Warp2(glm::vec2 p, float freq, float amp) {
	float wx = glm::perlin(p * freq + glm::vec2(5.2f, 1.3f));
	float wz = glm::perlin(p * freq + glm::vec2(-3.7f, 9.1f));
	return glm::vec2(wx, wz) * amp;
}

// Detail-preserving raise
static inline float RaisePreserve(float base, float env, float k) {
	float beta = 1.0f + env * k;
	return 1.0f - std::pow(1.0f - base, beta);
}

// Low band 3x3 with normalized weights near borders
static inline float LowBand3x3(const std::vector<float>& img, int W, int H, int x, int y) {
	auto Sample = [&](int ix, int iy)->float {
		ix = std::max(0, std::min(W - 1, ix));
		iy = std::max(0, std::min(H - 1, iy));
		return img[size_t(iy) * size_t(W) + size_t(ix)];
	};
	const int   dx[9] = { -1,0,1,-1,0,1,-1,0,1 };
	const int   dy[9] = { -1,-1,-1, 0,0,0, 1,1,1 };
	const float w[9] = { 1,2,1,  2,4,2,  1,2,1 };
	float s = 0.f, ws = 0.f;
	for (int i = 0; i < 9; ++i) { s += Sample(x + dx[i], y + dy[i]) * w[i]; ws += w[i]; }
	return (ws > 0.f) ? (s / ws) : Sample(x, y);
}

// Envelope with circle/hex blend + per-hex jitter
struct EnvelopeOut {
	float env;        // 0..1
	float t_blend;    // blended metric (0 center .. 1 border)
};

//------------------------------------------------------------------------
void TerrainGeneratorTool::_ApplySnapshotBiomesToHeightmap(
	const logic::WorldSnapshot& ws,
	/* chunk layout */ int Nx, int Ny, glm::ivec2 chunkIdx,
	/* hex size    */  float R,
	/* local patch */  float S0x, float S0z,
	/* affine      */  glm::vec2 chunk_scale_xz, glm::vec2 chunk_offset_xz,
	/* soft edges  */  float, /*feather_world_unused*/
	std::vector<float>& heightmap
)
{
	// Master switch for all debug logging/footprints in this function.
	constexpr bool kEnableBiomeDebug = false;

	// =================== DEBUG STRUCTS ======================
	struct DebugStats
	{
		int qMin = 1000000;
		int qMax = -1000000;
		int rMin = 1000000;
		int rMax = -1000000;

		int totalTexels = 0;
		int ownedTexels = 0; // chunkOwnsHex == true
		int unownedTexels = 0; // chunkOwnsHex == false

		int countPlain = 0;
		int countHill = 0;
		int countMountain = 0;
		int countWater = 0;
	};

	struct DebugHexFootprint
	{
		bool used = false;
		int  minX = 1000000;
		int  maxX = -1000000;
		int  minY = 1000000;
		int  maxY = -1000000;
		int  texelCount = 0;
	};

	// One set of debug structs per process; only used when kEnableBiomeDebug==true
	static DebugStats        dbg;
	static DebugHexFootprint dbgHex;
	static bool              s_traceOneTexel = true;
	static bool              s_dumped = false;
	static bool              s_sanityOnce = true;

	// Pick some hex to inspect in detail (adjust to any (q,r) you like)
	static const int DBG_Q0 = -47;
	static const int DBG_R0 = -31;

	if (heightmap.empty() || m_width == 0 || m_height == 0) return;

	// Keep hex math in sync with the rest of the engine
	math::Hex::SetOuterRadius(R);

	MountainParams mountain = m_tunables.mountain;
	HillParams     hill = m_tunables.hill;
	PlainParams    plain = m_tunables.plain;
	WaterParams    water = m_tunables.water;

	auto fbm_ridged01 = [&](glm::vec2 p, int oct, float lac, float gain)->float {
		float a = 1.0f, sum = 0.0f, norm = 0.0f;
		for (int i = 0; i < oct; ++i) {
			float n = 1.0f - std::fabs(glm::perlin(p));
			n *= n;
			sum += n * a;
			norm += a;
			p *= lac;
			a *= gain;
		}
		return (norm > 0.0f) ? glm::clamp(sum / norm, 0.0f, 1.0f) : 0.0f;
		};

	auto warp2 = [&](glm::vec2 p, float freq, float amp)->glm::vec2 {
		float wx = glm::perlin(p * freq + glm::vec2(5.2f, 1.3f));
		float wz = glm::perlin(p * freq + glm::vec2(-3.7f, 9.1f));
		return glm::vec2(wx, wz) * amp;
		};

	auto raise_preserve = [](float base, float env, float k)->float {
		float beta = 1.0f + env * k;
		return 1.0f - std::pow(1.0f - base, beta);
		};

	// --- Single source of truth: analytic inverse matching axial_to_world_flat ---
	struct HexSample {
		glm::ivec3 cube;  // rounded cube coords
		glm::vec2  qr_f;  // fractional axial (q_f, r_f)
	};

	auto hex_from_world = [&](float wx, float wz) -> HexSample
		{
			// Invert:
			//   X = 1.5 * R * q
			//   Z = sqrt(3) * R * (r + 0.5 * q)
			//
			// => q_f = (2/3) * X / R
			//    r_f = Z/(sqrt(3)*R) - X/(3*R)

			const float sqrt3 = 1.7320508075688772f;
			const float invR = 1.0f / R;

			const float q_f = (2.0f / 3.0f) * wx * invR;
			const float r_f = (wz / (sqrt3 * R)) - (wx / (3.0f * R));

			// axial fractional (q_f, r_f) → cube fractional (x_f, y_f, z_f)
			float x_f = q_f;
			float z_f = r_f;
			float y_f = -x_f - z_f;

			// Standard cube rounding
			int rx = static_cast<int>(std::round(x_f));
			int ry = static_cast<int>(std::round(y_f));
			int rz = static_cast<int>(std::round(z_f));

			float dx = std::fabs(rx - x_f);
			float dy = std::fabs(ry - y_f);
			float dz = std::fabs(rz - z_f);

			if (dx > dy && dx > dz) {
				rx = -ry - rz;
			}
			else if (dy > dz) {
				ry = -rx - rz;
			}
			else {
				rz = -rx - ry;
			}

			glm::ivec3 cube(rx, ry, rz);
			glm::vec2  qr_f_out(q_f, r_f);

			return { cube, qr_f_out };
		};

	// Axial→world helper (used inside biome shaping & chunkOwnsHex)
	auto axial_to_world_flat = [&](int q, int r)->glm::vec2 {
		float X = 1.5f * R * float(q);
		float Z = std::sqrt(3.0f) * R * (float(r) + 0.5f * float(q));
		return { X, Z };
		};

	// -------- Pre-copy base map for band-split --------
	std::vector<float> base0 = heightmap;

	auto fetch = [&](int ix, int iy)->float {
		ix = std::max(0, int(std::min<uint32_t>(ix, int(m_width) - 1)));
		iy = std::max(0, int(std::min<uint32_t>(iy, int(m_height) - 1)));
		return base0[size_t(iy) * size_t(m_width) + size_t(ix)];
		};

	auto low_band = [&](int x, int y)->float {
		const int   dx[9] = { -1,0,1,-1,0,1,-1,0,1 };
		const int   dy[9] = { -1,-1,-1, 0,0,0, 1,1,1 };
		const float w[9] = { 1,2,1,  2,4,2,  1,2,1 };
		float s = 0.0f, ws = 0.0f;
		for (int i = 0; i < 9; ++i) {
			s += fetch(x + dx[i], y + dy[i]) * w[i];
			ws += w[i];
		}
		return (ws > 0.0f) ? (s / ws) : fetch(x, y);
		};

	auto hash01 = [](int a, int b)->float {
		float h = std::sin(glm::dot(glm::vec2(float(a), float(b)),
			glm::vec2(12.9898f, 78.233f))) * 43758.5453f;
		return glm::fract(h);
		};

	auto tileBiome = [&](const glm::ivec3& cube) -> Biome {
		if (auto* t = logic::tryGetTileAtCube(ws, cube)) {
			switch (t->vBiome) {
			case logic::VerticalBiome::Mountains: return Biome::Mountain;
			case logic::VerticalBiome::Hills:     return Biome::Hill;
			case logic::VerticalBiome::Water:
			{
				if (t->water != logic::WaterBody::OpenSea) //@todo stub for rivers
					return Biome::Plain;
				return Biome::Water; }
			default:                              return Biome::Plain;
			}
		}
		return Biome::Plain;
		};

	// Chunk indices from generator
	const int qMin = chunkIdx.x * Nx;
	const int qMax = qMin + Nx - 1;
	const int rMin = chunkIdx.y * Ny;
	const int rMax = rMin + Ny - 1;

	// --- world-space rect of this chunk, matching world_from_texel ---
	const float chunkMinX = chunk_offset_xz.x - 0.5f * S0x * chunk_scale_xz.x;
	const float chunkMaxX = chunk_offset_xz.x + 0.5f * S0x * chunk_scale_xz.x;
	const float chunkMinZ = chunk_offset_xz.y - 0.5f * S0z * chunk_scale_xz.y;
	const float chunkMaxZ = chunk_offset_xz.y + 0.5f * S0z * chunk_scale_xz.y;

	auto chunkOwnsHex = [&](int q, int r) -> bool
		{
			glm::vec2 C = axial_to_world_flat(q, r);

			const float eps = 1e-3f;

			if (C.x < chunkMinX - eps) return false;
			if (C.x >= chunkMaxX + eps) return false;
			if (C.y < chunkMinZ - eps) return false;
			if (C.y >= chunkMaxZ + eps) return false;

			return true;
		};

	// Axial neighbors (q,r) – flat-top layout
	static const int AQ[6] = { 0, +1, +1,  0, -1, -1 };
	static const int AR[6] = { -1, -1,  0, +1, +1,  0 };

	auto world_from_texel = [&](int x, int y) -> glm::vec2
		{
			// Center-of-texel convention: x in [0..m_width-1] → u in (0,1)
			const float u = (m_width > 0) ? ((float(x) + 0.5f) / float(m_width)) : 0.0f;
			const float v = (m_height > 0) ? ((float(y) + 0.5f) / float(m_height)) : 0.0f;

			const float localX = (u - 0.5f) * S0x; // [-S0x/2, +S0x/2]
			const float localZ = (v - 0.5f) * S0z;

			return {
					localX * chunk_scale_xz.x + chunk_offset_xz.x,
					localZ * chunk_scale_xz.y + chunk_offset_xz.y
			};
		};

	// One-time sanity check that hex_from_world matches axial_to_world_flat for a known hex
	if (kEnableBiomeDebug && s_sanityOnce)
	{
		s_sanityOnce = false;

		const int Q0 = DBG_Q0;
		const int R0 = DBG_R0;

		glm::vec2 center = axial_to_world_flat(Q0, R0);
		HexSample hs = hex_from_world(center.x, center.y);

		printf("SANITY: hex center (q0=%d,r0=%d) -> hex_from_world gives cube=(%d,%d,%d), axial=(%d,%d)\n",
			Q0, R0,
			hs.cube.x, hs.cube.y, hs.cube.z,
			hs.cube.x, hs.cube.z);
	}

	// Precompute 6 neighbor direction unit vectors in world XZ
	glm::vec2 dir6[6];
	{
		glm::vec2 C0 = axial_to_world_flat(0, 0);
		for (int d = 0; d < 6; ++d)
		{
			glm::vec2 Cn = axial_to_world_flat(AQ[d], AR[d]);
			glm::vec2 v = Cn - C0;
			float len = glm::length(v);
			dir6[d] = (len > 1e-6f) ? (v / len) : glm::vec2(0.0f);
		}
	}

	for (int y = 0; y < int(m_height); ++y)
	{
		for (int x = 0; x < int(m_width); ++x)
		{
			glm::vec2 w = world_from_texel(x, y);
			const float tx = w.x;
			const float tz = w.y;

			HexSample hs = hex_from_world(tx, tz);
			glm::ivec3 cube = hs.cube;
			glm::vec2  qr_f = hs.qr_f;

			const int qR = cube.x;
			const int rR = cube.z; // cube.z is axial r

			const size_t idx = size_t(y) * size_t(m_width) + size_t(x);
			float base = base0[idx];

			// Fetch biome ONCE, reuse below
			Biome b = tileBiome(cube);

			// =============== DEBUG STATS UPDATE (guarded) ==================
			if (kEnableBiomeDebug)
			{
				dbg.totalTexels++;

				dbg.qMin = std::min(dbg.qMin, qR);
				dbg.qMax = std::max(dbg.qMax, qR);
				dbg.rMin = std::min(dbg.rMin, rR);
				dbg.rMax = std::max(dbg.rMax, rR);

				bool owned = chunkOwnsHex(qR, rR);
				if (owned) dbg.ownedTexels++;
				else       dbg.unownedTexels++;

				switch (b)
				{
				case Biome::Plain:    dbg.countPlain++;    break;
				case Biome::Hill:     dbg.countHill++;     break;
				case Biome::Mountain: dbg.countMountain++; break;
				case Biome::Water:    dbg.countWater++;    break;
				}

				if (qR == DBG_Q0 && rR == DBG_R0)
				{
					dbgHex.used = true;
					dbgHex.texelCount++;
					dbgHex.minX = std::min(dbgHex.minX, x);
					dbgHex.maxX = std::max(dbgHex.maxX, x);
					dbgHex.minY = std::min(dbgHex.minY, y);
					dbgHex.maxY = std::max(dbgHex.maxY, y);
				}

				if (s_traceOneTexel)
				{
					s_traceOneTexel = false;

					glm::vec3 cube_f = math::Hex::PixelToHex(
						glm::vec3(tx, 0.0f, tz),
						math::Hex::Orientation::Flat
					);

					glm::vec2 centerXZ = math::Hex(
						glm::vec3(float(qR), float(-qR - rR), float(rR)),
						math::Hex::Orientation::Flat
					).ToWorldSpaceXZPos();
					glm::vec3 centerWorld = glm::vec3{ centerXZ.x, 0.8f, centerXZ.y };

					printf("DEBUG TEXEL TRACE: x=%d y=%d", x, y);
					printf("  uv=(%.4f, %.4f)",
						(m_width > 1) ? float(x) / float(m_width - 1) : 0.0f,
						(m_height > 1) ? float(y) / float(m_height - 1) : 0.0f);
					printf("  world=(%.4f, %.4f)", tx, tz);
					printf("  cube_f=(%.4f, %.4f, %.4f)",
						cube_f.x, cube_f.y, cube_f.z);
					printf("  cube_i=(%d, %d, %d)", cube.x, cube.y, cube.z);
					printf("  axial(q,r)=(%d, %d)", qR, rR);
					printf("  hexCenterWorld=(%.4f, %.4f)", centerWorld.x, centerWorld.z);
					printf("  biome=%d   owned=%d   base=%.4f",
						int(b), chunkOwnsHex(qR, rR) ? 1 : 0, base);
				}
			}
			// ================= END DEBUG BLOCK =====================

			/*if (!chunkOwnsHex(qR, rR)) {
				continue;
			}*/

			if (b == Biome::Mountain)
			{
				// ===== MOUNTAIN =====
				const float fp = (mountain.FOOTPRINT_FR != 0.0f) ? mountain.FOOTPRINT_FR : 1.0f;
				const float comp = 1.0f / fp;

				const float q_rel = qr_f.x - float(qR);
				const float r_rel = qr_f.y - float(rR);
				const float s_rel = -q_rel - r_rel;
				float m = std::max(std::fabs(q_rel),
					std::max(std::fabs(r_rel), std::fabs(s_rel)));
				float t_hex = glm::clamp((m * 2.0f) / fp, 0.0f, 1.0f);

				glm::vec2 C = axial_to_world_flat(qR, rR);
				glm::vec2 d = glm::vec2(tx, tz) - C;
				float     dist = glm::length(d);
				float     ang = std::atan2(d.y, d.x);

				float rndA = hash01(qR, rR);
				float rndB = hash01(qR + 57, rR - 31);
				float lobes = glm::mix(mountain.LOBES_MIN, mountain.LOBES_MAX, rndA);
				float phase = 6.2831853f * rndB;
				float amp = glm::mix(mountain.JITTER_AMP_MIN, mountain.JITTER_AMP_MAX, rndB);
				float radialOsc = std::sin(lobes * ang + phase);

				float R_base = mountain.CIRCLE_RADIUS_FR * R;
				float Rj_uncl = R_base * (1.0f + amp * radialOsc);
				float Rj_eff = glm::clamp(Rj_uncl * fp, 0.80f * R * fp, 1.05f * R * fp);
				float t_cir_j = glm::clamp(dist / Rj_eff, 0.0f, 1.0f);

				float t_blend = glm::mix(t_hex, t_cir_j, mountain.ROUND_K);
				float env_blend = 1.0f - glm::smoothstep(mountain.SHOULDER_START, 1.0f, t_blend);
				float env_hex = 1.0f - glm::smoothstep(mountain.SHOULDER_START, 1.0f, t_hex);
				float env = std::pow(glm::clamp(env_blend, 0.0f, 1.0f),
					mountain.SHARPNESS) * env_hex;

				float low = low_band(x, y);
				float high = base - low;

				if (env < 1e-4f) { heightmap[idx] = base; continue; }

				float compW_m = (mountain.BASE_COMP_GAMMA > 1.0f)
					? glm::smoothstep(mountain.BASE_COMP_ENV_START,
						mountain.BASE_COMP_ENV_END,
						env)
					: 0.0f;
				float low_gamma = std::pow(glm::clamp(low, 0.0f, 1.0f),
					glm::max(mountain.BASE_COMP_GAMMA, 1.0f));
				float low_c = glm::mix(low, low_gamma, compW_m);

				float wBorder = (1.0f - env) * (1.0f - env);
				float liftK = glm::mix(mountain.LIFT_CENTER, mountain.LIFT_BORDER, wBorder);
				float lowLift = raise_preserve(low_c, env, liftK);

				glm::vec2 P = glm::vec2(tx, tz);
				glm::vec2 Pw = P + warp2(P,
					mountain.WARP_FREQ_OVER_R / R,
					mountain.WARP_AMP_FRR * R);

				float meso = fbm_ridged01(Pw * (mountain.MESO_FREQ_OVER_R / R),
					mountain.MESO_OCTAVES,
					mountain.LACUNARITY,
					mountain.GAIN);
				float micro = fbm_ridged01(Pw * (mountain.MICRO_FREQ_OVER_R / R),
					mountain.MICRO_OCTAVES,
					mountain.LACUNARITY,
					mountain.GAIN);
				float mesoTerm = 0.35f + 0.65f * meso;
				float microTerm = 0.35f + 0.65f * micro;

				float env_for_masks = glm::clamp(env * comp, 0.0f, 1.0f);
				float wSlope = glm::smoothstep(0.10f, 0.90f, env_for_masks);
				float wApex = glm::smoothstep(0.50f, 0.98f, env_for_masks);
				float highGain = glm::mix(mountain.HIGH_GAIN_BORDER,
					mountain.HIGH_GAIN_CENTER,
					env_for_masks);

				float t_blend_center = glm::clamp(1.0f - t_blend, 0.0f, 1.0f);
				float central = std::pow(t_blend_center, mountain.SPIKE_POWER);
				float spike = mountain.SPIKE_AMP * (0.8f + 0.2f * microTerm) * central;

				float lowWanted =
					spike +
					(mountain.MESO_AMP * wSlope * mesoTerm) +
					(mountain.MICRO_AMP * wApex * microTerm);

				float outLow = lowLift;
				float outHigh = high * highGain;

				float headroom = glm::max(0.0f,
					(1.0f - mountain.MARGIN) - (outLow + outHigh));
				float add = glm::min(lowWanted, headroom);

				float shaped = outLow + outHigh + add;
				heightmap[idx] = glm::clamp(shaped, 0.0f, 1.0f);
			}
			else if (b == Biome::Hill)
			{
				// ===== HILL =====
				glm::vec2 C_hex = axial_to_world_flat(qR, rR);
				float     innerR = hill.H_HILL_RADIUS_FR * R;

				glm::vec2 dC = glm::vec2(tx, tz) - C_hex;
				float     dist_center = glm::length(dC);

				float t_circ_raw = glm::clamp(dist_center / innerR, 0.0f, 1.0f);
				float t_circ = std::pow(t_circ_raw, hill.taper_exp);

				float denom = glm::max(1.0f - hill.H_SHOULDER_START, 1e-6f);
				float t_after = glm::clamp((t_circ - hill.H_SHOULDER_START) / denom, 0.0f, 1.0f);
				float env_raw = 1.0f - glm::smoothstep(0.0f, 1.0f, t_after);
				float env = std::pow(glm::clamp(env_raw, 0.0f, 1.0f), hill.H_SHARPNESS);

				float low = low_band(x, y);
				float high = base - low;

				if (env <= 1e-5f) { heightmap[idx] = base; continue; }

				float compW_h = (hill.BASE_COMP_GAMMA > 1.0f)
					? glm::smoothstep(hill.BASE_COMP_ENV_START,
						hill.BASE_COMP_ENV_END,
						env)
					: 0.0f;
				float low_gamma = std::pow(glm::clamp(low, 0.0f, 1.0f),
					glm::max(hill.BASE_COMP_GAMMA, 1.0f));
				float low_c = glm::mix(low, low_gamma, compW_h);

				float rndDir = 6.2831853f * hash01(qR * 17 + 3, rR * 29 - 11);
				float rndAmpF = glm::mix(hill.H_PEAK_OFFSET_MIN_FR,
					hill.H_PEAK_OFFSET_MAX_FR,
					hash01(qR * -71 + 5, rR * 43 + 19));
				glm::vec2 C_peak = C_hex +
					glm::vec2(std::cos(rndDir), std::sin(rndDir)) * (rndAmpF * innerR);

				float dist_peak = glm::length(glm::vec2(tx, tz) - C_peak);
				float t_peak_raw = glm::clamp(dist_peak / innerR, 0.0f, 1.0f);
				float t_peak = std::pow(t_peak_raw, hill.taper_exp);
				float apexW = std::pow(1.0f - t_peak, glm::max(hill.H_PEAK_POW, 1e-6f));

				float env_lift = glm::max(env, apexW);
				float liftK = hill.H_HEIGHT_SCALE *
					(hill.H_LIFT_CENTER_BASE + env_lift * hill.H_LIFT_CENTER_GAIN);
				float lowLift = raise_preserve(low_c, env, liftK);

				float dome = hill.H_HEIGHT_SCALE *
					(hill.H_DOME_AMP * std::pow(apexW, hill.H_DOME_POW));

				float suppressW = glm::max(env, apexW);
				float highGain = glm::mix(1.0f, hill.H_TOP_SUPPRESS, suppressW);

				float outLow = lowLift;
				float outHigh = high * highGain;

				// --- OLD (problematic) version that can create thin ridges ---
				// float headroom = glm::max(0.0f,
				//     (1.0f - hill.H_MARGIN) - (outLow + outHigh));
				// float add = glm::min(dome, headroom);
				// float shaped = outLow + outHigh + add;

				// --- NEW: smoother dome with a soft cap, no contour "ridge" ---
				float core = outLow + outHigh + dome;

				// Soft limit so we don't slam into 1.0, but uniformly,
				// not only along weird noise contour lines.
				float maxHeight = 1.0f - hill.H_MARGIN;   // same intent as before
				float shaped = glm::min(core, maxHeight);

				float preStart = glm::min(hill.H_BASEMENT_PREBLEND_START,
					hill.H_BLEND_START);
				float preStr = glm::clamp(hill.H_BASEMENT_PREBLEND_STRENGTH, 0.0f, 1.0f);
				float preT = glm::smoothstep(preStart, hill.H_BLEND_START, t_circ);
				preT = std::pow(preT, glm::max(hill.H_BASEMENT_PREBLEND_EXP, 1.0f));
				float edgeW_pre = preStr * preT;

				float edgeW_main = glm::smoothstep(hill.H_BLEND_START,
					hill.H_BLEND_END,
					t_circ);
				float edgeW = glm::clamp(edgeW_pre + edgeW_main, 0.0f, 1.0f);

				float final = glm::mix(shaped, base, edgeW);

				heightmap[idx] = glm::clamp(final, 0.0f, 1.0f);
			}
			else if (b == Biome::Water)
			{
				glm::vec2 C_hex = axial_to_world_flat(qR, rR);
				glm::vec2 P = glm::vec2(tx, tz);
				glm::vec2 dP = P - C_hex;

				const float baseVal = base; // continuous world noise

				// seaBase = flattened version of noise near SEA_LEVEL
				float seaBase = glm::mix(
					baseVal,
					water.SEA_LEVEL,
					glm::clamp(water.DEPTH_STRENGTH, 0.0f, 1.0f)
				);

				// ------------------- Neighbor scan for *this* water hex --------------------
				struct CoastInfo
				{
					int       landCount = 0;
					glm::vec2 landDirSum = glm::vec2(0.0f);
					bool      isWater = false;
					bool      neighborIsLand[6] = { false, false, false, false, false, false };
				};

				auto classifyWaterHex = [&](int qH, int rH) -> CoastInfo
					{
						CoastInfo info;

						glm::ivec3 cubeH(qH, -qH - rH, rH);
						if (tileBiome(cubeH) != Biome::Water)
							return info; // isWater=false

						info.isWater = true;

						for (int d = 0; d < 6; ++d)
						{
							int nq = qH + AQ[d];
							int nr = rH + AR[d];
							glm::ivec3 nc(nq, -nq - nr, nr);
							Biome nb = tileBiome(nc);
							if (nb != Biome::Water)
							{
								++info.landCount;
								info.landDirSum += dir6[d];
								info.neighborIsLand[d] = true;
							}
						}
						return info;
					};

				CoastInfo info = classifyWaterHex(qR, rR);

				const float shoreHalfWidth = water.SHORE_HALF_WIDTH_FR * R;

				// ------------------- 1) Open water hex -------------------
				if (info.landCount == 0)
				{
					float h = seaBase;

					if (water.WAVE_AMP > 0.0f)
					{
						glm::vec2 wP = P * (water.WAVE_FREQ_OVER_R / R);
						float     wave = std::sin(wP.x + water.WAVE_PHASE_SKEW * wP.y);
						h += water.WAVE_AMP * wave;
					}

					heightmap[idx] = glm::clamp(h, 0.0f, 1.0f);
					continue;
				}

				// ------------------- 2) Corner hex: exactly 1 land neighbor -------------------
				if (info.landCount == 1)
				{
					// ===== OPTIONAL DEBUG TOGGLE =====
					static bool s_dbg1Land = false;  // set true if you still want logs
					static int  s_dbgBudget = 300;

					// ---- 1) Find the single land neighbor direction ----
					int dLand = -1;
					for (int d = 0; d < 6; ++d)
					{
						if (info.neighborIsLand[d])
						{
							dLand = d;
							break;
						}
					}

					// Fallback: if somehow not found, just treat as open water
					if (dLand < 0)
					{
						heightmap[idx] = seaBase;
						continue;
					}

					// Land hex center in world space
					int qL = qR + AQ[dLand];
					int rL = rR + AR[dLand];
					glm::vec2 C_land = axial_to_world_flat(qL, rL);

					// Our water hex center
					glm::vec2 C_hex_local = axial_to_world_flat(qR, rR);
					glm::vec2 P_local = glm::vec2(tx, tz);

					// From land center to this texel
					glm::vec2 dL = P_local - C_land;
					float     rLand = glm::length(dL);
					glm::vec2 dirP = (rLand > 1e-5f) ? (dL / rLand) : glm::vec2(1.0f, 0.0f);

					// From water-hex center to this texel (used for rim masks etc.)
					glm::vec2 dP_local = P_local - C_hex_local;
					float     rHex = glm::length(dP_local);

					// ---- 2) Radial "corner bay" profile from land center ----
					float innerR = R * water.CORNER_INNER_R_FR;                      // near land
					float outerR = R + shoreHalfWidth * water.CORNER_OUTER_R_EXTRA;  // toward sea

					float tRad = 0.0f;
					if (outerR > innerR + 1e-5f)
						tRad = glm::clamp((rLand - innerR) / (outerR - innerR), 0.0f, 1.0f);

					float blendRad = std::pow(tRad, water.SHARPNESS);

					// Land-side = baseVal, deep corner = seaBase
					float hRadial = glm::mix(baseVal, seaBase, blendRad);

					// Optional waves: stronger deeper into water
					if (water.WAVE_AMP > 0.0f)
					{
						glm::vec2 wP = P_local * (water.WAVE_FREQ_OVER_R / R);
						float     wave = std::sin(wP.x + water.WAVE_PHASE_SKEW * wP.y);
						hRadial += water.WAVE_AMP * blendRad * wave;
					}

					// ---- 3) Align with a 2-land straight-coast neighbor (the good side) ----
					int       bestD2 = -1;
					CoastInfo bestInfo2;
					glm::vec2 C_best2(0.0f);

					for (int d = 0; d < 6; ++d)
					{
						if (info.neighborIsLand[d])
							continue; // skip the actual land hex

						int nq = qR + AQ[d];
						int nr = rR + AR[d];

						CoastInfo nInfo = classifyWaterHex(nq, nr);
						if (!nInfo.isWater)
							continue;

						if (nInfo.landCount == 2)
						{
							bestD2 = d;
							bestInfo2 = nInfo;
							C_best2 = axial_to_world_flat(nq, nr);
							break; // first 2-land neighbor is enough
						}
					}

					float hFinal = hRadial;

					if (bestD2 >= 0)
					{
						glm::vec2 shoreDirN = glm::normalize(bestInfo2.landDirSum); // towards land
						glm::vec2 shoreNormalN = -shoreDirN;                           // from land to sea

						glm::vec2 dP_N = P_local - C_best2;

						float vN = glm::dot(dP_N, shoreNormalN);
						float vNormN = glm::clamp(
							(vN + shoreHalfWidth) / (2.0f * shoreHalfWidth),
							0.0f, 1.0f
						);

						float shoreMaskN = 1.0f - vNormN;
						shoreMaskN = std::pow(shoreMaskN, water.SHARPNESS);
						float waterMaskN = 1.0f - shoreMaskN;

						float hLinear = glm::mix(baseVal, seaBase, waterMaskN);

						if (water.WAVE_AMP > 0.0f)
						{
							glm::vec2 wP = P_local * (water.WAVE_FREQ_OVER_R / R);
							float     wave = std::sin(wP.x + water.WAVE_PHASE_SKEW * wP.y);
							hLinear += water.WAVE_AMP * waterMaskN * wave;
						}

						// Blend radial vs straight along the 2-land edge direction
						glm::vec2 edgeDir2 = C_best2 - C_land;
						float     edgeLen2 = glm::length(edgeDir2);
						if (edgeLen2 > 1e-5f)
							edgeDir2 /= edgeLen2;
						else
							edgeDir2 = glm::vec2(1.0f, 0.0f);

						float cosAng2 = glm::dot(edgeDir2, dirP);

						float cosMax = water.CORNER_ALIGN_COS_MAX; // was cos30
						float wAlign2 = glm::clamp((cosAng2 - 0.0f) / (cosMax - 0.0f), 0.0f, 1.0f);
						wAlign2 = glm::smoothstep(0.0f, 1.0f, wAlign2);

						hFinal = glm::mix(hRadial, hLinear, wAlign2);
					}

					// ---- 4) WEDGE FILL toward 3-land neighbor: fix the "cake slice" ----
					glm::vec2 seamDir3(0.0f);
					bool      has3 = false;

					// Find the 3-land neighbor and its center direction
					for (int d = 0; d < 6; ++d)
					{
						if (info.neighborIsLand[d])
							continue;

						int nq = qR + AQ[d];
						int nr = rR + AR[d];

						CoastInfo nInfo = classifyWaterHex(nq, nr);
						if (!nInfo.isWater || nInfo.landCount != 3)
							continue;

						glm::vec2 C_nb = axial_to_world_flat(nq, nr);
						seamDir3 = glm::normalize(C_nb - C_hex_local);
						has3 = true;
						break; // just one 3-land neighbor is relevant here
					}

					if (has3)
					{
						// Direction from hex center to this texel
						glm::vec2 dirFromHex = (rHex > 1e-5f) ? (dP_local / rHex) : glm::vec2(1.0f, 0.0f);
						float cos3 = glm::dot(dirFromHex, seamDir3);

						float cosStart = water.CORNER_SEAM_COS_START; // was 0.60f
						if (cos3 > cosStart)
						{
							float rimMin = water.CORNER_SEAM_RIM_MIN_FR * R; // was 0.80f * R
							float rimMax = R + shoreHalfWidth;
							float rimT = glm::clamp(
								(rHex - rimMin) / glm::max(rimMax - rimMin, 1e-5f),
								0.0f, 1.0f
							);

							float angT = glm::clamp((cos3 - cosStart) / (1.0f - cosStart), 0.0f, 1.0f);
							angT = angT * angT; // concentrate near seam

							float seamMask = rimT * angT;

							if (seamMask > 0.0f)
							{
								float hFill = glm::mix(hRadial, baseVal, 0.5f);

								float seamStrength = water.CORNER_SEAM_STRENGTH;
								float t = glm::clamp(seamStrength * seamMask, 0.0f, 1.0f);

								hFinal = glm::mix(hFinal, hFill, t);

								if (s_dbg1Land && s_dbgBudget > 0)
								{
									--s_dbgBudget;
									printf("DBG wedge-fill 1-land (%d,%d): rHex=%.3f cos3=%.3f seamMask=%.3f\n",
										qR, rR, rHex, cos3, seamMask);
									printf("    baseVal=%.4f seaBase=%.4f hRadial=%.4f hFill=%.4f hFinal=%.4f\n",
										baseVal, seaBase, hRadial, hFill, hFinal);
								}
							}
						}
					}

					heightmap[idx] = glm::clamp(hFinal, 0.0f, 1.0f);
					continue;
				}

				// ------------------- 3) 3-land “bay” hex -------------------
				if (info.landCount >= 3)
				{
					glm::vec2 C_hex3 = axial_to_world_flat(qR, rR);
					glm::vec2 P3 = glm::vec2(tx, tz);
					glm::vec2 dP3 = P3 - C_hex3;
					float     r3 = glm::length(dP3);
					glm::vec2 dirP3 = (r3 > 1e-5f) ? (dP3 / r3) : glm::vec2(1.0f, 0.0f);

					// --- 3a) Blend the three straight-coast ramps (same as 2-land, but per-edge) ---
					float sumW = 0.0f;
					float sumH = 0.0f;
					float sumWM = 0.0f;   // avg waterMask for waves
					const float dirExp = water.BAY_DIR_EXP;

					for (int d = 0; d < 6; ++d)
					{
						if (!info.neighborIsLand[d])
							continue;

						glm::vec2 edgeDir = glm::normalize(dir6[d]);   // water -> land
						glm::vec2 shoreDir_d = edgeDir;             // towards land
						glm::vec2 shoreNormal_d = -shoreDir_d;         // from land to sea

						float v_d = glm::dot(dP3, shoreNormal_d);
						float vNorm_d = glm::clamp(
							(v_d + shoreHalfWidth) / (2.0f * shoreHalfWidth),
							0.0f, 1.0f
						);

						float shoreMask_d = 1.0f - vNorm_d;
						shoreMask_d = std::pow(shoreMask_d, water.SHARPNESS);
						float waterMask_d = 1.0f - shoreMask_d;

						float hCoast_d = glm::mix(baseVal, seaBase, waterMask_d);

						float cosEdge = glm::dot(dirP3, edgeDir);
						float w_d = (cosEdge > 0.0f) ? std::pow(cosEdge, dirExp) : 0.0f;

						if (w_d > 0.0f)
						{
							sumW += w_d;
							sumH += w_d * hCoast_d;
							sumWM += w_d * waterMask_d;
						}
					}

					float hCoast_base;
					float avgWaterMask;

					if (sumW > 1e-5f)
					{
						hCoast_base = sumH / sumW;
						avgWaterMask = sumWM / sumW;
					}
					else
					{
						hCoast_base = seaBase;
						avgWaterMask = 1.0f;
					}

					if (water.WAVE_AMP > 0.0f)
					{
						glm::vec2 wP = P3 * (water.WAVE_FREQ_OVER_R / R);
						float     wave = std::sin(wP.x + water.WAVE_PHASE_SKEW * wP.y);
						hCoast_base += water.WAVE_AMP * avgWaterMask * wave;
					}

					// --- 3b) Angular bay mask: pull bay interior down to seaBase ---
					float f_land = 0.0f;
					for (int d = 0; d < 6; ++d)
					{
						if (!info.neighborIsLand[d])
							continue;

						glm::vec2 edgeDir = glm::normalize(dir6[d]); // water→land
						float     cosEdge = glm::dot(dirP3, edgeDir);
						float     wEdge = glm::clamp(cosEdge, 0.0f, 1.0f);
						f_land = glm::max(f_land, wEdge);
					}

					float f_dir = 1.0f - f_land;
					float bayExp = water.BAY_ANG_EXP; // was 1.5f
					float bayMask = std::pow(glm::clamp(f_dir, 0.0f, 1.0f), bayExp);

					float hBay = glm::mix(hCoast_base, seaBase, bayMask);

					// --- 3c) Align with 2-land water neighbors (straight coast) ---
					float bestAlign = 0.0f;
					float bestNeighbor = hBay;

					for (int d = 0; d < 6; ++d)
					{
						int nq = qR + AQ[d];
						int nr = rR + AR[d];

						CoastInfo nInfo = classifyWaterHex(nq, nr);
						if (!nInfo.isWater || nInfo.landCount != 2)
							continue;

						glm::vec2 C_nb = axial_to_world_flat(nq, nr);
						glm::vec2 dP_nb = P3 - C_nb;

						glm::vec2 shoreDir_nb = glm::normalize(nInfo.landDirSum);
						glm::vec2 shoreNormal_nb = -shoreDir_nb;

						float v_n = glm::dot(dP_nb, shoreNormal_nb);
						float vNorm_n = glm::clamp(
							(v_n + shoreHalfWidth) / (2.0f * shoreHalfWidth),
							0.0f, 1.0f
						);

						float shoreMask_n = 1.0f - vNorm_n;
						shoreMask_n = std::pow(shoreMask_n, water.SHARPNESS);
						float waterMask_n = 1.0f - shoreMask_n;

						float hNeighbor = glm::mix(baseVal, seaBase, waterMask_n);

						if (water.WAVE_AMP > 0.0f)
						{
							glm::vec2 wP = P3 * (water.WAVE_FREQ_OVER_R / R);
							float     wave = std::sin(wP.x + water.WAVE_PHASE_SKEW * wP.y);
							hNeighbor += water.WAVE_AMP * waterMask_n * wave;
						}

						glm::vec2 centerDir = glm::normalize(C_nb - C_hex3);
						float     cosDir = glm::dot(dirP3, centerDir);
						if (cosDir > 0.0f)
						{
							float alignW = std::pow(cosDir, 3.0f);
							if (alignW > bestAlign)
							{
								bestAlign = alignW;
								bestNeighbor = hNeighbor;
							}
						}
					}

					float hFinal = hBay;
					if (bestAlign > 0.0f)
					{
						float alignStrength = 1.0f; // keep as-is for now; could param if needed
						float t = glm::clamp(alignStrength * bestAlign, 0.0f, 1.0f);
						hFinal = glm::mix(hBay, bestNeighbor, t);
					}

					// --- 3d) CENTRAL FLATTEN: kill tiny kink at hex center ---
					{
						float centerRadius = water.BAY_CENTER_RADIUS_FR * R; // was 0.30f * R
						if (centerRadius > 1e-5f)
						{
							float centerT = glm::clamp(1.0f - r3 / centerRadius, 0.0f, 1.0f);
							hFinal = glm::mix(hFinal, seaBase, centerT);
						}
					}

					// --- 3e) HARMONIZE WITH 1-LAND NEIGHBOR ALONG THE WHOLE SLOPE WEDGE ---
					{
						bool      has1 = false;
						glm::vec2 seamDir1(0.0f);

						for (int d = 0; d < 6; ++d)
						{
							int nq = qR + AQ[d];
							int nr = rR + AR[d];

							CoastInfo nInfo = classifyWaterHex(nq, nr);
							if (!nInfo.isWater || nInfo.landCount != 1)
								continue;

							glm::vec2 C_nb = axial_to_world_flat(nq, nr);
							seamDir1 = glm::normalize(C_nb - C_hex3);
							has1 = true;
							break;
						}

						if (has1)
						{
							glm::vec2 C_landSeam(0.0f);
							bool      haveLandSeam = false;
							float     bestCosLand = -1.0f;

							for (int d = 0; d < 6; ++d)
							{
								if (!info.neighborIsLand[d])
									continue;

								int qL = qR + AQ[d];
								int rL = rR + AR[d];

								glm::vec2 C_land = axial_to_world_flat(qL, rL);
								glm::vec2 landDirFromHex = glm::normalize(C_land - C_hex3);
								float     c = glm::dot(landDirFromHex, seamDir1);
								if (c > bestCosLand)
								{
									bestCosLand = c;
									C_landSeam = C_land;
									haveLandSeam = true;
								}
							}

							if (haveLandSeam)
							{
								glm::vec2 dL = P3 - C_landSeam;
								float     rLand = glm::length(dL);

								float innerR = R * water.CORNER_INNER_R_FR;
								float outerR = R + shoreHalfWidth * water.CORNER_OUTER_R_EXTRA;

								float tRad = 0.0f;
								if (outerR > innerR + 1e-5f)
									tRad = glm::clamp((rLand - innerR) / (outerR - innerR), 0.0f, 1.0f);

								float blendRad = std::pow(tRad, water.SHARPNESS);
								float hRadial = glm::mix(baseVal, seaBase, blendRad);

								if (water.WAVE_AMP > 0.0f)
								{
									glm::vec2 wP = P3 * (water.WAVE_FREQ_OVER_R / R);
									float     wave = std::sin(wP.x + water.WAVE_PHASE_SKEW * wP.y);
									hRadial += water.WAVE_AMP * blendRad * wave;
								}

								glm::vec2 dirFromHex = (r3 > 1e-5f) ? (dP3 / r3) : glm::vec2(1.0f, 0.0f);
								float     cos1 = glm::dot(dirFromHex, seamDir1);

								float cosStart1 = water.BAY_CORNER_COS_START; // was 0.65f
								if (cos1 > cosStart1)
								{
									float angT = glm::clamp((cos1 - cosStart1) / (1.0f - cosStart1), 0.0f, 1.0f);
									angT = angT * angT;

									float centerFade = 1.0f;
									{
										float coreR = water.BAY_CORNER_CORE_R_FR * R; // was 0.20f * R
										if (r3 < coreR)
										{
											float t = glm::clamp(r3 / coreR, 0.0f, 1.0f);
											centerFade = t;
										}
									}

									float seamMask = angT * centerFade;

									if (seamMask > 0.0f)
									{
										float seamStrength = water.BAY_CORNER_SEAM_STRENGTH; // was 1.0f
										float t = glm::clamp(seamStrength * seamMask, 0.0f, 1.0f);

										hFinal = glm::mix(hFinal, hRadial, t);
									}
								}
							}
						}
					}

					heightmap[idx] = glm::clamp(hFinal, 0.0f, 1.0f);
					continue;
				}

				// ------------------- 4) Default coast: 2 land neighbors -------------------
				{
					glm::vec2 shoreDir = glm::normalize(info.landDirSum); // towards land
					glm::vec2 shoreNormal = -shoreDir;                       // from land to sea

					float v = glm::dot(dP, shoreNormal);

					float vNorm = glm::clamp(
						(v + shoreHalfWidth) / (2.0f * shoreHalfWidth),
						0.0f, 1.0f
					);

					float shoreMask = 1.0f - vNorm;
					shoreMask = std::pow(shoreMask, water.SHARPNESS);

					float waterMask = 1.0f - shoreMask;

					float hCoast = glm::mix(baseVal, seaBase, waterMask);

					if (water.WAVE_AMP > 0.0f)
					{
						glm::vec2 wP = P * (water.WAVE_FREQ_OVER_R / R);
						float     wave = std::sin(wP.x + water.WAVE_PHASE_SKEW * wP.y);
						hCoast += water.WAVE_AMP * waterMask * wave;
					}

					heightmap[idx] = glm::clamp(hCoast, 0.0f, 1.0f);
				}
			}
		}
	}

	// ================== DUMP DEBUG SUMMARY ONCE ======================
	if (kEnableBiomeDebug && !s_dumped)
	{
		s_dumped = true;

		printf("=== TERRAIN BIOME DEBUG SUMMARY ===");
		printf("  texels: total=%d  owned=%d  unowned=%d",
			dbg.totalTexels, dbg.ownedTexels, dbg.unownedTexels);
		printf("  q-range: [%d .. %d]", dbg.qMin, dbg.qMax);
		printf("  r-range: [%d .. %d]", dbg.rMin, dbg.rMax);
		printf("  biomes: plain=%d hill=%d mountain=%d water=%d",
			dbg.countPlain, dbg.countHill, dbg.countMountain, dbg.countWater);

		if (dbgHex.used)
		{
			printf("  Debug hex (q=%d,r=%d): texelCount=%d",
				DBG_Q0, DBG_R0, dbgHex.texelCount);
			printf("    footprint in texture: x=[%d..%d], y=[%d..%d]",
				dbgHex.minX, dbgHex.maxX, dbgHex.minY, dbgHex.maxY);
			printf("    footprint size: w=%d, h=%d",
				dbgHex.maxX - dbgHex.minX + 1,
				dbgHex.maxY - dbgHex.minY + 1);
		}
		else
		{
			printf("  Debug hex (q=%d,r=%d) never hit by any texel!", DBG_Q0, DBG_R0);
		}

		printf("====================================");
	}
}







