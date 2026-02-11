#pragma once

#include "colorFBO.h"
#include "SimpleColorFbo.h"
#include "ShadowMapFbo.h"
#include "GFBO.h"
#include "bloomFBO.h"
#include "SSBO.h"
#include "IBufferView.h"

#include <map>
#include <array>
#include <functional>
#include <memory>

enum class eBuffer
{
	BUFFER_DEFAULT,
	BUFFER_SHADOW_DIR,
	BUFFER_SHADOW_CUBE_MAP,
	BUFFER_SHADOW_CSM,
	BUFFER_BRIGHT_FILTER,
	BUFFER_BRIGHT_FILTER_MASK,
	BUFFER_GAUSSIAN_ONE,
	BUFFER_GAUSSIAN_TWO,
	BUFFER_REFLECTION,
	BUFFER_REFRACTION,
	BUFFER_SCREEN,
	BUFFER_MTS,
	BUFFER_DEFFERED,
	BUFFER_DEFFERED1,
	BUFFER_DEFFERED2,
	BUFFER_SQUERE,
	BUFFER_SSAO,
	BUFFER_SSAO_BLUR,
	BUFFER_IBL_CUBEMAP,
	BUFFER_IBL_CUBEMAP_IRR,
	BUFFER_BLOOM,
	BUFFER_SSR,
	BUFFER_SSR_HIT_MASK,
	BUFFER_SSR_BLUR,
	BUFFER_SCREEN_MASK,
	BUFFER_SCREEN_WITH_SSR,
	BUFFER_ENVIRONMENT_CUBEMAP,
	BUFFER_DEPTH
};

enum class eSSBO
{
	MODEL_TO_PROJECTION_MATRIX = 5, // corresponds to binding
	MODEL_TO_WORLD_MATRIX = 6,
	INSTANCED_INFO_MATRIX = 7,
	HERALDRY_INSTANCED_INFO = 8,
	BONES_PACKED = 9,
	BONE_BASE_INDEX = 10,
};

//---------------------------------------------------------------------------
class eGlBufferContext
{
public:
	eGlBufferContext(const eGlBufferContext&)		= delete;
	eGlBufferContext& operator=(eGlBufferContext&)	= delete;
	
	void SetDefaultState();

	void   BufferInit(eBuffer, unsigned int, unsigned int);
	GLuint BufferCustomInit(const std::string& _name, unsigned int, unsigned int, bool = false, bool = false);

	uint32_t BufferSSBOInitMat4(eSSBO, size_t, bool);
	uint32_t BufferSSBOInitVec4(eSSBO, size_t, bool);
	uint32_t BufferSSBOInitInt(eSSBO, size_t, bool );

	void		 UploadSSBOData(eSSBO, const std::vector<glm::mat4>& _data);
	void		 UploadSSBOData(eSSBO, const std::vector<glm::vec4>& _data);
	void		 UploadSSBOData(eSSBO, const std::vector<GLuint>& _data);
	void		 BindSSBO(eSSBO);

	void EnableWrittingBuffer(eBuffer);
	void EnableReadingBuffer(eBuffer, GLenum slot);

	void EnableCustomWrittingBuffer(const std::string& _name);
	void EnableCustomReadingBuffer(const std::string& _name, GLenum slot);

	Texture GetTexture(eBuffer);
	Texture GetTexture(const std::string& _name);

	GLuint GetRboID(eBuffer _buffer);

	void ResolveMtsToScreen() { mtsFBO.ResolveToFBO(&screenFBO); }
	void BlitFromTo(eBuffer, eBuffer, GLenum bit);
	void BlitFromTo(eBuffer, const std::string& _name, GLenum bit);

	GLuint GetId(eBuffer);
	GLuint GetId(const std::string& _name);
	glm::ivec2 GetSize(eBuffer _buffer);
	glm::ivec2 GetSize(const std::string& _name);

	const std::vector<bloomMip>& MipChain() const {
		return bloomFBO.MipChain();
	}

	static eGlBufferContext& GetInstance() 
	{
		static eGlBufferContext  instance;
		return instance;
	}

private:
	static constexpr size_t BufferCount = static_cast<size_t>(eBuffer::BUFFER_DEPTH) + 1;
	static constexpr size_t Idx(eBuffer b) { return static_cast<size_t>(b); }

	std::array<std::unique_ptr<IBufferView>, BufferCount> m_views{};

	void BuildViews();

	IBufferView& View(eBuffer b)
	{
		auto& ptr = m_views[Idx(b)];
		if (!ptr) throw std::runtime_error("No view registered for eBuffer value");
		return *ptr;
	}

	const IBufferView& View(eBuffer b) const
	{
		auto& ptr = m_views[Idx(b)];
		if (!ptr) throw std::runtime_error("No view registered for eBuffer value");
		return *ptr;
	}

private:
	eGlBufferContext() { BuildViews(); }

	eColorFBO		   defaultFBO;
	ShadowMapFBO	 depthDirFBO;
	ShadowMapFBO	 depthFBO;
	ShadowMapFBO	 depthCubeFBO;
	ShadowMapFBO	 depthCSMFBO;
	eColorFBO		   screenFBO;
	eColorFBO		   mtsFBO;
	eColorFBO		   reflectionFBO;
	eColorFBO		   refractionFBO;
	eColorFBO		   brightFilterFBO;
	eColorFBO		   gausian1FBO;
	eColorFBO		   gausian2FBO;
	eColorFBO		   squereFBO;
	eGFBO			     gFBO;
	SimpleColorFBO ssaoFBO;
	SimpleColorFBO ssaoBlurFBO;
	CubemapFBO		 iblCubemapFBO;
	CubemapFBO		 iblCubemapIrrFBO;
	CubemapFBO		 environmentCubemapFBO;
	BloomFBO			 bloomFBO;
	eColorFBO			 ssrFBO;
	eColorFBO			 ssrblurFBO;
	eColorFBO		   screenSsrFBO;

	std::map<std::string, eColorFBO>  customBuffers;
	std::map<eSSBO, SSBO<glm::mat4> > ssboBuffers;
	std::map<eSSBO, SSBO<glm::vec4> > ssboBuffersVec4;
	std::map<eSSBO, SSBO<GLuint> >		ssboBuffersInt;
};
