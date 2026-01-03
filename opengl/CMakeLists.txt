cmake_minimum_required(VERSION 3.12)
project(opengl LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(assimp REQUIRED)
find_package(glfw3 REQUIRED)

set(GL_DEPS "${CMAKE_SOURCE_DIR}/dependencies")

set(IMGUI_SOURCES
    ${GL_DEPS}/imgui/imgui.cpp
    ${GL_DEPS}/imgui/imgui_draw.cpp
    ${GL_DEPS}/imgui/imgui_tables.cpp
    ${GL_DEPS}/imgui/imgui_widgets.cpp
    ${GL_DEPS}/imgui/imgui_impl_glfw.cpp
    ${GL_DEPS}/imgui/imgui_impl_opengl3.cpp
)

file(GLOB_RECURSE APP_SOURCES
     CONFIGURE_DEPENDS
     "${CMAKE_SOURCE_DIR}/src/*.cpp"
     "${CMAKE_SOURCE_DIR}/src/*/*.cpp"
)

file(GLOB_RECURSE APP_HEADERS
     CONFIGURE_DEPENDS
     "${CMAKE_SOURCE_DIR}/include/*.h"
     "${CMAKE_SOURCE_DIR}/include/*/*.h"
)

add_executable(opengl
    ${APP_SOURCES}
    ${GL_DEPS}/glad/src/glad.c
    ${GL_DEPS}/stb_image/src/stb_image.cpp
    ${IMGUI_SOURCES}
)

target_include_directories(opengl PRIVATE
    ${GL_DEPS}/glad/include
    ${GL_DEPS}/glm
    ${GL_DEPS}
    ${GL_DEPS}/stb_image/include
    ${GL_DEPS}/imgui
    ${CMAKE_SOURCE_DIR}/include
    ${ASSIMP_INCLUDE_DIRS}
)

target_compile_definitions(opengl PRIVATE ROOT_PATH="${CMAKE_SOURCE_DIR}")

target_compile_options(opengl PRIVATE -Wall -Wextra -Wpedantic)

if(APPLE)
    target_link_libraries(opengl PRIVATE
        glfw
        assimp::assimp
        "-framework OpenGL"
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
    )
elseif(UNIX AND NOT APPLE)
    target_link_libraries(opengl PRIVATE
        glfw
        assimp
        GL
        X11
        pthread
        Xrandr
        Xi
        dl
    )
endif()