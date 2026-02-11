#include "stdafx.h"
#include "TextureManager.h"
#include "Texture.h"

#include <fstream>
#include <sstream>

#include <base/Log.h>
#include <math/Random.h>

#define DEVIL_IMAGE
//#define SDL_IMAGE

#ifdef DEVIL_IMAGE
	#include "TextureImplDevIl.h"
#endif

#ifdef SDL_IMAGE
	#include "TextureImplSDL.h"
#endif

#include <glm/glm/gtc/matrix_transform.hpp>

//-----------------------------------------------------------------------------
eTextureManager::~eTextureManager()
{
  for (auto& node : m_Textures)
    node.second.freeTexture();

	// free static textures
	Texture::freeTexture(Texture::GetTexture1x1(WHITE).m_id);
	Texture::freeTexture(Texture::GetTexture1x1(BLACK).m_id);
	Texture::freeTexture(Texture::GetTexture1x1(BLUE).m_id);
	Texture::freeTexture(Texture::GetTexture1x1(PINK).m_id);
	Texture::freeTexture(Texture::GetTexture1x1(YELLOW).m_id);
	Texture::freeTexture(Texture::GetTexture1x1(GREY).m_id);

#ifdef SDL_IMAGE
	{ IMG_Quit(); }
#endif
}

//-----------------------------------------------------------------------------
void eTextureManager::Initialize()
{
	std::ifstream infile("textures.ini");
	if (infile.is_open())
	{
		std::stringstream sstream;
		std::copy(std::istreambuf_iterator<char>(infile),
			std::istreambuf_iterator<char>(),
			std::ostreambuf_iterator<char>(sstream));
		Texture text;

		std::string file_name , name, type, wrap, temp;
		std::vector<std::string> faces;
		while (!sstream.eof())
		{
			sstream >> file_name;
			sstream >> name;
			sstream >> type;
			if(type != ";")
				sstream >> wrap;

			if (!type.empty() && type != ";")
				text.m_type = type;

			if(type == "skybox" || type == "array" || type == "array_last" || type == "array_last_r")
				faces.push_back(folderPath + file_name);

			if (faces.size() == 6 && type == "skybox")
			{
				text.loadCubemap(faces);
				m_cubemap_ids.push_back(text.m_id);
				faces.clear();
			}
			else if (type == "array_last")
			{
				text.loadTexture2DArray(faces);
				faces.clear();
			}
			else if (type == "array_last_r")
			{
				text.loadTexture2DArray(faces, GL_RED);
				faces.clear();
			}
			else if (!wrap.empty() && wrap != ";")
			{
				text.loadTextureFromFile(folderPath + file_name, GL_RGBA, GL_REPEAT);
				sstream >> temp; // to read ";"
			}
			else if (type == "skybox" || type == "array")
				continue;
			else if (type == "hdr")
			{
				text.loadHdr(folderPath + file_name);
				m_hdr_ids.push_back(text.m_id);
			}
			else
				text.loadTextureFromFile(folderPath + file_name);

			if (type == "atlas4")
				text.setNumRows(4);
			else
				text.setNumRows(1);

			m_Textures.insert(std::pair<std::string, Texture>(name, text));
		}
	}

	_LoadHardcoded();
	Texture pcf = _BuildPCFTexture();
	AddExisting("TPCF", &pcf);
}

//-----------------------------------------------------------------------------
void eTextureManager::InitContext(const std::string& _folderPath)
{
	folderPath = _folderPath;
#ifdef DEVIL_IMAGE
	ilInit();
	iluInit();
	ilutInit();
	ilutRenderer(ILUT_OPENGL);
#endif

#ifdef SDL_IMAGE
	int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG;
	if(!(IMG_Init(imgFlags) & imgFlags))
	{
		std::cout << "error initing SDL Image" << std::endl;
	}
#endif
}

//-----------------------------------------------------------------------------
void eTextureManager::_LoadHardcoded()
{
	Texture text;
	text.loadTextureFromFile(folderPath + "empty.png");
	m_Textures.insert(std::pair<std::string, Texture>("Tempty", text));
	Texture::SetEmptyTextureId(text.m_id);

	text.loadTextureFromFile(folderPath + "light_icon.png");
	m_Textures.insert(std::pair<std::string, Texture>("Tlight_icon", text));
	Texture::SetLightIconTextureId(text.m_id);

	text.loadTextureFromFile(folderPath + "opengl.png");
	m_Textures.insert(std::pair<std::string, Texture>("Twelcome", text));

	text.generatePerlin(600, 600, true); //@todo HARDCODING!
	m_Textures.insert(std::pair<std::string, Texture>("Tperlin_n", text));

	text.makeImage(1000, 1000); //@todo temp HARDCODING!
	m_Textures.insert(std::pair<std::string, Texture>("computeImageRW", text));

	text.makeImage(1200, 750); //@todo!!! screen with & height
	m_Textures.insert(std::pair<std::string, Texture>("computeImageRWCameraInterpolation", text));
}

//----------------------------------------------------
const Texture* eTextureManager::Find(const std::string& texture_name) const
{
	if (m_Textures.find(texture_name) != m_Textures.end())
		return &m_Textures.find(texture_name)->second;
	else
	{
		base::Log("texture not found!");
		return nullptr;
	}
}

//----------------------------------------------------
const Texture* eTextureManager::FindByID(unsigned int _id) const
{
	for (auto& texture : m_Textures)
		if (texture.second.m_id == _id)
			return &texture.second;
	return nullptr;
}

//----------------------------------------------------
std::vector<std::string> eTextureManager::GetTextureAtlasesNames() const
{
	std::vector<std::string> ret;
	for (const auto& texture : m_Textures)
	{
		if (texture.second.m_num_rows > 1)
			ret.push_back(texture.first);
	}
	return ret;
}

//----------------------------------------------------
void eTextureManager::AddExisting(const std::string& _name, Texture* _text)
{
	m_Textures.insert(std::pair<std::string, Texture>(_name, *_text));
	if(_text->m_type == "skybox")
		m_cubemap_ids.push_back(_text->m_id);
}

//-----------------------------------------------------
uint64_t eTextureManager::LoadTexture(const std::string& _path, const std::string& _name, const std::string& _type)
{
	Texture text;
	text.loadTextureFromFile(_path);
	text.m_type = _type;
	m_Textures.insert(std::pair<std::string, Texture>(_name, text));
	return text.m_id;
}

//-----------------------------------------------------
Texture eTextureManager::_BuildPCFTexture()
{
	int samplesU = 4;
	int samplesV = 8;
	int jitterMapSize = 8;

	int size = jitterMapSize;
	int samples = samplesU * samplesV;
	int bufSize = size * size * samples * 2;
	std::vector<float> data(bufSize);

	for (int i = 0; i < size; i++)
	{
		for (int j = 0; j < size; j++)
		{
			for (int k = 0; k < samples; k += 2) 
			{
				int x1, y1, x2, y2;
				x1 = k % (samplesU);
				y1 = (samples - 1 - k) / samplesU;
				x2 = (k + 1) % samplesU;
				y2 = (samples - 1 - k - 1) / samplesU;

				glm::vec4 v;
				// Center on grid and jitter
				v.x = (x1 + 0.5f) + math::Random::RandomFloat(-0.5f, 0.5f);
				v.y = (y1 + 0.5f) + math::Random::RandomFloat(-0.5f, 0.5f);
				v.z = (x2 + 0.5f) + math::Random::RandomFloat(-0.5f, 0.5f);
				v.w = (y2 + 0.5f) + math::Random::RandomFloat(-0.5f, 0.5f);

				// Scale between 0 and 1
				v.x /= samplesU;
				v.y /= samplesV;
				v.z /= samplesU;
				v.w /= samplesV;

				// Warp to disk
				int cell = ((k / 2) * size * size + j * size + i) * 4;
				data[cell + 0] = sqrtf(v.y) * cosf(glm::two_pi<float>() * v.x);
				data[cell + 1] = sqrtf(v.y) * sinf(glm::two_pi<float>() * v.x);
				data[cell + 2] = sqrtf(v.w) * cosf(glm::two_pi<float>() * v.z);
				data[cell + 3] = sqrtf(v.w) * sinf(glm::two_pi<float>() * v.z);
			}
		}
	}

	glActiveTexture(GL_TEXTURE1);
	GLuint texID;
	glGenTextures(1, &texID);

	glBindTexture(GL_TEXTURE_3D, texID);

	glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA32F, size, size, samples / 2);
	glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, size, size, samples / 2, GL_RGBA, GL_FLOAT, &data[0]);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	Texture tex;
	tex.m_id = texID;
	tex.m_layers = 3;
	tex.m_width = samplesU;
	tex.m_height = samplesV;
	tex.m_type = "array";
	return tex;
}
