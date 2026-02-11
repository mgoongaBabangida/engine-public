#include "stdafx.h"
#include "ShadowMapFbo.h"
#include <stdio.h>

//-------------------------------------------------------------
 ShadowMapFBO::ShadowMapFBO()
 {
		m_fbo = 0;
 }

 //-------------------------------------------------------------
 ShadowMapFBO::~ShadowMapFBO()
 {
  if (m_fbo != 0) 
	{
		glDeleteFramebuffers(1, &m_fbo);
		m_shadowMap.freeTexture();
  }
 }

 //-------------------------------------------------------------
 bool ShadowMapFBO::Init(unsigned int WindowWidth, unsigned int WindowHeight, bool needsCubeMap)
 {
	 m_shadowMap.m_width = WindowWidth;
	 m_shadowMap.m_height = WindowHeight;
	 
	 glGenFramebuffers(1, &m_fbo);
	 glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);

	 if (needsCubeMap)
	 {
		 m_shadowMap.m_width = 1024; //!?
		 m_shadowMap.m_height = 1024;//!?
		 m_cubemap = needsCubeMap;
		 m_shadowMap.makeDepthCubeMap();
		 glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_shadowMap.m_id, 0);
		 glReadBuffer(GL_NONE); //?
	 }
	 else
	 {
		 m_shadowMap.makeDepthTexture();
		 glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_shadowMap.m_id, 0);
	 }
	 glDrawBuffer(GL_NONE);

	 GLenum Status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	  	  
	   if (Status != GL_FRAMEBUFFER_COMPLETE) {
	       printf("FB error, status: 0x%x\n", Status);
	       return false;
	  
	}
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	     return true;
	 }

 //-------------------------------------------------------------
 bool ShadowMapFBO::InitCSM(unsigned int _WindowWidth, unsigned int _WindowHeight, int32_t _layers)
 {
	 m_shadowMap.m_width = _WindowWidth;
	 m_shadowMap.m_height = _WindowHeight;

	 glGenFramebuffers(1, &m_fbo);

	 // Create/allocate the depth array texture
	 if (!m_shadowMap.makeDepthTextureArray(_layers))
		 return false;

	 // Bind FBO once, sanity check with a single-layer attach (optional)
	 glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

	 // Attach one layer temporarily just to check completeness
	 glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_shadowMap.m_id, 0, 0);

	 glDrawBuffer(GL_NONE);
	 glReadBuffer(GL_NONE);

	 GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	 if (status != GL_FRAMEBUFFER_COMPLETE) {
		 printf("Shadow FBO incomplete: 0x%x\n", status);
		 glBindFramebuffer(GL_FRAMEBUFFER, 0);
		 return false;
	 }

	 // Detach; we will attach the correct layer per pass at render time
	 glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, 0, 0);

	 glBindFramebuffer(GL_FRAMEBUFFER, 0);
	 m_array = true;
	 return true;
 }

 //-------------------------------------------------------------
 void ShadowMapFBO::BindForWriting()
 {
		glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
		glClear(GL_DEPTH_BUFFER_BIT);
 }

 //-------------------------------------------------------------
 void ShadowMapFBO::BindForReading(GLenum TextureUnit)
{
	 glActiveTexture(TextureUnit);
	 if (m_cubemap)
		 glBindTexture(GL_TEXTURE_CUBE_MAP, m_shadowMap.m_id);
	 else if(m_array)
		 glBindTexture(GL_TEXTURE_2D_ARRAY, m_shadowMap.m_id);
	 else
		 glBindTexture(GL_TEXTURE_2D, m_shadowMap.m_id);
}

 //-------------------------------------------------------------
 Texture ShadowMapFBO::GetTexture()
 {
	 return m_shadowMap;
 }