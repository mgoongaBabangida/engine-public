#include "stdafx.h"
#include "colorFBO.h"

#include <iostream>
#include <cmath>
#include <algorithm>

#ifdef GL_KHR_debug
static void TryLabel(GLenum type, GLuint id, const char* name)
{
  if (id != 0 && glObjectLabel) glObjectLabel(type, id, -1, name);
}
#else
static void TryLabel(GLenum, GLuint, const char*) {}
#endif

static bool CheckFboComplete(const char* label)
{
  const GLenum s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (s != GL_FRAMEBUFFER_COMPLETE)
  {
    std::cout << "ERROR::FBO::" << label << " incomplete. Status=0x" << std::hex << s << std::dec << "\n";
    return false;
  }
  return true;
}

// to reuse with simple color fbo
void eColorFBO::BindForWriting()
{
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);

  if (m_hasMask)
  {
    const GLenum bufs[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, bufs);
  }
  else
  {
    const GLenum bufs[1] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, bufs);
  }
}

//-----------------------------------------------------------------------
void eColorFBO::MoveFrom(eColorFBO&& o) noexcept
{
  m_fbo = o.m_fbo; o.m_fbo = 0;

  m_texture = o.m_texture; o.m_texture = 0;
  m_texture_mask = o.m_texture_mask; o.m_texture_mask = 0;

  m_msaaColorRbo = o.m_msaaColorRbo; o.m_msaaColorRbo = 0;
  m_msaaMaskRbo = o.m_msaaMaskRbo; o.m_msaaMaskRbo = 0;

  m_depthStencilRbo = o.m_depthStencilRbo; o.m_depthStencilRbo = 0;

  m_width = o.m_width; o.m_width = 0;
  m_height = o.m_height; o.m_height = 0;

  m_multisample = o.m_multisample;
  m_hasMask = o.m_hasMask;
  m_samples = o.m_samples;
}

void eColorFBO::Destroy()
{
  if (m_depthStencilRbo) { glDeleteRenderbuffers(1, &m_depthStencilRbo); m_depthStencilRbo = 0; }
  if (m_msaaColorRbo) { glDeleteRenderbuffers(1, &m_msaaColorRbo);     m_msaaColorRbo = 0; }
  if (m_msaaMaskRbo) { glDeleteRenderbuffers(1, &m_msaaMaskRbo);      m_msaaMaskRbo = 0; }

  if (m_texture) { glDeleteTextures(1, &m_texture);          m_texture = 0; }
  if (m_texture_mask) { glDeleteTextures(1, &m_texture_mask);     m_texture_mask = 0; }

  if (m_fbo) { glDeleteFramebuffers(1, &m_fbo);          m_fbo = 0; }

  m_width = m_height = 0;
  m_multisample = false;
  m_hasMask = false;
}

bool eColorFBO::Resize(unsigned w, unsigned h, bool multisample, bool mask_attachment)
{
  if (IsValid() &&
    m_width == w && m_height == h &&
    m_multisample == multisample &&
    m_hasMask == mask_attachment)
  {
    return true; // nothing to do
  }

  Destroy();
  return Init(w, h, multisample, mask_attachment);
}

void eColorFBO::CreateMsaaAttachments(unsigned w, unsigned h)
{
  // COLOR (MSAA renderbuffer)
  glGenRenderbuffers(1, &m_msaaColorRbo);
  glBindRenderbuffer(GL_RENDERBUFFER, m_msaaColorRbo);
  glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_samples, GL_RGBA16F, w, h);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_msaaColorRbo);
  TryLabel(GL_RENDERBUFFER, m_msaaColorRbo, "eColorFBO::MSAA_ColorRBO");

  if (m_hasMask)
  {
    glGenRenderbuffers(1, &m_msaaMaskRbo);
    glBindRenderbuffer(GL_RENDERBUFFER, m_msaaMaskRbo);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_samples, GL_R16F, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, m_msaaMaskRbo);
    TryLabel(GL_RENDERBUFFER, m_msaaMaskRbo, "eColorFBO::MSAA_MaskRBO");

    const GLenum attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);
  }
  else
  {
    const GLenum attachments[1] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, attachments);
  }
}

bool eColorFBO::Init(unsigned w, unsigned h, bool multisample, bool mask_attachment)
{
  // IMPORTANT: Init does not assume clean state; callers may call Resize() (which Destroy()s first)
  m_width = w;
  m_height = h;
  m_multisample = multisample;
  m_hasMask = mask_attachment;

  glGenFramebuffers(1, &m_fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
  TryLabel(GL_FRAMEBUFFER, m_fbo, "eColorFBO::FBO");

  if (!m_multisample)
  {
    // Color texture
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, w, h);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const GLfloat borderColor[] = { 1,1,1,1 };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0);
    TryLabel(GL_TEXTURE, m_texture, "eColorFBO::ColorTex");

    if (m_hasMask)
    {
      glGenTextures(1, &m_texture_mask);
      glBindTexture(GL_TEXTURE_2D, m_texture_mask);

      // If you really need mipmaps, you must call glGenerateMipmap after rendering.
      // Keeping your mip storage; just be aware.
      /*const int mipLevels = (int)std::floor(std::log2((double)std::max(w, h))) + 1;
      glTexStorage2D(GL_TEXTURE_2D, mipLevels, GL_R16F, w, h);*/

     /* GLint swizzleMask[] = { GL_RED, GL_RED, GL_RED, GL_ONE };
      glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);*/
      
      glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, w, h, 0, GL_RED, GL_FLOAT, nullptr);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_texture_mask, 0);
      TryLabel(GL_TEXTURE, m_texture_mask, "eColorFBO::MaskTex");

      const GLenum attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
      glDrawBuffers(2, attachments);
    }
    else
    {
      const GLenum attachments[1] = { GL_COLOR_ATTACHMENT0 };
      glDrawBuffers(1, attachments);
    }
  }
  else
  {
    CreateMsaaAttachments(w, h);
  }

  // Depth-stencil renderbuffer (separate from MSAA color rbos)
  glGenRenderbuffers(1, &m_depthStencilRbo);
  glBindRenderbuffer(GL_RENDERBUFFER, m_depthStencilRbo);

  if (!m_multisample)
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH32F_STENCIL8, w, h);
  else
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_samples, GL_DEPTH32F_STENCIL8, w, h);

  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilRbo);
  TryLabel(GL_RENDERBUFFER, m_depthStencilRbo, "eColorFBO::DepthStencilRBO");

  const bool ok = CheckFboComplete("eColorFBO");
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return ok;
}

void eColorFBO::ResolveToFBO(eColorFBO* other)
{
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, other->m_fbo);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo);

  // Always resolve color0 + depth/stencil like you had
  glReadBuffer(GL_COLOR_ATTACHMENT0);
  glDrawBuffer(GL_COLOR_ATTACHMENT0);
  glBlitFramebuffer(0, 0, (int)m_width, (int)m_height,
    0, 0, (int)other->m_width, (int)other->m_height,
    GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST);

  // Resolve mask only if both have it
  if (m_hasMask && other->m_hasMask)
  {
    glReadBuffer(GL_COLOR_ATTACHMENT1);
    glDrawBuffer(GL_COLOR_ATTACHMENT1);
    glBlitFramebuffer(0, 0, (int)m_width, (int)m_height,
      0, 0, (int)other->m_width, (int)other->m_height,
      GL_COLOR_BUFFER_BIT, GL_LINEAR);
  }

  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
}

//--------------------------------------------------------------------------------------------------------
CubemapFBO::~CubemapFBO()
{
	m_textureBuffer.freeTexture();
	if (m_fbo != 0) {
		glDeleteFramebuffers(1, &m_fbo);
		glDeleteTextures(1, &m_texture); // @todo double delete with freetexture ? check
	}
}

bool CubemapFBO::Init(unsigned int _size)
{
	m_width = _size;
	m_height = _size;
	glGenFramebuffers(1, &m_fbo);
	glGenRenderbuffers(1, &m_rbo);

	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	glBindRenderbuffer(GL_RENDERBUFFER, m_rbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, _size, _size);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_rbo);
	
	m_textureBuffer.makeCubemap(_size);
	m_texture = m_textureBuffer.m_id;

	for (unsigned int i = 0; i < 6; ++i)
	{
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_texture, 0);
	}
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "Cube Framebuffer not complete!" << std::endl;
	return true;
}

void CubemapFBO::BindForWriting()
{
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);
	glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_rbo);
}

void CubemapFBO::BindForReading(GLenum TextureUnit)
{
	glActiveTexture(TextureUnit);
	glBindTexture(GL_TEXTURE_CUBE_MAP, m_texture);
}

Texture CubemapFBO::GetTexture()
{
	return Texture(m_texture, m_width, m_height, m_textureBuffer.m_channels); //!
}
