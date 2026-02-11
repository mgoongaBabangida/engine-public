// opengl_assets.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "opengl_assets.h"

static std::thread::id g_gl_thread_id;

void SetOpenGLContextThreadId(const std::thread::id& _id)
{
  g_gl_thread_id = _id;
}

std::thread::id GetOpenGLContextThreadId()
{
  return g_gl_thread_id;
}


