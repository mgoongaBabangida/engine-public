#include "MeshLineRender.h"

#include "GlDrawContext.h"
#include <math/RigAnimator.h>

#include "GlPipelineState.h"
#include "GlBufferContext.h"

//---------------------------------------------------------------
eMeshLineRender::eMeshLineRender(const std::string& _vS, const std::string& _fS)
{
  m_shader.installShaders(_vS.c_str(), _fS.c_str());
  m_shader.GetUniformInfoFromShader();
  m_shader.GetUniformDataFromShader();
}

//---------------------------------------------------------------
eMeshLineRender::~eMeshLineRender()
{
}

//---------------------------------------------------------------
void eMeshLineRender::Render(const Camera& _camera, const Light& _light, const std::vector<shObject>& _objects)
{
  glUseProgram(m_shader.ID());

	m_shader.SetUniformData("outline", true);
	m_shader.SetUniformData("isInstanced", false);

	std::map<std::string, std::vector<shObject>> instanced;
	eGlBufferContext::GetInstance().BindSSBO(eSSBO::MODEL_TO_PROJECTION_MATRIX);
	eGlBufferContext::GetInstance().BindSSBO(eSSBO::MODEL_TO_WORLD_MATRIX);

	glm::mat4 worldToProjectionMatrix = _camera.getProjectionMatrix() * _camera.getWorldToViewMatrix();
	for (auto& object : _objects)
	{
		eGlBufferContext::GetInstance().UploadSSBOData(eSSBO::MODEL_TO_PROJECTION_MATRIX, { worldToProjectionMatrix * object->GetTransform()->getModelMatrix() });
		eGlBufferContext::GetInstance().UploadSSBOData(eSSBO::MODEL_TO_WORLD_MATRIX, { object->GetTransform()->getModelMatrix() });

		if (object->GetRigger() != nullptr)
		{
			matrices = object->GetRigger()->GetMatrices();
			RigAnimator* rigger = dynamic_cast<RigAnimator*>(object->GetRigger());
			m_shader.SetUniformData("outline_bone", rigger->GetActiveBoneIndex());
		}
		else
		{
			for (auto& m : matrices)
				m = UNIT_MATRIX;
			m_shader.SetUniformData("outline_bone", MAX_BONES);
		}
		glUniformMatrix4fv(glGetUniformLocation(m_shader.ID(), "gBones"), MAX_BONES, GL_FALSE, &matrices[0][0][0]); //@todo set with shader

		for (auto& mesh : object->GetModel()->Get3DMeshes())
		{
			mesh->BindVAO();
			eGlDrawContext::GetInstance().DrawElements(GL_LINES, (GLsizei)mesh->GetIndices().size(), GL_UNSIGNED_INT, 0, "eMeshLineRender");
			mesh->UnbindVAO();
		}
	}
	m_shader.SetUniformData("outline", false);
}
