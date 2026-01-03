
add_executable (vulkan 
  main.cpp
  vk_types.h
  vk_initializers.cpp
  vk_initializers.h
  vk_images.h
  vk_images.cpp 
  vk_descriptors.h
  vk_descriptors.cpp
  vk_pipelines.h
  vk_pipelines.cpp
  vk_engine.h
  vk_engine.cpp
  vk_loader.h
  vk_loader.cpp
  camera.cpp
  camera.h
  scene/directionalLight.h
  scene/directionalLight.cpp
  scene/robotArm.h
  scene/robotArm.cpp
)

set_property(TARGET vulkan PROPERTY CXX_STANDARD 20)
target_compile_definitions(vulkan PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_include_directories(vulkan PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(vulkan SYSTEM PRIVATE
  "${CMAKE_SOURCE_DIR}/third_party/vma"
  "${CMAKE_SOURCE_DIR}/third_party/fastgltf"
  "${CMAKE_SOURCE_DIR}/third_party/imgui"
  "${CMAKE_SOURCE_DIR}/third_party/glm"
  "${CMAKE_SOURCE_DIR}/third_party/SDL"
)
target_compile_definitions(vulkan PRIVATE SHADERS_DIR="${CMAKE_SOURCE_DIR}/shaders")
target_compile_definitions(vulkan PRIVATE
    $<$<CONFIG:Debug>:ENABLE_VALIDATION_LAYERS=1>
    $<$<CONFIG:Release>:ENABLE_VALIDATION_LAYERS=0>
)

target_link_libraries(vulkan PUBLIC vma glm Vulkan::Vulkan fmt::fmt stb_image SDL2::SDL2 vkbootstrap imgui fastgltf::fastgltf assimp::assimp)

target_precompile_headers(vulkan PUBLIC <optional> <vector> <memory> <string> <vector> <unordered_map> <glm/mat4x4.hpp>  <glm/vec4.hpp> <vulkan/vulkan.h>)

if(WIN32)
  add_custom_command(TARGET vulkan POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:vulkan> $<TARGET_FILE_DIR:vulkan>
    COMMAND_EXPAND_LISTS
    )
endif()