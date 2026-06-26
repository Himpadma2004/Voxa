cmake_minimum_required(VERSION 3.20)

# Automatically use vcpkg if VCPKG_ROOT is defined, or fall back to a common local install path.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VCPKG_ROOT})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "")
    elseif(EXISTS "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "")
    endif()
endif()

project(VOXA_Simulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL3 CONFIG REQUIRED
    HINTS
        "C:/vcpkg/installed/x64-windows/share/sdl3"
        "C:/vcpkg/packages/sdl3_x64-windows/share/sdl3"
)

add_executable(VOXA
    src/main.cpp
    src/core/Application.cpp
    src/graphics/Colors.cpp
    src/graphics/Fonts.cpp
    src/graphics/Icons.cpp
    src/graphics/Renderer.cpp
    src/screens/BootScreen.cpp
    src/screens/HomeScreen.cpp
    src/screens/IdeasScreen.cpp
    src/screens/OthersScreen.cpp
    src/screens/QuestionsScreen.cpp
    src/screens/RecordScreen.cpp
    src/screens/ReminderScreen.cpp
    src/screens/ScreenCommon.cpp
    src/screens/SearchScreen.cpp
    src/screens/SettingsScreen.cpp
    src/screens/SyncStatusScreen.cpp
    src/widgets/Button.cpp
    src/widgets/Card.cpp
    src/widgets/CircleButton.CPP
    src/widgets/FloatingButton.cpp
    src/widgets/ListTile.cpp
    src/widgets/ProgressBar.cpp
    src/widgets/SearchBar.cpp
    src/widgets/StatusBar.CPP
)

target_link_libraries(VOXA PRIVATE SDL3::SDL3)
