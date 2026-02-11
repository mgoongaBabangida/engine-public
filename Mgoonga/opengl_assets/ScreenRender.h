#pragma once

#include <math/Camera.h>
#include <base/base.h>

#include "Shader.h"
#include "ScreenMesh.h"
#include "UIMesh.h"

//---------------------------------------------------------------------------
class eScreenRender
{
public:
	eScreenRender(Texture tex, const std::string& vS, const std::string& fS, const std::string& cS,
								const std::string& atlaVS, const std::string& atlasFS, const std::string& msdfFS, const std::string& solidFS);

	void Render(glm::vec2 _top_left, glm::vec2 _right_botom,
							glm::vec2 _tex_top_left, glm::vec2 _tex_right_botom,
							float viewport_width, float viewport_height, int32_t _rendering_function);

	void RenderContrast(const Camera& camera, float blur_coef, float _tick, uint32_t lum_texture);
	void RenderFrame(glm::vec2 _top_left, glm::vec2 _right_botom,
									 float viewport_width, float viewport_height);
	void RenderKernel();
	
	void RenderUI(const UIMesh& uiMesh, float viewport_width, float viewport_height);

	void SetProgramFor(UiShaderKind kind, Shader prog) { /*programs[(int)kind] = std::move(prog);*/ }

	//-------------------------------------------------------------------------
	float&		GetExposure()							{ return m_exposure; }
	int32_t&	GetToneMappingIndex()			{ return m_tone_mapping_index; }
	bool&			GetGammaCorrection()			{ return m_gamma_correction; }
	float&		GetRotationAngle()				{ return rotation_angle; }
	bool&			GetAutoExposure()					{ return m_auto_exposure; }
	float&		GetTargetLuminance()			{ return m_target_lum;; }
	float&		GetAdaptionRate()					{ return m_adaption_rate; }

	//---------------------------------------------------------------------------
	void SetRenderingFunction(int32_t);

	void SetTexture(Texture t)										{ screenMesh->SetTextureOne(t); }
	void SetTextureContrast(Texture t)						{ screenMesh->SetTextureTwo(t); }
	void SetTextureMask(Texture t)								{ screenMesh->SetTextureThree(t); }
	void SetRotationAngle(float _rotation_angle)	{ rotation_angle = _rotation_angle; }

	Shader& GetShader() { return screenShader; }

protected:
	void initUniforms();

	void ApplyVirtualToClip(GLuint program, float viewport_width, float viewport_height, float virtualCanvas_width, float virtualCanvas_height);

	Shader							          screenShader;
	Shader							          computeShader;

	GLuint												exposureTex;

	std::unique_ptr<eFrameMesh>		frameMesh;
	std::unique_ptr<eScreenMesh>	screenMesh;

	GLuint							textureLoc;
	GLuint							frameLoc;
	GLuint							blendLoc;
	GLuint							kernelLoc;
  GLuint							blurCoefLoc;
	GLuint							rotationMatrixLoc;
	GLuint						  centerLoc;
	GLuint							aspectLoc;

	float								rotation_angle = 0.f;

	float		m_exposure = 1.0f;
	int32_t	m_tone_mapping_index = 3;
	bool		m_gamma_correction = true; // @connect with hdr shaders
	bool		m_auto_exposure;
	float		m_target_lum = 0.75f;
	float		m_adaption_rate = 1.0f;

	void ApplyPipeline(const UiPipelineKey& p, float viewportW, float viewportH);
	Shader& programFor(UiShaderKind k) { return programs[(int)k]; }

	Shader programs[8]; // Sprite at [UiShaderKind::Sprite], MSDF at [UiShaderKind::MSDF], etc.
	GLuint currentProgram = 0;
	UiPipelineKey last{}; bool have = false;
}; 


