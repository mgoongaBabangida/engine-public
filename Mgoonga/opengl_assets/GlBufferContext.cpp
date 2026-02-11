#include "stdafx.h"
#include "GlBufferContext.h"

//----------------------------------------------------------------
GLuint eGlBufferContext::BufferCustomInit(const std::string& name, unsigned w, unsigned h, bool multisample, bool mask_attachment)
{
	if (auto it = customBuffers.find(name); it != customBuffers.end())
	{
		// Resize existing custom buffer if needed
		it->second.Resize(w, h, multisample, mask_attachment);
		return it->second.ID();
	}

	eColorFBO buffer;
	buffer.Init(w, h, multisample, mask_attachment);
	const GLuint id = buffer.ID();
	customBuffers.insert({ name, std::move(buffer) });
	return id;
}

//-------------------------------------------------------
void eGlBufferContext::SetDefaultState()
{
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);

	glDisable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquation(GL_FUNC_ADD);

	glDisable(GL_STENCIL_TEST);
	glStencilMask(0xFF);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glDisable(GL_CLIP_DISTANCE0);
}

//----------------------------------------------------------------
uint32_t eGlBufferContext::BufferSSBOInitMat4(eSSBO _index, size_t _size, bool _persist)
{
	if(ssboBuffers.contains(_index))
		return 0; // already init
	else
	{
		SSBO<glm::mat4> buffer(static_cast<GLuint>(_index), _size, _persist);
		ssboBuffers.emplace(_index, std::move(buffer));
		return 1;
	}
	return 0;
}

//----------------------------------------------------------------
uint32_t eGlBufferContext::BufferSSBOInitVec4(eSSBO _index, size_t _size, bool _persist)
{
	if (ssboBuffersVec4.contains(_index))
		return 0; // already init
	else
	{
		SSBO<glm::vec4> buffer(static_cast<GLuint>(_index), _size, _persist);
		ssboBuffersVec4.emplace(_index, std::move(buffer));
		return 1;
	}
	return 0;
}

//----------------------------------------------------------------
uint32_t eGlBufferContext::BufferSSBOInitInt(eSSBO _index, size_t _size, bool _persist)
{
	if (ssboBuffersInt.contains(_index))
		return 0; // already init
	else
	{
		SSBO<GLuint> buffer(static_cast<GLuint>(_index), _size, _persist);
		ssboBuffersInt.emplace(_index, std::move(buffer));
		return 1;
	}
	return 0;
}

//----------------------------------------------------------------
void eGlBufferContext::UploadSSBOData(eSSBO _index, const std::vector<glm::mat4>& _data)
{
	auto it = ssboBuffers.find(_index);
	if (it != ssboBuffers.end())
		it->second.UploadData(_data);
}

//----------------------------------------------------------------
void eGlBufferContext::UploadSSBOData(eSSBO _index, const std::vector<glm::vec4>& _data)
{
	auto it = ssboBuffersVec4.find(_index);
	if (it != ssboBuffersVec4.end())
		it->second.UploadData(_data);
}

//----------------------------------------------------------------
void eGlBufferContext::UploadSSBOData(eSSBO _index, const std::vector<GLuint>& _data)
{
	auto it = ssboBuffersInt.find(_index);
	if (it != ssboBuffersInt.end())
		it->second.UploadData(_data);
}

//----------------------------------------------------------------
void eGlBufferContext::BindSSBO(eSSBO _index)
{
	auto it = ssboBuffers.find(_index);
	if (it != ssboBuffers.end())
		it->second.Bind();
}

//----------------------------------------------------------------
void eGlBufferContext::EnableCustomWrittingBuffer(const std::string& _name)
{
	if (auto it = customBuffers.find(_name); it != customBuffers.end())
		it->second.BindForWriting();
}

//----------------------------------------------------------------
void eGlBufferContext::EnableCustomReadingBuffer(const std::string& _name, GLenum slot)
{
	if (auto it = customBuffers.find(_name); it != customBuffers.end())
		it->second.BindForReading(slot);
}

//----------------------------------------------------------------
Texture eGlBufferContext::GetTexture(const std::string& _name)
{
	if (auto it = customBuffers.find(_name); it != customBuffers.end())
		return it->second.GetTexture();
	else
		return Texture();/*?*/
}

//----------------------------------------------------------------
void eGlBufferContext::BlitFromTo(eBuffer _from, eBuffer _to, GLenum bit)
{
	glBindFramebuffer(GL_READ_FRAMEBUFFER, GetId(_from));
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, GetId(_to));
	glBlitFramebuffer(0, 0, GetSize(_from).x, GetSize(_from).y, 0, 0,
													GetSize(_to).x, GetSize(_to).y, bit, GL_NEAREST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

//----------------------------------------------------------------
void eGlBufferContext::BlitFromTo(eBuffer _from, const std::string& _to, GLenum bit)
{
	glBindFramebuffer(GL_READ_FRAMEBUFFER, GetId(_from));
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, GetId(_to));
	glBlitFramebuffer(0, 0, GetSize(_from).x, GetSize(_from).y, 0, 0,
		GetSize(_to).x, GetSize(_to).y, bit, GL_NEAREST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

//----------------------------------------------------------------
GLuint eGlBufferContext::GetId(const std::string& _name)
{
	if (auto it = customBuffers.find(_name); it != customBuffers.end())
		return it->second.ID();
	else
		return -1;/*?*/
}

//---------------------------------------------------------------
glm::ivec2 eGlBufferContext::GetSize(const std::string& _name)
{
	if (auto it = customBuffers.find(_name); it != customBuffers.end())
		return it->second.Size();
	else
		return glm::ivec2();
}

//----------------------------------------------------------------
void eGlBufferContext::BufferInit(eBuffer b, unsigned w, unsigned h)
{
	View(b).Init(w, h);
}

//----------------------------------------------------------------
void eGlBufferContext::EnableWrittingBuffer(eBuffer b)
{
	View(b).BindForWriting();
}

//----------------------------------------------------------------
void eGlBufferContext::EnableReadingBuffer(eBuffer b, GLenum slot)
{
	View(b).BindForReading(slot);
}

//----------------------------------------------------------------
Texture eGlBufferContext::GetTexture(eBuffer b)
{
	return View(b).GetTexture();
}

//----------------------------------------------------------------
GLuint eGlBufferContext::GetId(eBuffer b)
{
	return View(b).ID();
}

//----------------------------------------------------------------
glm::ivec2 eGlBufferContext::GetSize(eBuffer b)
{
	return View(b).Size();
}

//----------------------------------------------------------------
GLuint eGlBufferContext::GetRboID(eBuffer b)
{
	return View(b).RboID();
}

//----------------------------------------------------------------
void eGlBufferContext::BuildViews()
{
	auto make = [&](eBuffer b, const char* name) -> LambdaBufferView&
	{
		auto v = std::make_unique<LambdaBufferView>();
		v->m_name = name;
		auto* raw = v.get();
		m_views[Idx(b)] = std::move(v);
		return *raw;
	};

	// -------------------------
	// Helpers for common patterns
	// -------------------------
	auto bindColorFbo = [&](eColorFBO& fbo, bool multisample, bool mask)
	{
		return [&, multisample, mask](unsigned w, unsigned h) { fbo.Resize(w, h, multisample, mask); };
	};

	auto bindSimpleFbo = [&](SimpleColorFBO& fbo, bool multisample, bool mask)
	{
		return [&, multisample, mask](unsigned w, unsigned h) { fbo.Resize(w, h, multisample, mask); };
	};

	// -------------------------
	// BUFFER_DEFAULT
	// -------------------------
	{
		auto& v = make(eBuffer::BUFFER_DEFAULT, "BUFFER_DEFAULT");
		v.m_init  = bindColorFbo(defaultFBO, false, false);
		v.m_write = [&]{ defaultFBO.BindForWriting(); };
		v.m_read  = [&](GLenum slot){ defaultFBO.BindForReading(slot); };
		v.m_tex   = [&]{ return defaultFBO.GetTexture(); };
		v.m_id    = [&]{ return defaultFBO.ID(); };
		v.m_size  = [&]{ return defaultFBO.Size(); };
	}

	// -------------------------
	// SCREEN (color) + SCREEN_MASK (mask view)
	// -------------------------
	{
		auto initScreen = bindColorFbo(screenFBO, false, true);

		auto& v0 = make(eBuffer::BUFFER_SCREEN, "BUFFER_SCREEN");
		v0.m_init  = initScreen;
		v0.m_write = [&]{ screenFBO.BindForWriting(); };
		v0.m_read  = [&](GLenum slot){ screenFBO.BindForReading(slot); };
		v0.m_tex   = [&]{ return screenFBO.GetTexture(); };
		v0.m_id    = [&]{ return screenFBO.ID(); };
		v0.m_size  = [&]{ return screenFBO.Size(); };

		auto& v1 = make(eBuffer::BUFFER_SCREEN_MASK, "BUFFER_SCREEN_MASK");
		v1.m_init  = initScreen; // init underlying FBO the same way
		v1.m_write = [&]{ screenFBO.BindForWriting(); };
		v1.m_read  = [&](GLenum slot){ screenFBO.BindForReadingMask(slot); };
		v1.m_tex   = [&]{ return screenFBO.GetTextureMask(); };
		v1.m_id    = [&]{ return screenFBO.ID(); };
		v1.m_size  = [&]{ return screenFBO.Size(); };
	}

	// -------------------------
	// SCREEN_WITH_SSR
	// -------------------------
	{
		auto& v = make(eBuffer::BUFFER_SCREEN_WITH_SSR, "BUFFER_SCREEN_WITH_SSR");
		v.m_init  = bindColorFbo(screenSsrFBO, false, false);
		v.m_write = [&]{ screenSsrFBO.BindForWriting(); };
		v.m_read  = [&](GLenum slot){ screenSsrFBO.BindForReading(slot); };
		v.m_tex   = [&]{ return screenSsrFBO.GetTexture(); };
		v.m_id    = [&]{ return screenSsrFBO.ID(); };
		v.m_size  = [&]{ return screenSsrFBO.Size(); };
	}

	// -------------------------
	// MTS (multisample)
	// -------------------------
	{
		auto& v = make(eBuffer::BUFFER_MTS, "BUFFER_MTS");
		v.m_init  = bindColorFbo(mtsFBO, true, true);
		v.m_write = [&]{ mtsFBO.BindForWriting(); };
		v.m_read  = [&](GLenum slot){ mtsFBO.BindForReading(slot); };
		v.m_tex   = [&]{ return mtsFBO.GetTexture(); };
		v.m_id    = [&]{ return mtsFBO.ID(); };
		v.m_size  = [&]{ return mtsFBO.Size(); };
	}

	// -------------------------
	// Reflection / Refraction
	// -------------------------
	{
		auto& v = make(eBuffer::BUFFER_REFLECTION, "BUFFER_REFLECTION");
		v.m_init  = bindColorFbo(reflectionFBO, false, false);
		v.m_write = [&]{ reflectionFBO.BindForWriting(); };
		v.m_read  = [&](GLenum slot){ reflectionFBO.BindForReading(slot); };
		v.m_tex   = [&]{ return reflectionFBO.GetTexture(); };
		v.m_id    = [&]{ return reflectionFBO.ID(); };
		v.m_size  = [&]{ return reflectionFBO.Size(); };
	}
	{
		auto& v = make(eBuffer::BUFFER_REFRACTION, "BUFFER_REFRACTION");
		v.m_init  = bindColorFbo(refractionFBO, false, false);
		v.m_write = [&]{ refractionFBO.BindForWriting(); };
		v.m_read  = [&](GLenum slot){ refractionFBO.BindForReading(slot); };
		v.m_tex   = [&]{ return refractionFBO.GetTexture(); };
		v.m_id    = [&]{ return refractionFBO.ID(); };
		v.m_size  = [&]{ return refractionFBO.Size(); };
	}

	// -------------------------
	// SSR (color) + SSR_HIT_MASK (mask view) + SSR_BLUR
	// -------------------------
	{
		auto initSSR = bindColorFbo(ssrFBO, false, true);

		auto& v0 = make(eBuffer::BUFFER_SSR, "BUFFER_SSR");
		v0.m_init  = initSSR;
		v0.m_write = [&]{ ssrFBO.BindForWriting(); };
		v0.m_read  = [&](GLenum slot){ ssrFBO.BindForReading(slot); };
		v0.m_tex   = [&]{ return ssrFBO.GetTexture(); };
		v0.m_id    = [&]{ return ssrFBO.ID(); };
		v0.m_size  = [&]{ return ssrFBO.Size(); };

		auto& v1 = make(eBuffer::BUFFER_SSR_HIT_MASK, "BUFFER_SSR_HIT_MASK");
		v1.m_init  = initSSR;
		v1.m_write = [&]{ ssrFBO.BindForWriting(); };
		v1.m_read  = [&](GLenum slot){ ssrFBO.BindForReadingMask(slot); };
		v1.m_tex   = [&]{ return ssrFBO.GetTextureMask(); };
		v1.m_id    = [&]{ return ssrFBO.ID(); };
		v1.m_size  = [&]{ return ssrFBO.Size(); };
	}
	{
		auto& v = make(eBuffer::BUFFER_SSR_BLUR, "BUFFER_SSR_BLUR");
		v.m_init  = bindColorFbo(ssrblurFBO, false, false);
		v.m_write = [&]{ ssrblurFBO.BindForWriting(); };
		v.m_read  = [&](GLenum slot){ ssrblurFBO.BindForReading(slot); };
		v.m_tex   = [&]{ return ssrblurFBO.GetTexture(); };
		v.m_id    = [&]{ return ssrblurFBO.ID(); };
		v.m_size  = [&]{ return ssrblurFBO.Size(); };
	}

	// -------------------------
	// Bright filter (color) + mask
	// -------------------------
	{
		auto initBF = bindColorFbo(brightFilterFBO, false, true);

		auto& v0 = make(eBuffer::BUFFER_BRIGHT_FILTER, "BUFFER_BRIGHT_FILTER");
		v0.m_init  = initBF;
		v0.m_write = [&]{ brightFilterFBO.BindForWriting(); };
		v0.m_read  = [&](GLenum slot){ brightFilterFBO.BindForReading(slot); };
		v0.m_tex   = [&]{ return brightFilterFBO.GetTexture(); };
		v0.m_id    = [&]{ return brightFilterFBO.ID(); };
		v0.m_size  = [&]{ return brightFilterFBO.Size(); };

		auto& v1 = make(eBuffer::BUFFER_BRIGHT_FILTER_MASK, "BUFFER_BRIGHT_FILTER_MASK");
		v1.m_init  = initBF;
		v1.m_write = [&]{ brightFilterFBO.BindForWriting(); };
		v1.m_read  = [&](GLenum slot){ brightFilterFBO.BindForReadingMask(slot); };
		v1.m_tex   = [&]{ return brightFilterFBO.GetTextureMask(); };
		v1.m_id    = [&]{ return brightFilterFBO.ID(); };
		v1.m_size  = [&]{ return brightFilterFBO.Size(); };
	}

	// -------------------------
	// Gaussian ping-pong
	// -------------------------
	{
		auto& v = make(eBuffer::BUFFER_GAUSSIAN_ONE, "BUFFER_GAUSSIAN_ONE");
		v.m_init  = bindColorFbo(gausian1FBO, false, false);
		v.m_write = [&]{ gausian1FBO.BindForWriting(); };
		v.m_read  = [&](GLenum slot){ gausian1FBO.BindForReading(slot); };
		v.m_tex   = [&]{ return gausian1FBO.GetTexture(); };
		v.m_id    = [&]{ return gausian1FBO.ID(); };
		v.m_size  = [&]{ return gausian1FBO.Size(); };
	}
	{
		auto& v = make(eBuffer::BUFFER_GAUSSIAN_TWO, "BUFFER_GAUSSIAN_TWO");
		v.m_init  = bindColorFbo(gausian2FBO, false, false);
		v.m_write = [&]{ gausian2FBO.BindForWriting(); };
		v.m_read  = [&](GLenum slot){ gausian2FBO.BindForReading(slot); };
		v.m_tex   = [&]{ return gausian2FBO.GetTexture(); };
		v.m_id    = [&]{ return gausian2FBO.ID(); };
		v.m_size  = [&]{ return gausian2FBO.Size(); };
	}

	// -------------------------
	// Square
	// -------------------------
	{
		auto& v = make(eBuffer::BUFFER_SQUERE, "BUFFER_SQUERE");
		v.m_init  = [&](unsigned w, unsigned h){
			// keep your intent explicit
			if (w != h) throw std::runtime_error("BUFFER_SQUERE requires w == h");
			squereFBO.Init(w, h);
		};
		v.m_write = [&]{ squereFBO.BindForWriting(); };
		v.m_read  = [&](GLenum slot){ squereFBO.BindForReading(slot); };
		v.m_tex   = [&]{ return squereFBO.GetTexture(); };
		v.m_id    = [&]{ return squereFBO.ID(); };
		v.m_size  = [&]{ return squereFBO.Size(); };
	}

	// -------------------------
	// Deferred GBuffer views (0/1/2)
	// -------------------------
	{
		auto initG = [&](unsigned w, unsigned h){ gFBO.Init(w, h); };

		auto& v0 = make(eBuffer::BUFFER_DEFFERED, "BUFFER_DEFFERED");
		v0.m_init  = initG;
		v0.m_write = [&]{ gFBO.BindForWriting(); };
		v0.m_read  = [&](GLenum slot){ gFBO.BindForReading0(slot); };
		v0.m_tex   = [&]{ return gFBO.GetTexture0(); };
		v0.m_id    = [&]{ return gFBO.ID(); };
		v0.m_size  = [&]{ return gFBO.Size(); };

		auto& v1 = make(eBuffer::BUFFER_DEFFERED1, "BUFFER_DEFFERED1");
		v1.m_init  = initG;
		v1.m_write = [&]{ gFBO.BindForWriting(); };
		v1.m_read  = [&](GLenum slot){ gFBO.BindForReading1(slot); };
		v1.m_tex   = [&]{ return gFBO.GetTexture1(); };
		v1.m_id    = [&]{ return gFBO.ID(); };
		v1.m_size  = [&]{ return gFBO.Size(); };

		auto& v2 = make(eBuffer::BUFFER_DEFFERED2, "BUFFER_DEFFERED2");
		v2.m_init  = initG;
		v2.m_write = [&]{ gFBO.BindForWriting(); };
		v2.m_read  = [&](GLenum slot){ gFBO.BindForReading2(slot); };
		v2.m_tex   = [&]{ return gFBO.GetTexture2(); };
		v2.m_id    = [&]{ return gFBO.ID(); };
		v2.m_size  = [&]{ return gFBO.Size(); };
	}

	// -------------------------
	// SSAO + blur (SimpleColorFBO)
	// -------------------------
	{
		auto& v = make(eBuffer::BUFFER_SSAO, "BUFFER_SSAO");
		v.m_init  = bindSimpleFbo(ssaoFBO, false, false);
		v.m_write = [&]{ ssaoFBO.BindForWriting(); };
		v.m_read  = [&](GLenum slot){ ssaoFBO.BindForReading(slot); };
		v.m_tex   = [&]{ return ssaoFBO.GetTexture(); };
		v.m_id    = [&]{ return ssaoFBO.ID(); };
		v.m_size  = [&]{ return ssaoFBO.Size(); };
	}
	{
		auto& v = make(eBuffer::BUFFER_SSAO_BLUR, "BUFFER_SSAO_BLUR");
		v.m_init  = bindSimpleFbo(ssaoBlurFBO, false, false);
		v.m_write = [&]{ ssaoBlurFBO.BindForWriting(); };
		v.m_read  = [&](GLenum slot){ ssaoBlurFBO.BindForReading(slot); };
		v.m_tex   = [&]{ return ssaoBlurFBO.GetTexture(); };
		v.m_id    = [&]{ return ssaoBlurFBO.ID(); };
		v.m_size  = [&]{ return ssaoBlurFBO.Size(); };
	}

	// -------------------------
	// IBL cubemaps (CubemapFBO)
	// -------------------------
	{
		auto& v = make(eBuffer::BUFFER_IBL_CUBEMAP, "BUFFER_IBL_CUBEMAP");
		v.m_init  = [&](unsigned w, unsigned){ iblCubemapFBO.Init(w); };
		v.m_write = [&]{ iblCubemapFBO.BindForWriting(); };
		v.m_read  = [&](GLenum slot){ iblCubemapFBO.BindForReading(slot); };
		v.m_tex   = [&]{ return iblCubemapFBO.GetTexture(); };
		v.m_id    = [&]{ return iblCubemapFBO.ID(); };
		v.m_size  = [&]{ return iblCubemapFBO.Size(); };
		v.m_rbo   = [&]{ return iblCubemapFBO.RboID(); }; // the only one you currently expose
	}
	{
		auto& v = make(eBuffer::BUFFER_IBL_CUBEMAP_IRR, "BUFFER_IBL_CUBEMAP_IRR");
		v.m_init  = [&](unsigned w, unsigned){ iblCubemapIrrFBO.Init(w); };
		v.m_write = [&]{ iblCubemapIrrFBO.BindForWriting(); };
		v.m_read  = [&](GLenum slot){ iblCubemapIrrFBO.BindForReading(slot); };
		v.m_tex   = [&]{ return iblCubemapIrrFBO.GetTexture(); };
		v.m_id    = [&]{ return iblCubemapIrrFBO.ID(); };
		v.m_size  = [&]{ return iblCubemapIrrFBO.Size(); };
	}
	{
		auto& v = make(eBuffer::BUFFER_ENVIRONMENT_CUBEMAP, "BUFFER_ENVIRONMENT_CUBEMAP");
		v.m_init  = [&](unsigned w, unsigned){ environmentCubemapFBO.Init(w); };
		v.m_write = [&]{ environmentCubemapFBO.BindForWriting(); };
		v.m_read  = [&](GLenum slot){ environmentCubemapFBO.BindForReading(slot); };
		v.m_tex   = [&]{ return environmentCubemapFBO.GetTexture(); };
		v.m_id    = [&]{ return environmentCubemapFBO.ID(); };
		v.m_size  = [&]{ return environmentCubemapFBO.Size(); };
	}

	// -------------------------
	// Bloom
	// -------------------------
	{
		auto& v = make(eBuffer::BUFFER_BLOOM, "BUFFER_BLOOM");
		v.m_init  = [&](unsigned w, unsigned h){ bloomFBO.Init(w, h, 5); };
		v.m_write = [&]{ bloomFBO.BindForWriting(); };
		v.m_read  = [&](GLenum slot){ bloomFBO.BindForReading(slot); };
		v.m_tex   = [&]{ return bloomFBO.GetTexture(); };
		v.m_id    = [&]{ return bloomFBO.ID(); };
		v.m_size  = [&]{ return bloomFBO.Size(); };
	}

	// -------------------------
	// Shadow FBOs (ShadowMapFBO)
	// -------------------------
	{
		auto& v = make(eBuffer::BUFFER_SHADOW_DIR, "BUFFER_SHADOW_DIR");
		v.m_init  = [&](unsigned w, unsigned h){ depthDirFBO.Init(w, h, false); };
		v.m_write = [&]{ depthDirFBO.BindForWriting(); };
		v.m_read  = [&](GLenum slot){ depthDirFBO.BindForReading(slot); };
		v.m_tex   = [&]{ return depthDirFBO.GetTexture(); };
		v.m_id    = [&]{ return depthDirFBO.ID(); };
		v.m_size  = [&]{ return depthDirFBO.Size(); };
	}
	{
		auto& v = make(eBuffer::BUFFER_DEPTH, "BUFFER_DEPTH");
		v.m_init  = [&](unsigned w, unsigned h){ depthFBO.Init(w, h, false); };
		v.m_write = [&]{ depthFBO.BindForWriting(); };
		v.m_read  = [&](GLenum slot){ depthFBO.BindForReading(slot); };
		v.m_tex   = [&]{ return depthFBO.GetTexture(); };
		v.m_id    = [&]{ return depthFBO.ID(); };
		v.m_size  = [&]{ return depthFBO.Size(); };
	}
	{
		auto& v = make(eBuffer::BUFFER_SHADOW_CUBE_MAP, "BUFFER_SHADOW_CUBE_MAP");
		v.m_init  = [&](unsigned w, unsigned h){ depthCubeFBO.Init(w, h, true); };
		v.m_write = [&]{ depthCubeFBO.BindForWriting(); };
		v.m_read  = [&](GLenum slot){ depthCubeFBO.BindForReading(slot); };
		v.m_tex   = [&]{ return depthCubeFBO.GetTexture(); };
		v.m_id    = [&]{ return depthCubeFBO.ID(); };
		v.m_size  = [&]{ return depthCubeFBO.Size(); };
	}
	{
		auto& v = make(eBuffer::BUFFER_SHADOW_CSM, "BUFFER_SHADOW_CSM");
		v.m_init  = [&](unsigned w, unsigned h){
			// preserve your original behavior; ignore passed w/h if you want fixed
			(void)w; (void)h;
			depthCSMFBO.InitCSM(2048, 2048, 5);
		};
		v.m_write = [&]{ depthCSMFBO.BindForWriting(); };
		v.m_read  = [&](GLenum slot){ depthCSMFBO.BindForReading(slot); };
		v.m_tex   = [&]{ return depthCSMFBO.GetTexture(); };
		v.m_id    = [&]{ return depthCSMFBO.ID(); };
		v.m_size  = [&]{ return depthCSMFBO.Size(); };
	}
}
