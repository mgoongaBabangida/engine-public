#pragma once

#include <functional>
#include <array>
#include <variant>
#include <vector>
#include <string>
#include <memory>

#include <glm\glm\glm.hpp>

#pragma warning( disable : 4251) // vector & unique_ptr have to be exported or not used @todo
#pragma warning( disable : 4275) // non-dll class I- used as base for dll-interface
#pragma warning( disable : 4273) // inconsistent dll-linkage

#ifdef DLLDIR_EX
#define DLL_BASE __declspec(dllexport)
#else
#define DLL_BASE __declspec(dllimport)
#endif

class eObject;

//-------------------------------------------------------
struct TextureInfo
{
	TextureInfo(const std::string& _type, const std::string& _path)
		: m_type(_type)
		, m_path(_path)
	{}
	std::string m_type;
	std::string m_path; //variant path or name in manager ? or enum ?
};

enum class eLightType { POINT = 0, DIRECTION = 1, SPOT =2, CSM =3, AREA_LIGHT =4};

//-------------------------------------------------------
struct Light
{
	glm::vec4  light_position;
	glm::vec4  light_direction;

	glm::vec4  ambient  = { 0.1f, 0.1f, 0.1f, 1.0f };
	glm::vec4  diffuse  = { 0.45f, 0.45f, 0.45f, 1.0f };
	glm::vec4  specular = { 0.45f, 0.45f, 0.45f, 1.0f };

	glm::vec4  intensity = { 10, 10, 10 , 1.0f };

	eLightType type = eLightType::POINT;
	
	float constant	= 1.0f; // for spot
	float linear		= 0.09f;
	float quadratic = 0.032f;
	float cutOff		= 0.3f;
	float outerCutOff = 0.0f;

	//for area light
	std::array<glm::vec4, 4> points;
	float radius = 100.f;//@todo use all this for area light, froward+ rendering
	float fallOff = 0.f;
	bool active = true;
};

//-------------------------------------------------------
using UniformData = std::variant<bool, float, int32_t, size_t,
																 glm::vec2, glm::vec4,
																 glm::mat2, glm::mat4>;

//-------------------------------------------------------
struct Uniform
{
	std::string name;
	int32_t location;
	int32_t type;
	UniformData data;
};

//----------------------------------------------------------
struct ShaderInfo
{
	std::string name;
	int32_t id;
	const std::vector<Uniform>& uniforms;
};

constexpr float PI = 3.1415926536f;

static constexpr glm::vec3 NONE{ glm::vec3(-100.0f, -100.0f, -100.0f) }; //@todo

constexpr glm::vec3 XAXIS		= glm::vec3(1.0f, 0.0f, 0.0f);
constexpr glm::vec3 YAXIS		= glm::vec3(0.0f, 1.0f, 0.0f);
constexpr glm::vec3 ZAXIS		= glm::vec3(0.0f, 0.0f, 1.0f);

constexpr glm::mat4 UNIT_MATRIX = glm::mat4(1.0f, 0.0f, 0.0f, 0.0f,
																						0.0f, 1.0f, 0.0f, 0.0f,
																						0.0f, 0.0f, 1.0f, 0.0f,
																						0.0f, 0.0f, 0.0f, 1.0f);

constexpr int32_t MAX_BONES = 100;

//-------------------------------------------------------
struct Vertex
{
	glm::vec3		Position;	//0
	glm::vec3		Color;	//1
	glm::vec3		Normal;		//2
	glm::vec2		TexCoords;	//3
	glm::vec4		tangent;	//4
	glm::vec3		bitangent;	//5
	glm::ivec4	boneIDs{ 0, 0 ,0 ,0 };	//6
	glm::vec4		weights{ 0.0f, 0.0f ,0.0f ,1.0f };	//7
};

//-------------------------------------------------------
struct eCollision
{
	eObject*  collider;
	eObject*  collidee;
	glm::vec3 intersaction;
	glm::mat3 triangle;
};

//-------------------------------------------------
struct DebugInfo
{
	std::array<glm::vec3, 13> axis;
};

//-------------------------------------------------
struct CollisionManifold
{
	bool colliding;
	glm::vec3 normal;
	float depth;
	std::vector<glm::vec3> contacts;
	DebugInfo info;

	//-------------------------------------------------------
	static void ResetCollisionManifold(CollisionManifold& result)
	{
		result.colliding = false;
		result.normal = glm::vec3(0, 0, 1);
		result.depth = FLT_MAX;
		result.contacts.clear();
	}
};

//-------------------------------------------------------
struct RaycastResult
{
	float t;
	bool hit;
	glm::vec3 point;
	glm::vec3 normal;
	float tmin;
	float tmax;
	static void ResetRaycastResult(RaycastResult* r)
	{
		r->t = 0;
		r->hit = false;
		r->point = {0,0,0};
		r->normal = {0,1,0};
		r->tmin = 0;
		r->tmax = 0;
	}
};

//-------------------------------------------------------
enum Side
{
	FORWARD,
	BACK,
	LEFT,
	RIGHT,
	UP,
	DOWN
};

//--------------------------------------------------------
struct extremDots
{
	float MaxX = -std::numeric_limits<float>::infinity();
	float MinX = std::numeric_limits<float>::infinity();
	float MaxY = -std::numeric_limits<float>::infinity();
	float MinY = std::numeric_limits<float>::infinity();
	float MaxZ = -std::numeric_limits<float>::infinity();
	float MinZ = std::numeric_limits<float>::infinity();
};

//--------------------------------------------------------
enum ePriority
{
	WEAK,
	STRONG,
	MONOPOLY,
	ALWAYS
};

//--------------------------------------------------------
struct Text
{
	std::string content;
	std::string font;
	glm::vec3 color;
	float pos_x;
	float pos_y;
	float scale;
	glm::mat4 mvp;
	bool visible = true;
};

//--------------------------------------------------------
struct AnimationSocket
{
	eObject* m_socket_object = nullptr;
	std::string m_bone_name;
	unsigned int m_bone_id;
	glm::mat4 m_pre_transform;
	bool m_map_global = false;
};

//-------------------------------------------------------
//@todo translation from SDL to ASCII should be on controller side
enum ASCII
{
	ASCII_W = 119,//87,
	ASCII_S = 115,//83
	ASCII_D = 100,//68,
	ASCII_E = 101,
	ASCII_A = 97, //65,
	ASCII_R = 114,//82,
	ASCII_F = 102,//70,
	ASCII_Q = 113,//81
	ASCII_J = 106,//74,
	ASCII_L = 108,//76,
	ASCII_K = 107,//75,
	ASCII_I = 105,//73,
	ASCII_Z = 122,//90,
	ASCII_X = 120,//88,
	ASCII_C = 99,//67,
	ASCII_V = 118,//86,
	ASCII_B = 98,//66,
	ASCII_N = 110,//78,
	ASCII_U = 117,//85,
	ASCII_H = 104,//72,
	ASCII_G = 103,//71,
	ASCII_T = 116,

	ASCII_TAB = 9,
	ASCII_ENTER = 13,
	ASCII_ESCAPE = 27,
	ASCII_SPACE = 32,
	ASCII_BACKSPACE = 31,

	// Extended (>255) – distinct navigation keys
	ASCII_LEFT = 0x100 + 1,
	ASCII_RIGHT = 0x100 + 2,
	ASCII_UP = 0x100 + 3,
	ASCII_DOWN = 0x100 + 4,
	ASCII_HOME = 0x100 + 5,
	ASCII_END = 0x100 + 6,
	ASCII_PAGEUP = 0x100 + 7,
	ASCII_PAGEDOWN = 0x100 + 8,

	ASCII_0 = 0x100 + 9,
	ASCII_1 = 0x100 + 10,
	ASCII_2 = 0x100 + 11,
	ASCII_3 = 0x100 + 12,
	ASCII_4 = 0x100 + 13,
	ASCII_5 = 0x100 + 14,
	ASCII_6 = 0x100 + 15,
	ASCII_7 = 0x100 + 16,
	ASCII_8 = 0x100 + 17,
	ASCII_9 = 0x100 + 18,
	ASCII_LAST_ENUM = ASCII_9+1
};

//-------------------------------------------------------
enum class KeyModifiers : uint32_t
{
	NONE = 0,
	SHIFT = 1,
	CTRL = 2,
	CTRL_SHIFT = SHIFT | CTRL,
	ALT = 4
};

//ImGui usage
//-------------------------------------------------------
struct eThreeFloat
{
	std::array<float, 3> data = { 0.f, 0.f, 0.f };
	float min, max;
};

//-------------------------------------------------------
struct eThreeFloatCallback
{
  std::array<float, 3> data = {0.f, 0.f, 0.f};
  std::function<void()> callback;
  float min, max;
};

//-------------------------------------------------------
struct eVectorStringsCallback // for combo-boxes
{
  std::vector<std::string> data;
  std::function<size_t(size_t)> callback;
	int current_item = INT_MAX;
};
