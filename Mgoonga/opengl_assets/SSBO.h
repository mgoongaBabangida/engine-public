#pragma once
#include "stdafx.h"
#include <cassert>
#include <cstring>
#include<iostream>
#include <glm/glm/glm.hpp>

template<typename T>
class SSBO
{
public:
  SSBO(GLuint binding, size_t count, bool persistent = true)
    : m_binding(binding), m_count(count), m_persistent(persistent)
  {
    Init();
  }

  ~SSBO() {
   Cleanup();
  }

  SSBO(const SSBO&) = delete;
  SSBO& operator=(const SSBO&) = delete;

  SSBO(SSBO&& other) noexcept
    : m_buffer(other.m_buffer),
    m_binding(other.m_binding),
    m_count(other.m_count),
    m_persistent(other.m_persistent),
    m_mappedPtr(other.m_mappedPtr)
  {
    other.m_buffer = 0;
    other.m_mappedPtr = nullptr;
  }

  SSBO& operator=(SSBO&& other) noexcept
  {
    if (this != &other)
    {
      Cleanup();

      m_buffer = other.m_buffer;
      m_binding = other.m_binding;
      m_count = other.m_count;
      m_persistent = other.m_persistent;
      m_mappedPtr = other.m_mappedPtr;

      other.m_buffer = 0;
      other.m_mappedPtr = nullptr;
    }
    return *this;
  }

  void* GetMappedPtr() { return m_mappedPtr; }
  T* GetTypedPtr() { return static_cast<T*>(m_mappedPtr); }

  //------------------------------------------------------------------------------------------
  void UploadData(const std::vector<T>& data)
  {
    assert(data.size() <= m_count);
    if (m_persistent && m_mappedPtr) {
      std::memcpy(m_mappedPtr, data.data(), data.size() * sizeof(T));
    }
    else {
      glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_buffer);
      // orphan (allocate fresh storage; driver can hand you new memory)
      glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(T) * m_count, nullptr, GL_DYNAMIC_DRAW);
      // then upload actual bytes
      glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, data.size() * sizeof(T), data.data());
      glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
  }

  //-----------------------------------------------------------------------------------------
  void Bind()
  {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, m_binding, m_buffer);
  }

private:
  GLuint m_buffer = 0;
  GLuint m_binding;
  size_t m_count;
  bool m_persistent;
  void* m_mappedPtr = nullptr;

  //---------------------------------------------------------------------------------------------
  void Init()
  {
    const GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
    const size_t bufferSize = sizeof(T) * m_count;

    glGenBuffers(1, &m_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_buffer);

    if (m_persistent) {
      glBufferStorage(GL_SHADER_STORAGE_BUFFER, bufferSize, nullptr, flags);
      m_mappedPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, bufferSize, flags);
    }
    else {
      glBufferData(GL_SHADER_STORAGE_BUFFER, bufferSize, nullptr, GL_DYNAMIC_DRAW);
    }

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, m_binding, m_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
  }

  //--------------------------------------------------------------------------------------
  void Cleanup()
  {
    if (m_persistent && m_mappedPtr) {
      glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_buffer);
      glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }
    if (m_buffer != 0) {
      glDeleteBuffers(1, &m_buffer);
      m_buffer = 0;
    }
    m_mappedPtr = nullptr;
  }
};
