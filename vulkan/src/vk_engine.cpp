
#include "vk_engine.h"

#include "fmt/core.h"
#include "vk_descriptors.h"
#include "vk_images.h"
#include "vk_loader.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <chrono>
#include <random>
#include <vk_initializers.h>
#include <vk_types.h>

#include "VkBootstrap.h"

#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

#include <filesystem>
#include <glm/gtx/transform.hpp>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#ifndef ENABLE_VALIDATION_LAYERS
#define ENABLE_VALIDATION_LAYERS 0
#endif

constexpr bool bUseValidationLayers = ENABLE_VALIDATION_LAYERS;

VulkanEngine* loadedEngine = nullptr;

VulkanEngine& VulkanEngine::Get()
{
    return *loadedEngine;
}

// Resolve shader path relative to the current working directory so the app
// works when launched from either the repo root or the bin directory.
static std::string shader_path(const char* filename)
{
    namespace fs = std::filesystem;
    fs::path from_bin = fs::path("../shaders") / filename; // running from build/<CONFIG>
    if (fs::exists(from_bin))
    {
        return from_bin.string();
    }
    fs::path from_root = fs::path("shaders") / filename; // running from repo root
    if (fs::exists(from_root))
    {
        return from_root.string();
    }
    // Fallback to previous relative path to preserve legacy behavior
    return (fs::path("../../shaders") / filename).string();
}

static std::string asset_path(std::string_view relativePath)
{
    namespace fs = std::filesystem;
    fs::path requested(relativePath);

    if (requested.is_absolute())
    {
        return requested.string();
    }

    auto begins_with_assets = [&]()
    {
        auto it = requested.begin();
        return it != requested.end() && *it == fs::path("assets");
    }();

    std::vector<fs::path> candidates;

    if (begins_with_assets)
    {
        candidates.emplace_back(requested);
        candidates.emplace_back(fs::path("..") / requested);
    }
    else
    {
        candidates.emplace_back(fs::path("assets") / requested);
        candidates.emplace_back(fs::path("../assets") / requested);
        candidates.emplace_back(fs::path("../../assets") / requested);
        candidates.emplace_back(fs::path("../../../assets") / requested);
    }

    // Compatibility fallbacks: try without any prefix in current and parent dirs
    candidates.emplace_back(requested);
    candidates.emplace_back(fs::path("..") / requested);

    for (const auto& candidate : candidates)
    {
        if (!candidate.empty() && fs::exists(candidate))
        {
            return candidate.string();
        }
    }

    // If nothing exists, default to the first candidate to preserve path structure
    return candidates.front().string();
}

void VulkanEngine::init()
{
    // only one engine initialization is allowed with the application.
    assert(loadedEngine == nullptr);
    loadedEngine = this;

    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

    _window = SDL_CreateWindow("Vulkan Renderer", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, _windowExtent.width,
                               _windowExtent.height, window_flags);

    // Hide the OS cursor and capture it to the window so mouse stays inside.
    SDL_ShowCursor(SDL_DISABLE);
    SDL_SetWindowGrab(_window, SDL_TRUE);
    SDL_SetRelativeMouseMode(SDL_TRUE);

    init_vulkan();

    init_swapchain();

    init_commands();

    init_sync_structures();

    init_descriptors();

    init_shadow_resources();

    init_pipelines();

    init_default_data();

    init_renderables();

    init_imgui();

    // everything went fine
    _isInitialized = true;
}

void VulkanEngine::init_default_data()
{
    // rectangle used for debugging shadow map texture
    std::array<Vertex, 6> rect_vertices{};

    rect_vertices[0].position = {0.0f, 1.0f, 0.0f};
    rect_vertices[1].position = {0.0f, 0.0f, 0.0f};
    rect_vertices[2].position = {1.0f, 0.0f, 0.0f};
    rect_vertices[3].position = {0.0f, 1.0f, 0.0f};
    rect_vertices[4].position = {1.0f, 0.0f, 0.0f};
    rect_vertices[5].position = {1.0f, 1.0f, 0.0f};

    for (auto& v : rect_vertices)
    {
        v.color = {1.0f, 1.0f, 1.0f, 1.0f};
    }

    rect_vertices[0].uv_x = 0.0f;
    rect_vertices[0].uv_y = 0.0f;
    rect_vertices[1].uv_x = 0.0f;
    rect_vertices[1].uv_y = 1.0f;
    rect_vertices[2].uv_x = 1.0f;
    rect_vertices[2].uv_y = 1.0f;
    rect_vertices[3].uv_x = 0.0f;
    rect_vertices[3].uv_y = 0.0f;
    rect_vertices[4].uv_x = 1.0f;
    rect_vertices[4].uv_y = 1.0f;
    rect_vertices[5].uv_x = 1.0f;
    rect_vertices[5].uv_y = 0.0f;

    std::array<uint32_t, 6> rect_indices{0, 1, 2, 3, 4, 5};

    _debugRectangle = uploadMesh(rect_indices, rect_vertices);

    // Debug cube geometry (unit cube centered at origin)
    {
        std::array<Vertex, 8> v{};
        const glm::vec3 p[8] = {
            {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f},
            {-0.5f, -0.5f, 0.5f},  {0.5f, -0.5f, 0.5f},  {0.5f, 0.5f, 0.5f},  {-0.5f, 0.5f, 0.5f},
        };
        for (int i = 0; i < 8; ++i)
        {
            v[i].position = p[i];
            v[i].uv_x = 0.0f;
            v[i].uv_y = 0.0f;
            v[i].normal = glm::vec3(0, 1, 0);
            v[i].color = glm::vec4(1, 1, 1, 1);
        }
        // clang-format off
        // 12 triangles (two per face)
        std::array<uint32_t, 36> idx = {
            // -Z
            0, 2, 1,
            0, 3, 2,
            // +Z
            4, 5, 6,
            4, 6, 7,
            // -X
            0, 4, 7,
            0, 7, 3,
            // +X
            1, 2, 6,
            1, 6, 5,
            // -Y
            0, 1, 5,
            0, 5, 4,
            // +Y
            2, 3, 7,
            2, 7, 6,
        };
        // clang-format on

        _debugCube = uploadMesh(idx, v);
    }

    // 3 default textures, white, grey, black. 1 pixel each
    uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
    _whiteImage =
        create_image((void*)&white, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
    _greyImage = create_image((void*)&grey, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
    _blackImage =
        create_image((void*)&black, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    // checkerboard image
    uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
    std::array<uint32_t, 16 * 16> pixels; // for 16x16 checkerboard texture
    for (int x = 0; x < 16; x++)
    {
        for (int y = 0; y < 16; y++)
        {
            pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
        }
    }

    _errorCheckerboardImage =
        create_image(pixels.data(), VkExtent3D{16, 16, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    // Create default samplers with mipmap filtering; anisotropy enabled if supported
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(_chosenGPU, &props);

    VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampl.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampl.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampl.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampl.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampl.minLod = 0.0f;
    sampl.maxLod = VK_LOD_CLAMP_NONE;
    sampl.mipLodBias = 0.0f;

    // Anisotropy: will be enabled at device creation; clamp to device limit here
    sampl.anisotropyEnable = VK_TRUE;
    sampl.maxAnisotropy = props.limits.maxSamplerAnisotropy > 0 ? props.limits.maxSamplerAnisotropy : 1.0f;

    // Nearest sampler
    sampl.magFilter = VK_FILTER_NEAREST;
    sampl.minFilter = VK_FILTER_NEAREST;
    vkCreateSampler(_device, &sampl, nullptr, &_defaultSamplerNearest);

    // Linear sampler
    sampl.magFilter = VK_FILTER_LINEAR;
    sampl.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(_device, &sampl, nullptr, &_defaultSamplerLinear);

    TextureID defaultTextureId = texCache.AddTexture(_whiteImage.imageView, _defaultSamplerLinear);
    texCache.set_fallback(defaultTextureId);
}

void VulkanEngine::cleanup()
{
    if (_isInitialized)
    {

        // make sure the gpu has stopped doing its things
        vkDeviceWaitIdle(_device);

        loadedAssets.clear();

        // Destroy engine-owned GPU resources that might not be tied to deletion
        // queues
        if (_debugRectangle.indexBuffer.buffer != VK_NULL_HANDLE)
        {
            destroy_buffer(_debugRectangle.indexBuffer);
            _debugRectangle.indexBuffer = {};
        }
        if (_debugRectangle.vertexBuffer.buffer != VK_NULL_HANDLE)
        {
            destroy_buffer(_debugRectangle.vertexBuffer);
            _debugRectangle.vertexBuffer = {};
            _debugRectangle.vertexBufferAddress = 0;
        }

        if (_debugCube.indexBuffer.buffer != VK_NULL_HANDLE)
        {
            destroy_buffer(_debugCube.indexBuffer);
            _debugCube.indexBuffer = {};
        }
        if (_debugCube.vertexBuffer.buffer != VK_NULL_HANDLE)
        {
            destroy_buffer(_debugCube.vertexBuffer);
            _debugCube.vertexBuffer = {};
            _debugCube.vertexBufferAddress = 0;
        }

        if (_whiteImage.image != VK_NULL_HANDLE)
        {
            destroy_image(_whiteImage);
            _whiteImage = {};
        }
        if (_greyImage.image != VK_NULL_HANDLE)
        {
            destroy_image(_greyImage);
            _greyImage = {};
        }
        if (_blackImage.image != VK_NULL_HANDLE)
        {
            destroy_image(_blackImage);
            _blackImage = {};
        }
        if (_errorCheckerboardImage.image != VK_NULL_HANDLE)
        {
            destroy_image(_errorCheckerboardImage);
            _errorCheckerboardImage = {};
        }

        if (_defaultSamplerLinear != VK_NULL_HANDLE)
        {
            vkDestroySampler(_device, _defaultSamplerLinear, nullptr);
            _defaultSamplerLinear = VK_NULL_HANDLE;
        }
        if (_defaultSamplerNearest != VK_NULL_HANDLE)
        {
            vkDestroySampler(_device, _defaultSamplerNearest, nullptr);
            _defaultSamplerNearest = VK_NULL_HANDLE;
        }

        metalRoughMaterial.clear_resources(_device);

        for (auto& frame : _frames)
        {
            frame._deletionQueue.flush();
        }

        _mainDeletionQueue.flush();

        destroy_swapchain();

        vkDestroySurfaceKHR(_instance, _surface, nullptr);

        vmaDestroyAllocator(_allocator);

        vkDestroyDevice(_device, nullptr);
        vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
        vkDestroyInstance(_instance, nullptr);

        // restore cursor state before destroying the window so the OS cursor
        // is not left hidden/locked after the app exits.
        if (_window)
        {
            SDL_ShowCursor(SDL_ENABLE);
            SDL_SetRelativeMouseMode(SDL_FALSE);
            SDL_SetWindowGrab(_window, SDL_FALSE);
        }

        SDL_DestroyWindow(_window);
    }
}

void VulkanEngine::init_background_pipelines()
{
    VkPipelineLayoutCreateInfo computeLayout{};
    computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computeLayout.pNext = nullptr;
    computeLayout.pSetLayouts = &_drawImageDescriptorLayout;
    computeLayout.setLayoutCount = 1;

    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(ComputePushConstants);
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    computeLayout.pPushConstantRanges = &pushConstant;
    computeLayout.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(_device, &computeLayout, nullptr, &_skyboxPipelineLayout));

    VkShaderModule skyShader;
    if (!vkutil::load_shader_module(shader_path("skybox.comp.spv").c_str(), _device, &skyShader))
    {
        fmt::print("Error when building the compute shader\n");
    }

    VkPipelineShaderStageCreateInfo stageinfo{};
    stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageinfo.pNext = nullptr;
    stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageinfo.module = skyShader;
    stageinfo.pName = "main";

    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.pNext = nullptr;
    computePipelineCreateInfo.layout = _skyboxPipelineLayout;
    computePipelineCreateInfo.stage = stageinfo;

    ComputeEffect sky;
    sky.layout = _skyboxPipelineLayout;
    sky.name = "sky";
    sky.data = {};

    VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &sky.pipeline));

    // TODO: delete vector since there is only 1 compute shader effect
    backgroundEffects.push_back(sky);

    // destroy structures properly
    vkDestroyShaderModule(_device, skyShader, nullptr);
    VkPipeline skyPipeline = sky.pipeline;
    VkPipelineLayout pipelineLayout = _skyboxPipelineLayout;
    _mainDeletionQueue.push_function(
        [=, this]()
        {
            vkDestroyPipeline(this->_device, skyPipeline, nullptr);
            vkDestroyPipelineLayout(this->_device, pipelineLayout, nullptr);
        });
}

void VulkanEngine::draw_main(VkCommandBuffer cmd)
{
    VkClearValue clearValue = {.color = {{0.0f, 0.0f, 0.0f, 1.0f}}};

    VkRenderingAttachmentInfo colorAttachment =
        vkinit::attachment_info(_drawImage.imageView, &clearValue, VK_IMAGE_LAYOUT_GENERAL);
    VkRenderingAttachmentInfo depthAttachment =
        vkinit::depth_attachment_info(_depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderInfo = vkinit::rendering_info(_drawExtent, &colorAttachment, &depthAttachment);

    vkCmdBeginRendering(cmd, &renderInfo);
    auto start = std::chrono::system_clock::now();
    draw_geometry(cmd);

    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    stats.mesh_draw_time = elapsed.count() / 1000.f;

    vkCmdEndRendering(cmd);
}

void VulkanEngine::draw_shadow_map(VkCommandBuffer cmd)
{
    // Transition to depth attach
    vkutil::transition_image(cmd, _shadowImage.image, VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    // Depth-only dynamic rendering
    VkRenderingAttachmentInfo depthAtt =
        vkinit::depth_attachment_info(_shadowImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    VkExtent2D extent = _shadowExtent;
    VkRenderingInfo ri = vkinit::rendering_info({extent.width, extent.height}, /*color*/ nullptr, &depthAtt);

    vkCmdBeginRendering(cmd, &ri);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _shadowPipeline);

    // Viewport/scissor to shadow size
    VkViewport vp{0, 0, (float)extent.width, (float)extent.height, 0.0f, 1.0f};
    vkCmdSetViewport(cmd, 0, 1, &vp);
    VkRect2D scissor{{0, 0}, extent};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Build a UBO that has the light view-proj in sceneData.viewproj
    AllocatedBuffer& shadowBuffer = get_current_frame().shadowSceneDataBuffer;
    GPUSceneData* ptr = (GPUSceneData*)shadowBuffer.allocation->GetMappedData();

    GPUSceneData lightScene = _sceneData;
    lightScene.viewproj = _sceneData.sunlightViewProj;
    *ptr = lightScene;

    // Allocate set=0 with only the UBO (no images needed for depth-only)
    VkDescriptorSetVariableDescriptorCountAllocateInfo varCount{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO};
    uint32_t zero = 0;
    varCount.descriptorSetCount = 1;
    varCount.pDescriptorCounts = &zero;

    VkDescriptorSet global =
        get_current_frame()._frameDescriptors.allocate(_device, _gpuSceneDataDescriptorLayout, &varCount);

    DescriptorWriter w;
    w.write_buffer(0, shadowBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    w.update_set(_device, global);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _shadowPipelineLayout, 0, 1, &global, 0, nullptr);

    // Draw all opaque into the shadow map
    auto issueDraw = [&](const RenderObject& r)
    {
        vkCmdBindIndexBuffer(cmd, r.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        GPUDrawPushConstants pc{};
        pc.worldMatrix = r.transform;
        pc.vertexBuffer = r.vertexBufferAddress;
        vkCmdPushConstants(cmd, _shadowPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);

        vkCmdDrawIndexed(cmd, r.indexCount, 1, r.firstIndex, 0, 0);
    };

    for (auto& ro : _drawCommands.OpaqueSurfaces)
    {
        issueDraw(ro);
    }

    vkCmdEndRendering(cmd);

    // Make the depth image readable by fragment shader
    VkImageMemoryBarrier ib{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    ib.image = _shadowImage.image;
    ib.oldLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    ib.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    ib.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    ib.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    ib.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    ib.subresourceRange.levelCount = 1;
    ib.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, // after depth writes
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,     // before sampling
                         0, 0, nullptr, 0, nullptr, 1, &ib);
}

void VulkanEngine::draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView)
{
    VkRenderingAttachmentInfo colorAttachment =
        vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
    VkRenderingInfo renderInfo = vkinit::rendering_info(_windowExtent, &colorAttachment, nullptr);

    vkCmdBeginRendering(cmd, &renderInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRendering(cmd);
}

void VulkanEngine::draw()
{
    auto t0 = std::chrono::high_resolution_clock::now();

    // wait until the gpu has finished rendering the last frame. Timeout of 1 second
    // set UINT64_MAX to debug in renderdoc/XCode
    // VK_CHECK(vkWaitForFences(_device, 1, &get_current_frame()._renderFence, true, 1000000000));
    VK_CHECK(vkWaitForFences(_device, 1, &get_current_frame()._renderFence, true, UINT64_MAX));

    auto t1 = std::chrono::high_resolution_clock::now();

    get_current_frame()._deletionQueue.flush();
    get_current_frame()._frameDescriptors.clear_pools(_device);

    auto t2 = std::chrono::high_resolution_clock::now();

    // request image from the swapchain
    uint32_t swapchainImageIndex;

    // set UINT64_MAX to debug in renderdoc/XCode
    VkResult e = vkAcquireNextImageKHR(_device, _swapchain, UINT64_MAX, get_current_frame()._swapchainSemaphore,
                                       nullptr, &swapchainImageIndex);
    if (e == VK_ERROR_OUT_OF_DATE_KHR)
    {
        resize_requested = true;
        return;
    }
    _drawExtent.height = std::min(_swapchainExtent.height, _drawImage.imageExtent.height) * 1.f;
    _drawExtent.width = std::min(_swapchainExtent.width, _drawImage.imageExtent.width) * 1.f;

    VK_CHECK(vkResetFences(_device, 1, &get_current_frame()._renderFence));

    // now that we are sure that the commands finished executing, we can safely
    // reset the command buffer to begin recording again.
    VK_CHECK(vkResetCommandBuffer(get_current_frame()._mainCommandBuffer, 0));

    // naming it cmd for shorter writing
    VkCommandBuffer cmd = get_current_frame()._mainCommandBuffer;

    // begin the command buffer recording. We will use this command buffer exactly
    // once, so we want to let vulkan know that
    VkCommandBufferBeginInfo cmdBeginInfo =
        vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    // transition our main draw image into general layout so we can write into it
    // we will overwrite it all so we dont care about what was the older layout
    vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    vkutil::transition_image(cmd, _depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    // 1) Render shadow map
    // draw_shadow_map(cmd);
    // draw_debug_texture(cmd);

    // 2) Main pass (compute background + geometry sampling the shadow map)
    draw_main(cmd);

    // transtion the draw image and the swapchain image into their correct
    // transfer layouts
    vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkExtent2D extent;
    extent.height = _windowExtent.height;
    extent.width = _windowExtent.width;
    // extent.depth = 1;

    // execute a copy from the draw image into the swapchain
    vkutil::copy_image_to_image(cmd, _drawImage.image, _swapchainImages[swapchainImageIndex], _drawExtent,
                                _swapchainExtent);

    // set swapchain image layout to Attachment Optimal so we can draw it
    vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    // draw imgui into the swapchain image
    draw_imgui(cmd, _swapchainImageViews[swapchainImageIndex]);

    // set swapchain image layout to Present so we can draw it
    vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                             VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    // finalize the command buffer (we can no longer add commands, but it can now
    // be executed)
    VK_CHECK(vkEndCommandBuffer(cmd));

    // prepare the submission to the queue.
    // we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
    // we will signal the _presentSemaphores[swapchainImageIndex], to signal that rendering has finished

    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);

    VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                                                                   get_current_frame()._swapchainSemaphore);
    // Signal the per-image present semaphore
    VkSemaphoreSubmitInfo signalInfo =
        vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, _presentSemaphores[swapchainImageIndex]);

    VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);

    // submit command buffer to the queue and execute it.
    //  _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, get_current_frame()._renderFence));

    auto t3 = std::chrono::high_resolution_clock::now();

    // prepare present
    //  this will put the image we just rendered to into the visible window.
    //  we want to wait on the _presentSemaphores for that,
    //  as its necessary that drawing commands have finished before the image is
    //  displayed to the user
    VkPresentInfoKHR presentInfo = vkinit::present_info();

    presentInfo.pSwapchains = &_swapchain;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &_presentSemaphores[swapchainImageIndex];
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &swapchainImageIndex;

    VkResult presentResult = vkQueuePresentKHR(_graphicsQueue, &presentInfo);

    auto t4 = std::chrono::high_resolution_clock::now();

    // Accumulate timing stats and update display values periodically
    stats.fence_time_accum += std::chrono::duration<float, std::milli>(t1 - t0).count();
    stats.flush_time_accum += std::chrono::duration<float, std::milli>(t2 - t1).count();
    stats.submit_time_accum += std::chrono::duration<float, std::milli>(t3 - t2).count();
    stats.present_time_accum += std::chrono::duration<float, std::milli>(t4 - t3).count();
    stats.sample_count++;

    if (stats.sample_count >= EngineStats::kSampleInterval)
    {
        stats.fence_time = stats.fence_time_accum / stats.sample_count;
        stats.flush_time = stats.flush_time_accum / stats.sample_count;
        stats.submit_time = stats.submit_time_accum / stats.sample_count;
        stats.present_time = stats.present_time_accum / stats.sample_count;
        stats.fence_time_accum = 0.f;
        stats.flush_time_accum = 0.f;
        stats.submit_time_accum = 0.f;
        stats.present_time_accum = 0.f;
        stats.sample_count = 0;
    }

    if (e == VK_ERROR_OUT_OF_DATE_KHR)
    {
        resize_requested = true;
        return;
    }
    // increase the number of frames drawn
    _frameNumber++;
}

bool is_visible(const RenderObject& obj, const glm::mat4& viewproj)
{
    std::array<glm::vec3, 8> corners{
        glm::vec3{1, 1, 1},  glm::vec3{1, 1, -1},  glm::vec3{1, -1, 1},  glm::vec3{1, -1, -1},
        glm::vec3{-1, 1, 1}, glm::vec3{-1, 1, -1}, glm::vec3{-1, -1, 1}, glm::vec3{-1, -1, -1},
    };

    glm::mat4 matrix = viewproj * obj.transform;

    glm::vec3 min = {1.5, 1.5, 1.5};
    glm::vec3 max = {-1.5, -1.5, -1.5};

    bool anyBehindCamera = false;
    bool anyInFront = false;

    for (int c = 0; c < 8; c++)
    {
        // project each corner into clip space
        glm::vec4 v = matrix * glm::vec4(obj.bounds.origin + (corners[c] * obj.bounds.extents), 1.f);

        // Check if any corner is behind the camera (w <= 0 means behind near plane)
        if (v.w <= 0.0f)
        {
            anyBehindCamera = true;
            continue; // Skip perspective division for points behind camera
        }

        anyInFront = true;

        // perspective correction
        v.x = v.x / v.w;
        v.y = v.y / v.w;
        v.z = v.z / v.w;

        min = glm::min(glm::vec3{v.x, v.y, v.z}, min);
        max = glm::max(glm::vec3{v.x, v.y, v.z}, max);
    }

    // If any corner is behind the camera and any is in front, the object spans the near plane - always visible
    if (anyBehindCamera && anyInFront)
    {
        return true;
    }

    // If all corners are behind the camera, it's not visible
    if (!anyInFront)
    {
        return false;
    }

    // check the clip space box is within the view
    if (min.z > 1.f || max.z < 0.f || min.x > 1.f || max.x < -1.f || min.y > 1.f || max.y < -1.f)
    {
        return false;
    }
    else
    {
        return true;
    }
}

void VulkanEngine::draw_debug_texture(VkCommandBuffer cmd)
{
    VkRenderingAttachmentInfo colorAttachment =
        vkinit::attachment_info(_drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
    VkRenderingInfo renderInfo = vkinit::rendering_info(_drawExtent, &colorAttachment, nullptr);

    if (_textureDebugPipeline != VK_NULL_HANDLE && _debugRectangle.indexBuffer.buffer != VK_NULL_HANDLE)
    {
        vkCmdBeginRendering(cmd, &renderInfo);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _textureDebugPipeline);

        // allocate a new uniform buffer for the scene data
        AllocatedBuffer gpuSceneDataBuffer =
            create_buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        // add it to the deletion queue of this frame so it gets deleted once its been used
        get_current_frame()._deletionQueue.push_function([=, this]() { destroy_buffer(gpuSceneDataBuffer); });

        // write the buffer
        GPUSceneData* sceneUniformData = (GPUSceneData*)gpuSceneDataBuffer.allocation->GetMappedData();
        *sceneUniformData = _sceneData;

        VkDescriptorSetVariableDescriptorCountAllocateInfo allocArrayInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO, .pNext = nullptr};

        uint32_t descriptorCounts = texCache.Cache.size();
        if (_maxSampledImageDescriptors != 0)
        {
            descriptorCounts = std::min(descriptorCounts, _maxSampledImageDescriptors);
        }
        allocArrayInfo.pDescriptorCounts = &descriptorCounts;
        allocArrayInfo.descriptorSetCount = 1;

        // create a descriptor set that binds that buffer and update it
        VkDescriptorSet globalDescriptor =
            get_current_frame()._frameDescriptors.allocate(_device, _gpuSceneDataDescriptorLayout, &allocArrayInfo);

        DescriptorWriter writer;
        writer.write_buffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        // Shadow map (binding 1): take the descriptor we registered in texCache when creating the shadow
        {
            VkWriteDescriptorSet shadowSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            shadowSet.descriptorCount = 1;
            shadowSet.dstArrayElement = 0;
            shadowSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            shadowSet.dstBinding = 1;
            shadowSet.pImageInfo = &texCache.Cache[_shadowTexId.Index];
            writer.writes.push_back(shadowSet);
        }
        // and ignore binding 2 (obj texture array)
        writer.update_set(_device, globalDescriptor);

        // Reuse the same global descriptor (set=0)
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _textureDebugPipelineLayout, 0, 1,
                                &globalDescriptor, 0, nullptr);

        // Viewport/scissor to shadow size
        VkViewport vp{0, 0, (float)_drawExtent.width, (float)_drawExtent.height, 0.0f, 1.0f};
        vkCmdSetViewport(cmd, 0, 1, &vp);
        VkRect2D scissor{{0, 0}, _drawExtent};
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        GPUDrawPushConstants pc{};
        pc.worldMatrix = glm::mat4(1.0f);
        pc.vertexBuffer = _debugRectangle.vertexBufferAddress;
        vkCmdPushConstants(cmd, _textureDebugPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);

        vkCmdBindIndexBuffer(cmd, _debugRectangle.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);

        vkCmdEndRendering(cmd);
    }
}

void VulkanEngine::draw_geometry(VkCommandBuffer cmd)
{
    std::vector<uint32_t> opaque_draws;
    opaque_draws.reserve(_drawCommands.OpaqueSurfaces.size());

    for (int i = 0; i < _drawCommands.OpaqueSurfaces.size(); i++)
    {
        // TODO: add frustum culling config variable
        // if (is_visible(drawCommands.OpaqueSurfaces[i], drawCommands.viewProj))
        // {
        //     opaque_draws.push_back(i);
        // }
        opaque_draws.push_back(i);
    }

    AllocatedBuffer& gpuSceneDataBuffer = get_current_frame().sceneDataBuffer;
    GPUSceneData* sceneUniformData = (GPUSceneData*)gpuSceneDataBuffer.allocation->GetMappedData();
    *sceneUniformData = _sceneData;

    VkDescriptorSetVariableDescriptorCountAllocateInfo allocArrayInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO, .pNext = nullptr};

    uint32_t descriptorCounts = texCache.Cache.size();
    if (_maxSampledImageDescriptors != 0)
    {
        descriptorCounts = std::min(descriptorCounts, _maxSampledImageDescriptors);
    }
    allocArrayInfo.pDescriptorCounts = &descriptorCounts;
    allocArrayInfo.descriptorSetCount = 1;

    // create a descriptor set that binds that buffer and update it
    VkDescriptorSet globalDescriptor =
        get_current_frame()._frameDescriptors.allocate(_device, _gpuSceneDataDescriptorLayout, &allocArrayInfo);

    DescriptorWriter writer;
    writer.write_buffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    // Shadow map (binding 1): take the descriptor we registered in texCache when creating the shadow
    {
        VkWriteDescriptorSet shadowSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        shadowSet.descriptorCount = 1;
        shadowSet.dstArrayElement = 0;
        shadowSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        shadowSet.dstBinding = 1;
        shadowSet.pImageInfo = &texCache.Cache[_shadowTexId.Index];
        writer.writes.push_back(shadowSet);
    }
    // binding 2: object texture array
    if (descriptorCounts > 0)
    {
        VkWriteDescriptorSet arraySet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        arraySet.descriptorCount = descriptorCounts;
        arraySet.dstArrayElement = 0;
        arraySet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        arraySet.dstBinding = 2;
        arraySet.pImageInfo = texCache.Cache.data();
        writer.writes.push_back(arraySet);
    }

    writer.update_set(_device, globalDescriptor);

    MaterialPipeline* lastPipeline = nullptr;
    MaterialInstance* lastMaterial = nullptr;
    VkBuffer lastIndexBuffer = VK_NULL_HANDLE;

    auto draw = [&](const RenderObject& r)
    {
        if (r.material != lastMaterial)
        {
            lastMaterial = r.material;
            if (r.material->pipeline != lastPipeline)
            {

                lastPipeline = r.material->pipeline;
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->pipeline);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->layout, 0, 1,
                                        &globalDescriptor, 0, nullptr);

                VkViewport viewport = {};
                viewport.x = 0;
                viewport.y = 0;
                viewport.width = (float)_drawExtent.width;
                viewport.height = (float)_drawExtent.height;
                viewport.minDepth = 0.f;
                viewport.maxDepth = 1.f;

                vkCmdSetViewport(cmd, 0, 1, &viewport);

                VkRect2D scissor = {};
                scissor.offset.x = 0;
                scissor.offset.y = 0;
                scissor.extent.width = _drawExtent.width;
                scissor.extent.height = _drawExtent.height;

                vkCmdSetScissor(cmd, 0, 1, &scissor);
            }

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->layout, 1, 1,
                                    &r.material->materialSet, 0, nullptr);
        }
        if (r.indexBuffer != lastIndexBuffer)
        {
            lastIndexBuffer = r.indexBuffer;
            vkCmdBindIndexBuffer(cmd, r.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        }
        // calculate final mesh matrix
        GPUDrawPushConstants push_constants;
        push_constants.worldMatrix = r.transform;
        push_constants.viewProj = r.viewProj;
        push_constants.vertexBuffer = r.vertexBufferAddress;

        vkCmdPushConstants(cmd, r.material->pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(GPUDrawPushConstants), &push_constants);

        stats.drawcall_count++;
        stats.triangle_count += r.indexCount / 3;
        vkCmdDrawIndexed(cmd, r.indexCount, 1, r.firstIndex, 0, 0);
    };

    stats.drawcall_count = 0;
    stats.triangle_count = 0;

    for (auto& r : opaque_draws)
    {
        draw(_drawCommands.OpaqueSurfaces[r]);
    }

    for (auto& r : _drawCommands.TransparentSurfaces)
    {
        draw(r);
    }

    // // Debug: draw a white cube at the sun position
    // if (_lightDebugPipeline != VK_NULL_HANDLE && _debugCube.indexBuffer.buffer != VK_NULL_HANDLE)
    // {
    //     vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _lightDebugPipeline);
    //     // Reuse the same global descriptor (set=0)
    //     vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _lightDebugPipelineLayout, 0, 1,
    //                             &globalDescriptor, 0, nullptr);

    //     VkViewport viewport{};
    //     viewport.x = 0;
    //     viewport.y = 0;
    //     viewport.width = (float)_drawExtent.width;
    //     viewport.height = (float)_drawExtent.height;
    //     viewport.minDepth = 0.f;
    //     viewport.maxDepth = 1.f;
    //     vkCmdSetViewport(cmd, 0, 1, &viewport);

    //     VkRect2D scissor{};
    //     scissor.offset = {0, 0};
    //     scissor.extent = {_drawExtent.width, _drawExtent.height};
    //     vkCmdSetScissor(cmd, 0, 1, &scissor);

    //     glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(10.0f));
    //     glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(sceneData.sunlightPosition));
    //     GPUDrawPushConstants pc{};
    //     pc.worldMatrix = T * S;
    //     pc.vertexBuffer = _debugCube.vertexBufferAddress;
    //     vkCmdPushConstants(cmd, _lightDebugPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);

    //     vkCmdBindIndexBuffer(cmd, _debugCube.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    //     vkCmdDrawIndexed(cmd, 36, 1, 0, 0, 0);
    // }

    // we delete the draw commands now that we processed them
    _drawCommands.OpaqueSurfaces.clear();
    _drawCommands.TransparentSurfaces.clear();
}

void VulkanEngine::run()
{
    SDL_Event e;
    bool bQuit = false;

    // main loop
    while (!bQuit)
    {
        auto start = std::chrono::system_clock::now();

        // Handle events on queue
        while (SDL_PollEvent(&e) != 0)
        {
            // close the window when user alt-f4s or clicks the X button
            if (e.type == SDL_QUIT || e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
                bQuit = true;

            if (e.type == SDL_WINDOWEVENT)
            {

                if (e.window.event == SDL_WINDOWEVENT_RESIZED)
                {
                    resize_requested = true;
                }
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED)
                {
                    freeze_rendering = true;
                }
                if (e.window.event == SDL_WINDOWEVENT_RESTORED)
                {
                    freeze_rendering = false;
                }
            }

            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT)
            {
                _spotlight.spotGain = 5.0f;
            }
            else
            {
                if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT)
                {
                    _spotlight.spotGain = 1.0f;
                }
            }

            ImGui_ImplSDL2_ProcessEvent(&e);
        }

        if (freeze_rendering)
            continue;

        if (resize_requested)
        {
            resize_swapchain();
        }

        _robotArm.processSDLEvent();

        const Uint8* keyboardState = SDL_GetKeyboardState(NULL);
        if (keyboardState[SDL_SCANCODE_SPACE])
        {
            _sunLight.setSunSpeed(1.0f);
        }
        else
        {
            _sunLight.setSunSpeed(0.1f);
        }

        // calculate avg fps over 5 sec
        auto currframetime = std::chrono::high_resolution_clock::now();

        if (stats.fps_frame_count == 0 && stats.fps_window_start.time_since_epoch().count() == 0)
        {
            stats.fps_window_start = currframetime;
        }

        stats.fps_frame_count++;

        float elapsed_sec = std::chrono::duration<float>(currframetime - stats.fps_window_start).count();

        if (elapsed_sec >= 5.0f)
        {
            stats.avg_fps = stats.fps_frame_count / elapsed_sec;
            stats.fps_frame_count = 0;
            stats.fps_window_start = currframetime;
        }

        // imgui new frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();

        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(15, 18), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(261, 190), ImGuiCond_FirstUseEver);
        ImGui::Begin("Stats");

        ImGui::Text("frametime %f ms", stats.frametime);
        ImGui::Text("drawtime %f ms", stats.mesh_draw_time);
        ImGui::Text("triangles %i", stats.triangle_count);
        ImGui::Text("draws %i", stats.drawcall_count);
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Text("avg FPS (5s): %.1f", stats.avg_fps);
        ImGui::Text("sun height: %.1f", glm::normalize(_sceneData.sunlightPosition).y);
        ImGui::Separator();
        ImGui::Text("fence: %.2f ms", stats.fence_time);
        ImGui::Text("flush: %.2f ms", stats.flush_time);
        ImGui::Text("submit: %.2f ms", stats.submit_time);
        ImGui::Text("present: %.2f ms", stats.present_time);
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(289, 19), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(411, 190), ImGuiCond_FirstUseEver);
        ImGui::Begin("Controls");
        if (ImGui::BeginTable("controls_table", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthStretch, 0.3f);
            ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch, 0.7f);
            ImGui::TableHeadersRow();
            const std::array<std::pair<const char*, const char*>, 8> controls = {{
                {"WASD", "Move camera"},
                {"Mouse drag", "Pan camera"},
                {"Mouse left click", "Boost flashlight intensity"},
                {"I / K", "Raise / lower the upper arm"},
                {"U / J", "Raise / lower the lower arm"},
                {"O / L", "Raise / lower the wrist (flashlight)"},
                {"Left Shift", "Run / speed boost while moving"},
                {"Space", "Speed up Sun rotation"},
            }};
            for (const auto& [key, desc] : controls)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(key);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(desc);
            }
            ImGui::EndTable();
        }
        ImGui::End();

        ImGui::Render();

        // imgui commands
        // ImGui::ShowDemoWindow();

        update_scene();

        draw();

        auto end = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        stats.frametime = elapsed.count() / 1000.f;
    }
}

void VulkanEngine::update_scene()
{
    _mainCamera.update();
    _sunLight.update();
    // robotArm.update(mainCamera);

    glm::mat4 view = _mainCamera.getViewMatrix();

    // camera projection
    glm::mat4 projection = glm::perspective(glm::radians(_mainCamera.getFOV()),
                                            (float)_windowExtent.width / (float)_windowExtent.height, 10000.f, 0.1f);
    // glm::perspective(glm::radians(70.f), (float)_windowExtent.width / (float)_windowExtent.height, 10000.f, 0.1f);

    // invert the Y direction on projection matrix so that we are more similar
    // to opengl and gltf axis
    projection[1][1] *= -1;

    _sceneData.sunlightPosition = glm::vec4(_sunLight.getSunPosition(), 1.0f);
    _sceneData.cameraPosition = glm::vec4(_mainCamera.getPosition(), 1.0f);
    _sceneData.sunlightColor = glm::vec4(1.0f);

    _sceneData.sunlightViewProj = _sunLight.getLightSpaceMatrix();
    // Make shadow map index visible to shaders
    _sceneData.shadowParams = glm::uvec4(_shadowTexId.Index, 0, 0, 0);

    // spotlight
    _sceneData.spotlightPos = glm::vec4(_robotArm.getSpotlightPos(), 1.0f);
    _sceneData.spotlightDir = glm::vec4(_robotArm.getSpotlightDir(), 0.0f);
    _sceneData.spotColor = glm::vec4(SpotlightConstants::kSpotColor, 1.0f);
    float innerCutoff = glm::cos(glm::radians(SpotlightConstants::kInnerCutDeg));
    float outerCutoff = glm::cos(glm::radians(SpotlightConstants::kOuterCutDeg));
    float intensity = SpotlightConstants::kIntensity * _spotlight.spotGain;
    _sceneData.spotCutoffAndIntensity = glm::vec4(innerCutoff, outerCutoff, intensity, 0.0f);

    // update skybox
    ComputeEffect& effect = backgroundEffects[currentBackgroundEffect];
    glm::mat4 rotationOnlyView = glm::mat4(glm::mat3(_mainCamera.getViewMatrix()));
    glm::mat4 invProjection = glm::inverse(projection);
    glm::mat4 invView = glm::inverse(rotationOnlyView);
    effect.data.inverseProjection = invProjection;
    effect.data.inverseView = invView;
    effect.data.sunDirection = glm::vec4(_sunLight.getSunDirection(), 0.0f);
    effect.data.screenSize = glm::vec4(_drawExtent.width, _drawExtent.height, 0.0f, 0.0f);

    _drawCommands.viewProj = projection * view;
    auto it = loadedAssets.find("asset1");

    // asteroid belt parameters
    int numAsteroids = 15000;
    float majorRadius = 35.0f;  // distance from center to the inside of tube
    float minorRadius = 7.5f;   // tube radius (belt thickness)
    float verticalScale = 0.3f; // make the belt thin vertically
    float minScale = 0.05f;     // min asteroid size
    float maxScale = 0.15f;     // max asteroid size

    if (it != loadedAssets.end() && it->second)
    {
        std::mt19937 rng(42);
        std::uniform_real_distribution<float> angleDist(0.0f, glm::two_pi<float>());
        std::uniform_real_distribution<float> radiusDist(0.0f, 1.0f);
        std::uniform_real_distribution<float> scaleDist(minScale, maxScale);
        std::uniform_real_distribution<float> rotDist(0.0f, glm::two_pi<float>());

        for (int i = 0; i < numAsteroids; ++i)
        {
            float u = angleDist(rng) + _asteroidTime; // angle around the major circle [0, 2π]
            float v = angleDist(rng);                 // angle around the minor circle [0, 2π]

            // add random variation in [0,1] range
            float randomVariation = minorRadius * radiusDist(rng);

            // polar coordinates to XZ
            float x = (majorRadius + randomVariation * std::cos(v)) * std::cos(u);
            float z = (majorRadius + randomVariation * std::cos(v)) * std::sin(u);
            float y = randomVariation * std::sin(v) * verticalScale;

            float scale = scaleDist(rng);

            float rotX = rotDist(rng) + _asteroidTime;
            float rotY = rotDist(rng) + _asteroidTime;
            float rotZ = rotDist(rng) + _asteroidTime;

            glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
            glm::mat4 R = glm::rotate(glm::mat4(1.0f), rotX, glm::vec3(1, 0, 0));
            R = glm::rotate(R, rotY, glm::vec3(0, 1, 0));
            R = glm::rotate(R, rotZ, glm::vec3(0, 0, 1));
            glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(scale));

            // TODO: change Draw name to addToDrawCommands?
            it->second->Draw(T * R * S, _drawCommands);
        }
        // wrap around every 2 pi because of floating point precision
        _asteroidTime -= 0.001f; // asteroid belt rotates counter-clockwise viewed from north pole
        if (_asteroidTime < -glm::two_pi<float>())
        {
            _asteroidTime += glm::two_pi<float>();
        }
    }
}

AllocatedBuffer VulkanEngine::create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
    // allocate buffer
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext = nullptr;
    bufferInfo.size = allocSize;

    bufferInfo.usage = usage;

    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = memoryUsage;
    vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    AllocatedBuffer newBuffer;

    // allocate the buffer
    VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaallocInfo, &newBuffer.buffer, &newBuffer.allocation,
                             &newBuffer.info));

    return newBuffer;
}

AllocatedImage VulkanEngine::create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
    AllocatedImage newImage;
    newImage.imageFormat = format;
    newImage.imageExtent = size;

    VkImageCreateInfo img_info = vkinit::image_create_info(format, usage, size);
    if (mipmapped)
    {
        img_info.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
    }

    // always allocate images on dedicated GPU memory
    VmaAllocationCreateInfo allocinfo = {};
    allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // allocate and create the image
    VK_CHECK(vmaCreateImage(_allocator, &img_info, &allocinfo, &newImage.image, &newImage.allocation, nullptr));

    // if the format is a depth format, we will need to have it use the correct
    // aspect flag
    VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == VK_FORMAT_D32_SFLOAT)
    {
        aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    // build a image-view for the image
    VkImageViewCreateInfo view_info = vkinit::imageview_create_info(format, newImage.image, aspectFlag);
    view_info.subresourceRange.levelCount = img_info.mipLevels;

    VK_CHECK(vkCreateImageView(_device, &view_info, nullptr, &newImage.imageView));

    return newImage;
}

AllocatedImage VulkanEngine::create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                                          bool mipmapped)
{
    size_t data_size = size.depth * size.width * size.height * 4;
    AllocatedBuffer uploadbuffer =
        create_buffer(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    memcpy(uploadbuffer.info.pMappedData, data, data_size);

    AllocatedImage new_image = create_image(
        size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped);

    immediate_submit(
        [&](VkCommandBuffer cmd)
        {
            vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            VkBufferImageCopy copyRegion = {};
            copyRegion.bufferOffset = 0;
            copyRegion.bufferRowLength = 0;
            copyRegion.bufferImageHeight = 0;

            copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.imageSubresource.mipLevel = 0;
            copyRegion.imageSubresource.baseArrayLayer = 0;
            copyRegion.imageSubresource.layerCount = 1;
            copyRegion.imageExtent = size;

            // copy the buffer into the image
            vkCmdCopyBufferToImage(cmd, uploadbuffer.buffer, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                                   &copyRegion);

            if (mipmapped)
            {
                vkutil::generate_mipmaps(cmd, new_image.image,
                                         VkExtent2D{new_image.imageExtent.width, new_image.imageExtent.height});
            }
            else
            {
                vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
        });
    destroy_buffer(uploadbuffer);
    return new_image;
}

GPUMeshBuffers VulkanEngine::uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices)
{
    const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

    GPUMeshBuffers newSurface;

    newSurface.vertexBuffer = create_buffer(vertexBufferSize,
                                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                            VMA_MEMORY_USAGE_GPU_ONLY);

    VkBufferDeviceAddressInfo deviceAdressInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                                               .buffer = newSurface.vertexBuffer.buffer};
    newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(_device, &deviceAdressInfo);

    newSurface.indexBuffer =
        create_buffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                      VMA_MEMORY_USAGE_GPU_ONLY);

    AllocatedBuffer staging =
        create_buffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void* data = staging.allocation->GetMappedData();

    // copy vertex buffer
    memcpy(data, vertices.data(), vertexBufferSize);
    // copy index buffer
    memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);

    immediate_submit(
        [&](VkCommandBuffer cmd)
        {
            VkBufferCopy vertexCopy{0};
            vertexCopy.dstOffset = 0;
            vertexCopy.srcOffset = 0;
            vertexCopy.size = vertexBufferSize;

            vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1, &vertexCopy);

            VkBufferCopy indexCopy{0};
            indexCopy.dstOffset = 0;
            indexCopy.srcOffset = vertexBufferSize;
            indexCopy.size = indexBufferSize;

            vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBuffer.buffer, 1, &indexCopy);
        });

    destroy_buffer(staging);

    return newSurface;
}

FrameData& VulkanEngine::get_current_frame()
{
    return _frames[_frameNumber % FRAME_OVERLAP];
}

FrameData& VulkanEngine::get_last_frame()
{
    return _frames[(_frameNumber - 1) % FRAME_OVERLAP];
}

void VulkanEngine::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function)
{
    VK_CHECK(vkResetFences(_device, 1, &_immFence));
    VK_CHECK(vkResetCommandBuffer(_immCommandBuffer, 0));

    VkCommandBuffer cmd = _immCommandBuffer;
    // begin the command buffer recording. We will use this command buffer exactly
    // once, so we want to let vulkan know that
    VkCommandBufferBeginInfo cmdBeginInfo =
        vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
    VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, nullptr, nullptr);

    // submit command buffer to the queue and execute it.
    //  _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, _immFence));

    VK_CHECK(vkWaitForFences(_device, 1, &_immFence, true, 9999999999));
}

void VulkanEngine::destroy_image(const AllocatedImage& img)
{
    vkDestroyImageView(_device, img.imageView, nullptr);
    vmaDestroyImage(_allocator, img.image, img.allocation);
}

void VulkanEngine::destroy_buffer(const AllocatedBuffer& buffer)
{
    vmaDestroyBuffer(_allocator, buffer.buffer, buffer.allocation);
}

void VulkanEngine::init_vulkan()
{
    vkb::InstanceBuilder builder;

    // make the vulkan instance, with basic debug features
    auto inst_ret = builder.set_app_name("Example Vulkan Application")
                        .request_validation_layers(bUseValidationLayers)
                        .use_default_debug_messenger()
                        .require_api_version(1, 3, 0)
                        .build();

    vkb::Instance vkb_inst = inst_ret.value();

    // grab the instance
    _instance = vkb_inst.instance;
    _debug_messenger = vkb_inst.debug_messenger;

    SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

    VkPhysicalDeviceVulkan13Features features13{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
    };
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;

    VkPhysicalDeviceVulkan12Features features12{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
    };
    features12.bufferDeviceAddress = VK_TRUE;
    features12.descriptorIndexing = VK_TRUE;
    features12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    features12.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
    features12.descriptorBindingPartiallyBound = VK_TRUE;
    features12.descriptorBindingVariableDescriptorCount = VK_TRUE;
    features12.runtimeDescriptorArray = VK_TRUE;

    // use vkbootstrap to select a gpu.
    // We want a gpu that can write to the SDL surface and supports vulkan 1.2
    // We also need core features (1.0) like samplerAnisotropy
    VkPhysicalDeviceFeatures features{};
    features.samplerAnisotropy = VK_TRUE;

    vkb::PhysicalDeviceSelector selector{vkb_inst};
    auto selection_result = selector.set_minimum_version(1, 3)
                                .set_required_features_13(features13)
                                .set_required_features_12(features12)
                                .set_required_features(features)
                                .set_surface(_surface)
                                .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
                                .select_devices();

    if (!selection_result.has_value() || selection_result->empty())
    {
        fmt::print("Failed to find a suitable GPU!\n");
        abort();
    }

    // NVIDIA vendor ID = 0x10DE, Intel = 0x8086, AMD = 0x1002
    constexpr uint32_t NVIDIA_VENDOR_ID = 0x10DE;

    // TODO: temporary fix, select nvidia if available
    vkb::PhysicalDevice physicalDevice = selection_result->front();
    for (auto& dev : selection_result.value())
    {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev.physical_device, &props);
        if (props.vendorID == NVIDIA_VENDOR_ID)
        {
            physicalDevice = dev;
            break;
        }
    }

    fmt::print("Selected GPU: {}\n", physicalDevice.name);

    // physicalDevice.features.
    // create the final vulkan device

    vkb::DeviceBuilder deviceBuilder{physicalDevice};

    vkb::Device vkbDevice = deviceBuilder.build().value();

    // Get the VkDevice handle used in the rest of a vulkan application
    _device = vkbDevice.device;
    _chosenGPU = physicalDevice.physical_device;

    // use vkbootstrap to get a Graphics queue
    _graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();

    _graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    // initialize the memory allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = _chosenGPU;
    allocatorInfo.device = _device;
    allocatorInfo.instance = _instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &_allocator);
}

void VulkanEngine::init_swapchain()
{
    create_swapchain(_windowExtent.width, _windowExtent.height);

    // depth image size will match the window
    VkExtent3D drawImageExtent = {_windowExtent.width, _windowExtent.height, 1};

    // hardcoding the draw format to 32 bit float
    // _drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    _drawImage.imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
    _drawImage.imageExtent = drawImageExtent;

    VkImageUsageFlags drawImageUsages{};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo rimg_info = vkinit::image_create_info(_drawImage.imageFormat, drawImageUsages, drawImageExtent);

    // for the draw image, we want to allocate it from gpu local memory
    VmaAllocationCreateInfo rimg_allocinfo = {};
    rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // allocate and create the image
    vmaCreateImage(_allocator, &rimg_info, &rimg_allocinfo, &_drawImage.image, &_drawImage.allocation, nullptr);

    // build a image-view for the draw image to use for rendering
    VkImageViewCreateInfo rview_info =
        vkinit::imageview_create_info(_drawImage.imageFormat, _drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

    VK_CHECK(vkCreateImageView(_device, &rview_info, nullptr, &_drawImage.imageView));

    // create a depth image too
    // hardcoding the draw format to 32 bit float
    _depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
    _depthImage.imageExtent = drawImageExtent;
    VkImageUsageFlags depthImageUsages{};
    depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    VkImageCreateInfo dimg_info = vkinit::image_create_info(_depthImage.imageFormat, depthImageUsages, drawImageExtent);

    // allocate and create the image
    vmaCreateImage(_allocator, &dimg_info, &rimg_allocinfo, &_depthImage.image, &_depthImage.allocation, nullptr);

    // build a image-view for the draw image to use for rendering
    VkImageViewCreateInfo dview_info =
        vkinit::imageview_create_info(_depthImage.imageFormat, _depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);

    VK_CHECK(vkCreateImageView(_device, &dview_info, nullptr, &_depthImage.imageView));

    // add to deletion queues
    _mainDeletionQueue.push_function(
        [this]()
        {
            vkDestroyImageView(_device, _drawImage.imageView, nullptr);
            vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);

            vkDestroyImageView(_device, _depthImage.imageView, nullptr);
            vmaDestroyImage(_allocator, _depthImage.image, _depthImage.allocation);
        });
}

void VulkanEngine::create_swapchain(uint32_t width, uint32_t height)
{
    vkb::SwapchainBuilder swapchainBuilder{_chosenGPU, _device, _surface};

    _swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    vkb::Swapchain vkbSwapchain =
        swapchainBuilder
            //.use_default_format_selection()
            .set_desired_format(
                VkSurfaceFormatKHR{.format = _swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
            // .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR) // use vsync present mode
            .set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
            .set_desired_extent(width, height)
            .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .build()
            .value();

    _swapchainExtent = vkbSwapchain.extent;
    // store swapchain and its related images
    _swapchain = vkbSwapchain.swapchain;
    _swapchainImages = vkbSwapchain.get_images().value();
    _swapchainImageViews = vkbSwapchain.get_image_views().value();

    // Create per-swapchain-image present semaphores
    _presentSemaphores.resize(_swapchainImages.size());
    VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();
    for (size_t i = 0; i < _presentSemaphores.size(); ++i)
    {
        VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_presentSemaphores[i]));
    }
}
void VulkanEngine::destroy_swapchain()
{
    vkDestroySwapchainKHR(_device, _swapchain, nullptr);

    // destroy swapchain resources
    for (int i = 0; i < _swapchainImageViews.size(); i++)
    {
        vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
    }
    for (VkSemaphore s : _presentSemaphores)
    {
        vkDestroySemaphore(_device, s, nullptr);
    }
    _presentSemaphores.clear();
}

void VulkanEngine::resize_swapchain()
{
    vkDeviceWaitIdle(_device);

    destroy_swapchain();

    int w, h;
    SDL_GetWindowSize(_window, &w, &h);
    _windowExtent.width = w;
    _windowExtent.height = h;

    create_swapchain(_windowExtent.width, _windowExtent.height);

    resize_requested = false;
}

void VulkanEngine::init_commands()
{
    // create a command pool for commands submitted to the graphics queue.
    // we also want the pool to allow for resetting of individual command buffers
    VkCommandPoolCreateInfo commandPoolInfo =
        vkinit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (int i = 0; i < FRAME_OVERLAP; i++)
    {

        VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_frames[i]._commandPool));

        // allocate the default command buffer that we will use for rendering
        VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1);

        VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_frames[i]._mainCommandBuffer));

        VkCommandPool cmdPool = _frames[i]._commandPool;
        _mainDeletionQueue.push_function([this, cmdPool]() { vkDestroyCommandPool(_device, cmdPool, nullptr); });
    }

    VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_immCommandPool));

    // allocate the default command buffer that we will use for rendering
    VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_immCommandPool, 1);

    VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_immCommandBuffer));

    _mainDeletionQueue.push_function([this]() { vkDestroyCommandPool(_device, _immCommandPool, nullptr); });
}

void VulkanEngine::init_sync_structures()
{
    // create syncronization structures
    // one fence to control when the gpu has finished rendering the frame,
    // and 2 semaphores to syncronize rendering with swapchain
    // we want the fence to start signalled so we can wait on it on the first
    // frame
    VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_immFence));

    _mainDeletionQueue.push_function([this]() { vkDestroyFence(_device, _immFence, nullptr); });

    for (int i = 0; i < FRAME_OVERLAP; i++)
    {

        VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_frames[i]._renderFence));

        VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

        VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._swapchainSemaphore));
        // Per-frame renderSemaphore is not used anymore; present waits on per-image
        // semaphores

        VkFence renderFence = _frames[i]._renderFence;
        VkSemaphore swapSemaphore = _frames[i]._swapchainSemaphore;

        _frames[i].sceneDataBuffer =
            create_buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        _frames[i].shadowSceneDataBuffer =
            create_buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        _mainDeletionQueue.push_function(
            [this, renderFence, swapSemaphore, i]()
            {
                destroy_buffer(_frames[i].sceneDataBuffer);
                destroy_buffer(_frames[i].shadowSceneDataBuffer);
                vkDestroyFence(_device, renderFence, nullptr);
                vkDestroySemaphore(_device, swapSemaphore, nullptr);
                // no per-frame render semaphore
            });
    }
}

void VulkanEngine::init_renderables()
{
    const std::string asset1 = asset_path("icosahedron-low.obj");
    auto scene1 = loadAssimpAssets(this, asset1);
    if (scene1.has_value())
    {
        loadedAssets["asset1"] = *scene1;
    }
    else
    {
        fmt::print("Warning: failed to load icosahedron-low.obj from '{}'.\n", asset1);
    }
}

void VulkanEngine::init_shadow_resources()
{
    // 2D depth image for shadow map
    VkExtent3D size{_shadowExtent.width, _shadowExtent.height, 1};
    _shadowImage = create_image(size, VK_FORMAT_D32_SFLOAT,
                                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                /*mipmapped=*/false);

    // Depth compare sampler
    VkSamplerCreateInfo sci{.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sci.magFilter = VK_FILTER_LINEAR;
    sci.minFilter = VK_FILTER_LINEAR;
    sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    sci.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE; // outside shadow map => lit
    sci.compareEnable = VK_FALSE;
    sci.compareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
    sci.minLod = 0.0f;
    sci.maxLod = 0.0f;

    VK_CHECK(vkCreateSampler(_device, &sci, nullptr, &_shadowSampler));

    // Register in texture cache so we can sample it (set=0, binding=1 array)
    _shadowTexId = texCache.AddTexture(_shadowImage.imageView, _shadowSampler);

    _mainDeletionQueue.push_function(
        [=, this]()
        {
            if (_shadowSampler)
                vkDestroySampler(this->_device, _shadowSampler, nullptr);
            this->destroy_image(_shadowImage);
        });
}

void VulkanEngine::init_shadow_pipeline()
{
    VkShaderModule meshVertexShader;
    if (!vkutil::load_shader_module(shader_path("shadow_map.vert.spv").c_str(), _device, &meshVertexShader))
    {
        fmt::println("Error when building shadow vertex shader module");
    }

    // Layout: only global set (set=0) + same push constants as mesh
    VkPushConstantRange range{};
    range.offset = 0;
    range.size = sizeof(GPUDrawPushConstants);
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayout sets[] = {_gpuSceneDataDescriptorLayout};

    VkPipelineLayoutCreateInfo plci = vkinit::pipeline_layout_create_info();
    plci.setLayoutCount = 1;
    plci.pSetLayouts = sets;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &range;

    VK_CHECK(vkCreatePipelineLayout(_device, &plci, nullptr, &_shadowPipelineLayout));

    PipelineBuilder pb;
    pb.set_shaders(meshVertexShader, VK_NULL_HANDLE); // vertex-only
    pb.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pb.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pb.set_cull_mode(VK_CULL_MODE_BACK_BIT,
                     VK_FRONT_FACE_CLOCKWISE); // for some reason clockwise? maybe due to obj file...
    pb.set_multisampling_none();
    pb.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    // No color attachments for shadow pass
    pb.set_depth_format(_shadowImage.imageFormat);
    pb._pipelineLayout = _shadowPipelineLayout;

    _shadowPipeline = pb.build_pipeline(_device);

    vkDestroyShaderModule(_device, meshVertexShader, nullptr);

    _mainDeletionQueue.push_function(
        [=, this]()
        {
            if (_shadowPipeline)
                vkDestroyPipeline(this->_device, _shadowPipeline, nullptr);
            if (_shadowPipelineLayout)
                vkDestroyPipelineLayout(this->_device, _shadowPipelineLayout, nullptr);
        });
}

void VulkanEngine::init_imgui()
{
    // 1: create descriptor pool for IMGUI
    //  the size of the pool is very oversize, but it's copied from imgui demo
    //  itself.
    VkDescriptorPoolSize pool_sizes[] = {{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
                                         {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
                                         {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
                                         {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    VkDescriptorPool imguiPool;
    VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &imguiPool));

    // 2: initialize imgui library

    // this initializes the core structures of imgui
    ImGui::CreateContext();

    // this initializes imgui for SDL
    ImGui_ImplSDL2_InitForVulkan(_window);

    // this initializes imgui for Vulkan
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = _instance;
    init_info.PhysicalDevice = _chosenGPU;
    init_info.Device = _device;
    init_info.Queue = _graphicsQueue;
    init_info.DescriptorPool = imguiPool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.UseDynamicRendering = true;

    // dynamic rendering parameters for imgui to use
    init_info.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &_swapchainImageFormat;

    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info);

    ImGui_ImplVulkan_CreateFontsTexture();

    // add the destroy the imgui created structures
    _mainDeletionQueue.push_function(
        [this, imguiPool]()
        {
            ImGui_ImplVulkan_Shutdown();
            vkDestroyDescriptorPool(_device, imguiPool, nullptr);
        });
}

void VulkanEngine::init_pipelines()
{
    // COMPUTE PIPELINES
    init_background_pipelines();
    init_shadow_pipeline();
    init_light_debug_pipeline();
    init_debug_texture_pipeline();
    metalRoughMaterial.build_pipelines(this);
}

void VulkanEngine::init_light_debug_pipeline()
{
    // Reuse mesh vertex shader (push constants + buffer reference)
    VkShaderModule vtx;
    if (!vkutil::load_shader_module(shader_path("mesh.vert.spv").c_str(), _device, &vtx))
    {
        fmt::println("Error when building debug vertex shader module");
    }
    VkShaderModule frag;
    if (!vkutil::load_shader_module(shader_path("debug_light.frag.spv").c_str(), _device, &frag))
    {
        fmt::println("Error when building debug fragment shader module");
    }

    VkPushConstantRange range{};
    range.offset = 0;
    range.size = sizeof(GPUDrawPushConstants);
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayout sets[] = {_gpuSceneDataDescriptorLayout};

    VkPipelineLayoutCreateInfo plci = vkinit::pipeline_layout_create_info();
    plci.setLayoutCount = 1;
    plci.pSetLayouts = sets;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &range;

    VK_CHECK(vkCreatePipelineLayout(_device, &plci, nullptr, &_lightDebugPipelineLayout));

    PipelineBuilder pb;
    pb.set_shaders(vtx, frag);
    pb.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pb.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pb.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    pb.set_multisampling_none();
    pb.disable_blending();
    // Make the debug marker always visible by disabling depth testing entirely
    // (we already draw it after the scene). If you want it to respect depth,
    // switch this to enable_depthtest(false, VK_COMPARE_OP_GREATER_OR_EQUAL).
    // pb.disable_depthtest();
    pb.enable_depthtest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);
    pb.set_color_attachment_format(_drawImage.imageFormat);
    pb.set_depth_format(_depthImage.imageFormat);
    pb._pipelineLayout = _lightDebugPipelineLayout;

    _lightDebugPipeline = pb.build_pipeline(_device);

    vkDestroyShaderModule(_device, vtx, nullptr);
    vkDestroyShaderModule(_device, frag, nullptr);

    _mainDeletionQueue.push_function(
        [=, this]()
        {
            if (_lightDebugPipeline)
                vkDestroyPipeline(this->_device, _lightDebugPipeline, nullptr);
            if (_lightDebugPipelineLayout)
                vkDestroyPipelineLayout(this->_device, _lightDebugPipelineLayout, nullptr);
        });
}

void VulkanEngine::init_debug_texture_pipeline()
{
    // Reuse mesh vertex shader (push constants + buffer reference)
    VkShaderModule vtx;
    if (!vkutil::load_shader_module(shader_path("debug_shadow_map.vert.spv").c_str(), _device, &vtx))
    {
        fmt::println("Error when building debug vertex shader module");
    }
    VkShaderModule frag;
    if (!vkutil::load_shader_module(shader_path("debug_shadow_map.frag.spv").c_str(), _device, &frag))
    {
        fmt::println("Error when building debug fragment shader module");
    }

    VkPushConstantRange range{};
    range.offset = 0;
    range.size = sizeof(GPUDrawPushConstants);
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayout sets[] = {_gpuSceneDataDescriptorLayout};

    VkPipelineLayoutCreateInfo plci = vkinit::pipeline_layout_create_info();
    plci.setLayoutCount = 1;
    plci.pSetLayouts = sets;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &range;

    VK_CHECK(vkCreatePipelineLayout(_device, &plci, nullptr, &_textureDebugPipelineLayout));

    PipelineBuilder pb;
    pb.set_shaders(vtx, frag);
    pb.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pb.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pb.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    pb.set_multisampling_none();
    pb.disable_blending();
    // Make the debug marker always visible by disabling depth testing entirely
    // (we already draw it after the scene). If you want it to respect depth,
    // switch this to enable_depthtest(false, VK_COMPARE_OP_GREATER_OR_EQUAL).
    pb.disable_depthtest();
    // pb.enable_depthtest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);
    pb.set_color_attachment_format(_drawImage.imageFormat);
    pb.set_depth_format(_depthImage.imageFormat);
    pb._pipelineLayout = _textureDebugPipelineLayout;

    _textureDebugPipeline = pb.build_pipeline(_device);

    vkDestroyShaderModule(_device, vtx, nullptr);
    vkDestroyShaderModule(_device, frag, nullptr);

    _mainDeletionQueue.push_function(
        [=, this]()
        {
            if (_textureDebugPipeline)
                vkDestroyPipeline(this->_device, _textureDebugPipeline, nullptr);
            if (_textureDebugPipelineLayout)
                vkDestroyPipelineLayout(this->_device, _textureDebugPipelineLayout, nullptr);
        });
}

void VulkanEngine::init_descriptors()
{
    // create a descriptor pool
    std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
    };

    globalDescriptorAllocator.init_pool(_device, 10, sizes);
    _mainDeletionQueue.push_function([&]()
                                     { vkDestroyDescriptorPool(_device, globalDescriptorAllocator.pool, nullptr); });

    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        _drawImageDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_COMPUTE_BIT);
    }
    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // dedicated shadow map slot
        builder.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // bindless color array

        VkDescriptorSetLayoutBindingFlagsCreateInfo bindFlags{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO, .pNext = nullptr};

        // Only the LAST binding (2) can be variable-count per spec.
        std::array<VkDescriptorBindingFlags, 3> flagArray{0, 0,
                                                          VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |
                                                              VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                                                              VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT};

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(_chosenGPU, &props);

        VkPhysicalDeviceDescriptorIndexingProperties indexingProps{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES};
        VkPhysicalDeviceProperties2 props2{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
                                           .pNext = &indexingProps};
        vkGetPhysicalDeviceProperties2(_chosenGPU, &props2);

        uint32_t descriptorLimit = std::numeric_limits<uint32_t>::max();
        auto clampLimit = [&](uint32_t value)
        {
            if (value > 0)
            {
                descriptorLimit = std::min(descriptorLimit, value);
            }
        };

        const bool hasUpdateAfterBindLimits = indexingProps.maxDescriptorSetUpdateAfterBindSampledImages > 0 ||
                                              indexingProps.maxDescriptorSetUpdateAfterBindSamplers > 0 ||
                                              indexingProps.maxPerStageDescriptorUpdateAfterBindSamplers > 0;

        if (hasUpdateAfterBindLimits)
        {
            clampLimit(indexingProps.maxPerStageDescriptorUpdateAfterBindSamplers);
            clampLimit(indexingProps.maxDescriptorSetUpdateAfterBindSamplers);
            clampLimit(indexingProps.maxDescriptorSetUpdateAfterBindSampledImages);
        }
        else
        {
            clampLimit(props.limits.maxPerStageDescriptorSamplers);
            clampLimit(props.limits.maxPerStageDescriptorSampledImages);
            clampLimit(props.limits.maxDescriptorSetSamplers);
            clampLimit(props.limits.maxDescriptorSetSampledImages);
        }

        if (descriptorLimit == std::numeric_limits<uint32_t>::max())
        {
            descriptorLimit = props.limits.maxPerStageDescriptorSamplers;
        }

        // clamp to avoid OOM on drivers that report high limits (Intel iGPU ~400 million).
        constexpr uint32_t kMaxDescriptors = 16384;
        descriptorLimit = std::min(descriptorLimit, kMaxDescriptors);

        const uint32_t shadowDescriptorCount = 1;

        // Reserve one descriptor from the HW limit for the dedicated shadow binding so that the
        // total number of accessible samplers never exceeds the device cap.
        uint32_t colorDescriptorCap = descriptorLimit;
        if (colorDescriptorCap > shadowDescriptorCount)
        {
            colorDescriptorCap -= shadowDescriptorCount;
        }
        else
        {
            // Extremely small descriptor limits are unlikely, but fall back to at least one slot so the
            // engine keeps running (this may clamp total to the HW minimum).
            colorDescriptorCap = 1;
        }

        builder.bindings[1].descriptorCount = shadowDescriptorCount;
        builder.bindings[2].descriptorCount = colorDescriptorCap; // variable-sized color array
        _maxSampledImageDescriptors = colorDescriptorCap;
        texCache.set_max(_maxSampledImageDescriptors);

        bindFlags.bindingCount = static_cast<uint32_t>(flagArray.size());
        bindFlags.pBindingFlags = flagArray.data();

        _gpuSceneDataDescriptorLayout =
            builder.build(_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, &bindFlags,
                          VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);
    }

    _mainDeletionQueue.push_function(
        [&]()
        {
            vkDestroyDescriptorSetLayout(_device, _drawImageDescriptorLayout, nullptr);
            vkDestroyDescriptorSetLayout(_device, _gpuSceneDataDescriptorLayout, nullptr);
        });

    _drawImageDescriptors = globalDescriptorAllocator.allocate(_device, _drawImageDescriptorLayout);
    {
        DescriptorWriter writer;
        writer.write_image(0, _drawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL,
                           VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        writer.update_set(_device, _drawImageDescriptors);
    }
    for (int i = 0; i < FRAME_OVERLAP; i++)
    {
        // create a descriptor pool
        std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frame_sizes = {
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
        };

        _frames[i]._frameDescriptors = DescriptorAllocatorGrowable{};
        _frames[i]._frameDescriptors.init(_device, 1000, frame_sizes, VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);
        _mainDeletionQueue.push_function([&, i]() { _frames[i]._frameDescriptors.destroy_pools(_device); });
    }
}

void GLTFMetallic_Roughness::build_pipelines(VulkanEngine* engine)
{
    VkShaderModule meshFragShader;
    if (!vkutil::load_shader_module(shader_path("basic_phong.frag.spv").c_str(), engine->_device, &meshFragShader))
    {
        fmt::println("Error when building the triangle fragment shader module");
    }

    VkShaderModule meshVertexShader;
    if (!vkutil::load_shader_module(shader_path("mesh.vert.spv").c_str(), engine->_device, &meshVertexShader))
    {
        fmt::println("Error when building the triangle vertex shader module");
    }

    VkPushConstantRange matrixRange{};
    matrixRange.offset = 0;
    matrixRange.size = sizeof(GPUDrawPushConstants);
    matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    materialLayout = layoutBuilder.build(engine->_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    VkDescriptorSetLayout layouts[] = {engine->_gpuSceneDataDescriptorLayout, materialLayout};

    VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
    mesh_layout_info.setLayoutCount = 2;
    mesh_layout_info.pSetLayouts = layouts;
    mesh_layout_info.pPushConstantRanges = &matrixRange;
    mesh_layout_info.pushConstantRangeCount = 1;

    VkPipelineLayout newLayout;
    VK_CHECK(vkCreatePipelineLayout(engine->_device, &mesh_layout_info, nullptr, &newLayout));

    opaquePipeline.layout = newLayout;
    transparentPipeline.layout = newLayout;

    // build the stage-create-info for both vertex and fragment stages. This lets
    // the pipeline know the shader modules per stage
    PipelineBuilder pipelineBuilder;

    pipelineBuilder.set_shaders(meshVertexShader, meshFragShader);

    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);

    pipelineBuilder.set_cull_mode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);

    pipelineBuilder.set_multisampling_none();

    pipelineBuilder.disable_blending();

    pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

    // render format
    pipelineBuilder.set_color_attachment_format(engine->_drawImage.imageFormat);
    pipelineBuilder.set_depth_format(engine->_depthImage.imageFormat);

    // use the triangle layout we created
    pipelineBuilder._pipelineLayout = newLayout;

    // finally build the pipeline
    opaquePipeline.pipeline = pipelineBuilder.build_pipeline(engine->_device);

    // create the transparent variant
    pipelineBuilder.enable_blending_additive();

    pipelineBuilder.enable_depthtest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);

    transparentPipeline.pipeline = pipelineBuilder.build_pipeline(engine->_device);

    vkDestroyShaderModule(engine->_device, meshFragShader, nullptr);
    vkDestroyShaderModule(engine->_device, meshVertexShader, nullptr);
}

void GLTFMetallic_Roughness::clear_resources(VkDevice device)
{
    if (materialLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device, materialLayout, nullptr);
        materialLayout = VK_NULL_HANDLE;
    }
    if (transparentPipeline.layout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device, transparentPipeline.layout, nullptr);
        transparentPipeline.layout = VK_NULL_HANDLE;
        opaquePipeline.layout = VK_NULL_HANDLE; // shared layout
    }
    if (transparentPipeline.pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, transparentPipeline.pipeline, nullptr);
        transparentPipeline.pipeline = VK_NULL_HANDLE;
    }
    if (opaquePipeline.pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, opaquePipeline.pipeline, nullptr);
        opaquePipeline.pipeline = VK_NULL_HANDLE;
    }
}

MaterialInstance GLTFMetallic_Roughness::write_material(VkDevice device, MaterialPass pass,
                                                        const MaterialResources& resources,
                                                        DescriptorAllocatorGrowable& descriptorAllocator)
{
    MaterialInstance matData;
    matData.passType = pass;
    if (pass == MaterialPass::Transparent)
    {
        matData.pipeline = &transparentPipeline;
    }
    else
    {
        matData.pipeline = &opaquePipeline;
    }

    matData.materialSet = descriptorAllocator.allocate(device, materialLayout);

    writer.clear();
    writer.write_buffer(0, resources.dataBuffer, sizeof(MaterialConstants), resources.dataBufferOffset,
                        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    writer.update_set(device, matData.materialSet);

    return matData;
}
void MeshNode::Draw(const glm::mat4& topMatrix, DrawContext& ctx)
{
    glm::mat4 nodeMatrix = topMatrix * worldTransform;

    for (auto& s : mesh->surfaces)
    {
        RenderObject def;
        def.indexCount = s.count;
        def.firstIndex = s.startIndex;
        def.indexBuffer = mesh->meshBuffers.indexBuffer.buffer;
        def.material = &s.material->data;
        def.bounds = s.bounds;
        def.transform = nodeMatrix;
        def.viewProj = ctx.viewProj;
        def.vertexBufferAddress = mesh->meshBuffers.vertexBufferAddress;

        if (s.material->data.passType == MaterialPass::Transparent)
        {
            ctx.TransparentSurfaces.push_back(def);
        }
        else
        {
            ctx.OpaqueSurfaces.push_back(def);
        }
    }

    // recurse down
    Node::Draw(topMatrix, ctx);
}

TextureID TextureCache::AddTexture(const VkImageView& image, VkSampler sampler)
{
    for (unsigned int i = 0; i < Cache.size(); i++)
    {
        if (Cache[i].imageView == image && Cache[i].sampler == sampler)
        {
            // found, return it
            return TextureID{i};
        }
    }

    const bool limitActive = maxDescriptors != std::numeric_limits<uint32_t>::max() && maxDescriptors > 0;

    if (limitActive && Cache.size() >= maxDescriptors)
    {
        if (!limitWarningEmitted)
        {
            fmt::print("Texture cache reached capacity ({}). Reusing fallback texture {}.\n", maxDescriptors,
                       fallbackTexture.Index);
            limitWarningEmitted = true;
        }

        if (!Cache.empty() && fallbackTexture.Index < Cache.size())
        {
            return fallbackTexture;
        }

        // Return last valid descriptor if no explicit fallback is available.
        return TextureID{Cache.empty() ? 0u : static_cast<uint32_t>(Cache.size() - 1)};
    }

    uint32_t idx = Cache.size();

    Cache.push_back(VkDescriptorImageInfo{
        .sampler = sampler, .imageView = image, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});

    if (fallbackTexture.Index >= Cache.size())
    {
        fallbackTexture.Index = idx;
    }

    return TextureID{idx};
}
