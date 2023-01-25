workspace "VulkanTutorial"
    architecture "x64"
    configurations { "Debug", "Release" }

include "dependencies/GLFW"

VULKAN_SDK_PATH = os.getenv("VK_SDK_PATH")

if os.istarget "windows" then
    GLSLC = VULKAN_SDK_PATH .. "/Bin/glslc.exe"
	CMAKE = ('"'..os.capture("where cmake")..'"') or path.join(os.getenv("CMAKE_PATH"), "cmake")
	-- print(CMAKE)
	OBJCOPY = "%{wks.location}/vendor/bin/binutils-x64/objcopy.exe"
    UTF_BOM_REMOVE = "%{wks.location}/vendor/bin/utf-bom-utils/Debug/bom_remove.exe"
else
	CMAKE = "cmake"
	OBJCOPY = "objcopy"
end

project "VulkanTest"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    location "VulkanTest"

    targetdir "%{wks.location}/build/bin/%{cfg.buildcfg}-%{cfg.architecture}/%{prj.name}"
    objdir    "%{wks.location}/build/obj/%{cfg.buildcfg}-%{cfg.architecture}/%{prj.name}"

    files {
        "%{prj.location}/src/**.h",
        "%{prj.location}/src/**.hpp",
        "%{prj.location}/src/**.cpp",
        
        "%{prj.location}/shaders/**.vert",
        "%{prj.location}/shaders/**.frag",
    }

    includedirs {
        "%{wks.location}/dependencies/glm",
        "%{wks.location}/dependencies/GLFW/include",
        "%{VULKAN_SDK_PATH}/Include",
    }

    libdirs {
        "%{VULKAN_SDK_PATH}/Lib",
    }

    links {
        "GLFW",
        "vulkan-1",
    }

    defines {
        "VKTUT_USE_GLFW",
    }

    filter "configurations:Debug"
        defines { "DEBUG", "VKTUT_VK_ENABLE_VALIDATION" }
        symbols "On"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"

    filter "files:**.vert or **.frag"
        buildmessage "Compiling shader %{file.name}"
        buildcommands {
            "%{GLSLC} %{file.relpath} -o %{cfg.objdir}/%{file.name}.spv",

        --    "%{UTF_BOM_REMOVE} %{file.relpath}",
		--	"%{OBJCOPY} --input-target binary --output-target pe-x86-64 --binary-architecture i386 %{cfg.objdir}/%{file.name}.spv %{cfg.objdir}/%{file.name}.obj",
		--	"{ECHO} Created %{cfg.objdir}/%{file.basename}.obj from %{file.relpath}"
        }
        buildoutputs {
            "%{cfg.objdir}/%{file.name}.spv",
        --    "%{cfg.objdir}/%{file.name}.obj"
        }