#include "stdafx.h"
#include "DecalRender.h"

//---------------------------------------------------------------------
eDecalRender::eDecalRender(const std::string& _vS, const std::string& _fS)
{
  mShader.installShaders(_vS.c_str(), _fS.c_str());
  mShader.GetUniformInfoFromShader();

  screenMesh.reset(new eScreenMesh({}, {}));
  screenMesh->SetViewPortToDefault();
}

//---------------------------------------------------------------------
eDecalRender::~eDecalRender()
{
}

//---------------------------------------------------------------------
void eDecalRender::Render(const Camera& _camera, const std::vector<Decal>& _decals)
{
  if (_decals.empty())
    return;

  glUseProgram(mShader.ID());

  // 1. Set global uniforms once
  glm::mat4 invProj = glm::inverse(_camera.getProjectionMatrix());
  glm::mat4 invView = glm::inverse(_camera.getWorldToViewMatrix());

  mShader.SetUniformData("invProj", invProj);
  mShader.SetUniformData("invView", invView);

  // shader now uses TexCoords directly,
  // so screenSize uniform is no longer needed
  glDisable(GL_DEPTH_TEST); // <- IMPORTANT
  glDepthMask(GL_FALSE);

  // Blending for compositing decals over existing color
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  //// Depth test ON (so decals only affect visible fragments),
  //// but depth writes OFF (so we don't mess up future passes)
  //glEnable(GL_DEPTH_TEST);
  //glDepthMask(GL_FALSE);

  for (const auto& decal : _decals)
  {
    // 3. Build decal model → world and invert it
    glm::mat4 decalModel =
      glm::translate(glm::mat4(1.0f), decal.box.origin) *
      glm::mat4_cast(glm::toQuat(decal.box.orientation)) *
      glm::scale(glm::mat4(1.0f), decal.box.size * 2.0f); // half-extents

    glm::mat4 decalMatrix = glm::inverse(decalModel); // world-to-decal
    mShader.SetUniformData("decalMatrix", decalMatrix);

    // 4. Bind decal texture (atlas or per-decal texture)
    glActiveTexture(GL_TEXTURE19);
    glBindTexture(GL_TEXTURE_2D, decal.decalTextureID);

    mShader.SetUniformData("decalInvertY", true);          // for this shield
    mShader.SetUniformData("decalGammaCorrect", true);     // if atlas is sRGB-authored

    // 5. Atlas UV transform: reuse HeraldryAtlas
    // If uvTransform is (0,0,0,0), we can skip drawing this decal
    if (decal.uvTransform.x == 0.0f && decal.uvTransform.y == 0.0f)
      continue;

    mShader.SetUniformData("decalUVTransform", decal.uvTransform);

    // 6. Draw fullscreen quad
    screenMesh->DrawUnTextured();
  }

  // Restore state
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
}

