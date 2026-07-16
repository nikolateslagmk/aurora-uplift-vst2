cmake_minimum_required(VERSION 3.22)

project(AuroraUpliftVST2 VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Evita dependência do Microsoft Visual C++ Redistributable na máquina do usuário.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(FetchContent)
FetchContent_Declare(
    DPF
    GIT_REPOSITORY https://github.com/DISTRHO/DPF.git
    GIT_TAG main
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(DPF)

dpf_add_plugin(AuroraUplift
    TARGETS vst2
    FILES_DSP
        Source/AuroraUpliftPlugin.cpp
)

target_include_directories(AuroraUplift PUBLIC Source)

if(MSVC)
    target_compile_options(AuroraUplift PUBLIC /W4 /permissive- /EHsc)
else()
    target_compile_options(AuroraUplift PUBLIC -Wall -Wextra -Wpedantic)
endif()
