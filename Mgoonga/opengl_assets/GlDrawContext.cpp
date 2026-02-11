#include "stdafx.h"
#include <glew-2.1.0\include\GL\glew.h>
#include "GlDrawContext.h"

//-------------------------------------------------------------------------------------------------------------------------
void eGlDrawContext::DrawElements(GLenum _mode, GLsizei _count, GLenum _type, const void* _indices, const std::string& _tag)
{
	glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, _tag.c_str());
	glDrawElements(_mode, _count, _type, _indices);
	if (m_debuging)
	{
		_Debug(_tag, _count/3);
	}
	glPopDebugGroup();
}

//-------------------------------------------------------------------------------------------------------------------------
void eGlDrawContext::DrawElementsInstanced(GLenum _mode, GLsizei _count, GLenum _type, const void* _indices, GLsizei _instances, const std::string& _tag)
{
	glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, _tag.c_str());
	glDrawElementsInstanced(_mode, _count, _type, _indices, _instances);
	if (m_debuging)
	{
		_Debug(_tag, _count / 3);
	}
	glPopDebugGroup();
}

//-------------------------------------------------------------------------------------------------------------------------
void eGlDrawContext::DrawArrays(GLenum _mode, GLint _first, GLsizei _count, const std::string& _tag)
{
	glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, _tag.c_str());
	glDrawArrays(_mode, _first, _count);
	if (m_debuging)
	{
		_Debug(_tag, _count / 3);
	}
	glPopDebugGroup();
}