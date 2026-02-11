#include "PBRRender.h"

#include <math/Camera.h>

#include <glm\glm\gtc\matrix_transform.hpp>
#include <glm\glm\gtx\transform.hpp>
#include <glm\glm\gtc\quaternion.hpp>
#include <glm\glm\gtx\quaternion.hpp>

#include "LTC.h"
#include "GlPipelineState.h"
#include "GlBufferContext.h"

#include <map>

//-------------------------------------------------------------------
ePBRRender::ePBRRender(const std::string& vS, const std::string& fS)
{
  m_vertex_shader_path = vS;
  m_fragment_shader_path = fS;

  pbrShader.installShaders(vS.c_str(), fS.c_str());
  pbrShader.GetUniformInfoFromShader();
  glUseProgram(pbrShader.ID());

  albedoLoc         = glGetUniformLocation(pbrShader.ID(), "albedo");
  emissionColorLoc  = glGetUniformLocation(pbrShader.ID(), "emission_color");
  metallicLoc       = glGetUniformLocation(pbrShader.ID(), "metallic");
  roughnessLoc      = glGetUniformLocation(pbrShader.ID(), "roughness");
  aoLoc             = glGetUniformLocation(pbrShader.ID(), "ao");
  emissionLoc       = glGetUniformLocation(pbrShader.ID(), "emission_strength");
  opacityLoc        = glGetUniformLocation(pbrShader.ID(), "opacity");
  camPosLoc         = glGetUniformLocation(pbrShader.ID(), "camPos");

  glUniform1f(aoLoc, 1.0f);

  pbrShader.SetUniformData("Fog.maxDist", 40.0f);
  pbrShader.SetUniformData("Fog.minDist", 20.0f);
  pbrShader.SetUniformData("Fog.color", glm::vec4(0.5f, 0.5f, 0.5f, 1.0f));
  pbrShader.SetUniformData("Fog.fog_on", true);
  pbrShader.SetUniformData("Fog.density", 0.007f);
  pbrShader.SetUniformData("Fog.gradient", 1.5f);

  //vertex shader
  BonesMatLocation                  = glGetUniformLocation(pbrShader.ID(), "gBones");
  fullTransformationUniformLocation = glGetUniformLocation(pbrShader.ID(), "modelToProjectionMatrix");
  modelToWorldMatrixUniformLocation = glGetUniformLocation(pbrShader.ID(), "modelToWorldMatrix");
  shadowMatrixUniformLocation       = glGetUniformLocation(pbrShader.ID(), "shadowMatrix"); //shadow

  //area lights
  m1.TextureFromBuffer<GLfloat>(LTC1, 64, 64, GL_RGBA, GL_NEAREST);
  m2.TextureFromBuffer<GLfloat>(LTC2, 64, 64, GL_RGBA, GL_NEAREST);
}

//-----------------------------------------------------------------------------------------------------------
void ePBRRender::Render(const Camera& camera, const std::vector<Light>& _lights, std::vector<shObject>& objects)
{
  const Light& _light = _lights[0];

  glUseProgram(pbrShader.ID());
  pbrShader.reloadIfNeeded({ {m_vertex_shader_path.c_str(), GL_VERTEX_SHADER} , {m_fragment_shader_path.c_str(), GL_FRAGMENT_SHADER}});

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

  std::vector<glm::vec4> lpositions;
  std::vector<glm::vec4> ldirections;
  std::vector<glm::vec4> lcolors;

  std::vector <float> lconstant;
  std::vector <float> llinear;
  std::vector <float> lquadratic;

  std::vector <float> lcutoffs;
  std::vector <float> louterCutoffs;

  std::vector<glm::mat4> lpoints;
  std::vector <bool> lactive;

  for(size_t i = 0; i < _lights.size(); ++i)
  {
    lpositions.push_back(_lights[i].light_position);
    ldirections.push_back(_lights[i].light_direction);
    if(_lights[i].type != eLightType::AREA_LIGHT)
      lcolors.push_back({ _lights[i].intensity });
    else
      lcolors.push_back({ glm::normalize(_lights[i].intensity)});
    lconstant.push_back(_lights[i].constant);
    llinear.push_back(_lights[i].linear);
    lquadratic.push_back(_lights[i].quadratic);
    lcutoffs.push_back(_lights[i].cutOff);
    louterCutoffs.push_back(_lights[i].outerCutOff);
    lpoints.push_back({ _lights[i].points[0],_lights[i].points[1],_lights[i].points[2],_lights[i].points[3] });
    lactive.push_back(_lights[i].active);

    pbrShader.SetUniformData("constant[" +std::to_string(i) + "]", lconstant[i]);
    pbrShader.SetUniformData("linear[" + std::to_string(i) + "]", llinear[i]);
    pbrShader.SetUniformData("quadratic[" + std::to_string(i) + "]", lquadratic[i]);

    pbrShader.SetUniformData("cutOff[" + std::to_string(i) + "]", lcutoffs[i]);
    pbrShader.SetUniformData("outerCutOff[" + std::to_string(i) + "]", louterCutoffs[i]);
    pbrShader.SetUniformData("flash[" + std::to_string(i) + "]", _lights[i].type == eLightType::SPOT);

    pbrShader.SetUniformData("intensity[" + std::to_string(i) + "]", _lights[i].intensity.x);
    pbrShader.SetUniformData("twoSided[" + std::to_string(i) + "]", true); //!@todo
    pbrShader.SetUniformData("isAreaLight[" + std::to_string(i) + "]", _lights[i].type == eLightType::AREA_LIGHT); //!@todo
    pbrShader.SetUniformData("radius[" + std::to_string(i) + "]", _lights[i].radius);
    pbrShader.SetUniformData("isActive[" + std::to_string(i) + "]", _lights[i].active);
  }

  pbrShader.SetUniformData("num_lights", _lights.size());

  GLuint loc_pos = glGetUniformLocation(pbrShader.ID(), "lightPositions");
  glUniform4fv(loc_pos, _lights.size(), &lpositions[0][0]);

  GLuint loc_dir = glGetUniformLocation(pbrShader.ID(), "lightDirections");
  glUniform4fv(loc_dir, _lights.size(), &ldirections[0][0]);

  GLuint loc_col = glGetUniformLocation(pbrShader.ID(), "lightColors");
  glUniform4fv(loc_col, _lights.size(), &lcolors[0][0]);

  GLuint pointsLoc = glGetUniformLocation(pbrShader.ID(), "points");
  glUniformMatrix4fv(pointsLoc, _lights.size(), GL_FALSE , &lpoints[0][0][0]);

  glActiveTexture(GL_TEXTURE7);
  glBindTexture(GL_TEXTURE_2D, m1.m_id);
  glActiveTexture(GL_TEXTURE8);
  glBindTexture(GL_TEXTURE_2D, m2.m_id);

  pbrShader.SetUniformData("view", camera.getWorldToViewMatrix());

  if (_light.type == eLightType::POINT)
  {
    //pbrShader.SetUniformData("shininess", 32.0f);
    glm::mat4 worldToViewMatrix = glm::lookAt(glm::vec3(_light.light_position),
                                              glm::vec3(_light.light_position) + glm::vec3(_light.light_direction),
                                              glm::vec3(0.0f, 1.0f, 0.0f));
    pbrShader.SetUniformData("shadow_directional", false);
    shadowMatrix = camera.getProjectionBiasedMatrix() * worldToViewMatrix;
  }
  else if (_light.type == eLightType::SPOT)
  {
    pbrShader.SetUniformData("shininess", 32.0f);
    glm::mat4 worldToViewMatrix = glm::lookAt(glm::vec3(_light.light_position), glm::vec3(_light.light_position) + glm::vec3(_light.light_direction),
      glm::vec3(0.0f, 1.0f, 0.0f));
    pbrShader.SetUniformData("shadow_directional", true); //?
    pbrShader.SetUniformData("use_csm_shadows", false);
    shadowMatrix = camera.getProjectionOrthoMatrix() * worldToViewMatrix;
  }
  else if (_light.type == eLightType::DIRECTION)
  {
    pbrShader.SetUniformData("shininess", 64.0f);
    glm::mat4 worldToViewMatrix = glm::lookAt(glm::vec3(_light.light_position),
      glm::vec3(0.0f, 0.0f, 0.0f), /*glm::vec3(light.light_position) + light.light_direction,*/
      glm::vec3(0.0f, 1.0f, 0.0f));
    pbrShader.SetUniformData("shadow_directional", true);
    pbrShader.SetUniformData("use_csm_shadows", false);
    shadowMatrix = camera.getProjectionOrthoMatrix() * worldToViewMatrix;
  }
  else if (_light.type == eLightType::CSM)
  {
    pbrShader.SetUniformData("shininess", 64.0f);
    pbrShader.SetUniformData("shadow_directional", true);
    pbrShader.SetUniformData("use_csm_shadows", true);
    pbrShader.SetUniformData("farPlane", camera.getFarPlane());
    pbrShader.SetUniformData("cascadeCount", m_shadowCascadeLevels.size());
    for (size_t i = 0; i < m_shadowCascadeLevels.size(); ++i)
    {
      pbrShader.SetUniformData("cascadePlaneDistances[" + std::to_string(i) + "]", m_shadowCascadeLevels[i]);
    }
  }

  glUniformMatrix4fv(shadowMatrixUniformLocation, 1, GL_FALSE, &shadowMatrix[0][0]);
  pbrShader.SetUniformData("camPos", glm::vec4(camera.getPosition(), 1.0f));
  pbrShader.SetUniformData("far_plane", camera.getFarPlane());

  glm::mat4 worldToViewMatrix = glm::lookAt(glm::vec3(_light.light_position), glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.0f, 1.0f, 0.0f));
  glm::mat4 worldToProjectionMatrix = camera.getProjectionMatrix() * camera.getWorldToViewMatrix();

  std::map<std::string, std::vector<shObject>> instanced;
  eGlBufferContext::GetInstance().BindSSBO(eSSBO::MODEL_TO_PROJECTION_MATRIX);
  eGlBufferContext::GetInstance().BindSSBO(eSSBO::MODEL_TO_WORLD_MATRIX);

  for (auto& object : objects)
  {
    if (object->GetInstancingTag().empty())
    {
      pbrShader.SetUniformData("isInstanced", false);
      pbrShader.SetUniformData("InstancedInfoRender", false);
      pbrShader.SetUniformData("enable_heraldry", false);

      if(!object->Is2DScreenSpace())
        modelToProjectionMatrix = worldToProjectionMatrix * object->GetTransform()->getModelMatrix();
      else
        modelToProjectionMatrix = object->GetTransform()->getModelMatrix();

      if (object->IsTransparent())
      {
        eGlPipelineState::GetInstance().EnableBlend();
        //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); //???
        //if (m_z_pre_pass) 
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

      if (object->IsFadeAlpha())
        pbrShader.SetUniformData("fadeAlpha", true);
      else
        pbrShader.SetUniformData("fadeAlpha", false);

     /* glUniformMatrix4fv(fullTransformationUniformLocation, 1, GL_FALSE, &modelToProjectionMatrix[0][0]);
      glUniformMatrix4fv(modelToWorldMatrixUniformLocation, 1, GL_FALSE, &object->GetTransform()->getModelMatrix()[0][0]);*/

      eGlBufferContext::GetInstance().UploadSSBOData(eSSBO::MODEL_TO_PROJECTION_MATRIX, { worldToProjectionMatrix * object->GetTransform()->getModelMatrix() });
      eGlBufferContext::GetInstance().UploadSSBOData(eSSBO::MODEL_TO_WORLD_MATRIX, { object->GetTransform()->getModelMatrix() });

      _SetMaterial(object);
      if (object->GetRigger() != nullptr) { matrices = object->GetRigger()->GetCachedMatrices();}
      else                                { for (auto& m : matrices) m = UNIT_MATRIX; }

      glUniformMatrix4fv(BonesMatLocation, MAX_BONES, GL_FALSE, &matrices[0][0][0]);
      object->GetModel()->Draw(pbrShader.ID());
    }
    else
    {
      if (auto it = instanced.find(object->GetInstancingTag()); it != instanced.end())
        it->second.push_back(object);
      else
        instanced.insert({ object->GetInstancingTag(), {object} });
    }
  }
  
  // Draw instanced
  for (auto& node : instanced)
  {
    auto& buf = eGlBufferContext::GetInstance();

    // Optional: bind once (safe even if you re-bind elsewhere)
    buf.BindSSBO(eSSBO::MODEL_TO_PROJECTION_MATRIX);
    buf.BindSSBO(eSSBO::MODEL_TO_WORLD_MATRIX);
    buf.BindSSBO(eSSBO::INSTANCED_INFO_MATRIX);
    buf.BindSSBO(eSSBO::HERALDRY_INSTANCED_INFO);

    // New SSBOs (you’ll add these in your context)
    buf.BindSSBO(eSSBO::BONES_PACKED);
    buf.BindSSBO(eSSBO::BONE_BASE_INDEX);

    // Must match your SSBO capacities
    constexpr size_t MAX_RIGID_INSTANCES_PER_DRAW = 1024; // pick what your SSBO can hold
    constexpr size_t MAX_SKINNED_INSTANCES_PER_DRAW = 256;  // pick what your SSBO can hold

    // Reusable scratch buffers (avoid reallocs)
    std::vector<glm::mat4> modelToWorld;
    std::vector<glm::mat4> modelToProjection;
    std::vector<glm::mat4> instancedInfo;
    std::vector<glm::vec4> heraldryInfo;

    std::vector<GLuint>    boneBaseIndex;
    std::vector<glm::mat4> bonesPacked;

    auto ApplyBatchPipelineState = [&](const shObject& proto)
      {
        if (proto->IsTransparent())
        {
          eGlPipelineState::GetInstance().EnableBlend();
          glDepthMask(GL_FALSE);
        }
        else
        {
          eGlPipelineState::GetInstance().DisableBlend();
          if (!m_z_pre_pass) glDepthMask(GL_TRUE);
        }

        if (proto->IsBackfaceCull())
          eGlPipelineState::GetInstance().EnableCullFace();
        else
          eGlPipelineState::GetInstance().DisableCullFace();
      };

    // -----------------------------------------------------------------------------
    for (auto& node : instanced)
    {
      shObject proto = node.second[0];

      pbrShader.SetUniformData("isInstanced", true);
      pbrShader.SetUniformData("enable_heraldry", false);

      ApplyBatchPipelineState(proto);

      const bool isHeraldry = (proto->GetInstancingTag() == "Heraldry");
      const bool hasInstInfo = proto->HasInstancedInfo() && !isHeraldry;

      pbrShader.SetUniformData("InstancedInfoRender", hasInstInfo);

      // Decide if this batch needs instanced skinning
      bool skinnedBatch = false;
      for (auto& obj : node.second)
      {
        if (obj->GetRigger() != nullptr) { skinnedBatch = true; break; }
      }
      pbrShader.SetUniformData("useInstancedSkinning", skinnedBatch);

      const size_t instanceCount = node.second.size();
      const size_t maxPerDraw = skinnedBatch ? MAX_SKINNED_INSTANCES_PER_DRAW : MAX_RIGID_INSTANCES_PER_DRAW;

      // Material is assumed identical inside the batch (same as your current approach)
      _SetMaterial(proto);

      // Heraldry uniforms once per batch (same as your current code)
      if (isHeraldry)
      {
        const float baseTexSize = 1024.0f;
        glm::vec4 shieldRect(
          65.0f / baseTexSize,   // minU
          400.0f / baseTexSize,  // minV
          475.0f / baseTexSize,  // sizeU
          565.0f / baseTexSize   // sizeV
        );
        pbrShader.SetUniformData("uShieldRectUV", shieldRect);
        pbrShader.SetUniformData("enable_heraldry", true);
      }

      // Chunked draws
      for (size_t chunkBase = 0; chunkBase < instanceCount; chunkBase += maxPerDraw)
      {
        const size_t chunkCount = std::min(maxPerDraw, instanceCount - chunkBase);

        modelToWorld.clear();
        modelToProjection.clear();
        modelToWorld.reserve(chunkCount);
        modelToProjection.reserve(chunkCount);

        if (isHeraldry)
        {
          heraldryInfo.clear();
          heraldryInfo.reserve(chunkCount);
        }
        else if (hasInstInfo)
        {
          instancedInfo.clear();
          instancedInfo.reserve(chunkCount);
        }

        if (skinnedBatch)
        {
          boneBaseIndex.clear();
          boneBaseIndex.reserve(chunkCount);

          bonesPacked.clear();
          bonesPacked.resize(chunkCount * MAX_BONES, UNIT_MATRIX);
        }

        // Build per-instance arrays for this chunk
        for (size_t local = 0; local < chunkCount; ++local)
        {
          const size_t idx = chunkBase + local;
          auto& obj = node.second[idx];

          const glm::mat4 M = obj->GetTransform()->getModelMatrix();
          modelToWorld.push_back(M);
          modelToProjection.push_back(worldToProjectionMatrix * M);

          if (isHeraldry)
          {
            heraldryInfo.push_back(obj->GetInstancedInfo()[0]);
          }
          else if (hasInstInfo)
          {
            instancedInfo.push_back(obj->GetInstancedInfo());
          }

          // Instanced skinning palette (fixed stride MAX_BONES, mat4)
          if (skinnedBatch)
          {
            boneBaseIndex.push_back(static_cast<GLuint>(local * MAX_BONES));
            const size_t dstBase = local * MAX_BONES;

            if (auto* r = obj->GetRigger())
            {
              const auto& palette = r->GetCachedMatrices();
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

        // Upload extra instanced info
        if (isHeraldry)
        {
          buf.UploadSSBOData(eSSBO::HERALDRY_INSTANCED_INFO, heraldryInfo);
        }
        else if (hasInstInfo)
        {
          buf.UploadSSBOData(eSSBO::INSTANCED_INFO_MATRIX, instancedInfo);
        }

        // Upload instanced skinning buffers (only for skinned batches)
        if (skinnedBatch)
        {
          buf.UploadSSBOData(eSSBO::BONE_BASE_INDEX, boneBaseIndex);
          buf.UploadSSBOData(eSSBO::BONES_PACKED, bonesPacked);
        }

        // Keep old uniform-bones path as a harmless fallback (shader will ignore when useInstancedSkinning=true)
        for (auto& m : matrices) m = UNIT_MATRIX;
        glUniformMatrix4fv(BonesMatLocation, MAX_BONES, GL_FALSE, &matrices[0][0][0]);

        // Draw this chunk
        proto->GetModel()->DrawInstanced(pbrShader.ID(), static_cast<int32_t>(chunkCount));
      }
    }
  }

  glDepthMask(GL_TRUE);
  eGlPipelineState::GetInstance().DisableBlend();
}

//---------------------------------------------------------------------------------
void ePBRRender::_SetMaterial(shObject _obj)
{
  if (_obj->GetModel()->HasMaterial())
  {
    Material material = _obj->GetModel()->GetMaterial().value();

    glUniform4f(albedoLoc, material.albedo[0], material.albedo[1], material.albedo[2], 1.0f);
    glUniform1f(metallicLoc,  material.metallic);
    glUniform1f(roughnessLoc, material.roughness);
    glUniform1f(aoLoc,        material.ao);
    glUniform4f(emissionColorLoc, material.emission_color[0], material.emission_color[1], material.emission_color[2], 1.0f);
    glUniform1f(emissionLoc, material.emission_strength);
    glUniform1f(opacityLoc , material.opacity);

    glUniform1i(glGetUniformLocation(pbrShader.ID(), "textured"), material.used_textures.contains(Material::TextureType::ALBEDO));
    glUniform1i(glGetUniformLocation(pbrShader.ID(), "use_metalic_texture"), material.used_textures.contains(Material::TextureType::METALLIC));
    glUniform1i(glGetUniformLocation(pbrShader.ID(), "use_normalmap_texture"), material.used_textures.contains(Material::TextureType::NORMAL));
    glUniform1i(glGetUniformLocation(pbrShader.ID(), "use_roughness_texture"), material.used_textures.contains(Material::TextureType::ROUGHNESS));
    glUniform1i(glGetUniformLocation(pbrShader.ID(), "use_ao_texture"), material.used_textures.contains(Material::TextureType::AO));
    glUniform1i(glGetUniformLocation(pbrShader.ID(), "use_opacity_mask"), material.used_textures.contains(Material::TextureType::OPACITY));

    if (material.used_textures.contains(Material::TextureType::ATLAS_0))
    {
      // Bind heraldry atlas texture (coats.png) to unit 19
      glActiveTexture(GL_TEXTURE0 + 19);
      glBindTexture(GL_TEXTURE_2D, material.textures[Material::TextureType::ATLAS_0]);
    }

    // draw object and return
  }
  else
  {
    auto meshes = _obj->GetModel()->GetMeshes();
    auto it = meshes.rbegin();
    while (it != meshes.rend()) //first mesh gets the data @todo every mesh should get it
    {
      if ((*it)->HasMaterial())
      {
        Material material = (*it)->GetMaterial().value();

        glUniform4f(albedoLoc, material.albedo[0], material.albedo[1], material.albedo[2], 1.0f);
        glUniform1f(metallicLoc, material.metallic);
        glUniform1f(roughnessLoc, material.roughness);
        glUniform1f(aoLoc, material.ao);
        glUniform1f(emissionLoc, material.emission_strength);
        glUniform1f(opacityLoc, material.opacity);

        glUniform1i(glGetUniformLocation(pbrShader.ID(), "textured"), material.used_textures.contains(Material::TextureType::ALBEDO));
        glUniform1i(glGetUniformLocation(pbrShader.ID(), "use_metalic_texture"), material.used_textures.contains(Material::TextureType::METALLIC));
        glUniform1i(glGetUniformLocation(pbrShader.ID(), "use_normalmap_texture"), material.used_textures.contains(Material::TextureType::NORMAL));
        glUniform1i(glGetUniformLocation(pbrShader.ID(), "use_roughness_texture"), material.used_textures.contains(Material::TextureType::ROUGHNESS));
        glUniform1i(glGetUniformLocation(pbrShader.ID(), "use_ao_texture"), material.used_textures.contains(Material::TextureType::AO));
        glUniform1i(glGetUniformLocation(pbrShader.ID(), "use_opacity_mask"), material.used_textures.contains(Material::TextureType::OPACITY));

        if (material.used_textures.contains(Material::TextureType::ATLAS_0))
        {
          // Bind heraldry atlas texture (coats.png) to unit 19
          glActiveTexture(GL_TEXTURE0 + 19);
          glBindTexture(GL_TEXTURE_2D, material.textures[Material::TextureType::ATLAS_0]);
        }
      }
      ++it;
      // draw mesh ? //set matterial and draw for each mesh
    }
  }
}
