#pragma once

#ifdef DLLDIR_EX
#define DLL_UI_LIB __declspec(dllexport)
#else
#define DLL_UI_LIB __declspec(dllimport)
#endif

#pragma warning( disable : 4251)
