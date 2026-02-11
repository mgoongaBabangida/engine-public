#ifndef COLORFBO_H 
#define COLORFBO_H

#include "Texture.h"
#include "SimpleColorFbo.h"

//-------------------------------------------------------------------------------------
class eColorFBO : public SimpleColorFBO
{
public:
  eColorFBO() = default;
  ~eColorFBO() { Destroy(); }

  eColorFBO(const eColorFBO&) = delete;
  eColorFBO& operator=(const eColorFBO&) = delete;

  eColorFBO(eColorFBO&& other) noexcept { MoveFrom(std::move(other)); }
  eColorFBO& operator=(eColorFBO&& other) noexcept
  {
    if (this != &other) { Destroy(); MoveFrom(std::move(other)); }
    return *this;
  }

  bool Init(unsigned w, unsigned h, bool multisample = false, bool mask_attachment = false) override;
  bool Resize(unsigned int w, unsigned int h, bool multisample = false, bool mask_attachment = false) override;
  void Destroy() override;

  bool IsValid() const override  { return m_fbo != 0; }
  bool HasMask() const { return m_hasMask; }
  bool IsMultisample() const { return m_multisample; }

  // existing API assumed:
  void BindForWriting() override;
  //void BindForReading(GLenum unit);
  //void BindForReadingMask(GLenum unit);

  //Texture GetTexture();
  //Texture GetTextureMask();

  GLuint ID() const { return m_fbo; }
  glm::ivec2 Size() const { return { (int)m_width, (int)m_height }; }

  void ResolveToFBO(eColorFBO* other);
  void ResolveToScreen(unsigned WindowWidth, unsigned WindowHeight);

private:
  void MoveFrom(eColorFBO&& other) noexcept;

  void CreateMsaaAttachments(unsigned w, unsigned h);

private:
 /* GLuint m_fbo = 0;*/

  // non-MSAA textures
 /* GLuint m_texture = 0;*/
 /* GLuint m_texture_mask = 0;*/

  // MSAA color attachments (renderbuffers)
  GLuint m_msaaColorRbo = 0;
  GLuint m_msaaMaskRbo = 0;

  // depth-stencil always stored here
  GLuint m_depthStencilRbo = 0;

 /* unsigned m_width = 0;
  unsigned m_height = 0;*/

  bool m_multisample = false;
  bool m_hasMask = false;

  int m_samples = 8; // 4 - 8
};

//-------------------------------------------------------------------------------------
class CubemapFBO
{
public:
	~CubemapFBO();

	 bool		Init(unsigned int _size);

	 void		BindForWriting();
	 void		BindForReading(GLenum TextureUnit);

	 Texture		GetTexture();
	 GLuint			ID() { return m_fbo; }
	 GLuint			RboID() { return m_rbo; }
	 GLuint			Width() { return  m_width; }
	 GLuint			Height() { return  m_height; }
	 glm::ivec2 Size() { return { m_width , m_height }; }

protected:
	GLuint		m_fbo;
	GLuint		m_texture;
	GLuint		m_width;
	GLuint		m_height;
	GLuint		m_rbo;

	Texture		m_textureBuffer;
};

#endif /* COLORFBO_H */


