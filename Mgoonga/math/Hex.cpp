#include "stdafx.h"

#include "Hex.h"
#include "Geometry.h"

#include <glm/glm/gtx/compatibility.hpp>

namespace std
{

template <>
struct hash<glm::vec3>
{
	size_t operator()(const glm::vec3& v) const
	{
		size_t h1 = std::hash<float>()(v.x);
		size_t h2 = std::hash<float>()(v.y);
		size_t h3 = std::hash<float>()(v.z);
		return h1 ^ (h2 << 1) ^ (h3 << 2);
	}
};

} //namespace std

namespace math
{

float	Hex::g_outer_radius = 1.0f;
float Hex::g_inner_radius = sqrtf(3.0f) * 0.5f * g_outer_radius;

//-----------------------------------------------------------------------------------
void Hex::SetOuterRadius(float _r)
{
	g_outer_radius = _r;
	g_inner_radius = sqrtf(3.0f) * 0.5f * g_outer_radius;
}

//----------------------------------------------------------------------------------
float Hex::GetOuterRadius()
{
	return Hex::g_outer_radius;
}

//----------------------------------------------------------------------------------
float Hex::GetInnerRadius()
{
	return Hex::g_inner_radius;
}

//-------------------------------------------------------------------------------------
Hex::Hex(glm::vec3 _pos, Orientation _orientation)
: m_cube_coordinates(_pos)
, m_orientation(_orientation)
{}
//-------------------------------------------------------------------------------------
float Hex::HorizontalSpacing() const
{
	return IsFlat() ? float(1.5f * g_outer_radius) : sqrtf(3) * g_outer_radius;
}
//-------------------------------------------------------------------------------------
float  Hex::VerticalSpacing() const
{
	return IsFlat() ? sqrtf(3) * g_outer_radius : float(1.5f * g_outer_radius);
}
//-------------------------------------------------------------------------------------
glm::vec2 Hex::FlatHexCorner(glm::vec2 _center, float _size, size_t _i)
{
	float angle_deg = (m_orientation == Orientation::Flat) ? 60 * _i : 60 * _i - 30;
	float angle_rad = PI / 180 * angle_deg;
	return glm::vec2(_center.x + _size * cos(angle_rad), _center.y + _size * sin(angle_rad));
}
//-------------------------------------------------------------------------------------
float Hex::Size()
{
	return g_outer_radius;
}
//-------------------------------------------------------------------------------------
Hex Hex::GetNeighbor(glm::vec3		_cube,
					 DirectionsFlat	_direction,
					 Orientation	_orientation)
{
	return Hex{ Add(_cube, GetCubeDirection(_direction)), _orientation };
}
//-------------------------------------------------------------------------------------
Hex Hex::GetNeighbor(glm::vec3			_cube,
					 DirectionsPointy	_direction,
					 Orientation		_orientation)
{
	return Hex{ Add(_cube, GetCubeDirection(_direction)), _orientation };
}
//-------------------------------------------------------------------------------------
std::vector<Hex> Hex::GetRangeHexes(const Hex& _center, int32_t _range)
{
	std::vector<Hex> results;

	for (int32_t q = -_range; q <= _range; ++q)
	{
		// Include the range boundary
		for (int32_t r = glm::max(-_range, -q - _range); r <= glm::min(_range, -q + _range); ++r)
		{
			int32_t s = -q - r; // Compute s since q + r + s = 0
			results.push_back(Hex(Add(_center.m_cube_coordinates, glm::vec3(q, r, s))));
		}
	}
	return results;
}

//-------------------------------------------------------------------------------------
glm::vec2 Hex::ToWorldSpaceXZPos() const
{
	// Use horizontal and vertical spacing to determine world space coordinates
	float x = HorizontalSpacing() * m_cube_coordinates.x;
	float y = VerticalSpacing() * (m_cube_coordinates.z + m_cube_coordinates.x / 2.0f);
	return glm::vec2(x, y);
}

//-------------------------------------------------------------------------------------
glm::vec3 Hex::PixelToHex(glm::vec3 position, Orientation orientation)
{
	glm::vec2 pt(position.x / g_outer_radius, position.z / g_outer_radius);
	glm::vec2 coords;
	if(orientation == Orientation::Flat)
	{
		coords.x = g_layout_flat[4] * pt.x + g_layout_flat[5] * pt.y;
		coords.y = g_layout_flat[6] * pt.x + g_layout_flat[7] * pt.y;
	}
	else
	{
		coords.x = g_layout_pointy[4] * pt.x + g_layout_pointy[5] * pt.y;
		coords.y = g_layout_pointy[6] * pt.x + g_layout_pointy[7] * pt.y;
	}
	return glm::vec3(dbb::round(coords.x, 0), dbb::round(- coords.x - coords.y, 0), dbb::round(coords.y, 0));
}

//-------------------------------------------------------------------------------------
bool Hex::IsOnWorldSpace(float _x, float _z) const
{
	glm::vec3 center = { ToWorldSpaceXZPos().x, 0, ToWorldSpaceXZPos().y };

	std::vector<glm::vec3> dots;
	if (m_orientation == Orientation::Flat)
	{
		dots = {
			glm::vec3(0.5,	0.0,	0.866)	* g_outer_radius,
			glm::vec3(1.0,	0.0,	0.0)		* g_outer_radius,
			glm::vec3(0.5,	0.0,	-0.866)	* g_outer_radius,
			glm::vec3(-0.5, 0.0,	-0.866)	* g_outer_radius,
			glm::vec3(-1.0,	0.0,	0.0)		* g_outer_radius,
			glm::vec3(-0.5,	0.0,	0.866)	* g_outer_radius,
			glm::vec3(0.5,	0.0,	0.866)	* g_outer_radius
		};
	}
	else
	{
		dots = {
			glm::vec3(0.866,	0.0,	0.5)	* g_outer_radius,
			glm::vec3(0.0,		0.0,	1.0)	* g_outer_radius,
			glm::vec3(-0.866,	0.0,	0.5)	* g_outer_radius,
			glm::vec3(-0.866,	0.0,	-0.5)	* g_outer_radius,
			glm::vec3(0.0,		0.0,	-1.0)	* g_outer_radius,
			glm::vec3(0.866,	0.0,	-0.5)	* g_outer_radius,
			glm::vec3(0.866,	0.0,	0.5)	* g_outer_radius
		};
	}

	for (size_t i = 0; i < 6; ++i)
	{
		glm::mat3  triangle(center, center + dots[i], center + dots[i + 1]);
		if (dbb::IsInside(triangle, glm::vec3(_x, 0, _z)))
			return true;
	}
	return false;
}

//--------------------------------------------------------------------------------------
glm::vec3 Hex::Subtract(Hex _a, Hex _b)
{
	return glm::vec3(_a.m_cube_coordinates.x - _b.m_cube_coordinates.x,
					 _a.m_cube_coordinates.y - _b.m_cube_coordinates.y,
					 _a.m_cube_coordinates.z - _b.m_cube_coordinates.z);
}

//--------------------------------------------------------------------------------------
float Hex::CubicDistance(Hex _a, Hex _b)
{
	glm::vec vec = Subtract(_a, _b);
	return (abs(vec.x) + abs(vec.y) + abs(vec.z)) / 2;
	// or: (abs(a.q - b.q) + abs(a.r - b.r) + abs(a.s - b.s)) / 2
}

//------------------------------------------------------------------------------------
float Hex::WorldSpaceDistance(Hex _a, Hex _b)
{
	return CubicDistance(_a, _b) * 2 * g_inner_radius;
}

//------------------------------------------------------------------------------------
// The reachable function
std::unordered_set<glm::vec3> Hex::GetReachableHexes(const Hex&			_start,
													 size_t				_movement,
													 const fIsBlocked&	_is_blocked)
{
	std::unordered_set<glm::vec3> visited; // Visited hexes
	visited.insert(_start.m_cube_coordinates);

	std::vector<std::vector<glm::vec3>> fringes(_movement + 1);
	fringes[0].push_back(_start.m_cube_coordinates);

	for (int k = 1; k <= _movement; ++k)
	{
		for (const auto& hex : fringes[k - 1])
		{
			for (int dir = 0; dir < 6; ++dir)
			{
				Hex neighbor = GetNeighbor(hex, DirectionsFlat(dir));
				if (visited.find(neighbor.m_cube_coordinates) == visited.end() && !_is_blocked(neighbor.m_cube_coordinates))
				{
					visited.insert(neighbor.m_cube_coordinates);
					fringes[k].push_back(neighbor.m_cube_coordinates);
				}
			}
		}
	}
	return visited;
}

//------------------------------------------------------------------------------------
std::vector<Hex> Hex::GetShortestPath(const Hex&		_start,
																			const Hex&		_end,
																			const fIsBlocked&	_is_blocked)
{
	std::unordered_map<glm::vec3, glm::vec3, HashVec3> came_from; // To reconstruct the path
	std::queue<glm::vec3> frontier; // Queue for BFS
	frontier.push(_start.m_cube_coordinates);

	came_from[_start.m_cube_coordinates] = glm::vec3(std::numeric_limits<float>::max()); // Mark start

	while (!frontier.empty())
	{
		glm::vec3 current = frontier.front();
		frontier.pop();

		// Stop if we reach the end
		if (current == _end.m_cube_coordinates) break;

		// Explore neighbors
		for (int dir = 0; dir < 6; ++dir)
		{
			Hex neighbor = GetNeighbor(current, DirectionsFlat(dir));

			// Skip if blocked or already visited
			if (came_from.find(neighbor.m_cube_coordinates) == came_from.end() && !_is_blocked(neighbor.m_cube_coordinates))
			{
				frontier.push(neighbor.m_cube_coordinates);
				came_from[neighbor.m_cube_coordinates] = current; // Keep track of where we came from
			}
		}
	}

	// Reconstruct the path
	std::vector<Hex> path;
	if (came_from.find(_end.m_cube_coordinates) == came_from.end())
	{
		// No path found
		return path;
	}

	for (glm::vec3 at = _end.m_cube_coordinates; at != glm::vec3(std::numeric_limits<float>::max()); at = came_from[at])
	{
		path.emplace_back(at, _start.m_orientation); // Construct Hex with the same orientation as start
	}

	std::reverse(path.begin(), path.end());
	return path;
}

//------------------------------------------------------------------------------------
glm::vec3 Hex::CubeLerp(Hex a, Hex b, float _t)
{
	return glm::vec3(glm::lerp(a.m_cube_coordinates.x, b.m_cube_coordinates.x, _t),
							   glm::lerp(a.m_cube_coordinates.y, b.m_cube_coordinates.y, _t),
							   glm::lerp(a.m_cube_coordinates.z, b.m_cube_coordinates.z, _t));
}

//--------------------------------------------------
std::vector<Hex> Hex::CubeLineDraw(Hex a, Hex b)
{
	float N = CubicDistance(a, b);
	//A + (B - A) * 1.0 / N * i // linear interpolation for each point
	std::vector<Hex> results;
	for (size_t i = 0; i <= N; ++i)
	{
		glm::vec3 cubic_lerp_coordinates = CubeLerp(a, b, 1.0f / N * i);
		results.push_back(Hex{ CubeRound(cubic_lerp_coordinates) }); //@todo Flat or Pointy
	}
	return results;
}

//---------------------------------------------------------------------
glm::vec3 Hex::CubeRound(glm::vec3 frac)
{
	float q = round(frac.x);
	float r = round(frac.y);
	float s = round(frac.z);

	float q_diff = abs(q - frac.x);
	float r_diff = abs(r - frac.y);
	float s_diff = abs(s - frac.z);

	if (q_diff > r_diff && q_diff > s_diff)
		q = -r - s;
	else if (r_diff > s_diff)
		r = -q - s;
	else
		s = -q - r;
	return {q,r,s};
}

//-------------------------------------------------------------------------
std::vector<Hex> Hex::GenerateHexGrid(int radius, Orientation orientation)
{
	std::vector<Hex> hexes;
	for (int q = -radius; q <= radius; ++q)
	{
		for (int r = glm::max(-radius, -q - radius); r <= glm::min(radius, -q + radius); ++r) {
			int s = -q - r;
			hexes.emplace_back( glm::vec3(q, r, s), orientation);
		}
	}
	return hexes;
}

} //namespace math