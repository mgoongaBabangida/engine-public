#pragma once

#include "Texture.h"

//-------------------------------------------------------------------------------------
class SimpleColorFBO
{
public:
	SimpleColorFBO() = default;
	virtual ~SimpleColorFBO();

	virtual void		BindForWriting();
	void		BindForReading(GLenum TextureUnit);
	void		BindForReadingMask(GLenum TextureUnit);

	Texture		GetTexture();
	Texture		GetTextureMask();

	GLuint			ID() { return m_fbo; }
	GLuint			Width() { return  m_width; }
	GLuint			Height() { return  m_height; }
	glm::ivec2	Size() { return { m_width , m_height }; }

	virtual bool		Init(unsigned int WindowWidth, unsigned int WindowHeight,
		bool multisample = false, bool mask_attachment = false);

	virtual bool Resize(unsigned int WindowWidth, unsigned int WindowHeight,
		bool multisample = false, bool mask_attachment = false);

	virtual void Destroy();

	virtual bool IsValid() const { return m_fbo != 0; }

protected:
	GLuint		m_fbo;
	GLuint		m_texture;
	GLuint		m_width;
	GLuint		m_height;
	GLuint		m_texture_mask;
};