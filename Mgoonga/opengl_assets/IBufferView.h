#pragma once

#include <stdexcept>
#include "Texture.h"

//-----------------------------------------------------------------------------
class IBufferView
{
public:
	virtual ~IBufferView() = default;

	virtual const char* Name() const = 0;

	virtual void Init(unsigned w, unsigned h) = 0;
	virtual void BindForWriting() = 0;
	virtual void BindForReading(GLenum slot) = 0;

	virtual Texture     GetTexture() const = 0;
	virtual GLuint      ID() const = 0;
	virtual glm::ivec2  Size() const = 0;

	// Optional; default throws if not supported
	virtual GLuint RboID() const { throw std::runtime_error("RboID() not supported by this view"); }
};

class LambdaBufferView final : public IBufferView
{
public:
	const char* m_name = "UNNAMED";

	std::function<void(unsigned, unsigned)> m_init;
	std::function<void()>                  m_write;
	std::function<void(GLenum)>            m_read;

	std::function<Texture()>               m_tex;
	std::function<GLuint()>                m_id;
	std::function<glm::ivec2()>            m_size;

	std::function<GLuint()>                m_rbo; // optional

	const char* Name() const override { return m_name; }

	void Init(unsigned w, unsigned h) override
	{
		if (!m_init) throw std::runtime_error(std::string("Init missing for view: ") + m_name);
		m_init(w, h);
	}

	void BindForWriting() override
	{
		if (!m_write) throw std::runtime_error(std::string("Write missing for view: ") + m_name);
		m_write();
	}

	void BindForReading(GLenum slot) override
	{
		if (!m_read) throw std::runtime_error(std::string("Read missing for view: ") + m_name);
		m_read(slot);
	}

	Texture GetTexture() const override
	{
		if (!m_tex) throw std::runtime_error(std::string("Texture missing for view: ") + m_name);
		return m_tex();
	}

	GLuint ID() const override
	{
		if (!m_id) throw std::runtime_error(std::string("ID missing for view: ") + m_name);
		return m_id();
	}

	glm::ivec2 Size() const override
	{
		if (!m_size) throw std::runtime_error(std::string("Size missing for view: ") + m_name);
		return m_size();
	}

	GLuint RboID() const override
	{
		if (!m_rbo) return IBufferView::RboID();
		return m_rbo();
	}
};
