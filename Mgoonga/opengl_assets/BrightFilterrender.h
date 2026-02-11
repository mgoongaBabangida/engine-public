#pragma once

#include "Texture.h"
#include "Shader.h"

//-----------------------------------------
class eBrightFilterRender
{
public:
	eBrightFilterRender(GLuint				widtht,
						GLuint				Height, 
						const std::string&	vS,
						const std::string&	fS);
  ~eBrightFilterRender();
	void Render();
	void SetTexture(Texture t) { texture = t; }

	Shader& GetShader() { return shader; }

	float& Amplifier() { return m_amplifier; }

private:
	Shader shader;
  Texture texture;
  GLuint quadVAO;
  GLuint quadVBO;

	float m_amplifier = 0.025f;
};
