#include "SimpleColorFbo.h"
#include <iostream>

//----------------------------------------------------------------------------------------------------------------------------
void SimpleColorFBO::BindForWriting()
{
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);
}

void SimpleColorFBO::BindForReading(GLenum TextureUnit)
{
	glActiveTexture(TextureUnit);
	glBindTexture(GL_TEXTURE_2D, m_texture);
}

void SimpleColorFBO::BindForReadingMask(GLenum TextureUnit)
{
	glActiveTexture(TextureUnit);
	glBindTexture(GL_TEXTURE_2D, m_texture_mask);
}

Texture SimpleColorFBO::GetTexture()
{
	return Texture(m_texture, m_width, m_height, 4);
}

Texture SimpleColorFBO::GetTextureMask()
{
	return Texture(m_texture_mask, m_width, m_height, 1);
}

bool SimpleColorFBO::Init(unsigned int WindowWidth, unsigned int WindowHeight, bool multisample, bool mask_attachment)
{
  // This keeps Init safe even if user calls it directly.
  // (If you enforce Resize() usage only, you can remove this.)
  if (m_fbo != 0)
    Destroy();

  m_width = WindowWidth;
  m_height = WindowHeight;

  // NOTE: SimpleColorFBO currently ignores multisample. If you later need MSAA SSAO,
  // you’ll add an MSAA path. For now it is a normal texture FBO.

  glGenFramebuffers(1, &m_fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

  glGenTextures(1, &m_texture);
  glBindTexture(GL_TEXTURE_2D, m_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, WindowWidth, WindowHeight, 0, GL_RED, GL_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0);

  if (mask_attachment)
  {
    glGenTextures(1, &m_texture_mask);
    glBindTexture(GL_TEXTURE_2D, m_texture_mask);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, WindowWidth, WindowHeight, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_texture_mask, 0);

    const GLenum attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);
  }
  else
  {
    const GLenum attachments[1] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, attachments);
  }

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    std::cout << "SimpleColorFBO framebuffer not complete!\n";

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return true;
}

void SimpleColorFBO::Destroy()
{
	if (m_fbo != 0) {
		glDeleteFramebuffers(1, &m_fbo);
		m_fbo = 0;
	}
	if (m_texture != 0) {
		glDeleteTextures(1, &m_texture);
		m_texture = 0;
	}
	if (m_texture_mask != 0) {
		glDeleteTextures(1, &m_texture_mask);
		m_texture_mask = 0;
	}

	m_width = m_height = 0;
}

bool SimpleColorFBO::Resize(unsigned int w, unsigned int h, bool multisample, bool mask_attachment)
{
	// If already matches, do nothing
	if (IsValid() &&
		m_width == w && m_height == h)
	{
		return true;
	}

	Destroy();
	return Init(w, h, multisample, mask_attachment);
}

SimpleColorFBO::~SimpleColorFBO()
{
  Destroy();
}