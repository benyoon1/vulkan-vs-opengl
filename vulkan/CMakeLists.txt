# CMakeList.txt : CMake project for vulkan_guide, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.10)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project ("vulkan_guide")

  
find_package(Vulkan REQUIRED)
find_package(assimp CONFIG REQUIRED)

add_subdirectory(third_party)
if (TARGET SDL2)
    target_compile_options(SDL2 PRIVATE -Wno-deprecated-declarations)
endif()
if (TARGET fastgltf)
    target_compile_options(fastgltf PRIVATE -Wno-deprecated-declarations)
endif()

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/build/$<CONFIG>")
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/build/$<CONFIG>")

add_subdirectory(src)

find_program(GLSL_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)

file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/shaders/*.frag"
    "${PROJECT_SOURCE_DIR}/shaders/*.vert"
    "${PROJECT_SOURCE_DIR}/shaders/*.comp"
    )

foreach(GLSL ${GLSL_SOURCE_FILES})
  message(STATUS "BUILDING SHADER")
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/${FILE_NAME}.spv")
  message(STATUS ${GLSL})
  message(STATUS COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV})
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
    Shaders ALL
    DEPENDS ${SPIRV_BINARY_FILES}
    )
