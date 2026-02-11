#include "ComputeShaderRender.h"
#include "Texture.h"
#include "GlPipelineState.h"

#include <math/Random.h>
#include <math/Utils.h>

#include "ScreenMesh.h"
#include <glm/glm/gtc/type_ptr.hpp>

#define PRIM_RESTART 0xffffff //?

static std::string m_forward_shader_path;

//--------------------------------------------------------
eComputeShaderRender::eComputeShaderRender(const std::string& cS, const std::string& pScS,
                                           const std::string& pSvS, const std::string& pSfS,
                                           const std::string& ClothvS, const std::string& ClothfS,
                                           const std::string& ClothcS, const std::string& ClothNormcS,
                                           const std::string& EdgecS, const std::string& WorlycS,
                                           const std::string& vRes, const std::string& f3DDebug,
                                           const std::string& cForwardPlus,
                                           const Texture* _textileTexture, const Texture* _computeShaderImage)
{
  mPSComputeShader.installShaders(pScS.c_str());
  mPSComputeShader.GetUniformInfoFromShader();
  _InitPSCompute();

  mPSRenderResultShader.installShaders(pSvS.c_str(), pSfS.c_str());
  mPSRenderResultShader.GetUniformInfoFromShader();

  //**********************************************************************//
  eGlPipelineState::GetInstance().EnablePrimitiveRestart();
  glPrimitiveRestartIndex(PRIM_RESTART);

  mRenderClothSimulShader.installShaders(ClothvS.c_str(), ClothfS.c_str());
  mRenderClothSimulShader.GetUniformInfoFromShader();
  mComputeClothSimulShader.installShaders(ClothcS.c_str());
  mComputeClothSimulShader.GetUniformInfoFromShader();
  mComputeClothSimulNorm.installShaders(ClothNormcS.c_str());
  mComputeClothSimulNorm.GetUniformInfoFromShader();
  _InitClothSimulCompute();

  //**********************************************************************//
  mEdgeFinding.installShaders(EdgecS.c_str());
  mEdgeFinding.GetUniformInfoFromShader();

  //**********************************************************************//
  mComputeShader.installShaders(cS.c_str());
  mComputeShader.GetUniformInfoFromShader();

  mTextileTextureId = _textileTexture->m_id;
  mImageId = _computeShaderImage->m_id;
  mImageWidth = _computeShaderImage->m_width;
  mImageHeight = _computeShaderImage->m_height;

  //***********************************************************************//
  mWorley3D.installShaders(WorlycS.c_str());
  mWorley3D.GetUniformInfoFromShader();
  _InitWorleyTexture3d();

  //***********************************************************************//
  mForwardPlus.installShaders(cForwardPlus.c_str());
  mForwardPlus.GetUniformInfoFromShader();
  m_forward_shader_path = cForwardPlus;

  //************************************************************************//
  mDebugShader.installShaders(vRes.c_str(), f3DDebug.c_str());
  mDebugShader.GetUniformInfoFromShader();
  InitForwardPlus();
}

//---------------------------------------------------------------
eComputeShaderRender::~eComputeShaderRender()
{
  glDeleteTextures(1, &m_worley3DID);
  glDeleteBuffers(1, &mWorleyPointsBuffer1);
  glDeleteBuffers(1, &mWorleyPointsBuffer2);
  glDeleteBuffers(1, &mWorleyPointsBuffer3);
}

//---------------------------------------------------------------
void eComputeShaderRender::DispatchCompute(const Camera& _camera)
{
  static bool first_call = false;
  if (!first_call)
  {
    _InitWorley3d();
    DispatchWorley3D(_camera);
  }
  first_call = true;

  if (m_redo_noise)
  {
    _InitWorley3d();
    DispatchWorley3D(_camera);
    m_redo_noise = false;
  }
}

//-------------------------------------------------------
void eComputeShaderRender::RenderComputeResult(const Camera& _camera)
{
  glUseProgram(mDebugShader.ID());

  // Bind the 3D texture
  GLint noiseTextureLocation = glGetUniformLocation(mDebugShader.ID(), "noiseTexture");
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_3D, m_worley3DID);
  glUniform1i(noiseTextureLocation, 0);

  mDebugShader.SetUniformData("z_slice", m_WorleyZ);
  mDebugShader.SetUniformData("debug_octave", m_WorleyOctave);

  static eScreenMesh screenMesh({}, {});
  screenMesh.SetViewPortToDefault();
  screenMesh.Draw();
}

//-------------------------------------------------------
void eComputeShaderRender::_InitPSCompute()
{
  // Initial positions of the particles
  std::vector<GLfloat> initPos;
  std::vector<GLfloat> initVel(psVars.totalParticles * 4, 0.0f);
  glm::vec4 p(0.0f, 0.0f, 0.0f, 1.0f);
  GLfloat dx = 2.0f / (psVars.nParticles.x - 1),
    dy = 2.0f / (psVars.nParticles.y - 1),
    dz = 2.0f / (psVars.nParticles.z - 1);
  // We want to center the particles at (0,0,0)
  glm::mat4 transf = glm::translate(glm::mat4(1.0f), glm::vec3(-1, -1, -1));
  for (int i = 0; i < psVars.nParticles.x; i++) {
    for (int j = 0; j < psVars.nParticles.y; j++) {
      for (int k = 0; k < psVars.nParticles.z; k++) {
        p.x = dx * i;
        p.y = dy * j;
        p.z = dz * k;
        p.w = 1.0f;
        p = transf * p;
        initPos.push_back(p.x);
        initPos.push_back(p.y);
        initPos.push_back(p.z);
        initPos.push_back(p.w);
      }
    }
  }

  // We need buffers for position , and velocity.
  GLuint bufs[2];
  glGenBuffers(2, bufs);
  GLuint posBuf = bufs[0];
  GLuint velBuf = bufs[1];

  GLuint bufSize = psVars.totalParticles * 4 * sizeof(GLfloat);

  // The buffers for positions
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, posBuf);
  glBufferData(GL_SHADER_STORAGE_BUFFER, bufSize, &initPos[0], GL_DYNAMIC_DRAW);

  // Velocities
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, velBuf);
  glBufferData(GL_SHADER_STORAGE_BUFFER, bufSize, &initVel[0], GL_DYNAMIC_COPY);

  // Set up the VAO
  glGenVertexArrays(1, &psVars.particlesVao);
  glBindVertexArray(psVars.particlesVao);

  glBindBuffer(GL_ARRAY_BUFFER, posBuf);
  glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
  glEnableVertexAttribArray(0);

  glBindVertexArray(0);

  // Set up a buffer and a VAO for drawing the attractors (the "black holes")
  glGenBuffers(1, &psVars.bhBuf);
  glBindBuffer(GL_ARRAY_BUFFER, psVars.bhBuf);
  GLfloat data[] = { psVars.bh1.x, psVars.bh1.y, psVars.bh1.z, psVars.bh1.w, psVars.bh2.x, psVars.bh2.y, psVars.bh2.z, psVars.bh2.w };
  glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(GLfloat), data, GL_DYNAMIC_DRAW);

  glGenVertexArrays(1, &psVars.bhVao);
  glBindVertexArray(psVars.bhVao);

  glBindBuffer(GL_ARRAY_BUFFER, psVars.bhBuf);
  glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
  glEnableVertexAttribArray(0);

  glBindVertexArray(0);
}

//-------------------------------------------------------
void eComputeShaderRender::_DispatchPs(const Camera& _camera)
{
  psVars.Update();
  // Rotate the attractors ("black holes")
  glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(psVars.angle), glm::vec3(0, 0, 1));
  psVars.att1 = glm::vec3(rot * psVars.bh1);
  psVars.att2 = glm::vec3(rot * psVars.bh2);

  // Execute the compute shader
  glUseProgram(mPSComputeShader.ID());
  mPSComputeShader.SetUniformData("BlackHolePos1", psVars.att1);
  mPSComputeShader.SetUniformData("BlackHolePos2", psVars.att2);
  glDispatchCompute(psVars.totalParticles / 1000, 1, 1);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

//-------------------------------------------------------
void eComputeShaderRender::_RenderPS(const Camera& _camera)
{
  // Draw the scene
  glUseProgram(mPSRenderResultShader.ID());
  glm::mat4 model = glm::mat4(1.0f);
  mPSRenderResultShader.SetUniformData("MVP", _camera.getProjectionMatrix() * _camera.getWorldToViewMatrix() * model);

  // Draw the particles
  glPointSize(1.0f);
  mPSRenderResultShader.SetUniformData("Color", glm::vec4(0, 0, 0, 0.2f));
  glBindVertexArray(psVars.particlesVao);
  glDrawArrays(GL_POINTS, 0, psVars.totalParticles);
  glBindVertexArray(0);

  // Draw the attractors
  glPointSize(5.0f);
  GLfloat data[] = { psVars.att1.x, psVars.att1.y, psVars.att1.z, 1.0f, psVars.att2.x, psVars.att2.y, psVars.att2.z, 1.0f };
  glBindBuffer(GL_ARRAY_BUFFER, psVars.bhBuf);
  glBufferSubData(GL_ARRAY_BUFFER, 0, 8 * sizeof(GLfloat), data);
  mPSRenderResultShader.SetUniformData("Color", glm::vec4(1, 1, 0, 1.0f));
  glBindVertexArray(psVars.bhVao);
  glDrawArrays(GL_POINTS, 0, 2);
  glBindVertexArray(0);
}

//***************************************************************//
//-------------------------------------------------------
void eComputeShaderRender::_DispatchSimpleCompute()
{
  glUseProgram(mComputeShader.ID());

  glActiveTexture(GL_TEXTURE0);
  glBindImageTexture(0, mImageId, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
  /*computeShader.setFloat("t", currentFrame);*/
  glDispatchCompute((unsigned int)mImageWidth / 10, (unsigned int)mImageHeight / 10, 1);

  // make sure writing to image has finished before read
  glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

//***************************************************************//
//-------------------------------------------------------
void eComputeShaderRender::_DispatchEgdeDetection()
{
  glUseProgram(mEdgeFinding.ID());

  glActiveTexture(GL_TEXTURE0); // ?1 !!!
  glBindImageTexture(0, mImageId, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F); //8!!!
  glDispatchCompute((unsigned int)mImageWidth / 25, (unsigned int)mImageHeight / 25, 1);

  // make sure writing to image has finished before read
  glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

//***************************************************************//
//-------------------------------------------------------
void eComputeShaderRender::_InitClothSimulCompute()
{
  // Initial transform
  glm::mat4 transf = glm::translate(glm::mat4(1.0), glm::vec3(0, clothVars.clothSize.y, 0));
  transf = glm::rotate(transf, glm::radians(-80.0f), glm::vec3(1, 0, 0));
  transf = glm::translate(transf, glm::vec3(0, -clothVars.clothSize.y, 0));

  // Initial positions of the particles
  std::vector<GLfloat> initPos;
  std::vector<GLfloat> initVel(clothVars.nParticles.x * clothVars.nParticles.y * 4, 0.0f);
  std::vector<float> initTc;
  float dx = clothVars.clothSize.x / (clothVars.nParticles.x - 1);
  float dy = clothVars.clothSize.y / (clothVars.nParticles.y - 1);
  float ds = 1.0f / (clothVars.nParticles.x - 1);
  float dt = 1.0f / (clothVars.nParticles.y - 1);
  glm::vec4 p(0.0f, 0.0f, 0.0f, 1.0f);
  for (int i = 0; i < clothVars.nParticles.y; i++) {
    for (int j = 0; j < clothVars.nParticles.x; j++) {
      p.x = dx * j;
      p.y = dy * i;
      p.z = 0.0f;
      p = transf * p;
      initPos.push_back(p.x);
      initPos.push_back(p.y);
      initPos.push_back(p.z);
      initPos.push_back(1.0f);

      initTc.push_back(ds * j);
      initTc.push_back(dt * i);
    }
  }

  // Every row is one triangle strip
  std::vector<GLuint> el;
  for (int row = 0; row < clothVars.nParticles.y - 1; row++) {
    for (int col = 0; col < clothVars.nParticles.x; col++) {
      el.push_back((row + 1) * clothVars.nParticles.x + (col));
      el.push_back((row)*clothVars.nParticles.x + (col));
    }
    el.push_back(PRIM_RESTART);
  }

  // We need buffers for position (2), element index,
  // velocity (2), normal, and texture coordinates.
  GLuint bufs[7];
  glGenBuffers(7, bufs);
 clothVars.posBufs[0] = bufs[0];
 clothVars.posBufs[1] = bufs[1];
 clothVars.velBufs[0] = bufs[2];
 clothVars.velBufs[1] = bufs[3];
 clothVars.normBuf = bufs[4];
 clothVars.elBuf = bufs[5];
 clothVars.tcBuf = bufs[6];

  GLuint parts = clothVars.nParticles.x * clothVars.nParticles.y;

  // The buffers for positions
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, clothVars.posBufs[0]);
  glBufferData(GL_SHADER_STORAGE_BUFFER, parts * 4 * sizeof(GLfloat), &initPos[0], GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, clothVars.posBufs[1]);
  glBufferData(GL_SHADER_STORAGE_BUFFER, parts * 4 * sizeof(GLfloat), NULL, GL_DYNAMIC_DRAW);

  // Velocities
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, clothVars.velBufs[0]);
  glBufferData(GL_SHADER_STORAGE_BUFFER, parts * 4 * sizeof(GLfloat), &initVel[0], GL_DYNAMIC_COPY);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, clothVars.velBufs[1]);
  glBufferData(GL_SHADER_STORAGE_BUFFER, parts * 4 * sizeof(GLfloat), NULL, GL_DYNAMIC_COPY);

  // Normal buffer
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, clothVars.normBuf);
  glBufferData(GL_SHADER_STORAGE_BUFFER, parts * 4 * sizeof(GLfloat), NULL, GL_DYNAMIC_COPY);

  // Element indicies
  glBindBuffer(GL_ARRAY_BUFFER, clothVars.elBuf);
  glBufferData(GL_ARRAY_BUFFER, el.size() * sizeof(GLuint), &el[0], GL_DYNAMIC_COPY);

  // Texture coordinates
  glBindBuffer(GL_ARRAY_BUFFER, clothVars.tcBuf);
  glBufferData(GL_ARRAY_BUFFER, initTc.size() * sizeof(GLfloat), &initTc[0], GL_STATIC_DRAW);

  clothVars.numElements = GLuint(el.size());

  // Set up the VAO
  glGenVertexArrays(1, &clothVars.clothVao);
  glBindVertexArray(clothVars.clothVao);

  glBindBuffer(GL_ARRAY_BUFFER, clothVars.posBufs[0]);
  glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
  glEnableVertexAttribArray(0);

  glBindBuffer(GL_ARRAY_BUFFER, clothVars.normBuf);
  glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, 0);
  glEnableVertexAttribArray(1);

  glBindBuffer(GL_ARRAY_BUFFER, clothVars.tcBuf);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, 0);
  glEnableVertexAttribArray(2);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, clothVars.elBuf);
  glBindVertexArray(0);

  glUseProgram(mRenderClothSimulShader.ID());
  mRenderClothSimulShader.SetUniformData("LightPosition", glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
  mRenderClothSimulShader.SetUniformData("LightIntensity", glm::vec4(1.0f));
  mRenderClothSimulShader.SetUniformData("Kd", glm::vec4(0.8f));
  mRenderClothSimulShader.SetUniformData("Ka", glm::vec4(0.2f));
  mRenderClothSimulShader.SetUniformData("Ks", glm::vec4(0.2f));
  mRenderClothSimulShader.SetUniformData("Shininess", 80.0f);

  glUseProgram(mComputeClothSimulShader.ID());
  dx = clothVars.clothSize.x / (clothVars.nParticles.x - 1);
  dy = clothVars.clothSize.y / (clothVars.nParticles.y - 1);
  mComputeClothSimulShader.SetUniformData("RestLengthHoriz", dx);
  mComputeClothSimulShader.SetUniformData("RestLengthVert", dy);
  mComputeClothSimulShader.SetUniformData("RestLengthDiag", sqrtf(dx * dx + dy * dy));
}

//-------------------------------------------------------
void eComputeShaderRender::_DispatchClothSimul(const Camera& _camera)
{
  glUseProgram(mComputeClothSimulShader.ID());
  for (int i = 0; i < 1000; ++i)
  {
    glDispatchCompute(clothVars.nParticles.x / 10, clothVars.nParticles.y / 10, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Swap buffers
    clothVars.readBuf = 1 - clothVars.readBuf;
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, clothVars.posBufs[clothVars.readBuf]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, clothVars.posBufs[1 - clothVars.readBuf]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, clothVars.velBufs[clothVars.readBuf]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, clothVars.velBufs[1 - clothVars.readBuf]);
  }

  // Compute the normals
  glUseProgram(mComputeClothSimulNorm.ID());
  glDispatchCompute(clothVars.nParticles.x / 10, clothVars.nParticles.y / 10, 1);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

//-------------------------------------------------------
void eComputeShaderRender::_RenderClothSimul(const Camera& _camera)
{
  // Now draw the scene
  glUseProgram(mRenderClothSimulShader.ID());
  glm::mat4 view = _camera.getWorldToViewMatrix();
  glm::mat4 model = glm::mat4(1.0f);
  glm::mat4 mv = view * model;
  glm::mat3 norm = glm::mat3(glm::vec3(mv[0]), glm::vec3(mv[1]), glm::vec3(mv[2]));

  mRenderClothSimulShader.SetUniformData("ModelViewMatrix", mv);
  mRenderClothSimulShader.SetUniformData("NormalMatrix", glm::mat4(norm));
  mRenderClothSimulShader.SetUniformData("MVP", _camera.getProjectionMatrix() * mv);

  // Draw the cloth
  glBindVertexArray(clothVars.clothVao);
  glDrawElements(GL_TRIANGLE_STRIP, clothVars.numElements, GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
}

//-------------------------------------------------------
void eComputeShaderRender::_InitWorleyTexture3d()
{
  //Generate cubic texture
  if (m_worley3DID == Texture::GetDefaultTextureId())
  {
    glGenTextures(1, &m_worley3DID);
    glBindTexture(GL_TEXTURE_3D, m_worley3DID);

    // Create a 3D texture and fill it with data
    std::vector<float> textureData(mWorleyDim * mWorleyDim * mWorleyDim * 4); // RGBA format

    // Generate perlin noise for 4th channel
    std::vector<float> perlinNoise;
    if (!dbb::ReadFromFile(perlinNoise, "perlin3D.bin"))
    {
      perlinNoise = dbb::generatePerlinNoise3D(mWorleyDim, mWorleyDim, mWorleyDim);
      dbb::WriteToFile(perlinNoise, "perlin3D.bin");
    }

    // Fill textureData with random data
    for (size_t i = 0; i < textureData.size(); i += 4)
    {
      textureData[i] = 0; // math::Random::RandomFloat(0.0f, 1.0f);       // Red
      textureData[i + 1] = 0; // math::Random::RandomFloat(0.0f, 1.0f);  // Green
      textureData[i + 2] = 0; // math::Random::RandomFloat(0.0f, 1.0f);  // Blue
      textureData[i + 3] = perlinNoise[i / 4];                     // Alpha -> perlin noise
    }

    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32F, mWorleyDim, mWorleyDim, mWorleyDim, 0, GL_RGBA, GL_FLOAT, textureData.data());

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_3D, 0);
  }
}

//-------------------------------------------------------
void eComputeShaderRender::_InitWorley3d()
{
  //Init points buffer 1
  int numPoints = mWorleyOctaveOneSize * mWorleyOctaveOneSize * mWorleyOctaveOneSize;
  mWorleyPoints1.resize(numPoints);

  // Fill the points buffer with random points in [0, 1] space
  for (int x = 0; x < mWorleyOctaveOneSize; ++x) {
    for (int y = 0; y < mWorleyOctaveOneSize; ++y) {
      for (int z = 0; z < mWorleyOctaveOneSize; ++z) {
        int index = x + y * mWorleyOctaveOneSize + z * mWorleyOctaveOneSize * mWorleyOctaveOneSize;
        mWorleyPoints1[index] = glm::vec4(
          static_cast<float>(x) / mWorleyOctaveOneSize + static_cast<float>(rand()) / RAND_MAX / mWorleyOctaveOneSize,
          static_cast<float>(y) / mWorleyOctaveOneSize + static_cast<float>(rand()) / RAND_MAX / mWorleyOctaveOneSize,
          static_cast<float>(z) / mWorleyOctaveOneSize + static_cast<float>(rand()) / RAND_MAX / mWorleyOctaveOneSize,
          1.0f
        );
      }
    }
  }
  if(mWorleyPointsBuffer1 == Texture::GetDefaultTextureId())
    glGenBuffers(1, &mWorleyPointsBuffer1);

  //Init points buffer 2
  numPoints = mWorleyOctaveTwoSize * mWorleyOctaveTwoSize * mWorleyOctaveTwoSize;
  mWorleyPoints2.resize(numPoints);

  // Fill the points buffer with random points in [0, 1] space
  for (int x = 0; x < mWorleyOctaveTwoSize; ++x) {
    for (int y = 0; y < mWorleyOctaveTwoSize; ++y) {
      for (int z = 0; z < mWorleyOctaveTwoSize; ++z) {
        int index = x + y * mWorleyOctaveTwoSize + z * mWorleyOctaveTwoSize * mWorleyOctaveTwoSize;
        mWorleyPoints2[index] = glm::vec4(
          static_cast<float>(x) / mWorleyOctaveTwoSize + static_cast<float>(rand()) / RAND_MAX / mWorleyOctaveTwoSize,
          static_cast<float>(y) / mWorleyOctaveTwoSize + static_cast<float>(rand()) / RAND_MAX / mWorleyOctaveTwoSize,
          static_cast<float>(z) / mWorleyOctaveTwoSize + static_cast<float>(rand()) / RAND_MAX / mWorleyOctaveTwoSize,
          1.0f
        );
      }
    }
  }
  if (mWorleyPointsBuffer2 == Texture::GetDefaultTextureId())
    glGenBuffers(1, &mWorleyPointsBuffer2);

  //Init points buffer 3
  numPoints = mWorleyOctaveThreeSize * mWorleyOctaveThreeSize * mWorleyOctaveThreeSize;
  mWorleyPoints3.resize(numPoints);

  // Fill the points buffer with random points in [0, 1] space
  for (int x = 0; x < mWorleyOctaveThreeSize; ++x) {
    for (int y = 0; y < mWorleyOctaveThreeSize; ++y) {
      for (int z = 0; z < mWorleyOctaveThreeSize; ++z) {
        int index = x + y * mWorleyOctaveThreeSize + z * mWorleyOctaveThreeSize * mWorleyOctaveThreeSize;
        mWorleyPoints3[index] = glm::vec4(
          static_cast<float>(x) / mWorleyOctaveThreeSize + static_cast<float>(rand()) / RAND_MAX / mWorleyOctaveThreeSize,
          static_cast<float>(y) / mWorleyOctaveThreeSize + static_cast<float>(rand()) / RAND_MAX / mWorleyOctaveThreeSize,
          static_cast<float>(z) / mWorleyOctaveThreeSize + static_cast<float>(rand()) / RAND_MAX / mWorleyOctaveThreeSize,
          1.0f
        );
      }
    }
  }
  if (mWorleyPointsBuffer3 == Texture::GetDefaultTextureId())
    glGenBuffers(1, &mWorleyPointsBuffer3);
}

//-------------------------------------------------------
void eComputeShaderRender::DispatchWorley3D(const Camera& _camera)
{
  glUseProgram(mWorley3D.ID());

  glBindBuffer(GL_SHADER_STORAGE_BUFFER, mWorleyPointsBuffer1);
  glBufferData(GL_SHADER_STORAGE_BUFFER, mWorleyPoints1.size() * sizeof(glm::vec4), mWorleyPoints1.data(), GL_STATIC_DRAW);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, mWorleyPointsBuffer1);

  glBindBuffer(GL_SHADER_STORAGE_BUFFER, mWorleyPointsBuffer2);
  glBufferData(GL_SHADER_STORAGE_BUFFER, mWorleyPoints2.size() * sizeof(glm::vec4), mWorleyPoints2.data(), GL_STATIC_DRAW);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, mWorleyPointsBuffer2);

  glBindBuffer(GL_SHADER_STORAGE_BUFFER, mWorleyPointsBuffer3);
  glBufferData(GL_SHADER_STORAGE_BUFFER, mWorleyPoints3.size() * sizeof(glm::vec4), mWorleyPoints3.data(), GL_STATIC_DRAW);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, mWorleyPointsBuffer3);

  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

  // Set uniform values
  glUniform1i(glGetUniformLocation(mWorley3D.ID(), "gamma"), m_noise_gamma);
  glUniform1i(glGetUniformLocation(mWorley3D.ID(), "octaveOneSize"), mWorleyOctaveOneSize);
  glUniform1i(glGetUniformLocation(mWorley3D.ID(), "octaveTwoSize"), mWorleyOctaveTwoSize);
  glUniform1i(glGetUniformLocation(mWorley3D.ID(), "octaveThreeSize"), mWorleyOctaveThreeSize);

  // Bind the 3D texture
  glActiveTexture(GL_TEXTURE0);
  glBindImageTexture(0, m_worley3DID, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F);

  //glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
  // Dispatch the compute shader
  glDispatchCompute((unsigned int)(mWorleyDim / 8), (unsigned int)(mWorleyDim / 8), (unsigned int)(mWorleyDim / 8));
  // Wait for the compute shader to finish
  glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

//------------------------------------------------------------------------------------
void eComputeShaderRender::InitForwardPlus()
{
  const unsigned int NUM_LIGHTS = 1024;
  const glm::ivec2 SCREEN_SIZE = { 960 , 600 }; //@todo external and syncronized
  // Define work group sizes in x and y direction based off screen size and tile size (in pixels)
  mWorkGroupsX = (SCREEN_SIZE.x + (SCREEN_SIZE.x % 16)) / 16;
  mWorkGroupsY = (SCREEN_SIZE.y + (SCREEN_SIZE.y % 16)) / 16;
  size_t numberOfTiles = mWorkGroupsX * mWorkGroupsY;

  // Generate our shader storage buffers
  glGenBuffers(1, &mLightBuffer);
  glGenBuffers(1, &mVisibleLightIndicesBuffer);

  // Bind light buffer
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, mLightBuffer);
  glBufferData(GL_SHADER_STORAGE_BUFFER, NUM_LIGHTS * sizeof(LightCompute), 0, GL_DYNAMIC_DRAW);

  // Bind visible light indices buffer
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, mVisibleLightIndicesBuffer);
  glBufferData(GL_SHADER_STORAGE_BUFFER, numberOfTiles * sizeof(VisibleIndex) * 1024, 0, GL_STATIC_DRAW);

  //// Set the default values for the light buffer
  //SetupLights();

  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);


  //setup lights
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, mLightBuffer);
  LightCompute* pointLights = (LightCompute*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_WRITE);

  for (int i = 0; i < NUM_LIGHTS; i++)
  {
    LightCompute& light = pointLights[i];
    light.position = glm::vec4(0,0,0, 1.0f);
    light.color = glm::vec4(1, 1, 1, 1.0f);
    light.paddingAndRadius = glm::vec4(glm::vec3(0.0f), 3.f);
  }
  glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

}

//-------------------------------------------------------------------------------------
void eComputeShaderRender::DispatchForwardPlus(const Camera& _camera, const std::vector<Light>& _lights)
{
  glUseProgram(mForwardPlus.ID());
  mForwardPlus.reloadIfNeeded({ {m_forward_shader_path.c_str(), GL_COMPUTE_SHADER} });

  auto projLoc = glGetUniformLocation(mForwardPlus.ID(), "projection");
  glUniformMatrix4fv(projLoc, 1, GL_FALSE, &_camera.getProjectionMatrix()[0][0]);
  auto viewLoc = glGetUniformLocation(mForwardPlus.ID(), "view");
  glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &_camera.getWorldToViewMatrix()[0][0]);
 
  mForwardPlus.SetUniformData("lightCount", _lights.size());
  mForwardPlus.SetUniformData("screenSize", glm::ivec2{ 960, 600}); //@todo
 // mForwardPlus.SetUniformData("numberOfTilesX", (size_t)mWorkGroupsX); ?

  //Depth map should be bind
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, mLightBuffer);
  LightCompute* pointLights = (LightCompute*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_WRITE);

  for (int i = 0; i < _lights.size(); i++)
  {
    LightCompute& light = pointLights[i];
    light.position = _camera.getWorldToViewMatrix() * _lights[i].light_position;
    light.paddingAndRadius.w = _lights[i].radius;
  }

  glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

  // Bind shader storage buffer objects for the light and indice buffers
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, mLightBuffer);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, mVisibleLightIndicesBuffer);

  // Dispatch the compute shader, using the workgroup values calculated earlier
  glDispatchCompute(mWorkGroupsX, mWorkGroupsY, 1);
  
  //unbind depth?
}
