cmake_minimum_required(VERSION 3.16)

project(spatial_player VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SPATIAL_PLAYER_STRICT_WARNINGS "Enable strict compiler warnings" ON)

if(SPATIAL_PLAYER_STRICT_WARNINGS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-Wall -Wextra -Wpedantic)
    endif()
endif()

find_package(OpenGL REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(glm REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavcodec
    libavformat
    libavutil
    libswscale
    libswresample
)

add_executable(spatial_player
    src/main.cpp
    src/Camera.cpp
    src/Geometry.cpp
    src/MediaPipeline.cpp
    src/ShaderProgram.cpp
    src/VideoRenderer.cpp
)

target_include_directories(spatial_player PRIVATE include)

target_link_libraries(spatial_player PRIVATE
    OpenGL::GL
    glfw
    glm::glm
    PkgConfig::FFMPEG
)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/shaders DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
