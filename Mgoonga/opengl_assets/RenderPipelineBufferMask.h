#pragma once
#include <cstdint>

//--------------------------------------
// Framebuffer selection mask
//--------------------------------------
enum class FboBits : uint64_t
{
	None = 0,
	Default = 1ull << 0,
	Screen = 1ull << 1,
	ScreenWithSSR = 1ull << 2,
	MTS = 1ull << 3,
	Reflection = 1ull << 4,
	Refraction = 1ull << 5,
	ShadowDir = 1ull << 6,
	ShadowCube = 1ull << 7,
	ShadowCSM = 1ull << 8,
	Depth = 1ull << 9,
	Deferred = 1ull << 10,
	SSAO = 1ull << 11,
	SSAOBlur = 1ull << 12,
	BrightFilter = 1ull << 13,
	Gaussian1 = 1ull << 14,
	Gaussian2 = 1ull << 15,
	Bloom = 1ull << 16,
	SSR = 1ull << 17,
	SSRBlur = 1ull << 18,
	IBLCubemap = 1ull << 19,
	IBLCubemapIrr = 1ull << 20,
	EnvironmentCubemap = 1ull << 21,
	Square = 1ull << 22,

	// Custom buffers
	CameraInterpolationBuffer = 1ull << 60,
	ComputeParticleBuffer = 1ull << 61,
	UIlessBuffer = 1ull << 62
};

//--------------------------------------
// SSBO selection mask
//--------------------------------------
enum class SsboBits : uint32_t
{
	None = 0,
	ModelToProjection = 1u << 0,
	ModelToWorld = 1u << 1,
	InstancedInfo = 1u << 2,
	HeraldryInfo = 1u << 3,
	BonesPacked = 1u << 4,
	BoneBaseIndexes = 1u << 5
};

//--------------------------------------
inline FboBits operator|(FboBits a, FboBits b)
{
	return static_cast<FboBits>(
		static_cast<uint64_t>(a) | static_cast<uint64_t>(b)
		);
}

inline bool HasFlag(FboBits mask, FboBits bit)
{
	return (static_cast<uint64_t>(mask) &
		static_cast<uint64_t>(bit)) != 0;
}

//--------------------------------------
inline SsboBits operator|(SsboBits a, SsboBits b)
{
	return static_cast<SsboBits>(
		static_cast<uint32_t>(a) | static_cast<uint32_t>(b)
		);
}

inline bool HasFlag(SsboBits mask, SsboBits bit)
{
	return (static_cast<uint32_t>(mask) &
		static_cast<uint32_t>(bit)) != 0;
}
