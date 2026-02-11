#include "stdafx.h"

#include "OpenGlRenderPipeline.h"

#include <math/Camera.h>
#include "GUI.h"

#include "GlBufferContext.h"
#include "GlDrawContext.h"
#include "GlPipelineState.h"

#include "RenderManager.h"
#include "TextureManager.h"
#include "ModelManager.h"
#include "StableObjectSorter.h"

//------------------------------------------------------------------------
void AddBoundingBoxIndices(std::vector<GLuint>& indices, GLuint offset)
{
	static const GLuint boxEdges[] = {
			0, 1, 1, 2, 2, 3, 3, 0,
			4, 5, 5, 6, 6, 7, 7, 4,
			0, 4, 1, 5, 2, 6, 3, 7
	};
	for (size_t i = 0; i < sizeof(boxEdges) / sizeof(boxEdges[0]); ++i)
		indices.push_back(offset + boxEdges[i]);
}

//------------------------------------------------------------------------
void eOpenGlRenderPipeline::EnsureInitialized(const Camera& cam)
{
	if (!m_first_call) return;
	RenderIBL(cam);
	LoadPCFOffsetsTextureToShaders();
	m_first_call = false;
}
//-----------------------------------------------------------------------------------------------
void eOpenGlRenderPipeline::RenderFrame(const Scene& scene, RenderBuckets& buckets, float tick)
{
	// Pull what we need from Scene
	/*const*/ Camera& _camera = const_cast<Camera&>(scene.m_cameras[scene.m_cur_camera]); //@todo make this const& if your API allows
	const auto& _lights = scene.m_lights;
	const Light& _light = _lights.front();

	EnsureInitialized(_camera);

	// Bucket refs (no copies)
	auto& bezier_objs = buckets[to_index(eObject::RenderType::BEZIER_CURVE)];
	auto& geometry_objs = buckets[to_index(eObject::RenderType::GEOMETRY)];
	auto& pbr_objs = buckets[to_index(eObject::RenderType::PBR)];
	auto& phong_objs = buckets[to_index(eObject::RenderType::PHONG)];
	auto& flags = buckets[to_index(eObject::RenderType::FLAG)];
	auto& lines_objs = buckets[to_index(eObject::RenderType::LINES)];
	auto& arealighted_objs = buckets[to_index(eObject::RenderType::AREA_LIGHT_ONLY)];
	auto& terrain_tes_objs = buckets[to_index(eObject::RenderType::TERRAIN_TESSELLATION)];
	auto& volumetric_objs = buckets[to_index(eObject::RenderType::VOLUMETRIC)];
	auto& leaf_objs = buckets[to_index(eObject::RenderType::LEAF)];
	auto& ui_objs = buckets[to_index(eObject::RenderType::UI_SYSTEM)];
	auto& focused = buckets[to_index(eObject::RenderType::OUTLINED)];
	auto& transparent_objs = buckets[to_index(eObject::RenderType::PBR_TRANSPARENT)];

	// Build combined list (shared_ptr copies are cheap)
	std::vector<shObject> phong_pbr_objects;
	phong_pbr_objects.reserve(phong_objs.size() + pbr_objs.size() + arealighted_objs.size() + terrain_tes_objs.size());
	phong_pbr_objects.insert(phong_pbr_objects.end(), phong_objs.begin(), phong_objs.end());
	phong_pbr_objects.insert(phong_pbr_objects.end(), pbr_objs.begin(), pbr_objs.end());
	phong_pbr_objects.insert(phong_pbr_objects.end(), arealighted_objs.begin(), arealighted_objs.end());
	phong_pbr_objects.insert(phong_pbr_objects.end(), terrain_tes_objs.begin(), terrain_tes_objs.end());

	// Sort in place (unchanged behavior)
	StableObjectSorter sorter(_camera, phong_pbr_objects);
	sorter.Sort(focused);
	sorter.Sort(phong_objs);
	sorter.Sort(pbr_objs);
	sorter.Sort(transparent_objs);

	// Cache animation pose (unchanged)
	for (const auto& obj : phong_pbr_objects)
		if (auto r = obj->GetRigger(); r) r->CacheMatrices();

	glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Shadow Render Pass");
	//Shadow Render Pass
	if (_light.type == eLightType::DIRECTION || _light.type == eLightType::SPOT)
	{
		eGlBufferContext::GetInstance().EnableWrittingBuffer(eBuffer::BUFFER_SHADOW_DIR);
		if (shadows) { RenderShadows(_camera, _light, phong_pbr_objects); }
		eGlBufferContext::GetInstance().EnableReadingBuffer(eBuffer::BUFFER_SHADOW_DIR, GL_TEXTURE1);
	}
	else if (_light.type == eLightType::POINT)
	{
		eGlBufferContext::GetInstance().EnableWrittingBuffer(eBuffer::BUFFER_SHADOW_CUBE_MAP);
		if (shadows) { RenderShadows(_camera, _light, phong_pbr_objects); }
		eGlBufferContext::GetInstance().EnableReadingBuffer(eBuffer::BUFFER_SHADOW_CUBE_MAP, GL_TEXTURE0);
	}
	else if (_light.type == eLightType::CSM)
	{
		renderManager->PhongRender()->SetShadowCascadeLevels(renderManager->CSMRender()->GetCascadePlaneDistances());
		renderManager->PBRRender()->SetShadowCascadeLevels(renderManager->CSMRender()->GetCascadePlaneDistances());
		eGlBufferContext::GetInstance().EnableWrittingBuffer(eBuffer::BUFFER_SHADOW_CSM);
		if (shadows) { RenderShadowsCSM(_camera, _light, phong_pbr_objects); }
		eGlBufferContext::GetInstance().EnableReadingBuffer(eBuffer::BUFFER_SHADOW_CSM, GL_TEXTURE13);
	}
	glPopDebugGroup();

	if (m_forward_plus /*|| m_z_pre_pass*/)
	{
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Render Depth Buffer");
		eGlBufferContext::GetInstance().EnableWrittingBuffer(eBuffer::BUFFER_DEPTH);
		RenderShadows(_camera, _light, phong_pbr_objects, true);
		eGlBufferContext::GetInstance().EnableReadingBuffer(eBuffer::BUFFER_DEPTH, GL_TEXTURE17);
		glPopDebugGroup();
	}

	mts ? glBindFramebuffer(GL_FRAMEBUFFER, eGlBufferContext::GetInstance().GetId(eBuffer::BUFFER_MTS))
		: glBindFramebuffer(GL_FRAMEBUFFER, eGlBufferContext::GetInstance().GetId(eBuffer::BUFFER_DEFAULT));
	glClear(GL_DEPTH_BUFFER_BIT);

	if (m_z_pre_pass)
	{
		//eGlBufferContext::GetInstance().BlitFromTo(eBuffer::BUFFER_DEPTH, mts ? eBuffer::BUFFER_MTS : eBuffer::BUFFER_DEFAULT, GL_DEPTH_BUFFER_BIT);
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Z prepass");
		RenderShadows(_camera, _light, phong_pbr_objects, true);
		glPopDebugGroup();
	}

	if (m_forward_plus)
	{
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Compute Forward plus shader");
		renderManager->ComputeShaderRender()->DispatchForwardPlus(_camera, _lights);
		glPopDebugGroup();
	}

	glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Reflection and Refraction to Textures");
	//Rendering reflection and refraction to Textures
	eGlBufferContext::GetInstance().EnableWrittingBuffer(eBuffer::BUFFER_REFLECTION);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	/*if (skybox)*/ { RenderSkybox(_camera); }

	eGlPipelineState::GetInstance().EnableClipDistance0(); //?
	eGlPipelineState::GetInstance().EnableDepthTest();
	glPopDebugGroup();

	if (water)
	{
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Water shader");
		RenderReflection(_camera, _lights, phong_objs, pbr_objs);
		eGlBufferContext::GetInstance().EnableWrittingBuffer(eBuffer::BUFFER_REFRACTION);
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		RenderRefraction(_camera, _lights, phong_objs, pbr_objs);
		glPopDebugGroup();
	}
	eGlPipelineState::GetInstance().DisableClipDistance0();

	// SSAO
	if (ssao || ssr || camera_interpolation) // need g - buffer
	{
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "G- buffer");
		eGlBufferContext::GetInstance().EnableWrittingBuffer(eBuffer::BUFFER_DEFFERED);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		renderManager->SSAORender()->RenderGeometry(_camera, _light, phong_pbr_objects);
		glPopDebugGroup();

		if (ssao)
		{
			glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "SSAO");
			RenderSSAO(_camera, _light, phong_pbr_objects);
			glActiveTexture(GL_TEXTURE14);
			glBindTexture(GL_TEXTURE_2D, eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_SSAO_BLUR).m_id);
			glPopDebugGroup();
		}
	}

	if (!ssao)
	{
		glActiveTexture(GL_TEXTURE14);
		glBindTexture(GL_TEXTURE_2D, Texture::GetTexture1x1(WHITE).m_id);
	}

	mts ? eGlBufferContext::GetInstance().EnableWrittingBuffer(eBuffer::BUFFER_MTS)
		: eGlBufferContext::GetInstance().EnableWrittingBuffer(eBuffer::BUFFER_DEFAULT);
	glClear(/*GL_DEPTH_BUFFER_BIT |*/ GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Sky noise");
	if (sky_noise) { RenderSkyNoise(_camera); }
	glPopDebugGroup();

	glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Outlining");
	eGlPipelineState::GetInstance().EnableStencilTest();
	//4. Rendering to main FBO with stencil
	if (outline_focused)
	{
		for (const auto& obj : focused)
		{
			if (obj->IsVisible())
			{
				glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
				glStencilFunc(GL_ALWAYS, 1, 0xFF);
				glStencilMask(0xFF);

				if (std::find(phong_objs.begin(), phong_objs.end(), obj) != phong_objs.end())
					RenderMain(_camera, _light, { obj });
				else if (std::find(pbr_objs.begin(), pbr_objs.end(), obj) != pbr_objs.end())
					RenderPBR(_camera, _lights, { obj });

				glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
				glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
				glStencilMask(0x00);

				RenderOutlineFocused(_camera, _light, { obj });

				glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
				glStencilFunc(GL_ALWAYS, 0, 0xFF);
				glStencilMask(0xFF);
			}
		}
	}
	else
		focused.clear();

	glPopDebugGroup();

	//Render not outlined objects
	{
		auto getNotOutlined = [&sorter](const std::vector<shObject>& all, const std::vector<shObject>& focused) {
			std::vector<shObject> result;
			std::set_difference(all.begin(), all.end(), focused.begin(), focused.end(), std::back_inserter(result), sorter.GetComparator());
			return result;
		};

		std::vector<shObject> not_outlined = getNotOutlined(phong_objs, focused);
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Main Phong Render");
		RenderMain(_camera, _light, not_outlined);
		glPopDebugGroup();

		not_outlined = getNotOutlined(pbr_objs, focused);
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Main PBR Render");
		RenderPBR(_camera, _lights, not_outlined);
		glPopDebugGroup();

		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Main RenderAreaLightsOnly Render");
		RenderAreaLightsOnly(_camera, _light, arealighted_objs);
		glPopDebugGroup();

		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Main TerrainTessellated Render");
		RenderTerrainTessellated(_camera, _light, terrain_tes_objs);
		glPopDebugGroup();

		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Main Leaf Render");
		renderManager->LeafRender()->Render(_camera, leaf_objs);
		glPopDebugGroup();
	}

	glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Main MeshLine Render");
	//Mesh Line
	if (m_mesh_line_on)
		renderManager->MeshLineRender()->Render(_camera, _light, phong_pbr_objects); //@todo !!!!
	glPopDebugGroup();

	glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Flags");
	//Flags
	if (flags_on) { RenderFlags(_camera, _light, flags); }
	glPopDebugGroup();

	mts ? eGlBufferContext::GetInstance().EnableWrittingBuffer(eBuffer::BUFFER_MTS)
		: eGlBufferContext::GetInstance().EnableWrittingBuffer(eBuffer::BUFFER_DEFAULT);

	glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "RenderWater");
	// Rendering WaterQuad
	if (water) { RenderWater(_camera, _light); }
	glPopDebugGroup();

	glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Geometry");
	// Geometry
	if (geometry)
	{
		if (!geometry_objs.empty())
		{
			for (auto& obj : geometry_objs)
			{
				auto* mesh = dynamic_cast<const SimpleGeometryMesh*>(obj->GetModel()->GetMeshes()[0]);
				RenderGeometry(_camera, *mesh);
			}
		}
	}
	glPopDebugGroup();

	glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Bezier");
	//Bezier
	if (bezier_curve)
	{
		if (!bezier_objs.empty())
		{
			std::vector<const BezierCurveMesh*> meshes;
			for (auto& bezier : bezier_objs)
			{
				for (auto m : bezier->GetModel()->GetMeshes())
				{
					if(auto* mesh = dynamic_cast<const BezierCurveMesh*>(m); mesh)
						meshes.push_back(mesh);
				}
			}
			renderManager->BezierRender()->Render(_camera, meshes);
		}
	}
	glPopDebugGroup();

	glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "LineMesh");
	std::vector<const LineMesh*> line_meshes{};
	for (auto obj : lines_objs)
	{
		const LineMesh* mesh = dynamic_cast<const LineMesh*>(obj->GetModel()->GetMeshes()[0]);
		if (mesh)
			line_meshes.push_back(mesh);
	}
	// Visualize cameras
	if (scene.m_cameras.size() > 1)
	{
		static LineMesh camera_frustum_mesh({}, {}, {});
		std::vector<glm::vec3> extrems_total;
		std::vector<GLuint> indices_total;
		for (int i = 1; i < scene.m_cameras.size(); ++i)
		{
			if (scene.m_cameras[i].VisualiseFrustum())
			{
				std::vector<glm::vec4> cornders = scene.m_cameras[i].getFrustumCornersWorldSpace();
				for (auto& c : cornders)
					extrems_total.push_back(glm::vec3(c));
				indices_total.push_back(2 + (i - 1) * 8);
				indices_total.push_back(0 + (i - 1) * 8);
				indices_total.push_back(0 + (i - 1) * 8);
				indices_total.push_back(4 + (i - 1) * 8);
				indices_total.push_back(4 + (i - 1) * 8);
				indices_total.push_back(6 + (i - 1) * 8);
				indices_total.push_back(6 + (i - 1) * 8);
				indices_total.push_back(2 + (i - 1) * 8);

				indices_total.push_back(0 + (i - 1) * 8);
				indices_total.push_back(1 + (i - 1) * 8);
				indices_total.push_back(2 + (i - 1) * 8);
				indices_total.push_back(3 + (i - 1) * 8);
				indices_total.push_back(4 + (i - 1) * 8);
				indices_total.push_back(5 + (i - 1) * 8);
				indices_total.push_back(6 + (i - 1) * 8);
				indices_total.push_back(7 + (i - 1) * 8);

				indices_total.push_back(1 + (i - 1) * 8);
				indices_total.push_back(3 + (i - 1) * 8);
				indices_total.push_back(3 + (i - 1) * 8);
				indices_total.push_back(7 + (i - 1) * 8);
				indices_total.push_back(7 + (i - 1) * 8);
				indices_total.push_back(5 + (i - 1) * 8);
				indices_total.push_back(5 + (i - 1) * 8);
				indices_total.push_back(1 + (i - 1) * 8);
			}
		}
		camera_frustum_mesh.UpdateData(extrems_total, indices_total, { 1.0f, 1.0f, 0.0f, 1.0f });
		line_meshes.push_back(&camera_frustum_mesh);
	}
	// Bounding boxes
	if (draw_bounding_boxes)
	{
		static LineMesh bounding_boxes_mesh({}, {}, {});
		std::vector<glm::vec3> extrems_total;
		std::vector<GLuint> indices_total;

		for (GLuint i = 0; i < phong_pbr_objects.size(); ++i)
		{
			if (!phong_pbr_objects[i]->GetCollider() || !phong_pbr_objects[i]->GetTransform())
				continue;

			if (auto* terrain = dynamic_cast<ITerrainModel*>(phong_pbr_objects[i]->GetModel()); terrain)
			{
				auto indexSize = extrems_total.size();
				auto vec_of_extrems = terrain->GetExtremsOfMeshesLocalSpace();
				for (GLuint j = 0; j < vec_of_extrems.size(); ++j)
				{
					extrems_total.insert(extrems_total.end(), vec_of_extrems[j].begin(), vec_of_extrems[j].end());
					AddBoundingBoxIndices(indices_total, indexSize + j * 8);
				}
			}
			else
			{
				std::vector<glm::vec3> extrems = phong_pbr_objects[i]->GetCollider()->GetExtrems(*phong_pbr_objects[i]->GetTransform());
				extrems_total.insert(extrems_total.end(), extrems.begin(), extrems.end());
				AddBoundingBoxIndices(indices_total, i * 8);
			}
		}
		// bounding boxes of particle systems
		size_t particles_start_bbox = extrems_total.size();
		for (GLuint i = 0; i < renderManager->GetParticleSystems().size(); ++i)
		{
			std::vector<glm::vec3> extrems = renderManager->GetParticleSystems()[i]->GetExtremsWorldSpace();
			if (!extrems.empty())
			{
				extrems_total.insert(extrems_total.end(), extrems.begin(), extrems.end());
				AddBoundingBoxIndices(indices_total, particles_start_bbox + i * 8);
			}
		}
		bounding_boxes_mesh.UpdateData(extrems_total, indices_total, { 1.0f, 1.0f, 0.0f, 1.0f });
		line_meshes.push_back(&bounding_boxes_mesh);
	}
		renderManager->LinesRender()->Render(_camera, line_meshes);
	glPopDebugGroup();

	glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "RenderSkybox");
	//  Draw skybox firs
	if (skybox)	{ RenderSkybox(_camera); }
	//glDepthFunc(GL_LEQUAL);
	glPopDebugGroup();

	glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Decals");
	if (!m_forward_plus) // forward+ already assigns depth to slot17
	{
		eGlBufferContext::GetInstance().BlitFromTo(mts ? eBuffer::BUFFER_MTS : eBuffer::BUFFER_DEFAULT, eBuffer::BUFFER_DEPTH, GL_DEPTH_BUFFER_BIT);
		eGlBufferContext::GetInstance().EnableReadingBuffer(eBuffer::BUFFER_DEPTH, GL_TEXTURE17);
	}

	mts ? eGlBufferContext::GetInstance().EnableWrittingBuffer(eBuffer::BUFFER_MTS)
		: eGlBufferContext::GetInstance().EnableWrittingBuffer(eBuffer::BUFFER_DEFAULT);

	renderManager.get()->DecalRender()->Render(_camera, scene.m_decals);
	glPopDebugGroup();

	glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Environment cubemaps");
	// Environment cubemaps
	if (m_environment_map)
		RenderEnvironmentSnapshot(buckets, scene.m_cameras, _light);
	glPopDebugGroup();

	glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Transparent PBR objects");
	RenderPBR(_camera, _lights, transparent_objs);
	glPopDebugGroup();

	glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Volumetric data");
	// Volumetric data
	eGlPipelineState::GetInstance().EnableBlend();
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	RenderVolumetric(_camera, _light, volumetric_objs);
	eGlPipelineState::GetInstance().DisableBlend();
	glPopDebugGroup();

	glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Particles");
	//7  Particles
	if (particles) { RenderParticles(_camera); }
	glPopDebugGroup();

	if (mts) // resolving mts to default
	{
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "MTS");
		eGlBufferContext::GetInstance().EnableWrittingBuffer(eBuffer::BUFFER_SCREEN);
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		eGlBufferContext::GetInstance().ResolveMtsToScreen();
		glPopDebugGroup();

		// screen + ssr
		if (ssr)
		{
			glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "SSR");
			RenderSSR(_camera);

			eGlBufferContext::GetInstance().EnableWrittingBuffer(eBuffer::BUFFER_SSR_BLUR);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
			eGlBufferContext::GetInstance().EnableReadingBuffer(eBuffer::BUFFER_SSR, GL_TEXTURE3);
			eGlBufferContext::GetInstance().EnableReadingBuffer(eBuffer::BUFFER_SSR_HIT_MASK, GL_TEXTURE4);
			renderManager->SSRRenderer()->RenderSSRBlur(_camera);//@todo separate render

			// mix ssr with screen buffer
			eGlBufferContext::GetInstance().EnableWrittingBuffer(eBuffer::BUFFER_SCREEN_WITH_SSR);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			renderManager->ScreenRender()->SetTexture(eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_SCREEN));
			renderManager->ScreenRender()->SetTextureContrast(eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_SSR_BLUR));
			renderManager->ScreenRender()->SetTextureMask(eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_SCREEN_MASK));
			renderManager->ScreenRender()->SetRenderingFunction(GUI::RenderFunc::MaskBlend);
			renderManager->ScreenRender()->Render({ 0,0 }, { m_width, m_height },
				{ 0,m_height }, { m_width, 0 }, //@todo y is inverted
				(float)m_width, (float)m_height, GUI::RenderFunc::MaskBlend);
			glPopDebugGroup();
		}

		Texture screen = ssr ? eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_SCREEN_WITH_SSR)
			: eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_SCREEN);

		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Bloom");
		Texture contrast;
		if (m_pb_bloom)//Physicly Based Bloom
		{
			RenderBloom();
			glViewport(0, 0, m_width, m_height);
			contrast = eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_BLOOM);
		}
		else
		{
			RenderBlur(_camera); // gaussian
			contrast = eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_GAUSSIAN_TWO);
		}
		glPopDebugGroup();

		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Contrast");
		eGlBufferContext::GetInstance().EnableWrittingBuffer(eBuffer::BUFFER_DEFAULT);
		glViewport(0, 0, m_width, m_height);
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		eGlPipelineState::GetInstance().DisableDepthTest();
		RenderContrast(_camera, screen, contrast, tick); // blend screen with gaussian (or pb bloom) -> to default
		glPopDebugGroup();
	}

	//Post-processing, need stencil
	if (kernel)
	{
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Kernel");
		eGlBufferContext::GetInstance().BlitFromTo(eBuffer::BUFFER_SCREEN, eBuffer::BUFFER_DEFAULT, GL_STENCIL_BUFFER_BIT);
		eGlPipelineState::GetInstance().EnableStencilTest();

		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
		glStencilFunc(GL_EQUAL, 1, 0xFF);
		glStencilMask(0xFF);

		eGlBufferContext::GetInstance().EnableWrittingBuffer(eBuffer::BUFFER_DEFAULT);
		renderManager->ScreenRender()->SetTexture(eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_SCREEN));
		renderManager->ScreenRender()->RenderKernel();
		glClear(GL_STENCIL_BUFFER_BIT);
		eGlPipelineState::GetInstance().DisableStencilTest();

		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
		glStencilFunc(GL_ALWAYS, 1, 0xFF);
		glStencilMask(0xFF);
		glPopDebugGroup();
	}

	glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "UIlessScreen");
	if (uiless_screen)
		eGlBufferContext::GetInstance().BlitFromTo(eBuffer::BUFFER_DEFAULT, "UIless", GL_COLOR_BUFFER_BIT);
	glPopDebugGroup();

	glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "CameraInterpolationCompute");
	if (camera_interpolation)
		RenderCameraInterpolationCompute(_camera);
	glPopDebugGroup();

	glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Texture visualization");
	//8.1 Texture visualization
	eGlBufferContext::GetInstance().EnableWrittingBuffer(eBuffer::BUFFER_DEFAULT);
	glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	eGlPipelineState::GetInstance().DisableDepthTest();

	if (mousepress && _camera.getCameraRay().IsPressed())
	{
		renderManager->ScreenRender()->RenderFrame(_camera.getCameraRay().GetFrame().first,
																							 _camera.getCameraRay().GetFrame().second,
																							 static_cast<float>(m_width),
																							 static_cast<float>(m_height));
	}

	RenderGui(scene.m_guis, _camera);

	eGlPipelineState::GetInstance().EnableBlend();
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	renderManager->TextRender()->RenderText(_camera, scene.m_texts, static_cast<float>(m_width), static_cast<float>(m_height));
	eGlPipelineState::GetInstance().DisableBlend();

	if (!ui_objs.empty()) // @todo  ugly dynamic cast
		renderManager->ScreenRender()->RenderUI(dynamic_cast<const UIMesh&>(*ui_objs.front()->GetModel()->GetMeshes().front()),
																																				static_cast<float>(m_width), static_cast<float>(m_height));
	glPopDebugGroup();

	m_draw_calls = eGlDrawContext::GetInstance().GetDrawCallsCount();
	m_draw_triangles = eGlDrawContext::GetInstance().GetDrawTrianglesCount();
	eGlDrawContext::GetInstance().Flush();

	if (m_compute_shader)
	{
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Compute shader");
		renderManager->ComputeShaderRender()->DispatchCompute(_camera);
		// set up buffer for writing
		eGlBufferContext::GetInstance().EnableCustomWrittingBuffer("ComputeParticleSystemBuffer");
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		eGlPipelineState::GetInstance().EnableBlend();
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		renderManager->ComputeShaderRender()->RenderComputeResult(_camera);
		glPopDebugGroup();
	}

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

#ifdef STANDALONE
	//render final texture to screen
	Texture total_screen = GetDefaultBufferTexture();
	renderManager->ScreenRender()->SetTexture(total_screen);
	renderManager->ScreenRender()->Render({ 0,0 }, { m_width, m_height },
		{ 0,m_height }, { m_width, 0 }, // y->
		m_width, m_height);
#endif
}

float eOpenGlRenderPipeline::GetWaterHeight() const { return waterHeight; }
bool& eOpenGlRenderPipeline::GetKernelOnRef() { return kernel; }
bool& eOpenGlRenderPipeline::GetSkyNoiseOnRef() { return sky_noise; }
bool& eOpenGlRenderPipeline::GetOutlineFocusedRef() { return outline_focused; }
bool& eOpenGlRenderPipeline::GetUIlessRef() { return uiless_screen; }
bool& eOpenGlRenderPipeline::IBL() { return ibl_on; }
float& eOpenGlRenderPipeline::IBLInfluance() { return ibl_influance; }

//---------------------------------------------------------------------------------------------
void	eOpenGlRenderPipeline::SetSkyBoxTexture(const Texture* _t)
{ renderManager->SkyBoxRender()->SetSkyBoxTexture(_t); }

//---------------------------------------------------------------------------------------------
void eOpenGlRenderPipeline::SetSkyIBL(unsigned int _irr, unsigned int _prefilter)
{
	glActiveTexture(GL_TEXTURE9);
	glBindTexture(GL_TEXTURE_CUBE_MAP, _irr);

	glActiveTexture(GL_TEXTURE10);
	glBindTexture(GL_TEXTURE_CUBE_MAP, _prefilter);
}

//---------------------------------------------------------------------------------------------
void eOpenGlRenderPipeline::AddParticleSystem(std::shared_ptr<IParticleSystem> _system)
{
	renderManager->AddParticleSystem(_system);
}

//---------------------------------------------------------------------------------------------
std::vector<std::shared_ptr<IParticleSystem> > eOpenGlRenderPipeline::GetParticleSystems()
{
	return renderManager->GetParticleSystems();
}

//---------------------------------------------------------------------------------------------
void eOpenGlRenderPipeline::AddParticleSystemGPU(glm::vec3 _startPos, const Texture* _texture)
{
	renderManager->AddParticleSystemGPU(_startPos, _texture);
}

void eOpenGlRenderPipeline::SwitchSkyBox(bool on) { skybox = on; }

void eOpenGlRenderPipeline::SwitchWater(bool on) { water = on; }

//---------------------------------------------------------------------------------------------
const std::vector<ShaderInfo>& eOpenGlRenderPipeline::GetShaderInfos() const
{ return renderManager->GetShaderInfos(); }

//---------------------------------------------------------------------------------------------
void eOpenGlRenderPipeline::UpdateShadersInfo()
{
	renderManager->UpdateShadersInfo();
}

//-------------------------------------------------------------------------------------------
bool eOpenGlRenderPipeline::SetUniformData(const std::string& _renderName, const std::string& _uniformName, const UniformData& _data)
{
	return renderManager.get()->SetUniformData(_renderName, _uniformName, _data);
}

//-------------------------------------------------------------------------------------------
std::function<void(const TessellationRenderingInfo&)> eOpenGlRenderPipeline::GetTessellationInfoUpdater()
{
	return [this](const TessellationRenderingInfo& _info) { renderManager->TerrainTessellatedRender()->UpdateMeshUniforms(_info); };
}

//-------------------------------------------------------------------------------------------
eOpenGlRenderPipeline::eOpenGlRenderPipeline(uint32_t _width, uint32_t _height)
: m_width(_width),
  m_height(_height),
  renderManager(new eRenderManager(_width, _height))
{
}

//-------------------------------------------------------------------------------------------
eOpenGlRenderPipeline::~eOpenGlRenderPipeline()
{
}

//-------------------------------------------------------------------------------------------
void eOpenGlRenderPipeline::UpdateSharedUniforms()
{
	//SetUniformData("class ePBRRender", "Fog.maxDist", m_foginfo.maxDist);
	//SetUniformData("class ePBRRender", "Fog.minDist", m_foginfo.minDist);
	SetUniformData("class ePBRRender", "Fog.color", m_foginfo.color);
	SetUniformData("class ePBRRender", "Fog.fog_on", m_foginfo.fog_on);
	SetUniformData("class ePBRRender", "Fog.density", m_foginfo.density);
	SetUniformData("class ePBRRender", "Fog.gradient", m_foginfo.gradient);

	SetUniformData("class ePBRRender", "max_penumbra", m_max_penumbra);
	SetUniformData("class ePBRRender", "light_radius", m_light_radius);
	SetUniformData("class ePBRRender", "lightSize", m_light_size);
	SetUniformData("class ePBRRender", "pcss_enabled", m_pcss_enabled);
	SetUniformData("class ePBRRender", "pcf_samples", m_pcf_samples);
	SetUniformData("class ePBRRender", "pcf_texture_sample_radius", m_pcf_texture_sample_radius);
	SetUniformData("class ePBRRender", "pcf_texture_sample_enabled", m_pcf_texture_sample_enabled);
	SetUniformData("class ePBRRender", "cascade_blend_distance", m_cascade_blend_distance);
	SetUniformData("class ePBRRender", "blendCascades", m_blend_cascades);
	SetUniformData("class ePBRRender", "max_shadow", m_max_shadow);
	SetUniformData("class ePBRRender", "csm_base_slope_bias", m_csm_base_slope_bias);
	SetUniformData("class ePBRRender", "csm_base_cascade_plane_bias", m_csm_base_cascade_plane_bias);

	SetUniformData("class ePBRRender", "ibl_on", ibl_on);
	SetUniformData("class ePBRRender", "ibl_influance", ibl_influance);
	SetUniformData("class ePBRRender", "forward_plus", m_forward_plus);
	SetUniformData("class ePBRRender", "numberOfTilesX", (size_t)m_width / 16);

	//SetUniformData("class ePhongRender", "Fog.maxDist", m_foginfo.maxDist);
	//SetUniformData("class ePhongRender", "Fog.minDist", m_foginfo.minDist);
	SetUniformData("class ePhongRender", "Fog.color", m_foginfo.color);
	SetUniformData("class ePhongRender", "Fog.fog_on", m_foginfo.fog_on);
	SetUniformData("class ePhongRender", "Fog.density", m_foginfo.density);
	SetUniformData("class ePhongRender", "Fog.gradient", m_foginfo.gradient);

	SetUniformData("class ePhongRender", "max_penumbra", m_max_penumbra);
	SetUniformData("class ePhongRender", "light_radius", m_light_radius);
	SetUniformData("class ePhongRender", "lightSize", m_light_size);
	SetUniformData("class ePhongRender", "pcss_enabled", m_pcss_enabled);
	SetUniformData("class ePhongRender", "pcf_samples", m_pcf_samples);
	SetUniformData("class ePhongRender", "pcf_texture_sample_radius", m_pcf_texture_sample_radius);
	SetUniformData("class ePhongRender", "pcf_texture_sample_enabled", m_pcf_texture_sample_enabled);
	SetUniformData("class ePhongRender", "cascade_blend_distance", m_cascade_blend_distance);
	SetUniformData("class ePhongRender", "blendCascades", m_blend_cascades);
	SetUniformData("class ePhongRender", "max_shadow", m_max_shadow);
	SetUniformData("class ePhongRender", "csm_base_slope_bias", m_csm_base_slope_bias);
	SetUniformData("class ePhongRender", "csm_base_cascade_plane_bias", m_csm_base_cascade_plane_bias);

	renderManager->PhongRender()->GetZPrePass() = m_z_pre_pass;
	renderManager->PBRRender()->GetZPrePass() = m_z_pre_pass;
}

//-------------------------------------------------------------------------------------------
void eOpenGlRenderPipeline::Initialize()
{
	eGlPipelineState::GetInstance().EnableDepthTest();
	eGlPipelineState::GetInstance().EnableStencilTest();
	eGlPipelineState::GetInstance().EnableCullFace();
	eGlPipelineState::GetInstance().EnableMultisample();
	eGlPipelineState::GetInstance().EnableLineSmooth();

	eGlPipelineState::GetInstance().EnableDebugging();
	glDebugMessageCallback(
		[](GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
			const GLchar* message, const void* userParam) {

				if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) // Ignore notifications to reduce clutter
					return;

				if (type == 33361) // info static draw
					return;
				else if (type == 33350 || type == 33360) // shader recompiled
					return;

					std::cout << "OpenGL Debug Message\n";
					std::cout << "Source: " << source << "\n";
					std::cout << "Type: " << type << "\n";
					std::cout << "ID: " << id << "\n";
					std::cout << "Severity: " << severity << "\n";
					std::cout << "Message: " << message << "\n\n";

		},
		nullptr);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

	/*GlPipelineState::GetInstance().EnableBlend();
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);*/
}

//-----------------------------------------------------------------------------------------------------
void eOpenGlRenderPipeline::InitializeBuffers(FboBits fboMask, SsboBits ssboMask)
{
	auto& ctx = eGlBufferContext::GetInstance();

	//----------------------------------------
	// FBO Initialization
	//----------------------------------------
	struct FboInitEntry
	{
		FboBits bit;
		void (*init)(eGlBufferContext&, int w, int h);
	};

	static const FboInitEntry fboTable[] =
	{
		{ FboBits::Default,           [](auto& c, int w, int h) { c.BufferInit(eBuffer::BUFFER_DEFAULT, w, h); }},
		{ FboBits::Screen,            [](auto& c, int w, int h) { c.BufferInit(eBuffer::BUFFER_SCREEN, w, h); }},
		{ FboBits::ScreenWithSSR,     [](auto& c, int w, int h) { c.BufferInit(eBuffer::BUFFER_SCREEN_WITH_SSR, w, h); }},
		{ FboBits::MTS,               [](auto& c, int w, int h) { c.BufferInit(eBuffer::BUFFER_MTS, w, h); }},
		{ FboBits::Reflection,        [](auto& c, int w, int h) { c.BufferInit(eBuffer::BUFFER_REFLECTION, w, h); }},
		{ FboBits::Refraction,        [](auto& c, int w, int h) { c.BufferInit(eBuffer::BUFFER_REFRACTION, w, h); }},
		{ FboBits::SSR,               [](auto& c, int w, int h) { c.BufferInit(eBuffer::BUFFER_SSR, w, h); }},
		{ FboBits::SSRBlur,           [](auto& c, int w, int h) { c.BufferInit(eBuffer::BUFFER_SSR_BLUR, w, h); }},
		{ FboBits::ShadowDir,         [](auto& c, int w, int h) { c.BufferInit(eBuffer::BUFFER_SHADOW_DIR, w * 2, h * 2); }},
		{ FboBits::Depth,             [](auto& c, int w, int h) { c.BufferInit(eBuffer::BUFFER_DEPTH, w, h); }},
		{ FboBits::ShadowCube,        [](auto& c, int w, int h) { c.BufferInit(eBuffer::BUFFER_SHADOW_CUBE_MAP, w * 2, h * 2); }},
		{ FboBits::ShadowCSM,         [](auto& c, int, int) { c.BufferInit(eBuffer::BUFFER_SHADOW_CSM, 2048, 2048); }},
		{ FboBits::Square,            [](auto& c, int, int h) { c.BufferInit(eBuffer::BUFFER_SQUERE, h, h); }},
		{ FboBits::BrightFilter,      [](auto& c, int w, int h) { c.BufferInit(eBuffer::BUFFER_BRIGHT_FILTER, w, h); }},
		{ FboBits::Gaussian1,         [](auto& c, int w, int h) { c.BufferInit(eBuffer::BUFFER_GAUSSIAN_ONE, w / 2, h / 2); }},
		{ FboBits::Gaussian2,         [](auto& c, int w, int h) { c.BufferInit(eBuffer::BUFFER_GAUSSIAN_TWO, w / 2, h / 2); }},
		{ FboBits::Deferred,          [](auto& c, int w, int h) { c.BufferInit(eBuffer::BUFFER_DEFFERED, w, h); }},
		{ FboBits::SSAO,              [](auto& c, int w, int h) { c.BufferInit(eBuffer::BUFFER_SSAO, w, h); }},
		{ FboBits::SSAOBlur,          [](auto& c, int w, int h) { c.BufferInit(eBuffer::BUFFER_SSAO_BLUR, w, h); }},
		{ FboBits::IBLCubemap,        [](auto& c, int, int) { c.BufferInit(eBuffer::BUFFER_IBL_CUBEMAP, 512, 512); }},
		{ FboBits::IBLCubemapIrr,     [](auto& c, int, int) { c.BufferInit(eBuffer::BUFFER_IBL_CUBEMAP_IRR, 32, 32); }},
		{ FboBits::EnvironmentCubemap,[](auto& c, int, int) { c.BufferInit(eBuffer::BUFFER_ENVIRONMENT_CUBEMAP, 512, 512); }},
		{ FboBits::Bloom,             [](auto& c, int w, int h) { c.BufferInit(eBuffer::BUFFER_BLOOM, w, h); }}
	};

	for (const auto& entry : fboTable)
	{
		if (HasFlag(fboMask, entry.bit))
			entry.init(ctx, m_width, m_height);
	}

	//----------------------------------------
	// Custom Buffer Initialization
	//----------------------------------------
	struct CustomBufferInitEntry
	{
		FboBits bit;
		const char* name;
	};

	static const CustomBufferInitEntry customBufferTable[] =
	{
		{ FboBits::CameraInterpolationBuffer, "CameraInterpolationCoordsBuffer" },
		{ FboBits::ComputeParticleBuffer,     "ComputeParticleSystemBuffer" },
		{ FboBits::UIlessBuffer,              "UIless" }
	};

	for (const auto& entry : customBufferTable)
	{
		if (HasFlag(fboMask, entry.bit))
		{
			ctx.BufferCustomInit(entry.name, m_width, m_height);
		}
	}

	//----------------------------------------
	// SSBO Initialization
	//----------------------------------------
	if (HasFlag(ssboMask, SsboBits::ModelToProjection))
		ctx.BufferSSBOInitMat4(eSSBO::MODEL_TO_PROJECTION_MATRIX, 6'150, false);

	if (HasFlag(ssboMask, SsboBits::ModelToWorld))
		ctx.BufferSSBOInitMat4(eSSBO::MODEL_TO_WORLD_MATRIX, 6'150, false);

	if (HasFlag(ssboMask, SsboBits::InstancedInfo))
		ctx.BufferSSBOInitMat4(eSSBO::INSTANCED_INFO_MATRIX, 6'150, false);

	if (HasFlag(ssboMask, SsboBits::HeraldryInfo))
		ctx.BufferSSBOInitVec4(eSSBO::HERALDRY_INSTANCED_INFO, 1'000, false);

	if (HasFlag(ssboMask, SsboBits::BonesPacked))
		ctx.BufferSSBOInitMat4(eSSBO::BONES_PACKED, 1'000, false);

	if (HasFlag(ssboMask, SsboBits::BoneBaseIndexes))
		ctx.BufferSSBOInitInt(eSSBO::BONE_BASE_INDEX, 1'000, false);
}

//-----------------------------------------------------------------------------------------------
void eOpenGlRenderPipeline::InitializeRenders(eModelManager& modelManager, eTextureManager& texManager, const std::string& shadersFolderPath)
{
	renderManager->Initialize(modelManager, texManager, shadersFolderPath);
	m_texture_manager = &texManager;
}

//-----------------------------------------------------------------------------------------------
Texture eOpenGlRenderPipeline::GetSkyNoiseTexture(const Camera& _camera)
{
	eGlBufferContext::GetInstance().EnableWrittingBuffer(eBuffer::BUFFER_SQUERE);
	RenderSkyNoise(_camera); //do we need camera?
	return eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_SQUERE);
}

float& eOpenGlRenderPipeline::GetSaoThresholdRef() { return renderManager->GetSsaoThresholdRef(); }
float& eOpenGlRenderPipeline::GetSaoStrengthRef() { return renderManager->GetSsaoStrengthRef(); }

float& eOpenGlRenderPipeline::GetExposureRef()
{
	return renderManager->ScreenRender()->GetExposure();
}

bool& eOpenGlRenderPipeline::GetAutoExposure()
{
	return m_auto_exposure;
}

float& eOpenGlRenderPipeline::GetTargetLuminance()
{
	return m_target_luminance;
}

float& eOpenGlRenderPipeline::GetAdaptionRate()
{
	return m_adaption_rate;
}


bool& eOpenGlRenderPipeline::GetGammaCorrectionRef()
{
	//@todo screen render as well
	return renderManager->PhongRender()->GetGammaCorrection();
}

int32_t& eOpenGlRenderPipeline::GetToneMappingRef()
{
	return renderManager->ScreenRender()->GetToneMappingIndex();
}

bool& eOpenGlRenderPipeline::GetDebugWhite()
{
	return renderManager->PhongRender()->GetDebugWhite();
}

bool& eOpenGlRenderPipeline::GetDebugTexCoords()
{
	return renderManager->PhongRender()->GetDebugTextCoords();
}

float& eOpenGlRenderPipeline::GetEmissionStrengthRef()
{
	return renderManager->PhongRender()->GetEmissionStrength();
}

float& eOpenGlRenderPipeline::WaveSpeedFactor()
{
	return renderManager->WaterRender()->WaveSpeedFactor();
}

float& eOpenGlRenderPipeline::Tiling()
{
	return renderManager->WaterRender()->Tiling();
}

float& eOpenGlRenderPipeline::WaveStrength()
{
	return renderManager->WaterRender()->WaveStrength();
}

float& eOpenGlRenderPipeline::ShineDumper()
{
	return renderManager->WaterRender()->ShineDumper();
}

float& eOpenGlRenderPipeline::Reflactivity()
{
	return renderManager->WaterRender()->Reflactivity();
}

glm::vec4& eOpenGlRenderPipeline::WaterColor()
{
	return renderManager->WaterRender()->WaterColor();
}

float& eOpenGlRenderPipeline::ColorMix()
{
	return renderManager->WaterRender()->ColorMix();
}

float& eOpenGlRenderPipeline::RefrectionFactor()
{
	return renderManager->WaterRender()->RefrectionFactor();
}

float& eOpenGlRenderPipeline::DistortionStrength()
{
	return renderManager->WaterRender()->DistortionStrength();
}

float& eOpenGlRenderPipeline::ZMult()
{
	return renderManager->CSMRender()->ZMult();
}

float& eOpenGlRenderPipeline::GetFirstCascadePlaneDistance()
{
	return renderManager->CSMRender()->GetFirstCascadePlaneDistance();
}

float& eOpenGlRenderPipeline::GetLightPlacementCoef()
{
	return renderManager->CSMRender()->GetLightPlacementCoef();
}

float& eOpenGlRenderPipeline::GetCascadeBlendDistance()
{
	return m_cascade_blend_distance;
}

bool& eOpenGlRenderPipeline::BlendCascades()
{
	return m_blend_cascades;
}

float& eOpenGlRenderPipeline::GetMaxShadow(){return m_max_shadow;}

float& eOpenGlRenderPipeline::Step()
{
	return renderManager->SSRRenderer()->Step();
}

float& eOpenGlRenderPipeline::MinRayStep()
{
	return renderManager->SSRRenderer()->MinRayStep();
}

float& eOpenGlRenderPipeline::MaxSteps()
{
	return renderManager->SSRRenderer()->MaxSteps();
}

int& eOpenGlRenderPipeline::NumBinarySearchSteps()
{
	return renderManager->SSRRenderer()->NumBinarySearchSteps();
}

float& eOpenGlRenderPipeline::ReflectionSpecularFalloffExponent()
{
	return renderManager->SSRRenderer()->ReflectionSpecularFalloffExponent();
}

glm::vec3& eOpenGlRenderPipeline::GetSecondCameraPositionRef()
{
	return renderManager->CameraInterpolationRender()->GetSecondCameraPositionRef();
}

float& eOpenGlRenderPipeline::GetDisplacementRef()
{
	return renderManager->CameraInterpolationRender()->GetDisplacementRef();
}

glm::mat4& eOpenGlRenderPipeline::GetLookAtMatrix()
{
	return renderManager->CameraInterpolationRender()->GetLookAtMatrix();
}

glm::mat4& eOpenGlRenderPipeline::GetProjectionMatrix()
{
	return renderManager->CameraInterpolationRender()->GetProjectionMatrix();
}

glm::mat4& eOpenGlRenderPipeline::GetLookAtProjectedMatrix()
{
	return renderManager->CameraInterpolationRender()->GetLookAtProjectedMatrix();
}

float& eOpenGlRenderPipeline::GetPenumbraRef() { return m_max_penumbra;  }
float& eOpenGlRenderPipeline::GetLightRadiusRef() { return m_light_radius; }
float& eOpenGlRenderPipeline::GetLightSizeRef() { return m_light_size; }
bool& eOpenGlRenderPipeline::GetPCSSEnabledRef(){ return m_pcss_enabled; }
size_t& eOpenGlRenderPipeline::GetPcfSamples() { return m_pcf_samples; }
bool& eOpenGlRenderPipeline::GetCSMCullingEnabled() { return m_csm_culling_enabled; }
bool& eOpenGlRenderPipeline::GetCSMCullingFront() { return m_csm_fromt_face_cull; }
bool& eOpenGlRenderPipeline::ForwardPlusPipeline() { return m_forward_plus; }
bool& eOpenGlRenderPipeline::ZPrePassPipeline() { return m_z_pre_pass; }
bool& eOpenGlRenderPipeline::IsFrustumCull() { return m_frustum_cull; }

float& eOpenGlRenderPipeline::PcfTextureSampleRadiusRef(){return m_pcf_texture_sample_radius;}
bool& eOpenGlRenderPipeline::PcfTextureSampleEnabledRef(){return m_pcf_texture_sample_enabled;}

float& eOpenGlRenderPipeline::GetCsmBaseSlopeBias() { return m_csm_base_slope_bias; }
float& eOpenGlRenderPipeline::GetCsmBaseCascadePlaneBias() { return m_csm_base_cascade_plane_bias; }
float& eOpenGlRenderPipeline::GetCsmPolygonOffset() { return m_csm_polygon_offset; }

size_t& eOpenGlRenderPipeline::KernelSize() { return renderManager->GaussianBlurRender()->KernelSize(); }
float& eOpenGlRenderPipeline::SampleSize() { return renderManager->GaussianBlurRender()->SampleSize(); }
float& eOpenGlRenderPipeline::BrightnessAmplifier() { return renderManager->BrightFilterRender()->Amplifier(); }

float& eOpenGlRenderPipeline::Metallic()
{
	return renderManager->SSRRenderer()->Metallic();
}

float& eOpenGlRenderPipeline::Spec()
{
	return renderManager->SSRRenderer()->Spec();
}

glm::vec4& eOpenGlRenderPipeline::Scale()
{
	return renderManager->SSRRenderer()->Scale();
}

float& eOpenGlRenderPipeline::K()
{
	return renderManager->SSRRenderer()->K();
}

//------------------------------------------------------
bool& eOpenGlRenderPipeline::EnvironmentMap(){return m_environment_map;}

//---------------------------------------------------------------------------
float& eOpenGlRenderPipeline::Noize3DZDebug()
{
	return renderManager->ComputeShaderRender()->WorleyNoiseZDebug();
}

int32_t& eOpenGlRenderPipeline::Noize3DOctaveDebug()
{
	return renderManager->ComputeShaderRender()->Noize3DOctaveDebug();
}

int32_t& eOpenGlRenderPipeline::GetCloudDensity()
{
	return renderManager->VolumetricRender()->GetCloudDensity();
}

int32_t& eOpenGlRenderPipeline::GetCloudAbsorption()
{
	return renderManager->VolumetricRender()->GetCloudAbsorption();
}

glm::vec3& eOpenGlRenderPipeline::GetCloudColor()
{
	return renderManager->VolumetricRender()->GetCloudColor();
}

float& eOpenGlRenderPipeline::GetCloudPerlinWeight()
{
	return renderManager->VolumetricRender()->GetPerilnWeight();
}

int32_t& eOpenGlRenderPipeline::GetCloudPerlinMotion()
{
	return renderManager->VolumetricRender()->GetCloudPerlinMotion();
}

int32_t& eOpenGlRenderPipeline::GetCloudWorleyMotion()
{
	return renderManager->VolumetricRender()->GetCloudWorleyMotion();
}

float& eOpenGlRenderPipeline::GetCloudGParam()
{
	return renderManager->VolumetricRender()->GetGParam();
}

glm::vec3& eOpenGlRenderPipeline::GetNoiseScale()
{
	return renderManager->VolumetricRender()->GetNoiseScale();
}

bool& eOpenGlRenderPipeline::GetApplyPowder()
{
	return renderManager->VolumetricRender()->GetApplyPowder();
}

float& eOpenGlRenderPipeline::GetSilverLiningDensity()
{
	return renderManager->VolumetricRender()->GetSilverLiningDensity();
}

int32_t& eOpenGlRenderPipeline::GetSilverLiningStrength()
{
	return renderManager->VolumetricRender()->GetSilverLiningStrength();
}

float& eOpenGlRenderPipeline::GetAlphathreshold()
{
	return renderManager->VolumetricRender()->GetAlphathreshold();
}

bool& eOpenGlRenderPipeline::GetCloudSilverLining()
{
	return renderManager->VolumetricRender()->GetSilverLining();
}

int32_t& eOpenGlRenderPipeline::GetWorleyOctaveSizeOne()
{
	return renderManager->ComputeShaderRender()->GetOctaveSizeOne();
}

int32_t& eOpenGlRenderPipeline::GetWorleyOctaveSizeTwo()
{
	return renderManager->ComputeShaderRender()->GetOctaveSizeTwo();
}

int32_t& eOpenGlRenderPipeline::GetWorleyOctaveSizeThree()
{
	return renderManager->ComputeShaderRender()->GetOctaveSizeThree();
}

int32_t& eOpenGlRenderPipeline::GetWorleyNoiseGamma()
{
	return renderManager->ComputeShaderRender()->GetNoiseGamma();
}

void eOpenGlRenderPipeline::RedoWorleyNoise()
{
	renderManager->ComputeShaderRender()->RedoNoise();
}

//------------------------------------------------------------------------------------------------------------------
void eOpenGlRenderPipeline::RenderShadows(const Camera& _camera, const Light& _light, std::vector<shObject>& _objects, bool _depth)
{
	// Bind the "depth only" FBO and set the viewport to the size of the depth texture
	glm::ivec2 size;
	if (_light.type == eLightType::POINT)
	{
		size = eGlBufferContext::GetInstance().GetSize(eBuffer::BUFFER_SHADOW_CUBE_MAP);
	}
	else // dir ? 
	{
		size = eGlBufferContext::GetInstance().GetSize(eBuffer::BUFFER_SHADOW_DIR);
	}
	glViewport(0, 0, size.x, size.y);
	
	eGlPipelineState::GetInstance().EnableCullFace();
	glCullFace(GL_FRONT);

	eGlPipelineState::GetInstance().EnableDepthTest();
	glDepthFunc(GL_LESS);

	if (_depth)
	{
		glCullFace(GL_BACK);
		glViewport(0, 0, m_width, m_height);
		renderManager->ShadowRender()->RenderDepthBuffer(_camera, _light, _objects);
	}
	else
	{
		renderManager->ShadowRender()->Render(_camera, _light, _objects);
	}
	glCullFace(GL_BACK);
	glViewport(0, 0, m_width, m_height);
}

void eOpenGlRenderPipeline::RenderShadowsCSM(const Camera& _camera, const Light& _light, std::vector<shObject>& _objects)
{
	glm::ivec2 size = eGlBufferContext::GetInstance().GetSize(eBuffer::BUFFER_SHADOW_CSM);
	glViewport(0, 0, size.x, size.y);

	if(!m_csm_culling_enabled)
		eGlPipelineState::GetInstance().DisableCullFace();

	if (m_csm_fromt_face_cull)
		glCullFace(GL_FRONT);
	else
		glCullFace(GL_BACK);

	// Clear
	eGlPipelineState::GetInstance().EnableDepthTest();
	glDepthFunc(GL_LESS);

	//glClearDepth(1.0f);
// Enable polygon offset to resolve depth-fighting isuses 
	/*glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(m_csm_polygon_offset, m_csm_polygon_offset);*/

	eGlPipelineState::GetInstance().EnableDepthClamp();
	renderManager->CSMRender()->Render(_camera, _light, _objects);
	eGlPipelineState::GetInstance().DisableDepthClamp();

	/*glDisable(GL_POLYGON_OFFSET_FILL);*/
	glCullFace(GL_BACK);
	eGlPipelineState::GetInstance().EnableCullFace();

	glViewport(0, 0, m_width, m_height);

}

void eOpenGlRenderPipeline::RenderSkybox(const Camera& _camera)
{
	glDepthMask(GL_FALSE);
	glDepthFunc(GL_LEQUAL);
	renderManager->SkyBoxRender()->Render(_camera);
	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);
}

void eOpenGlRenderPipeline::RenderReflection(Camera& _camera, const std::vector<Light>& _lights, std::vector<shObject>& _phong_objects, std::vector<shObject>& _pbr_objects)
{
	renderManager->PhongRender()->SetClipPlane(-waterHeight);

	Camera temp_cam = _camera;
	_camera.setPosition(glm::vec3(temp_cam.getPosition().x, waterHeight - (temp_cam.getPosition().y - waterHeight), temp_cam.getPosition().z));
	_camera.setDirection(glm::reflect(_camera.getDirection(), glm::vec3(0, 1, 0))); //water normal

	renderManager->PhongRender()->Render(_camera, _lights[0], _phong_objects);
	renderManager->PBRRender()->Render(_camera, _lights, _pbr_objects);
	_camera = temp_cam;
}

void eOpenGlRenderPipeline::RenderRefraction(Camera& _camera, const std::vector<Light>& _lights, std::vector<shObject>& _phong_objects, std::vector<shObject>& _pbr_objects)
{
	renderManager->PhongRender()->SetClipPlane(waterHeight);
	renderManager->PhongRender()->Render(_camera, _lights[0], _phong_objects);
	renderManager->PBRRender()->Render(_camera, _lights, _pbr_objects);
	renderManager->PhongRender()->SetClipPlane(10);

	//glDisable(GL_CLIP_DISTANCE0);
}

void eOpenGlRenderPipeline::RenderSkyNoise(const Camera& _camera)
{
	//sky noise
	eGlPipelineState::GetInstance().DisableCullFace();
	eGlPipelineState::GetInstance().EnableBlend();
	renderManager->SkyNoiseRender()->Render(_camera);
	eGlPipelineState::GetInstance().DisableBlend();
	eGlPipelineState::GetInstance().EnableCullFace();
}

void eOpenGlRenderPipeline::RenderMain(const Camera& _camera, const Light& _light, const std::vector<shObject>& _objects)
{
	renderManager->PhongRender()->Render(_camera, _light, _objects);
}

void eOpenGlRenderPipeline::RenderAreaLightsOnly(const Camera& _camera, const Light& _light, const std::vector<shObject>& _objects)
{
	renderManager->AreaLightsRender()->Render(_camera, _light, _objects);
}

void eOpenGlRenderPipeline::RenderOutlineFocused(const Camera& _camera, const Light& _light, const std::vector<shObject>& focused)
{
	//5. Rendering Stencil Outlineing
	if(!focused.empty())
	{
		renderManager->OutlineRender()->Render(_camera, _light, focused);
	}
}

void eOpenGlRenderPipeline::RenderFlags(const Camera& _camera, const Light& _light, std::vector<shObject> flags)
{
	eGlPipelineState::GetInstance().DisableCullFace();
	renderManager->PhongRender()->RenderWaves(_camera, _light, flags);
	eGlPipelineState::GetInstance().EnableCullFace();
}

void eOpenGlRenderPipeline::RenderWater(const Camera& _camera, const Light& _light)
{
	eGlPipelineState::GetInstance().DisableCullFace();
	renderManager->WaterRender()->Render(_camera, _light);
	eGlPipelineState::GetInstance().EnableCullFace();
}

void eOpenGlRenderPipeline::RenderGeometry(const Camera& _camera, const SimpleGeometryMesh& _mesh)
{
	glm::mat4 MVP = _camera.getProjectionMatrix() * _camera.getWorldToViewMatrix();
	renderManager->HexRender()->Render(MVP, const_cast<SimpleGeometryMesh&>(_mesh));//@ Draw is not const func unfortunately
}

void eOpenGlRenderPipeline::RenderParticles(const Camera& _camera)
{
	eGlPipelineState::GetInstance().EnableBlend();
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glDepthMask(GL_FALSE);
	glCullFace(GL_BACK);

	renderManager->ParticleRender()->Render(_camera);
	if(renderManager->ParticleRenderGPU())
		renderManager->ParticleRenderGPU()->Render(_camera);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	eGlPipelineState::GetInstance().DisableBlend();
	glDepthMask(GL_TRUE);
}

//-------------------------------------------------------
void eOpenGlRenderPipeline::RenderBloom()
{
	if (m_bloom_threshold) // threshold
	{
		renderManager->BrightFilterRender()->SetTexture(eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_SCREEN));
		eGlBufferContext::GetInstance().EnableWrittingBuffer(eBuffer::BUFFER_BRIGHT_FILTER);
		glViewport(0, 0, m_width, m_height);
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		renderManager->BrightFilterRender()->Render();

		renderManager->ScreenRender()->GetAutoExposure() = m_auto_exposure;
		if (m_auto_exposure)
		{
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_BRIGHT_FILTER_MASK).m_id);
			int mipLevels = static_cast<int>(std::floor(std::log2(m_width))) + 1;
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mipLevels);
			glGenerateMipmap(GL_TEXTURE_2D);
			
			renderManager->ScreenRender()->GetTargetLuminance() = m_target_luminance;
			renderManager->ScreenRender()->GetAdaptionRate() = m_adaption_rate;
		}
	}

	// Bind srcTexture (HDR color buffer) as initial texture input
	eGlBufferContext::GetInstance().EnableWrittingBuffer(eBuffer::BUFFER_BLOOM);
	//glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT); // ?
	eGlBufferContext::GetInstance().EnableReadingBuffer(m_bloom_threshold ? eBuffer::BUFFER_BRIGHT_FILTER : eBuffer::BUFFER_SCREEN, GL_TEXTURE0);
	renderManager->BloomRenderer()->RenderDownsamples(eGlBufferContext::GetInstance().MipChain(), glm::vec2{ (float)m_width , (float)m_height });

	// Enable additive blending !!!
	eGlPipelineState::GetInstance().EnableBlend();
	glBlendFunc(GL_ONE, GL_ONE);
	glBlendEquation(GL_FUNC_ADD);
	renderManager->BloomRenderer()->RenderUpsamples(eGlBufferContext::GetInstance().MipChain());
	// Disable additive blending !!!!!
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // Restore if this was default
	eGlPipelineState::GetInstance().DisableBlend();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

//-------------------------------------------------------
void eOpenGlRenderPipeline::RenderTerrainTessellated(const Camera& _camera, const Light& _light, std::vector<shObject> _objs)
{
	if(!_objs.empty())
		renderManager->TerrainTessellatedRender()->Render(_camera, _light, _objs);
}

//-------------------------------------------------------
void eOpenGlRenderPipeline::RenderVolumetric(const Camera& _camera, const Light& _light, std::vector<shObject> _objs)
{
	if(!_objs.empty())
		renderManager->VolumetricRender()->Render(_camera, _light, _objs);
}

//-------------------------------------------------------
void eOpenGlRenderPipeline::LoadPCFOffsetsTextureToShaders()
{
	const Texture* pcf = m_texture_manager->Find("TPCF");
	if (pcf != nullptr)
	{
		glActiveTexture(GL_TEXTURE15);
		glBindTexture(GL_TEXTURE_3D, pcf->m_id);
		SetUniformData("class ePBRRender", "pcf_OffsetTexSize", glm::vec4(pcf->m_height, pcf->m_height, pcf->m_width * pcf->m_height / 2.0f, 1.0f));
		SetUniformData("class ePhong", "pcf_OffsetTexSize", glm::vec4(pcf->m_height, pcf->m_height, pcf->m_width * pcf->m_height / 2.0f, 1.0f));
	}
}

//-------------------------------------------------------
void eOpenGlRenderPipeline::RenderBlur(const Camera& _camera)
{
	renderManager->BrightFilterRender()->SetTexture(eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_SCREEN));
	eGlBufferContext::GetInstance().EnableWrittingBuffer(eBuffer::BUFFER_BRIGHT_FILTER);
	glViewport(0, 0, m_width, m_height);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	renderManager->BrightFilterRender()->Render();
	renderManager->GaussianBlurRender()->SetTexture(eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_BRIGHT_FILTER));
	renderManager->GaussianBlurRender()->Render();
}

//------------------------------------------------
void eOpenGlRenderPipeline::RenderContrast(const Camera& _camera, const Texture& _screen, const Texture& _contrast, float _tick)
{
	renderManager->ScreenRender()->SetTexture(_screen);
	renderManager->ScreenRender()->SetTextureContrast( _contrast);
	renderManager->ScreenRender()->RenderContrast(_camera, blur_coef, _tick, eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_BRIGHT_FILTER_MASK).m_id);
}

//------------------------------------------------
void eOpenGlRenderPipeline::RenderGui(const std::vector<std::shared_ptr<GUI>>& guis, const Camera& _camera)
{
	for(const auto& gui : guis)
	{
		if (gui->GetTexture() && gui->IsVisible())
		{
			if (gui->IsTransparent())
			{
				eGlPipelineState::GetInstance().EnableBlend();
				eGlPipelineState::GetInstance().SetBlendFunctionEditive();
			}
			else
			{
				eGlPipelineState::GetInstance().EnableBlend();
				eGlPipelineState::GetInstance().SetBlendFunctionDefault();
			}

			renderManager->ScreenRender()->SetTexture(*(gui->GetTexture())); //copy texture
			renderManager->ScreenRender()->SetRotationAngle(gui->GetRotationAngle());
			renderManager->ScreenRender()->SetTextureMask(*(gui->GetTextureMask()));
			/*renderManager->ScreenRender()->SetRenderingFunction(gui->GetRenderingFunc());*/
			renderManager->ScreenRender()->Render(gui->GetTopLeft(),				gui->GetBottomRight(),
																						gui->GetTopLeftTexture(), gui->GetBottomRightTexture(),
																						static_cast<float>(m_width), static_cast<float>(m_height), gui->GetRenderingFunc());
		}

		gui->UpdateSync();
		for (auto& child : gui->GetChildren())
		{
			if (child->GetTexture() && child->IsVisible())
			{
				if (child->IsTransparent())
				{
					eGlPipelineState::GetInstance().EnableBlend();
					eGlPipelineState::GetInstance().SetBlendFunctionEditive();
				}
				else
				{
					eGlPipelineState::GetInstance().EnableBlend();
					eGlPipelineState::GetInstance().SetBlendFunctionDefault();
				}

				renderManager->ScreenRender()->SetTexture(*(child->GetTexture())); //copy texture
				renderManager->ScreenRender()->SetRotationAngle(child->GetRotationAngle());
				renderManager->ScreenRender()->SetTextureMask(*(child->GetTextureMask()));
				/*renderManager->ScreenRender()->SetRenderingFunction(gui->GetRenderingFunc());*/
				renderManager->ScreenRender()->Render(child->GetTopLeft(), child->GetBottomRight(),
																							child->GetTopLeftTexture(), child->GetBottomRightTexture(), 
																							static_cast<float>(m_width), static_cast<float>(m_height), gui->GetRenderingFunc());
			}
			child->UpdateSync();
		}
	}

	eGlPipelineState::GetInstance().DisableBlend();
}

//------------------------------------------------
void eOpenGlRenderPipeline::RenderPBR(const Camera& _camera, const std::vector<Light>& _lights, std::vector<shObject> _objs)
{
	if (!_objs.empty())
	{
		renderManager->PBRRender()->Render(_camera, _lights, _objs);
	}
}

//-------------------------------------------------------
void eOpenGlRenderPipeline::RenderSSAO(const Camera& _camera, const Light& _light, std::vector<shObject>& _objects)
{
	eGlBufferContext::GetInstance().EnableWrittingBuffer(eBuffer::BUFFER_SSAO);
	glClear(GL_COLOR_BUFFER_BIT);
	eGlBufferContext::GetInstance().EnableReadingBuffer(eBuffer::BUFFER_DEFFERED, GL_TEXTURE2);
	eGlBufferContext::GetInstance().EnableReadingBuffer(eBuffer::BUFFER_DEFFERED1, GL_TEXTURE3);
	renderManager->SSAORender()->RenderSSAO(_camera);
	eGlBufferContext::GetInstance().EnableWrittingBuffer(eBuffer::BUFFER_SSAO_BLUR);
	glClear(GL_COLOR_BUFFER_BIT);
	eGlBufferContext::GetInstance().EnableReadingBuffer(eBuffer::BUFFER_SSAO, GL_TEXTURE2);
	renderManager->SSAORender()->RenderSSAOBlur(_camera);//@to separate render
}

//-------------------------------------------------------
void eOpenGlRenderPipeline::RenderSSR(const Camera& _camera)
{
	eGlBufferContext::GetInstance().EnableWrittingBuffer(eBuffer::BUFFER_SSR);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	eGlBufferContext::GetInstance().EnableReadingBuffer(eBuffer::BUFFER_DEFFERED, GL_TEXTURE2);
	eGlBufferContext::GetInstance().EnableReadingBuffer(eBuffer::BUFFER_DEFFERED1, GL_TEXTURE3);
	eGlBufferContext::GetInstance().EnableReadingBuffer(eBuffer::BUFFER_SCREEN_MASK, GL_TEXTURE4);
	eGlBufferContext::GetInstance().EnableReadingBuffer(eBuffer::BUFFER_SCREEN, GL_TEXTURE5);
	renderManager->SSRRenderer()->Render(_camera);
}

//-------------------------------------------------------
void eOpenGlRenderPipeline::RenderIBL(const Camera& _camera)
{
	eGlPipelineState::GetInstance().DisableCullFace();

	// @todo should it be this framebuffer ?
	//Pre-computing the BRDF
// then re-configure capture framebuffer object and render screen-space quad with BRDF shader.
	glBindFramebuffer(GL_FRAMEBUFFER, eGlBufferContext::GetInstance().GetId(eBuffer::BUFFER_IBL_CUBEMAP));
	glBindRenderbuffer(GL_RENDERBUFFER, eGlBufferContext::GetInstance().GetRboID(eBuffer::BUFFER_IBL_CUBEMAP));
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderManager->IBLRender()->GetLUTTextureID(), 0); //!?

	glViewport(0, 0, 512, 512);
	renderManager->IBLRender()->RenderBrdf();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glActiveTexture(GL_TEXTURE11);
	glBindTexture(GL_TEXTURE_2D, renderManager->IBLRender()->GetLUTTextureID());

	for (int i = 0; i < renderManager->IBLRender()->HDRCount(); ++i)
	{
		Texture cube, irr, prefilter;
		// load hdr env
		glViewport(0, 0, (GLsizei)512, (GLsizei)512); //@todo numbers // don't forget to configure the viewport to the capture dimensions.
		eGlBufferContext::GetInstance().EnableWrittingBuffer(eBuffer::BUFFER_IBL_CUBEMAP);
		auto cube_id = eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_IBL_CUBEMAP).m_id;
		glBindTexture(GL_TEXTURE_CUBE_MAP, cube_id); // to what slot ???
		renderManager->IBLRender()->RenderCubemap(_camera, cube_id); // write hdr texture into cube_id

		cube.makeCubemap(512, false); //@todo works only the first time, does not second @bugfix
		glCopyImageSubData(cube_id, GL_TEXTURE_CUBE_MAP, 0, 0, 0, 0,
											 cube.m_id, GL_TEXTURE_CUBE_MAP, 0, 0, 0, 0,
											 512, 512, 6); //copy from buffer to texture

   // irradiance
		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_CUBE_MAP, cube_id);

		glViewport(0, 0, 32, 32);
		eGlBufferContext::GetInstance().EnableWrittingBuffer(eBuffer::BUFFER_IBL_CUBEMAP_IRR);
		auto irr_id = eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_IBL_CUBEMAP_IRR).m_id;
		renderManager->IBLRender()->RenderIBLMap(_camera, irr_id); // write irr to irr buffer
		irr.makeCubemap(32);
		glCopyImageSubData(irr_id,   GL_TEXTURE_CUBE_MAP, 0, 0, 0, 0,
											 irr.m_id, GL_TEXTURE_CUBE_MAP, 0, 0, 0, 0,
											 32, 32, 6); //copy from buffer to texture

   // dots artifacts
		glBindTexture(GL_TEXTURE_CUBE_MAP, cube_id);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

		eGlPipelineState::GetInstance().EnableTextureCubmapSeamless();

		// prefilter
		eGlBufferContext::GetInstance().EnableReadingBuffer(eBuffer::BUFFER_IBL_CUBEMAP, GL_TEXTURE2);
		//glActiveTexture(GL_TEXTURE2); //any
		//glBindTexture(GL_TEXTURE_CUBE_MAP, cube_id);
		prefilter.makeCubemap(128, true);
		eGlBufferContext::GetInstance().EnableWrittingBuffer(eBuffer::BUFFER_IBL_CUBEMAP);
		renderManager->IBLRender()->RenderPrefilterMap(_camera, prefilter.m_id, eGlBufferContext::GetInstance().GetRboID(eBuffer::BUFFER_IBL_CUBEMAP));

		//set textures to pbr
		SetSkyIBL(irr.m_id, prefilter.m_id);

		glViewport(0, 0, m_width, m_height);

		m_texture_manager->AddExisting("cube_id"+ std::to_string(i), &cube);
		m_texture_manager->AddCubeMapId(cube.m_id);
		m_texture_manager->AddExisting("irr_id" + std::to_string(i), &irr);
		m_texture_manager->AddCubeMapId(irr.m_id);
		m_texture_manager->AddExisting("prefilter" + std::to_string(i), &prefilter);
		m_texture_manager->AddCubeMapId(prefilter.m_id);
		m_texture_manager->AddIBLId(irr.m_id, prefilter.m_id);
	}

	eGlPipelineState::GetInstance().EnableCullFace();
}

//-------------------------------------------------------
void	eOpenGlRenderPipeline::RenderEnvironmentSnapshot(const RenderBuckets& _objects,
																											 const std::vector<Camera>& _cameras,
																											 const Light& _light)
{
	////1. Copy skybox to environment map
	GLuint sourceCubeMap	= eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_IBL_CUBEMAP).m_id; // ID of the source cube map texture
	GLuint destCubeMap		= eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_ENVIRONMENT_CUBEMAP).m_id; // ID of the destination cube map texture
	
	int width = eGlBufferContext::GetInstance().GetSize(eBuffer::BUFFER_IBL_CUBEMAP).x;  // Width of each face of the cube map
	int height = eGlBufferContext::GetInstance().GetSize(eBuffer::BUFFER_IBL_CUBEMAP).y; // Height of each face of the cube map

	if (width != eGlBufferContext::GetInstance().GetSize(eBuffer::BUFFER_ENVIRONMENT_CUBEMAP).x
			|| height != eGlBufferContext::GetInstance().GetSize(eBuffer::BUFFER_ENVIRONMENT_CUBEMAP).y)
		return; //@todo regenerate buffer

	GLenum cubeFaces[6] = {
		GL_TEXTURE_CUBE_MAP_POSITIVE_X,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
		GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
		GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
	};

	for (unsigned int i = 0; i < 6; ++i)
	{
		eGlBufferContext::GetInstance().EnableWrittingBuffer(eBuffer::BUFFER_ENVIRONMENT_CUBEMAP);
		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, destCubeMap, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	glCopyImageSubData(sourceCubeMap, GL_TEXTURE_CUBE_MAP, 0, 0, 0, 0,
		destCubeMap, GL_TEXTURE_CUBE_MAP, 0, 0, 0, 0,
		width, height, 6); //copy from buffer to texture

	//2. Render to all the faces
	eGlBufferContext::GetInstance().EnableWrittingBuffer(eBuffer::BUFFER_ENVIRONMENT_CUBEMAP);
	eGlPipelineState::GetInstance().DisableCullFace();

	static glm::vec3 capture_position = glm::vec3(3.0f, -1.0f, -2.0f);

	glm::vec3 captureViews[6] = { glm::vec3(1.0f, 0.0f, 0.0f) , glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f),
															glm::vec3(0.0f, -1.0f, 0.0f) , glm::vec3(0.0f, 0.0f, 1.0f) , glm::vec3(0.0f, 0.0f, -1.0f) };

	glm::vec3 captureUps[6] = { glm::vec3(0.0f, -1.0f, 0.0f) , glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
															glm::vec3(0.0f, 0.0f, 1.0f) , glm::vec3(0.0f, -1.0f, 0.0f) , glm::vec3(0.0f, -1.0f, 0.0f) };

	for (unsigned int i = 0; i < 6; ++i)
	{
		Camera camera((float)width, (float)height, _cameras[0].getNearPlane(), _cameras[0].getFarPlane(),
									90.0f, capture_position, captureViews[i], captureUps[i]);

		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, destCubeMap, 0);
		glClear(GL_DEPTH_BUFFER_BIT);
		glViewport(0, 0, width, height);
		std::vector<shObject> phong_objs = _objects[to_index(eObject::RenderType::PHONG)];
		RenderMain(camera, _light, phong_objs);
		std::vector<shObject> pbr_objs = _objects[to_index(eObject::RenderType::PBR)];
		RenderPBR(camera, { _light }, pbr_objs);
	}
	glViewport(0, 0, m_width, m_height);
	eGlPipelineState::GetInstance().EnableCullFace();

	//3. Render reflective objects
	// Assign Correct Draw framebuffer!!!
	static Shader shader;
	if (shader.ID() == UINT32_MAX)
	{
		shader.installShaders("../game_assets/shaders/EnvironmentTestVertex.glsl", "../game_assets/shaders/ParalaxCorrectedCubemapSampling.glsl");
		shader.GetUniformInfoFromShader();
		shader.GetUniformDataFromShader();
	}

	glUseProgram(shader.ID());

	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_CUBE_MAP, eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_ENVIRONMENT_CUBEMAP).m_id);

	eGlBufferContext::GetInstance().EnableWrittingBuffer(eBuffer::BUFFER_MTS); // or  eGlBufferContext::GetInstance().EnableWrittingBuffer(eBuffer::BUFFER_DEFAULT);
	
	shader.SetUniformData("view", _cameras[0].getWorldToViewMatrix());
	shader.SetUniformData("projection", _cameras[0].getProjectionMatrix());
	shader.SetUniformData("CameraWS", glm::vec4(_cameras[0].getPosition(),1.0f));
	shader.SetUniformData("CubemapPositionWS", glm::vec4(capture_position, 1.0f));

	Transform capture_transform;
	capture_transform.setTranslation(-capture_position);
	shader.SetUniformData("WorldToLocal", capture_transform.getModelMatrix());

	std::vector<shObject> env_objs = _objects[to_index(eObject::RenderType::ENVIRONMENT_PROBE)];
	for (auto& obj : env_objs)
	{
		shader.SetUniformData("model", obj->GetTransform()->getModelMatrix());
		obj->GetModel()->Draw();
	}
}

//-------------------------------------------------------
bool& eOpenGlRenderPipeline::GetRotateSkyBoxRef()
{
	return renderManager->SkyBoxRender()->GetRotateSkyBoxRef();
}

//-------------------------------------------------------
Texture eOpenGlRenderPipeline::GetDefaultBufferTexture() const
{
	return eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_DEFAULT);
}

//-------------------------------------------------------
Texture eOpenGlRenderPipeline::GetDepthBufferTexture() const
{
	return eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_DEPTH);
}

//-------------------------------------------------------
Texture eOpenGlRenderPipeline::GetReflectionBufferTexture() const
{
	return eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_REFLECTION);
}

//-------------------------------------------------------
Texture eOpenGlRenderPipeline::GetRefractionBufferTexture() const
{
	return eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_REFRACTION);
}

//-------------------------------------------------------
Texture eOpenGlRenderPipeline::GetShadowBufferTexture() const
{
	return eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_SHADOW_DIR);
}

//-------------------------------------------------------
Texture eOpenGlRenderPipeline::GetGausian1BufferTexture() const
{
	return eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_GAUSSIAN_ONE);
}

//-------------------------------------------------------
Texture eOpenGlRenderPipeline::GetGausian2BufferTexture() const
{
	return eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_GAUSSIAN_TWO);
}

//-------------------------------------------------------
Texture eOpenGlRenderPipeline::GetMtsBufferTexture() const
{
	return eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_MTS);
}

//-------------------------------------------------------
Texture eOpenGlRenderPipeline::GetScreenBufferTexture() const
{
	return eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_SCREEN);
}

//----------------------------------------------------
Texture eOpenGlRenderPipeline::GetBrightFilter() const
{
	return eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_BRIGHT_FILTER);
}

//----------------------------------------------------
Texture eOpenGlRenderPipeline::GetSSAO() const
{
	return eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_SSAO_BLUR);
}

//----------------------------------------------------
Texture eOpenGlRenderPipeline::GetDefferedOne() const
{
	return eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_DEFFERED);
}

//----------------------------------------------------
Texture eOpenGlRenderPipeline::GetDefferedTwo() const
{
	return eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_DEFFERED1);
}

//----------------------------------------------------
Texture eOpenGlRenderPipeline::GetHdrCubeMap() const
{
	return eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_IBL_CUBEMAP); // does not work like this
}

//----------------------------------------------------
Texture eOpenGlRenderPipeline::GetEnvironmentCubeMap() const
{
	return eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_ENVIRONMENT_CUBEMAP); // does not work like this
}

//----------------------------------------------------
Texture eOpenGlRenderPipeline::GetLUT() const
{
	return Texture(renderManager->IBLRender()->GetLUTTextureID(), 512, 512, 3);
}

//----------------------------------------------------
Texture eOpenGlRenderPipeline::GetCSMMapLayer1() const{return csm_dump1;}
Texture eOpenGlRenderPipeline::GetCSMMapLayer2() const{return csm_dump2;}
Texture eOpenGlRenderPipeline::GetCSMMapLayer3() const{return csm_dump3;}
Texture eOpenGlRenderPipeline::GetCSMMapLayer4() const{return csm_dump4;}
Texture eOpenGlRenderPipeline::GetCSMMapLayer5() const{return csm_dump5;}

Texture eOpenGlRenderPipeline::GetBloomTexture() const
{ 
	return eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_BLOOM);
}

Texture eOpenGlRenderPipeline::GetSSRTexture() const
{
	return eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_SSR);
}

Texture eOpenGlRenderPipeline::GetSSRWithScreenTexture() const
{
	return eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_SCREEN_WITH_SSR);
}

Texture eOpenGlRenderPipeline::GetSSRTextureScreenMask() const
{
	return eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_SSR_HIT_MASK); //@todo screen mask check too
}

Texture eOpenGlRenderPipeline::GetCameraInterpolationCoords() const
{
	return eGlBufferContext::GetInstance().GetTexture("CameraInterpolationCoordsBuffer");
}

Texture eOpenGlRenderPipeline::GetComputeParticleSystem() const
{
	return eGlBufferContext::GetInstance().GetTexture("ComputeParticleSystemBuffer");
}

Texture eOpenGlRenderPipeline::GetLuminanceTexture() const
{
	return eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_BRIGHT_FILTER_MASK);
}

Texture eOpenGlRenderPipeline::GetUIlessTexture() const
{
	return eGlBufferContext::GetInstance().GetTexture("UIless");
}

void eOpenGlRenderPipeline::DumpCSMTextures() const
{
	Texture t = eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_SHADOW_CSM);
	//t.saveToFile("dump_csm.png", GL_TEXTURE_2D_ARRAY, GL_DEPTH_COMPONENT, GL_FLOAT);

	static std::vector<GLfloat> buffer(2400 * 1200 * 5);
	glBindTexture(GL_TEXTURE_2D_ARRAY, t.m_id);
	glGetTexImage(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT, GL_FLOAT, &buffer[0]);

	Texture* csmp1 = const_cast<Texture*>(&csm_dump1);
	csmp1->TextureFromBuffer<GLfloat>((GLfloat*)&buffer[0], 2400 ,1200, GL_RED);

	Texture* csmp2 = const_cast<Texture*>(&csm_dump2);
	csmp2->TextureFromBuffer<GLfloat>((GLfloat*)&buffer[2400 * 1200], 2400, 1200, GL_RED);

	Texture* csmp3 = const_cast<Texture*>(&csm_dump3);
	csmp3->TextureFromBuffer<GLfloat>((GLfloat*)&buffer[2400 * 1200 * 2], 2400, 1200, GL_RED);

	Texture* csmp4 = const_cast<Texture*>(&csm_dump4);
	csmp4->TextureFromBuffer<GLfloat>((GLfloat*)&buffer[2400 * 1200 * 3], 2400, 1200, GL_RED);

	Texture* csmp5 = const_cast<Texture*>(&csm_dump5);
	csmp5->TextureFromBuffer<GLfloat>((GLfloat*)&buffer[2400 * 1200 * 4], 2400, 1200, GL_RED);
}

//------------------------------------------------------------------------------------------
void eOpenGlRenderPipeline::RenderCameraInterpolationCompute(const Camera& _camera)
{
	glViewport(0, 0, m_width, m_height);
	eGlBufferContext::GetInstance().EnableReadingBuffer(eBuffer::BUFFER_DEFAULT, GL_TEXTURE1);
	eGlBufferContext::GetInstance().EnableReadingBuffer(eBuffer::BUFFER_DEFFERED, GL_TEXTURE2);
	renderManager->CameraInterpolationRender()->DispatchCompute(_camera);
}


#include <cmath>

float Lerp(int start, int end, float t)
{
	// Clamp t to the range [0, 1]
	t = std::max(0.0f, std::min(1.0f, t));

	// Perform linear interpolation
	return static_cast<float>(start) + t * (static_cast<float>(end) - static_cast<float>(start));
}

//-------------------------------------------------------
Texture* eOpenGlRenderPipeline::RenderCameraInterpolation(const Camera& _camera)
{
	static std::vector<GLfloat> buffer_image(m_width * m_height * 4);
	static std::vector<GLfloat> buffer_coords(m_width * m_height * 4);
	static std::vector<GLfloat> buffer_new_image(m_width * m_height * 4);
	static Texture new_image_texture;

	eGlBufferContext::GetInstance().EnableCustomWrittingBuffer("CameraInterpolationCoordsBuffer");
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	eGlBufferContext::GetInstance().EnableReadingBuffer(eBuffer::BUFFER_DEFFERED, GL_TEXTURE2);
	renderManager->CameraInterpolationRender()->Render(_camera);

	if (ssr)
		eGlBufferContext::GetInstance().EnableReadingBuffer(eBuffer::BUFFER_SCREEN_WITH_SSR, GL_TEXTURE2);
	else
		eGlBufferContext::GetInstance().EnableReadingBuffer(eBuffer::BUFFER_SCREEN, GL_TEXTURE2);
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, &buffer_image[0]);

	eGlBufferContext::GetInstance().EnableCustomReadingBuffer("CameraInterpolationCoordsBuffer", GL_TEXTURE3);
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, &buffer_coords[0]);

	for (uint32_t row = 0; row < m_height; ++row)
	{
		for (uint32_t column = 0; column < m_width; ++column)
		{
			float r = buffer_image[row * m_width * 4 + column*4];
			float g = buffer_image[row * m_width * 4 + column*4 +1];
			float b = buffer_image[row * m_width * 4 + column*4 +2];
			float a = 1.f;

			float new_coord_x = buffer_coords[row * m_width * 4 + column * 4];
			float new_coord_y = buffer_coords[row * m_width * 4 + column * 4 + 1];
			float new_coord_z = buffer_coords[row * m_width * 4 + column * 4 + 2];
			float new_coord_a = buffer_coords[row * m_width * 4 + column * 4 + 3];

			int new_coord_x_int = static_cast<int>(std::round(Lerp(0, m_width, new_coord_x)));
			int new_coord_y_int = static_cast<int>(std::round(Lerp(0, m_height, new_coord_y)));

			int index = new_coord_y_int * m_width * 4 + new_coord_x_int * 4;
			if (index < buffer_new_image.size()-3 && index >= 0)
			{
				buffer_new_image[new_coord_y_int * m_width * 4 + new_coord_x_int *4] = r;
				buffer_new_image[new_coord_y_int * m_width * 4 + new_coord_x_int *4 + 1] = g;
				buffer_new_image[new_coord_y_int * m_width * 4 + new_coord_x_int *4 + 2] = b;
				buffer_new_image[new_coord_y_int * m_width * 4 + new_coord_x_int *4 + 3] = a;
			}
		}
	}
	new_image_texture.TextureFromBuffer<GLfloat>((GLfloat*)&buffer_new_image[0], m_width, m_height, GL_RGBA);
	return &new_image_texture;
}
