#include "stdafx.h"

#include "PhongRender.h"
#include <math/Transform.h>
#include <math/RigAnimator.h>

#include "GlPipelineState.h"
#include "GlBufferContext.h"

//---------------------------------------------------------------------------------
ePhongRender::ePhongRender(const std::string& vS, const std::string& wave_vS, const std::string& fS,
	std::unique_ptr<TerrainModel> model, Texture* tex)
{
	m_vertex_shader_path = vS;
	m_fragment_shader_path = fS;
	mainShader.installShaders(vS.c_str(), fS.c_str()); //main pass
	glUseProgram(mainShader.ID());

	//Uniform Locs
	BonesMatLocation					= glGetUniformLocation(mainShader.ID(), "gBones");
	mLightingIndexDirectionalLoc	= glGetSubroutineIndex(mainShader.ID(), GL_FRAGMENT_SHADER, "calculateBlinnPhongDirectionalSpecDif");
	mLightingIndexPointLoc				= glGetSubroutineIndex(mainShader.ID(), GL_FRAGMENT_SHADER, "calculateBlinnPhongPointSpecDif");
	mLightingIndexSpotLoc					= glGetSubroutineIndex(mainShader.ID(), GL_FRAGMENT_SHADER, "calculateBlinnPhongFlashSpecDif");

	mainShader.SetUniformData("Fog.maxDist", 40.0f);
	mainShader.SetUniformData("Fog.minDist", 20.0f);
	mainShader.SetUniformData("Fog.color", glm::vec4(0.5f, 0.5f, 0.5f, 1.0f));
	mainShader.SetUniformData("Fog.fog_on", true);
	mainShader.SetUniformData("Fog.density", 0.007f);
	mainShader.SetUniformData("Fog.gradient", 1.5f);

	//Wave
	waveShader.installShaders(wave_vS.c_str(), fS.c_str());
	glUseProgram(waveShader.ID());
	waveShader.GetUniformInfoFromShader();

	clock.start();

	model->Initialize(tex, tex);
	m_wave_object.reset(new eObject);
	m_wave_object->SetModel(model.release());
	m_wave_object->SetTransform(new Transform);
	//@todo move this outside
	m_wave_object->GetTransform()->setTranslation(glm::vec3(3.0f, 2.0f, 0.0f));
	m_wave_object->GetTransform()->setScale(glm::vec3(0.03f, 0.03f, 0.03f));
	m_wave_object->GetTransform()->setRotation(PI / 2, 0.0f, 0.0f);

	LightingIndexDirectionalWave = glGetSubroutineIndex(waveShader.ID(), GL_FRAGMENT_SHADER, "calculateBlinnPhongDirectionalSpecDif");
	LightingIndexPointWave = glGetSubroutineIndex(waveShader.ID(), GL_FRAGMENT_SHADER, "calculateBlinnPhongPointSpecDif");
	LightingIndexSpotWave = glGetSubroutineIndex(waveShader.ID(), GL_FRAGMENT_SHADER, "calculateBlinnPhongFlashSpecDif");

	waveShader.SetUniformData("Fog.maxDist", 40.0f);
	waveShader.SetUniformData("Fog.minDist", 20.0f);
	waveShader.SetUniformData("Fog.color", glm::vec4(0.5f, 0.5f, 0.5f, 1.0f));
	waveShader.SetUniformData("Fog.fog_on", true);
	waveShader.SetUniformData("Fog.density", 0.007f);
	waveShader.SetUniformData("Fog.gradient", 1.5f);
}

//---------------------------------------------------------------------------------
ePhongRender::~ePhongRender()
{
}

//-----------------------------------------------------------------------------------------------------
void ePhongRender::Render(const Camera&								camera,
						             const Light&									light,
						             const std::vector<shObject>&	objects)
{
	glUseProgram(mainShader.ID());
	mainShader.reloadIfNeeded({{m_vertex_shader_path.c_str(), GL_VERTEX_SHADER} , {m_fragment_shader_path.c_str(), GL_FRAGMENT_SHADER} });
	_SetCommonVariables(camera, light, mainShader);

	if (m_z_pre_pass)
	{
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);     // Required after Z-prepass
		glDepthMask(GL_FALSE);      // Opaque depth already written
	}
	else
	{
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glDepthMask(GL_TRUE);
	}

	eGlBufferContext::GetInstance().BindSSBO(eSSBO::MODEL_TO_PROJECTION_MATRIX);
	eGlBufferContext::GetInstance().BindSSBO(eSSBO::MODEL_TO_WORLD_MATRIX);
	glm ::mat4 worldToProjectionMatrix = camera.getProjectionMatrix() * camera.getWorldToViewMatrix();
	for (auto &object : objects)
	{
		glm::mat4 modelToProjectionMatrix = worldToProjectionMatrix * object->GetTransform()->getModelMatrix();

		//GLuint fullTransformationUniformLocation = glGetUniformLocation(mainShader.ID(), "modelToProjectionMatrix"); // glGetUniformLocation slow!
		//GLuint modelToWorldMatrixUniformLocation = glGetUniformLocation(mainShader.ID(), "modelToWorldMatrix");
		//
		//glUniformMatrix4fv(fullTransformationUniformLocation, 1, GL_FALSE, &modelToProjectionMatrix[0][0]);
		//glUniformMatrix4fv(modelToWorldMatrixUniformLocation, 1, GL_FALSE, &object->GetTransform()->getModelMatrix()[0][0]);
		
		/*mainShader.SetUniformData("modelToProjectionMatrix", modelToProjectionMatrix);
		mainShader.SetUniformData("modelToWorldMatrix", object->GetTransform()->getModelMatrix());*/

		eGlBufferContext::GetInstance().UploadSSBOData(eSSBO::MODEL_TO_PROJECTION_MATRIX, { modelToProjectionMatrix });
		eGlBufferContext::GetInstance().UploadSSBOData(eSSBO::MODEL_TO_WORLD_MATRIX, { object->GetTransform()->getModelMatrix() });

		if (object->IsTextureBlending())
			mainShader.SetUniformData("texture_blending", true);
		else
			mainShader.SetUniformData("texture_blending", false);

		if (object->GetInstancingTag().empty())
			mainShader.SetUniformData("isInstanced", false);
		else
			mainShader.SetUniformData("isInstanced", true);

		if (object->IsTransparent())
		{
			eGlPipelineState::GetInstance().EnableBlend();
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glDepthMask(GL_FALSE); // do not write depth for transparent
		}
		else
		{
			eGlPipelineState::GetInstance().DisableBlend();
			if (!m_z_pre_pass) glDepthMask(GL_TRUE);
		}

		if (object->IsBackfaceCull())
			eGlPipelineState::GetInstance().EnableCullFace();
		else
			eGlPipelineState::GetInstance().DisableCullFace();

		if (object->GetRigger() != nullptr)
		{
			matrices = object->GetRigger()->GetCachedMatrices();
		}
		else
		{
			for (auto& m : matrices)
				m = UNIT_MATRIX;
		}
		glUniformMatrix4fv(BonesMatLocation, MAX_BONES, GL_FALSE, &matrices[0][0][0]);

		// Select lighting mode
		switch (light.type)
		{
		case eLightType::POINT:       glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &mLightingIndexPointLoc); break;
		case eLightType::SPOT:        glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &mLightingIndexSpotLoc); break;
		case eLightType::DIRECTION:
		case eLightType::CSM:         glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &mLightingIndexDirectionalLoc); break;
		}
		object->GetModel()->Draw(mainShader.ID());
	}
	glDepthMask(GL_TRUE);
	eGlPipelineState::GetInstance().DisableBlend();
}

//---------------------------------------------------------------------------
void ePhongRender::RenderWaves(const Camera& camera, const Light& light, std::vector<shObject> flags)
{
	glUseProgram(waveShader.ID());
	_SetCommonVariables(camera, light, waveShader);

	waveShader.SetUniformData("Time", time);
	int32_t msc = (int32_t)clock.newFrame();
	float dur = (float)msc / 1000.0f;
	time += dur;

	glUniform1i(glGetUniformLocation(waveShader.ID(), "normalMapping"), GL_FALSE); // @todo glGetUniformLocation slow !!!

	glm::mat4 worldToProjectionMatrix = camera.getProjectionMatrix() * camera.getWorldToViewMatrix();
	for (auto& flag : flags)
	{
		m_wave_object->GetTransform()->setTranslation(flag->GetTransform()->getTranslation());
		m_wave_object->GetTransform()->setScale(flag->GetTransform()->getScaleAsVector());
		m_wave_object->GetTransform()->billboard(-camera.getDirection());

		glm::quat cur = m_wave_object->GetTransform()->getRotation();
		glm::quat plus = glm::toQuat(glm::rotate(UNIT_MATRIX, (float)PI / 2, XAXIS));
		m_wave_object->GetTransform()->setRotation(cur * plus);

		// set with texture id
		m_wave_object->GetModel()->SetMaterial(flag->GetModel()->GetMaterial().value());

		glm::mat4 modelToProjectionMatrix = worldToProjectionMatrix * m_wave_object->GetTransform()->getModelMatrix();
		waveShader.SetUniformData("MVP", modelToProjectionMatrix);
		waveShader.SetUniformData("modelToWorldMatrix", m_wave_object->GetTransform()->getModelMatrix());

		glm::mat4 modelViewMatrix = camera.getWorldToViewMatrix() * m_wave_object->GetTransform()->getModelMatrix();
		waveShader.SetUniformData("ModelViewMatrix", modelViewMatrix);

		if (light.type == eLightType::POINT)
			glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &mLightingIndexPointLoc);
		else if (light.type == eLightType::SPOT)
			glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &mLightingIndexSpotLoc);
		else if (light.type == eLightType::DIRECTION)
			glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &mLightingIndexDirectionalLoc);
		else if (light.type == eLightType::CSM)
			glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &mLightingIndexDirectionalLoc);
		m_wave_object->GetModel()->Draw();
	}

	glUniform1i(glGetUniformLocation(waveShader.ID(), "normalMapping"), GL_TRUE);
}

//---------------------------------------------------------------------------
void ePhongRender::SetClipPlane(float Height)
{
	glUseProgram(mainShader.ID());
	GLuint clipPlaneLoc = glGetUniformLocation(mainShader.ID(), "clip_plane");
	glUniform4f(clipPlaneLoc, 0, 1, 0, Height);
}

//---------------------------------------------------------------------------
void ePhongRender::_SetCommonVariables(const Camera& camera, const Light& light, Shader& _shader)
{
	_shader.SetUniformData("debug_white_color", m_debug_white);
	_shader.SetUniformData("debug_white_texcoords", m_debug_text_coords);
	_shader.SetUniformData("gamma_correction", m_gamma_correction);
	_shader.SetUniformData("tone_mapping", m_tone_mapping);
	_shader.SetUniformData("hdr_exposure", m_exposure);
	_shader.SetUniformData("ssao_threshold", m_ssao_threshold);
	_shader.SetUniformData("ssao_strength", m_ssao_strength);
	_shader.SetUniformData("emission_strength", m_emission_strength);

	_shader.SetUniformData("light.ambient", light.ambient);
	_shader.SetUniformData("light.diffuse", light.diffuse);
	_shader.SetUniformData("light.specular", light.specular);
	_shader.SetUniformData("light.position", light.light_position);
	_shader.SetUniformData("light.direction", light.light_direction);

	_shader.SetUniformData("light.constant", light.constant);
	_shader.SetUniformData("light.linear", light.linear);
	_shader.SetUniformData("light.quadratic", light.quadratic);
	_shader.SetUniformData("light.cutOff", light.cutOff);
	_shader.SetUniformData("light.outerCutOff", light.outerCutOff);

	_shader.SetUniformData("view", camera.getWorldToViewMatrix());

	if (light.type == eLightType::POINT)
	{
		_shader.SetUniformData("shininess", 32.0f);
		glm::mat4 worldToViewMatrix = glm::lookAt(glm::vec3(light.light_position), glm::vec3(light.light_position) + glm::vec3(light.light_direction),
			glm::vec3(0.0f, 1.0f, 0.0f));
		_shader.SetUniformData("shadow_directional", false);
		glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &mLightingIndexPointLoc);
		shadowMatrix = camera.getProjectionBiasedMatrix() * worldToViewMatrix;
	}
	else if (light.type == eLightType::SPOT)
	{
		_shader.SetUniformData("shininess", 32.0f);
		glm::mat4 worldToViewMatrix = glm::lookAt(glm::vec3(light.light_position), glm::vec3(light.light_position) + glm::vec3(light.light_direction),
			glm::vec3(0.0f, 1.0f, 0.0f));
		_shader.SetUniformData("shadow_directional", true); // ?
		_shader.SetUniformData("use_csm_shadows", false);
		glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &mLightingIndexSpotLoc);
		shadowMatrix = camera.getProjectionOrthoMatrix() * worldToViewMatrix;
	}
	else if (light.type == eLightType::DIRECTION)
	{
		_shader.SetUniformData("shininess", 64.0f);
		glm::mat4 worldToViewMatrix = glm::lookAt(glm::vec3(light.light_position),
			glm::vec3(0.0f, 0.0f, 0.0f), /*glm::vec3(light.light_position) + light.light_direction,*/
			glm::vec3(0.0f, 1.0f, 0.0f));
		_shader.SetUniformData("shadow_directional", true);
		_shader.SetUniformData("use_csm_shadows", false);
		glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &mLightingIndexDirectionalLoc);
		shadowMatrix = camera.getProjectionOrthoMatrix() * worldToViewMatrix;
	}
	else if (light.type == eLightType::CSM)
	{
		_shader.SetUniformData("shininess", 64.0f);
		_shader.SetUniformData("shadow_directional", true);
		_shader.SetUniformData("use_csm_shadows", true);
		glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &mLightingIndexDirectionalLoc);
		_shader.SetUniformData("farPlane", camera.getFarPlane());
		_shader.SetUniformData("cascadeCount", m_shadowCascadeLevels.size());
		for (size_t i = 0; i < m_shadowCascadeLevels.size(); ++i)
		{
			_shader.SetUniformData("cascadePlaneDistances[" + std::to_string(i) + "]", m_shadowCascadeLevels[i]);
		}
	}

	_shader.SetUniformData("shadowMatrix", shadowMatrix); //shadow
	_shader.SetUniformData("eyePositionWorld", glm::vec4(camera.getPosition(), 1.0f));
	_shader.SetUniformData("far_plane", camera.getFarPlane());
	_shader.SetUniformData("farPlane", camera.getFarPlane()); //@todo
}
