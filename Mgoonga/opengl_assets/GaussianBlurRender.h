#pragma once

#include "Texture.h"
#include "Shader.h"

//---------------------------------------------
class eGaussianBlurRender
{
public:
	eGaussianBlurRender(GLuint	width,
						GLuint				height, 
						const std::string&	vS,
						const std::string&	fS);
  virtual ~eGaussianBlurRender();

	void Render();
	void SetTexture(Texture t) { texture = t; }

  Shader& GetShader() { return shader; }

  size_t& KernelSize() { return kernelSize; }
  float& SampleSize() { return sampleSize; }

private:
  Shader shader;

  Texture texture;

  GLuint quadVAO;
  GLuint quadVBO;

  GLuint textureLoc;
  GLuint TexWidthLoc;

  GLuint width;
  GLuint height;

  size_t kernelSize = 5;
  float sampleSize = 1.0f;
};