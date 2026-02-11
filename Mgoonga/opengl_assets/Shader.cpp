#include "stdafx.h"
#include "Shader.h"

#include <base/Log.h>

#include <fstream>
#include <sstream>
#include <filesystem>
#include <unordered_map>

//------------------------------------------------------------------------------------
void Shader::installShaders(const char* ComputeShaderName)
{
	installShaders({ { ComputeShaderName, GL_COMPUTE_SHADER } }, false);
}

//------------------------------------------------------------------------------------
void Shader::installShaders(const char* VertexShaderName, const char* FragmentShaderName, bool _transformFeedback)
{
	installShaders({
			 { VertexShaderName, GL_VERTEX_SHADER },
			 { FragmentShaderName, GL_FRAGMENT_SHADER }
		}, _transformFeedback);
}

//--------------------------------------------------------------------------------------
void Shader::installShaders(const char* VertexShaderName, const char* FragmentShaderName, const char* GeometryShaderName, bool _transformFeedback)
{
	installShaders({
				{ VertexShaderName, GL_VERTEX_SHADER },
				{ FragmentShaderName, GL_FRAGMENT_SHADER },
				{ GeometryShaderName, GL_GEOMETRY_SHADER }
		}, _transformFeedback);
}

//--------------------------------------------------------------------
void	Shader::installShaders(const char* VertexShaderName,
														 const char* FragmentShaderName,
														 const char* Tessellation1ShaderName,
														 const char* Tessellation2ShaderName)
{
	installShaders({
				{ VertexShaderName, GL_VERTEX_SHADER },
				{ FragmentShaderName, GL_FRAGMENT_SHADER },
				{ Tessellation1ShaderName, GL_TESS_CONTROL_SHADER },
				{ Tessellation2ShaderName, GL_TESS_EVALUATION_SHADER }
		}, false);
}

//-------------------------------------------------------------------------------------------------------------
void Shader::installShaders(std::vector<std::pair<const char*, GLenum>> shaderList, bool transformFeedback)
{
	id = glCreateProgram();
	std::vector<GLuint> shaders;

	// Compile and attach shaders
	for (const auto& shaderPair : shaderList)
	{
		GLuint shaderID = compileShader(shaderPair.first, shaderPair.second);
		if (shaderID)
		{
			glAttachShader(id, shaderID);
			shaders.push_back(shaderID);
		}
	}

	// Handle transform feedback if needed
	if (transformFeedback) {
		const GLchar* varyings[] = { "Position", "Velocity", "StartTime" };
		glTransformFeedbackVaryings(id, 3, varyings, GL_SEPARATE_ATTRIBS);
	}

	glLinkProgram(id);

	// Cleanup: Detach & delete shaders after linking
	for (GLuint shaderID : shaders) {
		glDetachShader(id, shaderID);
		glDeleteShader(shaderID);
	}
	checkProgramStatus();
}

//--------------------------------------------------------------------
bool Shader::checkShaderStatus(GLint shaderID)
{
	GLint compileStatus;
	glGetShaderiv(shaderID, GL_COMPILE_STATUS, &compileStatus);
	if (compileStatus != GL_TRUE)
	{
		GLint infologlength;
		glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &infologlength);
		GLchar* buffer = new GLchar[infologlength];

		GLint bufferSize;
		glGetShaderInfoLog(shaderID, infologlength, &bufferSize, buffer);
		base::Log(buffer);
		delete[] buffer;
	}
	return true;
}

//----------------------------------------------------------------
bool Shader::checkProgramStatus()
{
	GLint linkStatus;
	glGetProgramiv(id, GL_LINK_STATUS, &linkStatus);
	if (linkStatus != GL_TRUE)
	{
		GLint infologlength;
		glGetProgramiv(id, GL_INFO_LOG_LENGTH, &infologlength);
		GLchar* buffer = new GLchar[infologlength];

		GLint bufferSize;
		glGetProgramInfoLog(id, infologlength, &bufferSize, buffer);
		base::Log(buffer);
		delete[] buffer;
	}
	return true;
}

//---------------------------------------------------------
std::string Shader::readShaderCode(const char * filename)
{

	std::ifstream meInput(filename);
	if (!meInput.good())
	{
		base::Log("File failed to load..." + std::string(filename));
		exit(1);
	}
	return std::string(std::istreambuf_iterator<char>(meInput), std::istreambuf_iterator<char>());
}

//--------------------------------------------------
std::string Shader::preprocessShader(const std::string& shaderCode, const std::string& currentFilePath)
{
	std::stringstream output;
	std::istringstream stream(shaderCode);
	std::string line;
	std::filesystem::path currentDir = std::filesystem::path(currentFilePath).parent_path();

	while (std::getline(stream, line)) {
		if (line.find("#include") == 0) {
			size_t start = line.find("\"") + 1;
			size_t end = line.find("\"", start);
			std::string includeFile = line.substr(start, end - start);
			std::filesystem::path includePath = currentDir / includeFile;

			m_shaderDependencies.insert(includePath.string());

			std::ifstream includeStream(includePath);
			if (!includeStream.good()) {
				throw std::runtime_error("Failed to open include file: " + includePath.string());
			}

			std::string includedCode((std::istreambuf_iterator<char>(includeStream)), {});
			output << preprocessShader(includedCode, includePath.string()) << "\n";
		}
		else {
			output << line << "\n";
		}
	}
	return output.str();
}

//--------------------------------------------------------------------------------
GLuint Shader::compileShader(const char* shaderPath, GLenum shaderType)
{
	GLuint shader = glCreateShader(shaderType);
	std::string rawCode = readShaderCode(shaderPath);
	std::string shaderCode = preprocessShader(rawCode, shaderPath);
	const char* adapter = shaderCode.c_str();

	glShaderSource(shader, 1, &adapter, nullptr);
	glCompileShader(shader);

	if (!checkShaderStatus(shader))
	{
		glDeleteShader(shader);
		return 0;  // Return 0 to indicate failure
	}
	return shader;
}

//-----------------------------------------------------------------------
bool Shader::reloadIfNeeded(const std::vector<std::pair<const char*, GLenum>>& shaderList)
{
	static auto lastCheck = std::chrono::steady_clock::now();
	constexpr auto checkInterval = std::chrono::milliseconds(1000);  // every 1.0 seconds

	auto now = std::chrono::steady_clock::now();
	if (now - lastCheck < checkInterval)
		return false;  // Skip check this frame

	lastCheck = now;

	static std::unordered_map<std::string, std::filesystem::file_time_type> lastWriteTimes;
	static bool firstRun = true;
	bool needsReload = false;

	m_shaderDependencies.clear();  // Clear from previous preprocess
	for (const auto& [path, type] : shaderList) {
		m_shaderDependencies.insert(std::string(path));  // <-- track the main file too
		std::string code = readShaderCode(path);
		preprocessShader(code, path);  // Collect all includes
	}

	for (const std::string& file : m_shaderDependencies) {
		auto currentTime = std::filesystem::last_write_time(file);

		// On first run: just store the time
		if (firstRun) {
			lastWriteTimes[file] = currentTime;
		}
		else {
			if (lastWriteTimes[file] != currentTime) {
				lastWriteTimes[file] = currentTime;
				needsReload = true;
			}
		}
	}

	firstRun = false;

	if (!needsReload) return false;

	installShaders(shaderList, false); // recompile entire shader program
	base::Log("Hot reloaded shaders and includes.");
	return true;
}

//--------------------------------------------------
Shader::~Shader()
{
	glDeleteShader(vertexShaderID);
	glDeleteShader(geometryShaderID);
	glDeleteShader(tessellation1ShaderID);
	glDeleteShader(tessellation2ShaderID);
	glDeleteShader(fragmentShaderID);
	glDeleteShader(computeShaderID);
	glDeleteProgram(id);
}

//---------------------------------------------------
void	Shader::GetUniformInfoFromShader()
{
	if (m_uniforms.empty())
	{
		GLint numuniforms = 0;
		glGetProgramInterfaceiv(id, GL_UNIFORM, GL_ACTIVE_RESOURCES, &numuniforms);

		base::Log("Active uniforms " + id);
		GLenum properties[] = { GL_NAME_LENGTH, GL_TYPE, GL_LOCATION, GL_BLOCK_INDEX };
		for (int i = 0; i < numuniforms; ++i)
		{
			GLint results[4];
			glGetProgramResourceiv(id, GL_UNIFORM, i, 4, properties, 4, NULL, results);
			if (results[3] != -1)
				continue;
			GLint nameBufSize = results[0] + 1;
			char* name = new char[nameBufSize];
			glGetProgramResourceName(id, GL_UNIFORM, i, nameBufSize, NULL, name);
			m_uniforms.push_back(Uniform{ std::string(name),results[2],results[1],{} });
			delete[] name;
			base::Log(m_uniforms.back().name + " " + std::to_string(m_uniforms.back().type) + " " + std::to_string(m_uniforms.back().location));
		}
	}
}

//---------------------------------------------------
void	Shader::GetUniformDataFromShader()
{
	for (auto& uniform : m_uniforms)
	{
		switch (uniform.type)
		{
			case GL_BOOL:
			{
				GLint res;
				glGetUniformiv(id, uniform.location, &res);
				uniform.data = static_cast<bool>(res);
			}
			break;
			case GL_INT:
			{
				GLint res;
				glGetUniformiv(id, uniform.location, &res);
				uniform.data = static_cast<int32_t>(res);
			}
			break;
			case GL_UNSIGNED_INT:
			{
				GLuint res;
				glGetUniformuiv(id, uniform.location, &res);
				uniform.data = static_cast<size_t>(res);
			}
			break;
			case GL_FLOAT:
			{
				GLfloat res;
				glGetUniformfv(id,uniform.location,&res);
				uniform.data = static_cast<float>(res);
			}
			break;
			case GL_FLOAT_VEC2:
			{
				glm::vec2 res;
				glGetUniformfv(id, uniform.location, &res[0]);
				uniform.data = res;
			}
			break;
			case GL_INT_VEC2:
			{
				glm::ivec2 res;
				glGetUniformiv(id, uniform.location, &res[0]);
				uniform.data = res;
			}
			break;
			case GL_FLOAT_VEC4:
			{
				glm::vec4 res;
				glGetUniformfv(id, uniform.location, &res[0]);
				uniform.data = res;
			}
			break;
			case GL_FLOAT_MAT2:
			{
				glm::mat2 res;
				glGetUniformfv(id, uniform.location, &res[0][0]);
				uniform.data = res;
			}
			break;
			case GL_FLOAT_MAT4:
			{
				glm::mat4 res;
				glGetUniformfv(id, uniform.location, &res[0][0]);
				uniform.data = res;
			}
			break;
			case GL_SAMPLER_2D:
			{
				GLint res; // just the location of texture slot not texture id
				glGetUniformiv(id, uniform.location, &res);
				uniform.data = static_cast<int32_t>(res);
			}
			break;
			case GL_SAMPLER_CUBE:
			{
				GLint res; // just the location of texture slot not texture id
				glGetUniformiv(id, uniform.location, &res);
				uniform.data = static_cast<int32_t>(res);
			}
			break;
			case GL_SAMPLER_2D_SHADOW:
			{
				GLint res; // just the location of texture slot not texture id
				glGetUniformiv(id, uniform.location, &res);
				uniform.data = static_cast<int32_t>(res);
			}
			break;
			case GL_SAMPLER_2D_ARRAY:
			{
				GLint res; // just the location of texture slot not texture id
				glGetUniformiv(id, uniform.location, &res);
				uniform.data = static_cast<int32_t>(res);
			}
			break;
			default:
			{
				base::Log("there is not uniform handler for type " + std::to_string(uniform.type) + " " + uniform.name);
			}
		}
	}
}

//---------------------------------------------------
bool Shader::SetUniformData(const std::string& _name, const UniformData& _data)
{
	auto it = std::find_if(m_uniforms.begin(), m_uniforms.end(), [_name](const Uniform& u) {return u.name == _name; });
	if (it != m_uniforms.end())
	{
		it->data = _data;
		_SetUniform(*it);
		return true;
	}
	else // try to set it anyway
	{
		if (const int32_t* pval = std::get_if<int32_t>(&_data))
			glUniform1i(glGetUniformLocation(id, _name.c_str()), *pval);
		else if (const size_t* pval = std::get_if<size_t>(&_data))
			glUniform1i(glGetUniformLocation(id, _name.c_str()), *pval);
		else if (const bool* pval = std::get_if<bool>(&_data))
			glUniform1i(glGetUniformLocation(id, _name.c_str()), *pval);
		else if (const float* pval = std::get_if<float>(&_data))
			glUniform1f(glGetUniformLocation(id, _name.c_str()), *pval);
		else if (const glm::vec4* pval = std::get_if<glm::vec4>(&_data))
			glUniform4f(glGetUniformLocation(id, _name.c_str()), (*pval)[0], (*pval)[1], (*pval)[2], (*pval)[3]);
		//@todo other types
	}
	return false;
}

//---------------------------------------------------
void Shader::_SetUniform(const Uniform& _uniform)
{
	glUseProgram(this->id);
	switch (_uniform.type)
	{
		case GL_BOOL:
		{
			bool data = std::get<bool>(_uniform.data);
			glUniform1i(_uniform.location, data);
		}
		break;
		case GL_INT:
		{
			if(const int32_t* pval =  std::get_if<int32_t>(&_uniform.data))
				glUniform1i(_uniform.location, *pval);
			else if(const size_t* pval = std::get_if<size_t>(&_uniform.data))
				glUniform1i(_uniform.location, *pval);
		}
		break;
		case GL_UNSIGNED_INT:
		{
			if (const int32_t* pval = std::get_if<int32_t>(&_uniform.data))
				glUniform1i(_uniform.location, *pval);
			else if (const size_t* pval = std::get_if<size_t>(&_uniform.data))
				glUniform1i(_uniform.location, *pval);
		}
		break;
		case GL_FLOAT:
		{
			float data = std::get<float>(_uniform.data);
			glUniform1f(_uniform.location, data);
		}
		break;
		case GL_FLOAT_VEC2:
		{
			glm::vec2 data = std::get<glm::vec2>(_uniform.data);
			glUniform2f(_uniform.location, data[0], data[1]);
		}
		break;
		case GL_INT_VEC2:
		{
			glm::ivec2 data = std::get<glm::vec2>(_uniform.data);
			glUniform2i(_uniform.location, data[0], data[1]);
		}
		break;
		case GL_FLOAT_VEC4:
		{
			glm::vec4 data = std::get<glm::vec4>(_uniform.data);
			//glUniform4f(_uniform.location, data[0], data[1], data[2], data[3]);
			glUniform4fv(_uniform.location, 1, &data[0]);
		}
		break;
		case GL_FLOAT_MAT2:
		{
			glm::mat2 data = std::get<glm::mat2>(_uniform.data);
			glUniformMatrix2fv(_uniform.location, 1, GL_FALSE, &data[0][0]);
		}
		break;
		case GL_FLOAT_MAT4:
		{
			glm::mat4 data = std::get<glm::mat4>(_uniform.data);
			glUniformMatrix4fv(_uniform.location, 1, GL_FALSE, &data[0][0]);
		}
		break;
		case GL_SAMPLER_2D:
		{

		}
		break;
		case GL_SAMPLER_CUBE:
		{

		}
		break;
		case GL_SAMPLER_2D_SHADOW:
		{

		}
		break;
		default:
		{
			base::Log("there is not uniform handler for type " + std::to_string(_uniform.type) + " " + _uniform.name);
		}
	}
}		
