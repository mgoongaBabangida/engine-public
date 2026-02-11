#pragma once

#include "math/math.h"

#include <math.h>
#include <glm/glm/glm.hpp>
#include <glm/glm/gtc/constants.hpp>

#include <array>
#include <cmath>
#include <vector>
#include <unordered_set>
#include <functional>
#include <queue>

namespace math
{
	constexpr float PI = 3.1415926536f;

	using fIsBlocked = std::function<bool(const glm::vec3&)>;

	struct DLL_MATH Hex
	{
		enum class Orientation
		{
			Flat,
			Pointy
		};

		enum class DirectionsFlat
		{
			DownRight = 0,
			TopRight,
			Top,
			TopLeft,
			DownLeft,
			Down
		};

		enum class DirectionsPointy
		{
			Right = 0,
			TopRight,
			TopLeft,
			Left,
			DownLeft,
			DownRight
		};

		static float	g_outer_radius;
		static float	g_inner_radius;

		static void SetOuterRadius(float _r);
		static float GetOuterRadius();
		static float GetInnerRadius();

		static const inline std::array<float, 9> g_layout_pointy	= {sqrtf(3.0), sqrtf(3.0) / 2.0f, 0.0f, 3.0f / 2.0f, sqrtf(3.0) / 3.0f, -1.0f / 3.0f, 0.0f, 2.0f / 3.0f, 0.5f};
		static const inline std::array<float, 9> g_layout_flat		= {3.0f / 2.0f, 0.0f, sqrtf(3.0f) / 2.0f, sqrtf(3.0f), 2.0f / 3.0f, 0.0f, -1.0f / 3.0f, sqrtf(3.0f) / 3.0f, 0.0f};

		static const inline std::array<glm::vec3, 6> g_cube_direction_vectors =
		{
			glm::vec3(+1, 0, -1), glm::vec3(+1, -1, 0), glm::vec3(0, -1, +1),
			glm::vec3(-1, 0, +1), glm::vec3(-1, +1, 0), glm::vec3(0, +1, -1),
		};

		explicit				Hex(glm::vec3 pos, Orientation orientation = Orientation::Flat);

		static float			Size();

		inline bool				IsFlat()								const	{ return m_orientation == Orientation::Flat; }
		bool							IsValid()								const	{ return m_cube_coordinates.x + m_cube_coordinates.y + m_cube_coordinates.z == 0; }

		float					HorizontalSpacing()						const;
		float					VerticalSpacing()						const;

		glm::vec2				FlatHexCorner(glm::vec2 center, float size, size_t i);

		static glm::vec3		GetCubeDirection(DirectionsFlat _direction)		{ return g_cube_direction_vectors[(size_t)_direction]; }
		static glm::vec3		GetCubeDirection(DirectionsPointy _direction)	{ return g_cube_direction_vectors[(size_t)_direction]; }

		static glm::vec3		Add(glm::vec3 hex, glm::vec3 vec)				{ return glm::vec3(hex.x + vec.x, hex.y + vec.y, hex.z + vec.z); }

		static Hex				GetNeighbor(glm::vec3		cube,
											DirectionsFlat	direction,
											Orientation		orientation = Orientation::Flat); //@todo add checks for orientation

		static Hex				GetNeighbor(glm::vec3			cube,
											DirectionsPointy	direction,
											Orientation			orientation = Orientation::Flat);

		static std::vector<Hex>	GetRangeHexes(const Hex& center, int32_t range);

		glm::vec2						ToWorldSpaceXZPos()						const;
		static glm::vec3		PixelToHex(glm::vec3 position, Orientation);

		bool							IsOnWorldSpace(float x, float z)		const;

		static bool				IsWithinRange(const Hex&	_center, const Hex&	_hex, int32_t		_range)	{ return CubicDistance(_center, _hex) <= _range; }

		static std::unordered_set<glm::vec3> GetReachableHexes(const Hex&			start, size_t movement,const fIsBlocked&	is_blocked);

		static std::vector<Hex>	GetShortestPath(const Hex&			start,
												const Hex&			end,
												const fIsBlocked&	is_blocked);

		static std::vector<Hex> GenerateHexGrid(int radius, Orientation _orientation = Orientation::Flat);

		static glm::vec3	Subtract(Hex a, Hex b);
		static float			CubicDistance(Hex a, Hex b);
		static float			WorldSpaceDistance(Hex a, Hex b);

		static glm::vec3				CubeLerp(Hex a, Hex b, float _t);
		static std::vector<Hex>	CubeLineDraw(Hex a, Hex b);
		static glm::vec3				CubeRound(glm::vec3);

		// Nested hash function for glm::vec3
		struct HashVec3
		{
			size_t operator()(const glm::vec3& v) const
			{
				size_t h1 = std::hash<float>()(v.x);
				size_t h2 = std::hash<float>()(v.y);
				size_t h3 = std::hash<float>()(v.z);
				return h1 ^ (h2 << 1) ^ (h3 << 2);
			 }
		};

		glm::vec3			m_cube_coordinates; // (q,r,s)
		Orientation		m_orientation = Orientation::Flat;
	};
}