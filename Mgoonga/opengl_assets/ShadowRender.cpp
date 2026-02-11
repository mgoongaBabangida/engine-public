#include "stdafx.h"
#include "ShadowRender.h"
#include "GlPipelineState.h"
#include "GlBufferContext.h"

#include <map>

#include <math/Transform.h>

//-----------------------------------------------------------------------------
eShadowRender::eShadowRender(const std::string& vS, const std::string& fS, const std::string& gSP, const std::string& fSP)
{
	shaderDir.installShaders(vS.c_str(), fS.c_str());
	shaderDir.GetUniformInfoFromShader();
	glUseProgram(shaderDir.ID());
	MVPUniformLocationDir = glGetUniformLocation(shaderDir.ID(), "MVP");
	BonesMatLocationDir = glGetUniformLocation(shaderDir.ID(), "gBones");

	shaderPoint.installShaders(vS.c_str(), fSP.c_str(), gSP.c_str());
	shaderPoint.GetUniformInfoFromShader();
	glUseProgram(shaderPoint.ID());

	ModelUniformLocationPoint = glGetUniformLocation(shaderPoint.ID(), "MVP");
	ProjectionTransformsUniformLocation = glGetUniformLocation(shaderPoint.ID(), "shadowMatrices");
	FarPlaneUniformLocation = glGetUniformLocation(shaderPoint.ID(), "far_plane");
	BonesMatLocationPoint = glGetUniformLocation(shaderPoint.ID(), "gBones");
}

//-----------------------------------------------------------------------------
void eShadowRender::Render(const Camera&					camera,
													 const Light&						light,
													 std::vector<shObject>&	objects)
{
	if (light.type == eLightType::POINT)
	{
		float aspect = 1.0f; // (float)SHADOW_WIDTH/(float)SHADOW_HEIGHT (depth buffer or viewport)
		glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), aspect, camera.getNearPlane(), camera.getFarPlane()); //should be 90
		glm::vec3 lightPos = light.light_position;
		std::vector<glm::mat4> shadowTransforms;
		shadowTransforms.push_back(shadowProj * 
			glm::lookAt(lightPos, lightPos + glm::vec3(1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0)));
		shadowTransforms.push_back(shadowProj *
			glm::lookAt(lightPos, lightPos + glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0)));
		shadowTransforms.push_back(shadowProj *
			glm::lookAt(lightPos, lightPos + glm::vec3(0.0, 1.0, 0.0), glm::vec3(0.0, 0.0, 1.0)));
		shadowTransforms.push_back(shadowProj *
			glm::lookAt(lightPos, lightPos + glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, 0.0, -1.0)));
		shadowTransforms.push_back(shadowProj *
			glm::lookAt(lightPos, lightPos + glm::vec3(0.0, 0.0, 1.0), glm::vec3(0.0, -1.0, 0.0)));
		shadowTransforms.push_back(shadowProj *
			glm::lookAt(lightPos, lightPos + glm::vec3(0.0, 0.0, -1.0), glm::vec3(0.0, -1.0, 0.0)));

		glUseProgram(shaderPoint.ID());
		glUniformMatrix4fv(ProjectionTransformsUniformLocation, 6, GL_FALSE, &shadowTransforms[0][0][0]);
		glUniform1f(FarPlaneUniformLocation, camera.getFarPlane());
		shaderPoint.SetUniformData("lightPosition", light.light_position);

		//RENDER DEPTH
		for (auto &object : objects)
		{
			glm::mat4 modelMatrix = object->GetTransform()->getModelMatrix();
			glUniformMatrix4fv(ModelUniformLocationPoint, 1, GL_FALSE, &modelMatrix[0][0]);

			if (object->GetRigger() != nullptr)
			{
				matrices = object->GetRigger()->GetMatrices();
			}
			else
			{
				for (auto& m : matrices)
					m = UNIT_MATRIX;
			}
			glUniformMatrix4fv(BonesMatLocationPoint, MAX_BONES, GL_FALSE, &matrices[0][0][0]);

			object->GetModel()->Draw();
		}
	}
	else if (light.type == eLightType::DIRECTION || light.type == eLightType::SPOT)
	{
		glUseProgram(shaderDir.ID());
		glm::mat4 worldToViewMatrix;
		if (light.type == eLightType::DIRECTION)
		{
			worldToViewMatrix = glm::lookAt(glm::vec3(light.light_position), glm::vec3(0.0f, 0.0f, 0.0f), /*glm::vec3(light.light_position) + light.light_direction,*/
				glm::vec3(0.0f, 1.0f, 0.0f));
			shadowMatrix = camera.getProjectionOrthoMatrix() * worldToViewMatrix;
		}
		else
		{
			worldToViewMatrix = glm::lookAt(glm::vec3(light.light_position), glm::vec3(light.light_position) + glm::vec3(light.light_direction),
				glm::vec3(0.0f, 1.0f, 0.0f));
			shadowMatrix = camera.getProjectionOrthoMatrix() * worldToViewMatrix;
		}

		std::map<std::string, std::vector<shObject>> instanced;

		auto& buf = eGlBufferContext::GetInstance();

		// Bind SSBOs used by the shared vertex shader
		buf.BindSSBO(eSSBO::MODEL_TO_PROJECTION_MATRIX);
		buf.BindSSBO(eSSBO::MODEL_TO_WORLD_MATRIX);     // shared VS reads it (may be DCE’d, but bind anyway)
		buf.BindSSBO(eSSBO::INSTANCED_INFO_MATRIX);     // optional safety if VS still reads instancedData
		buf.BindSSBO(eSSBO::BONES_PACKED);              // new
		buf.BindSSBO(eSSBO::BONE_BASE_INDEX);           // new

		// Pick capacities matching your SSBO allocations
		constexpr size_t MAX_RIGID_INSTANCES_PER_DRAW = 1024;
		constexpr size_t MAX_SKINNED_INSTANCES_PER_DRAW = 256;

		// Scratch buffers (reuse to avoid reallocs)
		std::vector<glm::mat4> modelToProjection;
		std::vector<glm::mat4> modelToWorld;
		std::vector<glm::mat4> dummyInstancedData;

		std::vector<GLuint>    boneBaseIndex;
		std::vector<glm::mat4> bonesPacked;

		// --------------------------
		// Non-instanced depth draws
		// --------------------------
		shaderDir.SetUniformData("isInstanced", false);
		shaderDir.SetUniformData("useInstancedSkinning", false);
		shaderDir.SetUniformData("instanceIndex", 0);

		for (auto& object : objects)
		{
			if (object->GetInstancingTag().empty())
			{
				const glm::mat4 M = object->GetTransform()->getModelMatrix();

				// Shared VS expects SSBO model matrices even for non-instanced path
				modelToProjection.clear();
				modelToWorld.clear();
				dummyInstancedData.clear();

				modelToProjection.push_back(shadowMatrix * M);
				modelToWorld.push_back(M);
				dummyInstancedData.push_back(UNIT_MATRIX); // safety: instancedData[0] exists if not DCE’d

				buf.UploadSSBOData(eSSBO::MODEL_TO_PROJECTION_MATRIX, modelToProjection);
				buf.UploadSSBOData(eSSBO::MODEL_TO_WORLD_MATRIX, modelToWorld);
				buf.UploadSSBOData(eSSBO::INSTANCED_INFO_MATRIX, dummyInstancedData);

				// Old uniform-bones path remains for non-instanced
				if (object->GetRigger() != nullptr)
					matrices = object->GetRigger()->GetMatrices();
				else
					for (auto& m : matrices) m = UNIT_MATRIX;

				glUniformMatrix4fv(BonesMatLocationDir, MAX_BONES, GL_FALSE, &matrices[0][0][0]);
				object->GetModel()->Draw();
			}
			else
			{
				if (auto it = instanced.find(object->GetInstancingTag()); it != instanced.end())
					it->second.push_back(object);
				else
					instanced.insert({ object->GetInstancingTag(), { object } });
			}
		}

		// --------------------------
		// Instanced depth draws
		// --------------------------
		shaderDir.SetUniformData("isInstanced", true);
		shaderDir.SetUniformData("instanceIndex", 0); // unused when isInstanced=true

		for (auto& node : instanced)
		{
			const size_t instanceCount = node.second.size();
			shObject proto = node.second[0];

			// Decide if any instance in this batch is skinned
			bool skinnedBatch = false;
			for (auto& obj : node.second)
			{
				if (obj->GetRigger() != nullptr) { skinnedBatch = true; break; }
			}
			shaderDir.SetUniformData("useInstancedSkinning", skinnedBatch);

			const size_t maxPerDraw = skinnedBatch ? MAX_SKINNED_INSTANCES_PER_DRAW : MAX_RIGID_INSTANCES_PER_DRAW;

			for (size_t chunkBase = 0; chunkBase < instanceCount; chunkBase += maxPerDraw)
			{
				const size_t chunkCount = std::min(maxPerDraw, instanceCount - chunkBase);

				modelToProjection.clear();
				modelToWorld.clear();
				dummyInstancedData.clear();

				modelToProjection.reserve(chunkCount);
				modelToWorld.reserve(chunkCount);
				dummyInstancedData.reserve(chunkCount);

				if (skinnedBatch)
				{
					boneBaseIndex.clear();
					boneBaseIndex.reserve(chunkCount);

					bonesPacked.clear();
					bonesPacked.resize(chunkCount * MAX_BONES, UNIT_MATRIX);
				}

				// Build this chunk
				for (size_t local = 0; local < chunkCount; ++local)
				{
					const size_t idx = chunkBase + local;
					auto& obj = node.second[idx];

					const glm::mat4 M = obj->GetTransform()->getModelMatrix();

					modelToProjection.push_back(shadowMatrix * M);
					modelToWorld.push_back(M);

					// safety: ensure instancedData[local] exists if VS still reads it
					dummyInstancedData.push_back(UNIT_MATRIX);

					// Instanced skinning palette
					if (skinnedBatch)
					{
						boneBaseIndex.push_back(static_cast<GLuint>(local * MAX_BONES));
						const size_t dstBase = local * MAX_BONES;

						if (auto* r = obj->GetRigger())
						{
							const auto& palette = r->GetMatrices(); // or GetCachedMatrices(), whichever is your "current pose"
							const size_t n = std::min(palette.size(), static_cast<size_t>(MAX_BONES));

							for (size_t b = 0; b < n; ++b)
								bonesPacked[dstBase + b] = palette[b];

							for (size_t b = n; b < static_cast<size_t>(MAX_BONES); ++b)
								bonesPacked[dstBase + b] = UNIT_MATRIX;
						}
						else
						{
							for (size_t b = 0; b < static_cast<size_t>(MAX_BONES); ++b)
								bonesPacked[dstBase + b] = UNIT_MATRIX;
						}
					}
				}

				// Upload per-instance transforms
				buf.UploadSSBOData(eSSBO::MODEL_TO_PROJECTION_MATRIX, modelToProjection);
				buf.UploadSSBOData(eSSBO::MODEL_TO_WORLD_MATRIX, modelToWorld);

				// Optional safety upload for instancedData (binding=7 in your shared VS)
				buf.UploadSSBOData(eSSBO::INSTANCED_INFO_MATRIX, dummyInstancedData);

				// Upload instanced skinning SSBOs for this chunk
				if (skinnedBatch)
				{
					buf.UploadSSBOData(eSSBO::BONE_BASE_INDEX, boneBaseIndex);
					buf.UploadSSBOData(eSSBO::BONES_PACKED, bonesPacked);
				}

				// Keep a harmless fallback palette in uniforms (ignored when useInstancedSkinning=true)
				for (auto& m : matrices) m = UNIT_MATRIX;
				glUniformMatrix4fv(BonesMatLocationDir, MAX_BONES, GL_FALSE, &matrices[0][0][0]);

				// Draw this chunk
				proto->GetModel()->DrawInstanced(shaderDir.ID(), static_cast<int32_t>(chunkCount));
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------------------
void eShadowRender::RenderDepthBuffer(const Camera& camera, const Light& light, std::vector<shObject>& objects)
{
	glUseProgram(shaderDir.ID());

	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	glDisable(GL_BLEND);     // for safety
	glEnable(GL_CULL_FACE);  // optional: helps performance
	glCullFace(GL_BACK);     // avoid rendering backfaces in Z-prepass

	//glm::mat4 reversedProjectionMatrix = glm::perspectiveRH_ZO(glm::radians(camera.getZoom()), (float)camera.getWidth() / (float)camera.getHeight(), camera.getFarPlane(), camera.getNearPlane());
	shadowMatrix = camera.getProjectionMatrix() * camera.getWorldToViewMatrix();

	//RENDER DEPTH
	for (auto& object : objects)
	{
		if (!object->IsTransparent())
		{
			if (object->IsBackfaceCull())
				eGlPipelineState::GetInstance().EnableCullFace();
			else
				eGlPipelineState::GetInstance().DisableCullFace();

			glm::mat4 modelToProjectionMatrix = shadowMatrix * object->GetTransform()->getModelMatrix();
			glUniformMatrix4fv(MVPUniformLocationDir, 1, GL_FALSE, &modelToProjectionMatrix[0][0]);

			if (object->GetRigger() != nullptr)
			{
				matrices = object->GetRigger()->GetCachedMatrices();
			}
			else
			{
				for (auto& m : matrices)
					m = UNIT_MATRIX;
			}
			glUniformMatrix4fv(BonesMatLocationDir, MAX_BONES, GL_FALSE, &matrices[0][0][0]);
			object->GetModel()->Draw();
		}
	}
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	eGlPipelineState::GetInstance().EnableCullFace();
}
