#include "stdafx.h"
#include "GaussianBlurRender.h"
#include "GlBufferContext.h"

//---------------------------------------------------------------------------------------------------
eGaussianBlurRender::eGaussianBlurRender(GLuint				_width, 
										                     GLuint				_height, 
										                     const std::string&	vS,
										                     const std::string&	fS)
: width(_width/2), height(_height/2)
{
	shader.installShaders(vS.c_str(), fS.c_str());

  /*eGlBufferContext::GetInstance().BufferInit(eBuffer::BUFFER_GAUSSIAN_ONE, width, height);
  eGlBufferContext::GetInstance().BufferInit(eBuffer::BUFFER_GAUSSIAN_TWO, width, height);*/
	
	TexWidthLoc = glGetUniformLocation(shader.ID(), "targetWidth");
	textureLoc	= glGetUniformLocation(shader.ID(), "screenTexture");

	GLfloat quadVertices[] = {
		// Positions   // TexCoords
		-1.0f,  1.0f,  0.0f, 1.0f,
		-1.0f, -1.0f,  0.0f, 0.0f,
		1.0f, -1.0f,  1.0f, 0.0f,

		-1.0f,  1.0f,  0.0f, 1.0f,
		1.0f, -1.0f,  1.0f, 0.0f,
		1.0f,  1.0f,  1.0f, 1.0f
	};

	// Setup screen VAO
	glGenVertexArrays(1, &quadVAO);
	glGenBuffers(1, &quadVBO);
	glBindVertexArray(quadVAO);
	glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid*)(2 * sizeof(GLfloat)));
	glBindVertexArray(0);
}

//---------------------------------------------------------------------------------------------------
eGaussianBlurRender::~eGaussianBlurRender()
{
  glDeleteVertexArrays(1, &quadVAO);
  glDeleteBuffers(1, &quadVBO);
}

//---------------------------------------------------------------------------------------------------
void eGaussianBlurRender::Render()
{
		glUseProgram(shader.ID());
		shader.SetUniformData("kernelSize", kernelSize);

		eGlBufferContext::GetInstance().EnableWrittingBuffer(eBuffer::BUFFER_GAUSSIAN_ONE);
		glViewport(0, 0, width, height);
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		glBindVertexArray(quadVAO);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, texture.m_id);	// Use the color attachment texture as the texture of the quad plane

		float px = 1.f / (float)width * sampleSize;
		shader.SetUniformData("pixelSize", px);
		GLuint BlurIndex = glGetSubroutineIndex(shader.ID(), GL_VERTEX_SHADER, "HorizontalBlur");
		glUniformSubroutinesuiv(GL_VERTEX_SHADER, 1, &BlurIndex);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);

		eGlBufferContext::GetInstance().EnableReadingBuffer(eBuffer::BUFFER_GAUSSIAN_ONE, GL_TEXTURE1);
		eGlBufferContext::GetInstance().EnableWrittingBuffer(eBuffer::BUFFER_GAUSSIAN_TWO);
		glViewport(0, 0, width, height);
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		glBindVertexArray(quadVAO);

		px = 1.f / (float)height * sampleSize;
		shader.SetUniformData("pixelSize", px);
		BlurIndex = glGetSubroutineIndex(shader.ID(), GL_VERTEX_SHADER, "VerticalBlur");
		glUniformSubroutinesuiv(GL_VERTEX_SHADER, 1, &BlurIndex);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);
}

