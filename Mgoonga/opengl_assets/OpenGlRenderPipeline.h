#ifndef PIPELINE_H
#define PIPELINE_H

#include <glew-2.1.0\include\GL\glew.h>
#include "opengl_assets.h"

#include <base/base.h>
#include <base/Object.h>
#include <math/CameraRay.h>

#include "Texture.h"
#include "Scene.h"
#include "RenderPipelineBufferMask.h"

#include <map>
#include <thread>

class Camera;
class GUI;

class eModelManager;
class eTextureManager;
class eRenderManager;
class SimpleGeometryMesh;

//------------------------------------------------------------------------------------------------------------
class DLL_OPENGL_ASSETS eOpenGlRenderPipeline
{
public:
	eOpenGlRenderPipeline(uint32_t width, uint32_t height);
	~eOpenGlRenderPipeline();

	void			RenderFrame(const Scene& scene, RenderBuckets& buckets, float tick);
	
	void			UpdateSharedUniforms();

	void			Initialize();
	void			InitializeBuffers(FboBits fboMask, SsboBits ssboMask);

	void			InitializeRenders(eModelManager&, eTextureManager&, const std::string& shadersFolderPath);
	
	uint32_t  Width() const { return m_width; }
	uint32_t  Height() const { return m_height; }

	const std::vector<ShaderInfo>& GetShaderInfos() const;
	void			UpdateShadersInfo();
	bool			SetUniformData(const std::string& _renderName,
													 const std::string& _uniformName,
													 const UniformData& _data);

	std::function<void(const TessellationRenderingInfo&)> GetTessellationInfoUpdater();

	void			SetCurrentCamera(size_t _index) { m_active_camera = _index; }
	void			SetSkyBoxTexture(const Texture* _t);
	void			SetSkyIBL(unsigned int _irr, unsigned int _prefilter);
	
	void			AddParticleSystem(std::shared_ptr<IParticleSystem> system);
	void			AddParticleSystemGPU(glm::vec3 _startPos, const Texture* _texture);

	std::vector<std::shared_ptr<IParticleSystem> > GetParticleSystems();

	void			SwitchSkyBox(bool on);
	void			SwitchWater(bool on);

	// Getters
	float			GetWaterHeight() const;

  bool& GetBoundingBoxBoolRef() { return draw_bounding_boxes; }
  bool& GetMultiSamplingBoolRef() { return mts; }
  bool& GetSkyBoxOnRef() { return skybox; }
	bool& GetWaterOnRef() { return water; }
	bool& GetGeometryOnRef() { return geometry; }
  float& GetBlurCoefRef() { return blur_coef; }
	bool& GetKernelOnRef();
	bool& GetSkyNoiseOnRef();
	bool& GetOutlineFocusedRef();
	bool& GetUIlessRef();
	bool& GetSSAOEnabledRef() { return ssao; }
	bool& GetSSREnabledRef() { return ssr; }
	bool& GetEnabledCameraInterpolationRef() { return camera_interpolation; }
	bool& GetRotateSkyBoxRef();
	float& GetSaoThresholdRef();
	float& GetSaoStrengthRef();
	bool& ShadowingRef() { return shadows; }
	bool& PBBloomRef() { return m_pb_bloom; }
	bool& GetBloomThreshold() { return m_bloom_threshold; }
	bool& GetMeshLineOn() { return m_mesh_line_on; }
	bool& GetComputeShaderRef() { return m_compute_shader; }

	//Phong Render
	bool& GetDebugWhite();
	bool& GetDebugTexCoords();
	float& GetEmissionStrengthRef();

	//HDR -> LDR blending
	float& GetExposureRef();
	bool& GetAutoExposure();
	float& GetTargetLuminance();
	float& GetAdaptionRate();
	bool& GetGammaCorrectionRef();
	int32_t& GetToneMappingRef();

	bool& IBL();
	float& IBLInfluance();

	//Water Render
	float& WaveSpeedFactor();
	float& Tiling();
	float& WaveStrength();
	float& ShineDumper();
	float& Reflactivity();
	glm::vec4& WaterColor();
	float& ColorMix();
	float& RefrectionFactor();
	float& DistortionStrength();

	//CSM
	float& ZMult();
	float& GetFirstCascadePlaneDistance();
	float& GetLightPlacementCoef();
	float& GetCascadeBlendDistance();
	bool& BlendCascades();
	float& GetMaxShadow();

	//SSR
	float& Step();
	float& MinRayStep();
	float& MaxSteps();
	int& NumBinarySearchSteps();
	float& ReflectionSpecularFalloffExponent();

	// Fog
	struct FogInfo
	{
		float maxDist = 40.0f;
		float minDist = 10.0f;
		glm::vec4 color = glm::vec4 {0.5f,0.5f ,0.5f ,1.0f};
		bool fog_on = true;
		float density = 0.03f;
		float gradient = 4.0f;
	};

	//CameraInterpolation Debug
	glm::vec3& GetSecondCameraPositionRef();
	float& GetDisplacementRef();
	glm::mat4& GetLookAtMatrix();
	glm::mat4& GetProjectionMatrix();
	glm::mat4& GetLookAtProjectedMatrix();

	// PCSS
	float& GetPenumbraRef();
	float& GetLightRadiusRef();
	float& GetLightSizeRef();
	bool& GetPCSSEnabledRef();
	size_t& GetPcfSamples();

	float&	PcfTextureSampleRadiusRef();
	bool&		PcfTextureSampleEnabledRef();

	float& GetCsmBaseSlopeBias();
	float& GetCsmBaseCascadePlaneBias();

	float& GetCsmPolygonOffset();

	// Gaussian Blur
	size_t& KernelSize();
	float& SampleSize();
	float& BrightnessAmplifier();

	FogInfo& GetFogInfo() { return m_foginfo; }

	float& Metallic();
	float& Spec();
	glm::vec4& Scale();
	float& K();

	bool& EnvironmentMap();

	//Volumetric Clouds
	float& Noize3DZDebug();
	int32_t& Noize3DOctaveDebug();
	int32_t& GetCloudDensity();
	int32_t& GetCloudAbsorption();
	glm::vec3& GetCloudColor();
	float& GetCloudPerlinWeight();
	int32_t& GetCloudPerlinMotion();
	int32_t& GetCloudWorleyMotion();
	float& GetCloudGParam();
	glm::vec3& GetNoiseScale();
	bool& GetApplyPowder();
	float& GetSilverLiningDensity();
	int32_t& GetSilverLiningStrength();
	float& GetAlphathreshold();
	bool& GetCloudSilverLining();
	int32_t& GetWorleyOctaveSizeOne();
	int32_t& GetWorleyOctaveSizeTwo();
	int32_t& GetWorleyOctaveSizeThree();
	int32_t& GetWorleyNoiseGamma();
	bool& GetCSMCullingEnabled();
	bool& GetCSMCullingFront();
	bool& ForwardPlusPipeline();
	bool& ZPrePassPipeline();

	bool& IsFrustumCull();

	bool& ShowFPS() { return m_show_fps; }
	bool*& IsMeasurementGridEnabled() { return m_measurement_grid; } //@todo why pointer?

	void RedoWorleyNoise();

	//statistic
	uint32_t& GetDrawCalls() { return m_draw_calls; }
	size_t&		GetDrawTriangles() { return m_draw_triangles; }

	glm::vec4 debug_float = {0.0f,0.0f,0.0f,0.0f};

	Texture GetDefaultBufferTexture() const;
	Texture GetDepthBufferTexture() const;
	Texture GetSkyNoiseTexture(const Camera& _camera);
  Texture GetReflectionBufferTexture() const;
  Texture GetRefractionBufferTexture() const;
  Texture GetShadowBufferTexture() const;
  Texture GetGausian1BufferTexture() const;
  Texture GetGausian2BufferTexture() const;
  Texture GetMtsBufferTexture() const;
  Texture GetScreenBufferTexture() const;
  Texture GetBrightFilter() const;
	Texture GetSSAO() const;
	Texture GetDefferedOne() const;
	Texture GetDefferedTwo() const;
	Texture GetHdrCubeMap() const;
	Texture GetEnvironmentCubeMap() const;
	Texture GetLUT() const;
	Texture GetCSMMapLayer1() const;
	Texture GetCSMMapLayer2() const;
	Texture GetCSMMapLayer3() const;
	Texture GetCSMMapLayer4() const;
	Texture GetCSMMapLayer5() const;
	Texture GetBloomTexture() const;
	Texture GetSSRTexture() const;
	Texture GetSSRWithScreenTexture() const;
	Texture GetSSRTextureScreenMask() const;
	Texture GetCameraInterpolationCoords() const;
	Texture GetComputeParticleSystem() const;
	Texture GetLuminanceTexture() const;
	Texture GetUIlessTexture() const;

	void DumpCSMTextures() const;

protected:
	void			RenderShadows(const Camera&, const Light&, std::vector<shObject>&, bool _depth = false);
	void			RenderShadowsCSM(const Camera& _camera, const Light& _light, std::vector<shObject>& _objects);
	void			RenderSkybox(const Camera&);
	void			RenderReflection(Camera&, const std::vector<Light>&, std::vector<shObject>&, std::vector<shObject>&);
	void			RenderRefraction(Camera&, const std::vector<Light>&, std::vector<shObject>&, std::vector<shObject>&);
	void			RenderSkyNoise(const Camera&);
	void			RenderMain(const Camera&, const Light&, const std::vector<shObject>&);
	void			RenderAreaLightsOnly(const Camera& _camera, const Light& _light, const std::vector<shObject>& _objects);
	void			RenderOutlineFocused(const Camera&, const Light&, const std::vector<shObject>&);
	void			RenderFlags(const Camera&, const Light&, std::vector<shObject>);
	void			RenderWater(const Camera&, const Light&);
	void			RenderGeometry(const Camera&, const SimpleGeometryMesh& _mesh);
	void			RenderParticles(const Camera&);
	void			RenderBlur(const Camera&);
	void			RenderContrast(const Camera& _camera, const Texture& _screen, const Texture& _contrast, float _tick);
	void			RenderGui(const std::vector<std::shared_ptr<GUI>>&, const Camera&);
	void			RenderPBR(const Camera&, const std::vector<Light>&, std::vector<shObject> _objs);
	void			RenderSSAO(const Camera&, const Light&, std::vector<shObject>&);
	void			RenderSSR(const Camera& _camera);
	void			RenderCameraInterpolationCompute(const Camera& _camera);
	Texture*  RenderCameraInterpolation(const Camera& _camera);
	void			RenderIBL(const Camera& _camera);
	void			RenderEnvironmentSnapshot(const RenderBuckets& _objects, const std::vector<Camera>& _cameras, const Light& _light);
	void			RenderBloom();
	void			RenderTerrainTessellated(const Camera&, const Light& _light, std::vector<shObject> _objs);
	void			RenderVolumetric(const Camera&, const Light& _light, std::vector<shObject> _objs);

	void			LoadPCFOffsetsTextureToShaders();
	void			EnsureInitialized(const Camera&);

	size_t		m_active_camera = 0;

	bool			mts			= true;
	bool			skybox		= false;
	bool			shadows		= true;
	bool			water		= true;
	bool			flags_on		= true;
	bool			geometry	= true;
	bool			particles	= true;
	bool			draw_bounding_boxes = false;
	bool			kernel = false;
	bool			sky_noise = true;
	bool			bezier_curve = true;
	bool			outline_focused = true;
	bool			ssao = false;
	bool			ssr = false;
	bool			camera_interpolation = false;
	bool			uiless_screen = false;
	bool			m_pb_bloom = true;
	bool			m_bloom_threshold = true;
	float			blur_coef = 1.0f;
	bool			m_auto_exposure = true;
	float			m_target_luminance = 0.25f;
	float			m_adaption_rate = 0.75;

	bool			m_mesh_line_on = false;
	bool			ibl_on = true;
	float			ibl_influance = 1.0f;

	bool			m_compute_shader = false;
	bool			m_environment_map = false;
	bool			m_forward_plus = false;
	bool			m_z_pre_pass = false;
	bool			m_frustum_cull = false;

	//csc + pcss
	float			m_max_penumbra = 8.0f;
	float			m_light_radius = 2.5f;
	float			m_light_size = 4.0f;
	bool			m_pcss_enabled = true;
	size_t		m_pcf_samples = 3; // 3-4
	float			m_pcf_texture_sample_radius = 2.5f;
	bool			m_pcf_texture_sample_enabled = true;
	float			m_cascade_blend_distance = 0.25f;
	bool			m_blend_cascades = false;
	float			m_max_shadow = 0.7f;
	float			m_csm_base_slope_bias = 0.002f; // [0.005 - 0.05].
	float			m_csm_base_cascade_plane_bias = 0.12f; // 0.1 - 0.25
	float			m_csm_polygon_offset = 0.1f;
	bool			m_csm_culling_enabled = false;
	bool			m_csm_fromt_face_cull = false;

	FogInfo		m_foginfo;

	bool			mousepress = true; //to draw framed objects
	float			waterHeight = 2.0f;
	bool			m_first_call = true;
	bool			m_show_fps = true;
	bool*			m_measurement_grid = nullptr;

	//statistic
	uint32_t m_draw_calls = 0;
	size_t	 m_draw_triangles = 0;

	const uint32_t  m_width		  = 1200; //@todo make resizable
	const uint32_t  m_height		= 750;

	std::unique_ptr<eRenderManager>	renderManager;
	eTextureManager* m_texture_manager = nullptr;

	Texture csm_dump1;
	Texture csm_dump2;
	Texture csm_dump3;
	Texture csm_dump4;
	Texture csm_dump5;
};

#endif // PIPELINE_H

