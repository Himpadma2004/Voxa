cmake_minimum_required(VERSION 3.20)

# Automatically use vcpkg if VCPKG_ROOT is defined
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "")
endif()

project(VOXA_Simulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL3 CONFIG REQUIRED)

add_executable(VOXA
    src/main.cpp
)

target_link_libraries(VOXA PRIVATE SDL3::SDL3)