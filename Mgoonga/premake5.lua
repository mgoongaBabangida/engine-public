-- premake5.lua — VS2022 / C++20 with stdafx PCH enabled

-- ===== knobs you may change =====
local OUTPUTDIR = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

-- Assimp paths/names (set to your tree)
local ASSIMP_ROOT = "../../third_party/assimp-3.1.1"
-- Typical 5.4.3 MSVC names (vc143). If yours are different, change these:
local ASSIMP_DEBUG_LIB   = "assimpd"  -- or "assimpd" "assimp-vc143-mtd"
local ASSIMP_RELEASE_LIB = "assimpd"   -- or "assimp" "assimp-vc143-mt"

-- ===== workspace/toolchain =====
workspace "Mgoonga"
    architecture "x64"
    configurations { "Debug", "Release" }
    startproject "Mgoonga"
    systemversion "latest"
    toolset "v143"

outputdir = OUTPUTDIR

os.execute("check_third_party.bat")

-- ===== common dirs =====
local TP_ROOT        = "../../third_party"
local GLEW_LIBDIR    = TP_ROOT .. "/glew-2.1.0/lib/Release/x64"
local SDL_LIBDIR     = TP_ROOT .. "/SDL/lib/x64"
local DEVIL_LIBDIR   = TP_ROOT .. "/IL"
local FT_ROOT        = TP_ROOT .. "/freetype-2.11.0"
local FT_INCLUDES    = FT_ROOT .. "/include"
local FT_OBJDIR      = FT_ROOT .. "/objs"
local OPENAL_INCLUDE = TP_ROOT .. "/openal/include"
local OPENAL_LIBDIR  = TP_ROOT .. "/openal/libs/Win64"

-- ===== helpers =====
local function common_msvc_flags()
    language "C++"
    cppdialect "C++20"
    flags { "MultiProcessorCompile" }
    buildoptions { "/Zc:__cplusplus", "/Zc:preprocessor", "/permissive-" }
    defines { "_CRT_SECURE_NO_WARNINGS" }
end

local function common_includedirs(extra)
    includedirs {
        "./",
        TP_ROOT,
        FT_INCLUDES,
        OPENAL_INCLUDE,
        "yaml-cpp/include",
        ASSIMP_ROOT .. "/include"
    }
    if extra then includedirs(extra) end
end

local function common_libdirs()
    libdirs {
        TP_ROOT .. "/libs/Win64",
        GLEW_LIBDIR,
        SDL_LIBDIR,
        DEVIL_LIBDIR,
        FT_OBJDIR,
        OPENAL_LIBDIR,
        -- Assimp common CMake layouts:
        ASSIMP_ROOT .. "/build/lib/Debug",
        ASSIMP_ROOT .. "/build/lib/Release",
        ASSIMP_ROOT .. "/build/code/Debug",
        ASSIMP_ROOT .. "/build/code/Release"
    }
end

local function common_links(extra)
    links {
        "opengl32",
        "glew32",
        "DevIL", "ILU", "ILUT",
        "OpenAL32",
        "SDL2", "SDL2main",
        "freetype"
    }
    filter "configurations:Debug"   links { ASSIMP_DEBUG_LIB }
    filter "configurations:Release" links { ASSIMP_RELEASE_LIB }
    filter {}
    if extra then links(extra) end
end

-- Precompiled header: stdafx.h / stdafx.cpp per project
local function use_stdafx()
    pchheader "stdafx.h"
    pchsource ("%{prj.name}/stdafx.cpp")
    forceincludes { "stdafx.h" }   -- ensure it’s first for MSVC
end

-- ===== projects =====

------------------------------------------------------------------------------------------------
project "base"
    location "base"
    kind "SharedLib"
    common_msvc_flags()
    targetdir ("bin/" .. outputdir .. "/")
    objdir    ("bin-int/" .. outputdir .. "/%{prj.name}")
    files { "%{prj.name}/**.h", "%{prj.name}/**.cpp" }
    common_includedirs()
    use_stdafx()

    filter "configurations:Debug"
        defines { "DEBUG","_DEBUG","_WINDOWS","_USRDLL","BASE_EXPORTS" }
        symbols "On"
    filter "configurations:Release"
        defines { "NDEBUG","_WINDOWS","_USRDLL","BASE_EXPORTS" }
        optimize "On"
    filter {}

------------------------------------------------------------------------------------------------
project "yaml-cpp"
    location "yaml-cpp"
    kind "StaticLib"
    common_msvc_flags()
    targetdir ("bin/" .. outputdir .. "/")
    objdir    ("bin-int/" .. outputdir .. "/%{prj.name}")
    files {
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.cpp",
        "%{prj.name}/include/**.h"
    }
    common_includedirs()
    -- use_stdafx() only if you actually have stdafx in yaml-cpp (usually you don't)
    -- use_stdafx()

    filter "configurations:Debug"
        defines { "DEBUG","_DEBUG","_WINDOWS","_USRDLL","BASE_EXPORTS" }
        symbols "On"
    filter "configurations:Release"
        defines { "NDEBUG","_WINDOWS","_USRDLL","BASE_EXPORTS" }
        optimize "On"
    filter {}

------------------------------------------------------------------------------------------------
project "math"
    location "math"
    kind "SharedLib"
    common_msvc_flags()
    targetdir ("bin/" .. outputdir .. "/")
    objdir    ("bin-int/" .. outputdir .. "/%{prj.name}")
    files { "%{prj.name}/**.h", "%{prj.name}/**.cpp" }
    common_includedirs()
    links { "base" }
    use_stdafx()

    vpaths {
        ["SkeletalAnimation"] = {
            "**/SleletalAnimation.h", "**/SleletalAnimation.cpp",
            "**/Bone.h", "**/Bone.cpp",
            "**/Frame.h",
            "**/IAnimatedModel.h",
            "**/RigAnimator.h", "**/RigAnimator.cpp",
            "**/SkeletalAnimation.h", "**/SkeletalAnimation.cpp",
            "**/AnimationUtils.h", "**/AnimationUtils.cpp"
        }
    }

    filter "configurations:Debug"
        defines { "DEBUG","_DEBUG","_WINDOWS","_USRDLL","BASE_EXPORTS" }
        symbols "On"
    filter "configurations:Release"
        defines { "NDEBUG","_WINDOWS","_USRDLL","BASE_EXPORTS" }
        optimize "On"
    filter {}

------------------------------------------------------------------------------------------------
project "tcp_lib"
    location "tcp_lib"
    kind "SharedLib"
    common_msvc_flags()
    targetdir ("bin/" .. outputdir .. "/")
    objdir    ("bin-int/" .. outputdir .. "/%{prj.name}")
    files { "%{prj.name}/**.h", "%{prj.name}/**.cpp" }
    common_includedirs()
    links { "base" }

    filter "configurations:Debug"
        defines { "DEBUG","_DEBUG","_WINDOWS","_USRDLL","BASE_EXPORTS" }
        symbols "On"
    filter "configurations:Release"
        defines { "NDEBUG","_WINDOWS","_USRDLL","BASE_EXPORTS" }
        optimize "On"
    filter {}

------------------------------------------------------------------------------------------------
project "opengl_assets"
    location "opengl_assets"
    kind "SharedLib"
    common_msvc_flags()
    targetdir ("bin/" .. outputdir .. "/")
    objdir    ("bin-int/" .. outputdir .. "/%{prj.name}")
    files { "%{prj.name}/**.h", "%{prj.name}/**.cpp" }
    common_includedirs()
    common_libdirs()
    links { "base", "math" }
    common_links()
    use_stdafx()

    filter "configurations:Debug"
        defines { "DEBUG","_DEBUG","_WINDOWS","_USRDLL","BASE_EXPORTS" }
        symbols "On"
    filter "configurations:Release"
        defines { "NDEBUG","_WINDOWS","_USRDLL","BASE_EXPORTS" }
        optimize "On"
    filter {}

------------------------------------------------------------------------------------------------
project "sdl_assets"
    location "sdl_assets"
    kind "SharedLib"
    common_msvc_flags()
    targetdir ("bin/" .. outputdir .. "/")
    objdir    ("bin-int/" .. outputdir .. "/%{prj.name}")
    files { "%{prj.name}/**.h", "%{prj.name}/**.cpp" }
    common_includedirs()
    common_libdirs()
    links { "base", "math", "opengl_assets" }
    common_links()
    use_stdafx()

    filter "configurations:Debug"
        defines { "DEBUG","_DEBUG","_WINDOWS","_USRDLL","BASE_EXPORTS" }
        symbols "On"
    filter "configurations:Release"
        defines { "NDEBUG","_WINDOWS","_USRDLL","BASE_EXPORTS" }
        optimize "On"
    filter {}

------------------------------------------------------------------------------------------------
project "ui_lib"
    location "ui_lib"
    kind "SharedLib"
    common_msvc_flags()
    targetdir ("bin/" .. outputdir .. "/")
    objdir    ("bin-int/" .. outputdir .. "/%{prj.name}")
    files { "%{prj.name}/**.h", "%{prj.name}/**.cpp" }
    common_includedirs()
    common_libdirs()
    links { "base","math","yaml-cpp","opengl_assets","sdl_assets","tcp_lib" }
    common_links()

    filter "configurations:Debug"
        defines { "DEBUG","_DEBUG","_WINDOWS","_USRDLL","BASE_EXPORTS" }
        symbols "On"
    filter "configurations:Release"
        defines { "NDEBUG","_WINDOWS","_USRDLL","BASE_EXPORTS" }
        optimize "On"
    filter {}

------------------------------------------------------------------------------------------------
project "game_assets"
    location "game_assets"
    kind "SharedLib"
    common_msvc_flags()
    targetdir ("bin/" .. outputdir .. "/")
    objdir    ("bin-int/" .. outputdir .. "/%{prj.name}")
    files { "%{prj.name}/**.h", "%{prj.name}/**.cpp" }
    common_includedirs()
    common_libdirs()
    links { "base","math","yaml-cpp","opengl_assets","sdl_assets","tcp_lib" }
    common_links()

    filter "configurations:Debug"
        defines { "DEBUG","_DEBUG","_WINDOWS","_USRDLL","BASE_EXPORTS" }
        symbols "On"
    filter "configurations:Release"
        defines { "NDEBUG","_WINDOWS","_USRDLL","BASE_EXPORTS" }
        optimize "On"
    filter {}

------------------------------------------------------------------------------------------------
project "AmericanTreasureGame"
    location "AmericanTreasureGame"
    kind "ConsoleApp"
    common_msvc_flags()
    targetdir ("bin/" .. outputdir .. "/")
    objdir    ("bin-int/" .. outputdir .. "/%{prj.name}")
    files { "%{prj.name}/**.h", "%{prj.name}/**.cpp" }
    common_includedirs()
    common_libdirs()
    links { "base","math","opengl_assets","sdl_assets","tcp_lib","game_assets" }
    common_links()
    use_stdafx()

    filter "configurations:Debug"
        defines { "DEBUG","_DEBUG","_WINDOWS" }
        symbols "On"
    filter "configurations:Release"
        defines { "NDEBUG","_WINDOWS" }
        optimize "On"
    filter {}

------------------------------------------------------------------------------------------------
project "SandBoxGame"
    location "SandBoxGame"
    kind "ConsoleApp"
    common_msvc_flags()
    targetdir ("bin/" .. outputdir .. "/")
    objdir    ("bin-int/" .. outputdir .. "/%{prj.name}")
    files { "%{prj.name}/**.h", "%{prj.name}/**.cpp" }
    common_includedirs()
    common_libdirs()
    links { "base","math","opengl_assets","sdl_assets","game_assets" }
    common_links()
    use_stdafx()

    filter "configurations:Debug"
        defines { "DEBUG","_DEBUG","_WINDOWS" }
        symbols "On"
    filter "configurations:Release"
        defines { "NDEBUG","_WINDOWS" }
        optimize "On"
    filter {}
