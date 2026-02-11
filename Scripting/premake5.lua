workspace "Scripting"
	architecture "x64"
	configurations { "Debug", "Release" }
	startproject "Scripting"

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

project "Scripting"
	location "Scripting"
	kind "SharedLib"
	language "C++"
	cppdialect "C++17"
	staticruntime "off"
	
	targetdir ("bin/" .. outputdir .. "/Scripting/")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	postbuildcommands {
   		-- Copy the DLL to Game/Scripts after build
   		'{COPY} "%{cfg.buildtarget.abspath}" "../../Mgoonga/scripts/"'
	}

	files { "%{prj.name}/**.h", "%{prj.name}/**.cpp" }

	includedirs {
		"../Mgoonga",
		"../../third_party",
		"../../third_party/openal/include"
	}

	libdirs {
		"../Mgoonga/bin/Debug-windows-x86_64",
		"../../third_party/libs/Win64",
		"../../third_party/glew-2.1.0/lib/Release/x64",
		"../../third_party/SDL/lib/x64/",
		"../../third_party/assimp-3.1.1/build/code/Debug",
		"../../third_party/IL",
		"../../third_party/freetype-2.11.0/objs",
		"../../third_party/openal/libs/Win64"
	}

	links {
		"base",
		"math",
		"yaml-cpp",
		"opengl_assets",
		"game_assets",
		"sdl_assets",
		"tcp_lib",
		"assimpd",
		"opengl32",
		"glew32",
		"DevIL",
		"ILU",
		"ILUT",
		"OpenAl32",
		"SDL2",
		"SDL2main",
		"freetype.lib"
	}

	filter "configurations:Debug"
		defines { "DEBUG", "_DEBUG", "_WINDOWS", "_USRDLL", "BASE_EXPORTS" }
		symbols "On"

	filter "configurations:Release"
		defines { "NDEBUG", "_WINDOWS", "_USRDLL", "BASE_EXPORTS" }
		optimize "On"
