#include "stdafx.h"
#include "ScreenRender.h"

#include <glm/glm/gtc/type_ptr.hpp>

//--------------------------------------------------------------------------
eScreenRender::eScreenRender(Texture tex, const std::string& vS, const std::string& fS, const std::string& cS,
														 const std::string& atlaVS, const std::string& atlasFS, const std::string& msdfFS, const std::string&  solidFS)
{
	screenShader.installShaders(vS.c_str(), fS.c_str());
	screenMesh.reset(new eScreenMesh(tex, tex));
	frameMesh.reset(new eFrameMesh);
	initUniforms();

	computeShader.installShaders(cS.c_str());
	computeShader.GetUniformInfoFromShader();

	glGenTextures(1, &exposureTex);
	glBindTexture(GL_TEXTURE_2D, exposureTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 1, 1, 0, GL_RGBA, GL_FLOAT, nullptr);
	// Optional: Set texture parameters (not required for image load/store)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	programs[(uint8_t)UiShaderKind::Sprite].installShaders(atlaVS.c_str(), atlasFS.c_str());
	programs[(uint8_t)UiShaderKind::Sprite].GetUniformInfoFromShader();
	programs[(uint8_t)UiShaderKind::NineSlice] = programs[(uint8_t)UiShaderKind::Sprite];
	programs[(uint8_t)UiShaderKind::MSDF].installShaders(atlaVS.c_str(), msdfFS.c_str());
	programs[(uint8_t)UiShaderKind::MSDF].GetUniformInfoFromShader();
	programs[(uint8_t)UiShaderKind::Solid].installShaders(atlaVS.c_str(), solidFS.c_str());
	programs[(uint8_t)UiShaderKind::Solid].GetUniformInfoFromShader();
}

//-----------------------------------------------------------------------
void eScreenRender::initUniforms()
{
	//@todo change for uniform function pointer
	textureLoc = glGetUniformLocation(screenShader.ID(), "screenTexture");
	frameLoc = glGetUniformLocation(screenShader.ID(), "frame");
	blendLoc = glGetUniformLocation(screenShader.ID(), "blend");
	kernelLoc = glGetUniformLocation(screenShader.ID(), "kernel");
	blurCoefLoc = glGetUniformLocation(screenShader.ID(), "blurCoef");
	centerLoc = glGetUniformLocation(screenShader.ID(), "center");
	aspectLoc = glGetUniformLocation(screenShader.ID(), "aspectRatio");
	rotationMatrixLoc = glGetUniformLocation(screenShader.ID(), "rotationMatrix");

	glm::mat2 rotationMatrix = glm::mat2(1.0f);
	glUniformMatrix2fv(rotationMatrixLoc, 1, GL_FALSE, glm::value_ptr(rotationMatrix));
	screenShader.GetUniformInfoFromShader();
}

//----------------------------------------------------------------------------------------------------------------
void eScreenRender::ApplyVirtualToClip(GLuint program, float viewport_width, float viewport_height, float virtualCanvas_width, float virtualCanvas_height)
{
	const float sx = viewport_width / virtualCanvas_width, sy = viewport_height / virtualCanvas_height;
	const float scale = std::min(sx, sy);
	const float padX = (viewport_width - virtualCanvas_width * scale) * 0.5f;
	const float padY = (viewport_height - virtualCanvas_height * scale) * 0.5f;

	const float Sx = 2.0f * scale / viewport_width;
	const float Ox = (2.0f * padX / viewport_width) - 1.0f;
	const float Sy = -2.0f * scale / viewport_height; // minus = top-left UI → GL bottom-left
	const float Oy = 1.0f - (2.0f * padY / viewport_height);

	glUseProgram(program);
	const GLint loc = glGetUniformLocation(program, "uVirtualToClip");
	glUniform4f(loc, Sx, Sy, Ox, Oy);
}

//--------------------------------------------------------------------------
void eScreenRender::Render(glm::vec2 _top_left, glm::vec2 _right_botom,
													 glm::vec2 _tex_top_left, glm::vec2 _tex_right_botom,
													 float viewport_width, float viewport_height, int32_t _rendering_function)
{
	glUseProgram(screenShader.ID());

	glUniform1i(frameLoc, GL_FALSE);
  glUniform1i(blendLoc, GL_FALSE);
	glUniform1i(kernelLoc, GL_FALSE);

	float angle = glm::radians(rotation_angle);
	glm::mat2 rotationMatrix = glm::mat2(cos(angle), -sin(angle), sin(angle), cos(angle));
	glUniformMatrix2fv(rotationMatrixLoc, 1, GL_FALSE, glm::value_ptr(rotationMatrix));

	SetRenderingFunction(_rendering_function);

	/*GLuint currentIndex;
	GLuint subroutineUniformLocation = glGetSubroutineUniformLocation(screenShader.ID(), GL_FRAGMENT_SHADER, "ColorFunction");
	glGetUniformSubroutineuiv(GL_FRAGMENT_SHADER, subroutineUniformLocation, &currentIndex);*/

	screenMesh->UpdateFrame(_top_left.x,			_top_left.y,				_right_botom.x,			_right_botom.y,
													_tex_top_left.x, _tex_right_botom.y,	_tex_right_botom.x,	_tex_top_left.y,
													viewport_width, viewport_height);

	glUniform2f(centerLoc, screenMesh->GetNDCCenter().x, screenMesh->GetNDCCenter().y);
	glUniform1f(aspectLoc, viewport_width/viewport_height);

	screenMesh->Draw();
	screenMesh->SetViewPortToDefault();
}

//--------------------------------------------------------------------------
void eScreenRender::RenderContrast(const Camera& camera, float blur_coef, float _tick, uint32_t lum_texture)
{
	glUseProgram(computeShader.ID());

	computeShader.SetUniformData("targetLuminance", m_target_lum);
	computeShader.SetUniformData("adaptationRate", m_adaption_rate);
	computeShader.SetUniformData("deltaTime", _tick / 1000.0f);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, lum_texture);

	glBindImageTexture(1 /*bind unit */, exposureTex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16F);
	glDispatchCompute(1, 1, 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	glUseProgram(screenShader.ID());

  glUniform1f(blurCoefLoc, blur_coef);
	glUniform1i(frameLoc, GL_FALSE);
	glUniform1i(blendLoc, GL_TRUE);

	glm::mat2 rotationMatrix = glm::mat2(1.0f);
	glUniformMatrix2fv(rotationMatrixLoc, 1, GL_FALSE, glm::value_ptr(rotationMatrix));
	glUniform2f(centerLoc, screenMesh->GetNDCCenter().x, screenMesh->GetNDCCenter().y);

	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, exposureTex);

	screenShader.SetUniformData("hdr_exposure", m_exposure);
	screenShader.SetUniformData("tone_mapping_type", m_tone_mapping_index);
	screenShader.SetUniformData("gamma_correction", m_gamma_correction);
	screenShader.SetUniformData("auto_exposure", m_auto_exposure);

	screenMesh->SetViewPortToDefault();
	screenMesh->Draw();
	glUniform1i(blendLoc, GL_FALSE);
}

//--------------------------------------------------------------------------
void eScreenRender::RenderFrame(glm::vec2 _top_left, glm::vec2 _right_botom, float viewport_width, float viewport_height)
{
	glUseProgram(screenShader.ID());
	glUniform1i(frameLoc, GL_TRUE);
	glm::mat2 rotationMatrix = glm::mat2(1.0f);
	glUniformMatrix2fv(rotationMatrixLoc, 1, GL_FALSE, glm::value_ptr(rotationMatrix));
	glUniform2f(centerLoc, screenMesh->GetNDCCenter().x, screenMesh->GetNDCCenter().y);

	frameMesh->UpdateFrame(_top_left.x, _top_left.y, _right_botom.x, _right_botom.y, viewport_width, viewport_height);
	frameMesh->Draw();
	glUniform1i(frameLoc, GL_FALSE);
}

//--------------------------------------------------------------------------
void eScreenRender::RenderKernel()
{
	glUseProgram(screenShader.ID());
	glm::mat2 rotationMatrix = glm::mat2(1.0f);
	glUniformMatrix2fv(rotationMatrixLoc, 1, GL_FALSE, glm::value_ptr(rotationMatrix));
	glUniform1i(frameLoc, GL_FALSE);
	glUniform1i(blendLoc, GL_FALSE);
	glUniform1i(kernelLoc, GL_TRUE);
	screenMesh->Draw();
	glUniform1i(kernelLoc, GL_FALSE);
}

//---------------------------------------------------------------
void eScreenRender::SetRenderingFunction(int32_t _function)
{
	glUseProgram(screenShader.ID());
	static GLuint currentIndex;
	if (_function == 0)
	{
		GLuint subroutineUniformLocation = glGetSubroutineUniformLocation(screenShader.ID(), GL_FRAGMENT_SHADER, "ColorFunction");
		glGetUniformSubroutineuiv(GL_FRAGMENT_SHADER, subroutineUniformLocation, &currentIndex);

		GLuint DefaultRendering = glGetSubroutineIndex(screenShader.ID(), GL_FRAGMENT_SHADER, "DefaultColor");
		glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &DefaultRendering);

		glGetUniformSubroutineuiv(GL_FRAGMENT_SHADER, subroutineUniformLocation, &currentIndex);
	}
	else if (_function == 1)
	{
		GLuint CursorFollowRendering = glGetSubroutineIndex(screenShader.ID(), GL_FRAGMENT_SHADER, "TestColor");
		glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &CursorFollowRendering);
	}
	else if (_function == 2)
	{
		GLuint GreyKernelRendering = glGetSubroutineIndex(screenShader.ID(), GL_FRAGMENT_SHADER, "GreyKernelColor");
		glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &GreyKernelRendering);
	}
	else if (_function == 3)
	{
		GLuint BlendOnMaskRendering = glGetSubroutineIndex(screenShader.ID(), GL_FRAGMENT_SHADER, "MaskBlendColor");
		glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &BlendOnMaskRendering);
	}
	else if (_function == 4)
	{
		GLuint subroutineUniformLocation = glGetSubroutineUniformLocation(screenShader.ID(), GL_FRAGMENT_SHADER, "ColorFunction");
		glGetUniformSubroutineuiv(GL_FRAGMENT_SHADER, subroutineUniformLocation, &currentIndex);

		GLuint SolidColorRendering = glGetSubroutineIndex(screenShader.ID(), GL_FRAGMENT_SHADER, "SolidColor");
		glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &SolidColorRendering);

		glGetUniformSubroutineuiv(GL_FRAGMENT_SHADER, subroutineUniformLocation, &currentIndex);
	}
	else if (_function == 5)
	{
		GLuint subroutineUniformLocation = glGetSubroutineUniformLocation(screenShader.ID(), GL_FRAGMENT_SHADER, "ColorFunction");
		glGetUniformSubroutineuiv(GL_FRAGMENT_SHADER, subroutineUniformLocation, &currentIndex);

		GLuint GradientColorRendering = glGetSubroutineIndex(screenShader.ID(), GL_FRAGMENT_SHADER, "GradientColor");
		glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &GradientColorRendering);

		glGetUniformSubroutineuiv(GL_FRAGMENT_SHADER, subroutineUniformLocation, &currentIndex);
	}
}

	//-------------------------------------------------------------------------------------------
	void eScreenRender::RenderUI(const UIMesh& uiMesh, float viewport_width, float viewport_height)
	{
		GLuint invert_y_loc = glGetUniformLocation(programs[(uint8_t)UiShaderKind::Sprite].ID(), "invert_y");

		UiCallbacks cb;
		cb.begin = [&]() {
			currentProgram = 0; have = false;

			//@todo tem -> out
			glEnable(GL_BLEND);
			glDisable(GL_DEPTH_TEST);
			/*optional?*/
			glDisable(GL_CULL_FACE);
			glEnable(GL_SCISSOR_TEST);
			glDisable(GL_FRAMEBUFFER_SRGB);
		};

		cb.before = [&](const UiPipelineKey& p, const UiCmdWire&)
			{
			// Blend
			/*if (p.premultiplied) glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			else  */               glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		/*	if(p.shader == UiShaderKind::MSDF)
				glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);*/

			// Program selection by shader kind
			Shader& prog = programFor(p.shader);
			GLuint pid = prog.ID();
			if (pid != currentProgram)
				{ glUseProgram(pid); currentProgram = pid; }

			if(p.invert_y && currentProgram == programs[(uint8_t)UiShaderKind::Sprite].ID())
				glUniform1i(invert_y_loc, GL_TRUE);
			else
				glUniform1i(invert_y_loc, GL_FALSE);

			// Variant (subroutine/uniform switch)
			//if (!have || std::memcmp(&last, &p, sizeof(UiPipelineKey)) != 0)
			//{
			//	SetRenderingFunction(p.renderFunc);  //existing hook
			//	last = p; have = true;
			//}
			ApplyVirtualToClip(prog.ID(), viewport_width, viewport_height, 1920.f, 1080.f); //@todo virtual canvas make visible constants !!!
		};

		cb.end = [&]() {
			// optional reset
			glEnable(GL_DEPTH_TEST);
			glDisable(GL_SCISSOR_TEST);
		/*	glEnable(GL_FRAMEBUFFER_SRGB);*/
		};
		const_cast<UIMesh&>(uiMesh).SetCallbacks(cb);
		const_cast<UIMesh&>(uiMesh).Draw();
	}

	//--------------------------------------------------------------------------------------------
	void eScreenRender::ApplyPipeline(const UiPipelineKey & p, float viewportW, float viewportH)
	{
		// Blend mode
		if (p.premultiplied) glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		else                 glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		//legacy “mode”/subroutine selection
		SetRenderingFunction(p.renderFunc);

		// If you had aspect/center uniforms in the screen shader and still need them:
		// glUniform1f(aspectLoc, viewportW/viewportH);
		// glUniform2f(centerLoc, 0.0f, 0.0f); // or whatever is relevant now
		// rotationMatrixLoc typically stays identity for UI; rotate in vertex if needed later.
	}
