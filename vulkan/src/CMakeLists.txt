
add_executable(vulkan
  main.cpp
  vk_engine.cpp
  
  scene/camera.cpp
  scene/directionalLight.cpp
  scene/scene.cpp
  
  vk_utils/vk_initializers.cpp
  vk_utils/vk_images.cpp
  vk_utils/vk_descriptors.cpp
  vk_utils/vk_pipelines.cpp
  vk_utils/vk_loader.cpp
  vk_utils/vk_context.cpp
  vk_utils/vk_swapchain.cpp
  vk_utils/vk_resources.cpp
  vk_utils/vk_material.cpp
)

set_property(TARGET vulkan PROPERTY CXX_STANDARD 20)
target_compile_definitions(vulkan PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)

target_include_directories(vulkan PUBLIC 
  "${CMAKE_SOURCE_DIR}/include"
  "${CMAKE_SOURCE_DIR}/include/scene"
  "${CMAKE_SOURCE_DIR}/include/vk_utils"
  "${CMAKE_CURRENT_SOURCE_DIR}"
)
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

target_precompile_headers(vulkan PUBLIC <optional> <vector> <memory> <string> <vector> <unordered_map> <glm/mat4x4.hpp> <glm/vec4.hpp> <vulkan/vulkan.h>)