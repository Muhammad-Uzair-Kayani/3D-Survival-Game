-- Visual Studio is the build/run frontend. Godot's own SCons build compiles
-- the engine and built-in game module; no CMake or GDExtension is involved.
local root = path.getabsolute(".")
local wrapper = path.translate(path.join(root, "BuildFromVS.bat"), "\\")
local vcvars = '"$(VCInstallDir)Auxiliary\\Build\\vcvars64.bat"'
local command = 'call "' .. wrapper .. '" --vcvars ' .. vcvars .. ' --jobs 1'

workspace "LastLightGodot"
    configurations { "Debug", "Release" }
    architecture "x86_64"
    location ("build/premake/" .. (_ACTION or "projects"))
    startproject "LastLight"
    language "C++"
    cppdialect "C++17"

project "LastLight"
    kind "Makefile"
    targetdir (path.join(root, "dist"))
    targetname "LastLight"
    targetextension ".exe"
    files {
        "modules/**.cpp", "modules/**.h", "modules/**.py", "modules/**/SCsub",
        "scripts/**.py", "python/**.py", "game/content/**.json",
        "game/project.godot", "game/main.tscn", "godot.lock.json", "*.bat"
    }
    includedirs { path.join(root, "vendor/godot"), path.join(root, "modules/last_light") }
    defines { "WINDOWS_ENABLED", "GLES3_ENABLED" }
    debugcommand (path.join(root, "dist/LastLight.exe"))
    debugdir (path.join(root, "dist"))
    buildoutputs { path.join(root, "dist/LastLight.exe") }

    filter "configurations:Debug"
        buildcommands { command .. " --debug" }
        rebuildcommands { command .. " --debug --rebuild" }
        cleancommands { command .. " --debug --clean" }
    filter "configurations:Release"
        buildcommands { command }
        rebuildcommands { command .. " --rebuild" }
        cleancommands { command .. " --clean" }
    filter {}
