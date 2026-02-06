
#include "vk_engine.h"

#include "fmt/core.h"
#include "vk_context.h"
#include "vk_descriptors.h"
#include "vk_images.h"
#include "vk_initializers.h"
#include "vk_loader.h"
#include "vk_pipelines.h"
#include "vk_types.h"

#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"
#include <SDL.h>
#include <SDL_vulkan.h>
#include <glm/gtx/transform.hpp>

#include <chrono>

#ifndef ENABLE_VALIDATION_LAYERS
#define ENABLE_VALIDATION_LAYERS 0
#endif

constexpr bool bUseValidationLayers = ENABLE_VALIDATION_LAYERS;

VulkanEngine* loadedEngine = nullptr;

VulkanEngine& VulkanEngine::Get()
{
    return *loadedEngine;
}

void VulkanEngine::init()
{
    // only one engine initialization is allowed with the application.
    assert(loadedEngine == nullptr);
    loadedEngine = this;

    // init context (window, vulkan instance, device, allocator)
    ctx.init();

    initSwapchain();
    initCommands();
    initSyncStructures();
    initDescriptors();
    initShadowResources();
    initPipelines();
    initDefaultData();
    scene.initRenderables(ctx, resources, metalRoughMaterial);
    initImgui();

    // everything went fine
    _isInitialized = true;
}

void VulkanEngine::initDefaultData()
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

    _debugRectangle = resources.uploadMesh(ctx, rect_indices, rect_vertices);

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

        _debugCube = resources.uploadMesh(ctx, idx, v);
    }

    // init resource manager (creates default textures, samplers)
    resources.init(ctx);
}

void VulkanEngine::cleanup()
{
    if (_isInitialized)
    {

        // make sure the gpu has stopped doing its things
        vkDeviceWaitIdle(ctx.device);

        scene.cleanup();

        // Destroy engine-owned GPU resources that might not be tied to deletion
        // queues
        if (_debugRectangle.indexBuffer.buffer != VK_NULL_HANDLE)
        {
            resources.destroyBuffer(ctx, _debugRectangle.indexBuffer);
            _debugRectangle.indexBuffer = {};
        }
        if (_debugRectangle.vertexBuffer.buffer != VK_NULL_HANDLE)
        {
            resources.destroyBuffer(ctx, _debugRectangle.vertexBuffer);
            _debugRectangle.vertexBuffer = {};
            _debugRectangle.vertexBufferAddress = 0;
        }

        if (_debugCube.indexBuffer.buffer != VK_NULL_HANDLE)
        {
            resources.destroyBuffer(ctx, _debugCube.indexBuffer);
            _debugCube.indexBuffer = {};
        }
        if (_debugCube.vertexBuffer.buffer != VK_NULL_HANDLE)
        {
            resources.destroyBuffer(ctx, _debugCube.vertexBuffer);
            _debugCube.vertexBuffer = {};
            _debugCube.vertexBufferAddress = 0;
        }

        resources.cleanup(ctx);

        metalRoughMaterial.clear_resources(ctx.device);

        for (auto& frame : _frames)
        {
            frame._deletionQueue.flush();
        }

        _mainDeletionQueue.flush();

        swapchain.destroy(ctx);

        ctx.cleanup();
    }
}

void VulkanEngine::initBackgroundPipelines()
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

    VK_CHECK(vkCreatePipelineLayout(ctx.device, &computeLayout, nullptr, &_skyboxPipelineLayout));

    VkShaderModule skyShader;
    if (!vkutil::load_shader_module(shader_path("skybox.comp.spv").c_str(), ctx.device, &skyShader))
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

    VK_CHECK(
        vkCreateComputePipelines(ctx.device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &sky.pipeline));

    // TODO: delete vector since there is only 1 compute shader effect
    _backgroundEffects.push_back(sky);

    // destroy structures properly
    vkDestroyShaderModule(ctx.device, skyShader, nullptr);
    VkPipeline skyPipeline = sky.pipeline;
    VkPipelineLayout pipelineLayout = _skyboxPipelineLayout;
    _mainDeletionQueue.push_function(
        [=, this]()
        {
            vkDestroyPipeline(this->ctx.device, skyPipeline, nullptr);
            vkDestroyPipelineLayout(this->ctx.device, pipelineLayout, nullptr);
        });
}

void VulkanEngine::drawMain(VkCommandBuffer cmd)
{
    VkClearValue clearValue = {.color = {{0.0f, 0.0f, 0.0f, 1.0f}}};

    VkRenderingAttachmentInfo colorAttachment =
        vkinit::attachment_info(swapchain.drawImage.imageView, &clearValue, VK_IMAGE_LAYOUT_GENERAL);
    VkRenderingAttachmentInfo depthAttachment =
        vkinit::depth_attachment_info(swapchain.depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderInfo = vkinit::rendering_info(swapchain.drawExtent, &colorAttachment, &depthAttachment);

    vkCmdBeginRendering(cmd, &renderInfo);
    auto start = std::chrono::system_clock::now();
    drawGeometry(cmd);

    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    _stats.mesh_draw_time = elapsed.count() / 1000.f;

    vkCmdEndRendering(cmd);
}

void VulkanEngine::drawShadowMap(VkCommandBuffer cmd)
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
    AllocatedBuffer& shadowBuffer = getCurrentFrame().shadowSceneDataBuffer;
    GPUSceneData* ptr = (GPUSceneData*)shadowBuffer.info.pMappedData;

    GPUSceneData lightScene = scene.sceneData;
    lightScene.viewproj = scene.sceneData.sunlightViewProj;
    *ptr = lightScene;

    // Allocate set=0 with only the UBO (no images needed for depth-only)
    VkDescriptorSetVariableDescriptorCountAllocateInfo varCount{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO};
    uint32_t zero = 0;
    varCount.descriptorSetCount = 1;
    varCount.pDescriptorCounts = &zero;

    VkDescriptorSet global =
        getCurrentFrame()._frameDescriptors.allocate(ctx.device, gpuSceneDataDescriptorLayout, &varCount);

    DescriptorWriter w;
    w.write_buffer(0, shadowBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    w.update_set(ctx.device, global);

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

void VulkanEngine::drawImgui(VkCommandBuffer cmd, VkImageView targetImageView)
{
    VkRenderingAttachmentInfo colorAttachment =
        vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
    VkRenderingInfo renderInfo = vkinit::rendering_info(ctx.windowExtent, &colorAttachment, nullptr);

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
    VK_CHECK(vkWaitForFences(ctx.device, 1, &getCurrentFrame()._renderFence, true, UINT64_MAX));

    auto t1 = std::chrono::high_resolution_clock::now();

    getCurrentFrame()._deletionQueue.flush();
    getCurrentFrame()._frameDescriptors.clear_pools(ctx.device);

    auto t2 = std::chrono::high_resolution_clock::now();

    // request image from the swapchain
    uint32_t swapchainImageIndex;

    // set UINT64_MAX to debug in renderdoc/XCode
    VkResult e = vkAcquireNextImageKHR(ctx.device, swapchain.swapchain, UINT64_MAX,
                                       getCurrentFrame()._swapchainSemaphore, nullptr, &swapchainImageIndex);
    if (e == VK_ERROR_OUT_OF_DATE_KHR)
    {
        resizeRequested = true;
        return;
    }
    swapchain.drawExtent.height = std::min(swapchain.extent.height, swapchain.drawImage.imageExtent.height) * 1.f;
    swapchain.drawExtent.width = std::min(swapchain.extent.width, swapchain.drawImage.imageExtent.width) * 1.f;

    VK_CHECK(vkResetFences(ctx.device, 1, &getCurrentFrame()._renderFence));

    // now that we are sure that the commands finished executing, we can safely
    // reset the command buffer to begin recording again.
    VK_CHECK(vkResetCommandBuffer(getCurrentFrame()._mainCommandBuffer, 0));

    // naming it cmd for shorter writing
    VkCommandBuffer cmd = getCurrentFrame()._mainCommandBuffer;

    // begin the command buffer recording. We will use this command buffer exactly
    // once, so we want to let vulkan know that
    VkCommandBufferBeginInfo cmdBeginInfo =
        vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    // transition our main draw image into general layout so we can write into it
    // we will overwrite it all so we dont care about what was the older layout
    vkutil::transition_image(cmd, swapchain.drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    vkutil::transition_image(cmd, swapchain.depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    // 1) Render shadow map
    // draw_shadow_map(cmd);
    // draw_debug_texture(cmd);

    // 2) Main pass (compute background + geometry sampling the shadow map)
    drawMain(cmd);

    // transtion the draw image and the swapchain image into their correct
    // transfer layouts
    vkutil::transition_image(cmd, swapchain.drawImage.image, VK_IMAGE_LAYOUT_GENERAL,
                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::transition_image(cmd, swapchain.images[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkExtent2D extent;
    extent.height = ctx.windowExtent.height;
    extent.width = ctx.windowExtent.width;
    // extent.depth = 1;

    // execute a copy from the draw image into the swapchain
    vkutil::copy_image_to_image(cmd, swapchain.drawImage.image, swapchain.images[swapchainImageIndex],
                                swapchain.drawExtent, swapchain.extent);

    // set swapchain image layout to Attachment Optimal so we can draw it
    vkutil::transition_image(cmd, swapchain.images[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    // draw imgui into the swapchain image
    drawImgui(cmd, swapchain.imageViews[swapchainImageIndex]);

    // set swapchain image layout to Present so we can draw it
    vkutil::transition_image(cmd, swapchain.images[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                             VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    // finalize the command buffer (we can no longer add commands, but it can now
    // be executed)
    VK_CHECK(vkEndCommandBuffer(cmd));

    // prepare the submission to the queue.
    // we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
    // we will signal the _presentSemaphores[swapchainImageIndex], to signal that rendering has finished

    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);

    VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                                                                   getCurrentFrame()._swapchainSemaphore);
    // Signal the per-image present semaphore
    VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                                                                     swapchain.presentSemaphores[swapchainImageIndex]);

    VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);

    // submit command buffer to the queue and execute it.
    //  _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(ctx.graphicsQueue, 1, &submit, getCurrentFrame()._renderFence));

    auto t3 = std::chrono::high_resolution_clock::now();

    // prepare present
    //  this will put the image we just rendered to into the visible window.
    //  we want to wait on the _presentSemaphores for that,
    //  as its necessary that drawing commands have finished before the image is
    //  displayed to the user
    VkPresentInfoKHR presentInfo = vkinit::present_info();

    presentInfo.pSwapchains = &swapchain.swapchain;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &swapchain.presentSemaphores[swapchainImageIndex];
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &swapchainImageIndex;

    VkResult presentResult = vkQueuePresentKHR(ctx.graphicsQueue, &presentInfo);

    auto t4 = std::chrono::high_resolution_clock::now();

    // Accumulate timing stats and update display values periodically
    _stats.fence_time_accum += std::chrono::duration<float, std::milli>(t1 - t0).count();
    _stats.flush_time_accum += std::chrono::duration<float, std::milli>(t2 - t1).count();
    _stats.submit_time_accum += std::chrono::duration<float, std::milli>(t3 - t2).count();
    _stats.present_time_accum += std::chrono::duration<float, std::milli>(t4 - t3).count();
    _stats.sample_count++;

    if (_stats.sample_count >= EngineStats::kSampleInterval)
    {
        _stats.fence_time = _stats.fence_time_accum / _stats.sample_count;
        _stats.flush_time = _stats.flush_time_accum / _stats.sample_count;
        _stats.submit_time = _stats.submit_time_accum / _stats.sample_count;
        _stats.present_time = _stats.present_time_accum / _stats.sample_count;
        _stats.fence_time_accum = 0.f;
        _stats.flush_time_accum = 0.f;
        _stats.submit_time_accum = 0.f;
        _stats.present_time_accum = 0.f;
        _stats.sample_count = 0;
    }

    if (e == VK_ERROR_OUT_OF_DATE_KHR)
    {
        resizeRequested = true;
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

void VulkanEngine::drawDebugTexture(VkCommandBuffer cmd)
{
    VkRenderingAttachmentInfo colorAttachment =
        vkinit::attachment_info(swapchain.drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
    VkRenderingInfo renderInfo = vkinit::rendering_info(swapchain.drawExtent, &colorAttachment, nullptr);

    if (_textureDebugPipeline != VK_NULL_HANDLE && _debugRectangle.indexBuffer.buffer != VK_NULL_HANDLE)
    {
        vkCmdBeginRendering(cmd, &renderInfo);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _textureDebugPipeline);

        // allocate a new uniform buffer for the scene data
        AllocatedBuffer gpuSceneDataBuffer = resources.createBuffer(
            ctx, sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        // add it to the deletion queue of this frame so it gets deleted once its been used
        getCurrentFrame()._deletionQueue.push_function([=, this]()
                                                       { resources.destroyBuffer(ctx, gpuSceneDataBuffer); });

        // write the buffer
        GPUSceneData* sceneUniformData = (GPUSceneData*)gpuSceneDataBuffer.info.pMappedData;
        *sceneUniformData = scene.sceneData;

        VkDescriptorSetVariableDescriptorCountAllocateInfo allocArrayInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO, .pNext = nullptr};

        uint32_t descriptorCounts = resources.texCache.Cache.size();
        if (ctx.maxSampledImageDescriptors != 0)
        {
            descriptorCounts = std::min(descriptorCounts, ctx.maxSampledImageDescriptors);
        }
        allocArrayInfo.pDescriptorCounts = &descriptorCounts;
        allocArrayInfo.descriptorSetCount = 1;

        // create a descriptor set that binds that buffer and update it
        VkDescriptorSet globalDescriptor =
            getCurrentFrame()._frameDescriptors.allocate(ctx.device, gpuSceneDataDescriptorLayout, &allocArrayInfo);

        DescriptorWriter writer;
        writer.write_buffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        // Shadow map (binding 1): take the descriptor we registered in texCache when creating the shadow
        {
            VkWriteDescriptorSet shadowSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            shadowSet.descriptorCount = 1;
            shadowSet.dstArrayElement = 0;
            shadowSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            shadowSet.dstBinding = 1;
            shadowSet.pImageInfo = &resources.texCache.Cache[_shadowTexId.Index];
            writer.writes.push_back(shadowSet);
        }
        // and ignore binding 2 (obj texture array)
        writer.update_set(ctx.device, globalDescriptor);

        // Reuse the same global descriptor (set=0)
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _textureDebugPipelineLayout, 0, 1,
                                &globalDescriptor, 0, nullptr);

        // Viewport/scissor to shadow size
        VkViewport vp{0, 0, (float)swapchain.drawExtent.width, (float)swapchain.drawExtent.height, 0.0f, 1.0f};
        vkCmdSetViewport(cmd, 0, 1, &vp);
        VkRect2D scissor{{0, 0}, swapchain.drawExtent};
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

void VulkanEngine::drawGeometry(VkCommandBuffer cmd)
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

    AllocatedBuffer& gpuSceneDataBuffer = getCurrentFrame().sceneDataBuffer;
    GPUSceneData* sceneUniformData = (GPUSceneData*)gpuSceneDataBuffer.info.pMappedData;
    *sceneUniformData = scene.sceneData;

    VkDescriptorSetVariableDescriptorCountAllocateInfo allocArrayInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO, .pNext = nullptr};

    uint32_t descriptorCounts = resources.texCache.Cache.size();
    if (ctx.maxSampledImageDescriptors != 0)
    {
        descriptorCounts = std::min(descriptorCounts, ctx.maxSampledImageDescriptors);
    }
    allocArrayInfo.pDescriptorCounts = &descriptorCounts;
    allocArrayInfo.descriptorSetCount = 1;

    // create a descriptor set that binds that buffer and update it
    VkDescriptorSet globalDescriptor =
        getCurrentFrame()._frameDescriptors.allocate(ctx.device, gpuSceneDataDescriptorLayout, &allocArrayInfo);

    DescriptorWriter writer;
    writer.write_buffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    // Shadow map (binding 1): take the descriptor we registered in texCache when creating the shadow
    {
        VkWriteDescriptorSet shadowSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        shadowSet.descriptorCount = 1;
        shadowSet.dstArrayElement = 0;
        shadowSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        shadowSet.dstBinding = 1;
        shadowSet.pImageInfo = &resources.texCache.Cache[_shadowTexId.Index];
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
        arraySet.pImageInfo = resources.texCache.Cache.data();
        writer.writes.push_back(arraySet);
    }

    writer.update_set(ctx.device, globalDescriptor);

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
                viewport.width = (float)swapchain.drawExtent.width;
                viewport.height = (float)swapchain.drawExtent.height;
                viewport.minDepth = 0.f;
                viewport.maxDepth = 1.f;

                vkCmdSetViewport(cmd, 0, 1, &viewport);

                VkRect2D scissor = {};
                scissor.offset.x = 0;
                scissor.offset.y = 0;
                scissor.extent.width = swapchain.drawExtent.width;
                scissor.extent.height = swapchain.drawExtent.height;

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

        _stats.drawcall_count++;
        _stats.triangle_count += r.indexCount / 3;
        vkCmdDrawIndexed(cmd, r.indexCount, 1, r.firstIndex, 0, 0);
    };

    _stats.drawcall_count = 0;
    _stats.triangle_count = 0;

    for (auto& r : opaque_draws)
    {
        draw(_drawCommands.OpaqueSurfaces[r]);
    }

    for (auto& r : _drawCommands.TransparentSurfaces)
    {
        draw(r);
    }

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
                    resizeRequested = true;
                }
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED)
                {
                    freezeRendering = true;
                }
                if (e.window.event == SDL_WINDOWEVENT_RESTORED)
                {
                    freezeRendering = false;
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

        if (freezeRendering)
            continue;

        if (resizeRequested)
        {
            resizeSwapchain();
        }

        _sunLight.processSDLEvent();
        scene.processSliderEvent();

        // calculate avg fps over 5 sec
        auto currframetime = std::chrono::high_resolution_clock::now();

        if (_stats.fps_frame_count == 0 && _stats.fps_window_start.time_since_epoch().count() == 0)
        {
            _stats.fps_window_start = currframetime;
        }

        _stats.fps_frame_count++;

        float elapsed_sec = std::chrono::duration<float>(currframetime - _stats.fps_window_start).count();

        if (elapsed_sec >= 5.0f)
        {
            _stats.avg_fps = _stats.fps_frame_count / elapsed_sec;
            _stats.fps_frame_count = 0;
            _stats.fps_window_start = currframetime;
        }

        // imgui new frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();

        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(15, 18), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(261, 190), ImGuiCond_FirstUseEver);
        ImGui::Begin("Stats");

        ImGui::Text("frametime %f ms", _stats.frametime);
        ImGui::Text("drawtime %f ms", _stats.mesh_draw_time);
        ImGui::Text("triangles %i", _stats.triangle_count);
        ImGui::Text("draws %i", _stats.drawcall_count);
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Text("avg FPS (5s): %.1f", _stats.avg_fps);
        ImGui::Separator();
        ImGui::SliderScalar("num of asteroids", ImGuiDataType_S32, &scene.numAsteroids, &scene.kSliderMin,
                            &scene.kSliderMax, "%u");
        // ImGui::Text("fence: %.2f ms", _stats.fence_time);
        // ImGui::Text("flush: %.2f ms", _stats.flush_time);
        // ImGui::Text("submit: %.2f ms", _stats.submit_time);
        // ImGui::Text("present: %.2f ms", _stats.present_time);
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
                {"J / K", "Increase / Decrease num of asteroids"},
                {"Left Shift", "Run / speed boost while moving"},
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

        scene.update(ctx.windowExtent, _drawCommands, _mainCamera, _sunLight);

        draw();

        auto end = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        _stats.frametime = elapsed.count() / 1000.f;
    }
}

FrameData& VulkanEngine::getCurrentFrame()
{
    return _frames[_frameNumber % FRAME_OVERLAP];
}

FrameData& VulkanEngine::getLastFrame()
{
    return _frames[(_frameNumber - 1) % FRAME_OVERLAP];
}

void VulkanEngine::initSwapchain()
{
    swapchain.init(ctx);
}

void VulkanEngine::resizeSwapchain()
{
    vkDeviceWaitIdle(ctx.device);
    swapchain.resize(ctx);
    resizeRequested = false;
}

void VulkanEngine::initCommands()
{
    // create a command pool for commands submitted to the graphics queue.
    // we also want the pool to allow for resetting of individual command buffers
    VkCommandPoolCreateInfo commandPoolInfo =
        vkinit::command_pool_create_info(ctx.graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (int i = 0; i < FRAME_OVERLAP; i++)
    {

        VK_CHECK(vkCreateCommandPool(ctx.device, &commandPoolInfo, nullptr, &_frames[i]._commandPool));

        // allocate the default command buffer that we will use for rendering
        VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1);

        VK_CHECK(vkAllocateCommandBuffers(ctx.device, &cmdAllocInfo, &_frames[i]._mainCommandBuffer));

        VkCommandPool cmdPool = _frames[i]._commandPool;
        _mainDeletionQueue.push_function([this, cmdPool]() { vkDestroyCommandPool(ctx.device, cmdPool, nullptr); });
    }
}

void VulkanEngine::initSyncStructures()
{
    // create syncronization structures
    // one fence to control when the gpu has finished rendering the frame,
    // and 2 semaphores to syncronize rendering with swapchain
    // we want the fence to start signalled so we can wait on it on the first
    // frame
    VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);

    for (int i = 0; i < FRAME_OVERLAP; i++)
    {

        VK_CHECK(vkCreateFence(ctx.device, &fenceCreateInfo, nullptr, &_frames[i]._renderFence));

        VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

        VK_CHECK(vkCreateSemaphore(ctx.device, &semaphoreCreateInfo, nullptr, &_frames[i]._swapchainSemaphore));
        // Per-frame renderSemaphore is not used anymore; present waits on per-image
        // semaphores

        VkFence renderFence = _frames[i]._renderFence;
        VkSemaphore swapSemaphore = _frames[i]._swapchainSemaphore;

        _frames[i].sceneDataBuffer = resources.createBuffer(
            ctx, sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        _frames[i].shadowSceneDataBuffer = resources.createBuffer(
            ctx, sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        _mainDeletionQueue.push_function(
            [this, renderFence, swapSemaphore, i]()
            {
                resources.destroyBuffer(ctx, _frames[i].sceneDataBuffer);
                resources.destroyBuffer(ctx, _frames[i].shadowSceneDataBuffer);
                vkDestroyFence(ctx.device, renderFence, nullptr);
                vkDestroySemaphore(ctx.device, swapSemaphore, nullptr);
                // no per-frame render semaphore
            });
    }
}

void VulkanEngine::initShadowResources()
{
    // 2D depth image for shadow map
    VkExtent3D size{_shadowExtent.width, _shadowExtent.height, 1};
    _shadowImage = resources.createImage(ctx, size, VK_FORMAT_D32_SFLOAT,
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

    VK_CHECK(vkCreateSampler(ctx.device, &sci, nullptr, &_shadowSampler));

    // Register in texture cache so we can sample it (set=0, binding=1 array)
    _shadowTexId = resources.texCache.AddTexture(_shadowImage.imageView, _shadowSampler);

    _mainDeletionQueue.push_function(
        [=, this]()
        {
            if (_shadowSampler)
                vkDestroySampler(ctx.device, _shadowSampler, nullptr);
            resources.destroyImage(ctx, _shadowImage);
        });
}

void VulkanEngine::initShadowPipeline()
{
    VkShaderModule meshVertexShader;
    if (!vkutil::load_shader_module(shader_path("shadow_map.vert.spv").c_str(), ctx.device, &meshVertexShader))
    {
        fmt::println("Error when building shadow vertex shader module");
    }

    // Layout: only global set (set=0) + same push constants as mesh
    VkPushConstantRange range{};
    range.offset = 0;
    range.size = sizeof(GPUDrawPushConstants);
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayout sets[] = {gpuSceneDataDescriptorLayout};

    VkPipelineLayoutCreateInfo plci = vkinit::pipeline_layout_create_info();
    plci.setLayoutCount = 1;
    plci.pSetLayouts = sets;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &range;

    VK_CHECK(vkCreatePipelineLayout(ctx.device, &plci, nullptr, &_shadowPipelineLayout));

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

    _shadowPipeline = pb.build_pipeline(ctx.device);

    vkDestroyShaderModule(ctx.device, meshVertexShader, nullptr);

    _mainDeletionQueue.push_function(
        [=, this]()
        {
            if (_shadowPipeline)
                vkDestroyPipeline(ctx.device, _shadowPipeline, nullptr);
            if (_shadowPipelineLayout)
                vkDestroyPipelineLayout(ctx.device, _shadowPipelineLayout, nullptr);
        });
}

void VulkanEngine::initImgui()
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
    VK_CHECK(vkCreateDescriptorPool(ctx.device, &pool_info, nullptr, &imguiPool));

    // 2: initialize imgui library

    // this initializes the core structures of imgui
    ImGui::CreateContext();

    // this initializes imgui for SDL
    ImGui_ImplSDL2_InitForVulkan(ctx.window);

    // this initializes imgui for Vulkan
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = ctx.instance;
    init_info.PhysicalDevice = ctx.chosenGPU;
    init_info.Device = ctx.device;
    init_info.Queue = ctx.graphicsQueue;
    init_info.DescriptorPool = imguiPool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.UseDynamicRendering = true;

    // dynamic rendering parameters for imgui to use
    init_info.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &swapchain.imageFormat;

    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info);

    ImGui_ImplVulkan_CreateFontsTexture();

    // add the destroy the imgui created structures
    _mainDeletionQueue.push_function(
        [this, imguiPool]()
        {
            ImGui_ImplVulkan_Shutdown();
            vkDestroyDescriptorPool(ctx.device, imguiPool, nullptr);
        });
}

void VulkanEngine::initPipelines()
{
    // COMPUTE PIPELINES
    initBackgroundPipelines();
    initShadowPipeline();
    initLightDebugPipeline();
    initDebugTexturePipeline();
    metalRoughMaterial.build_pipelines(ctx.device, gpuSceneDataDescriptorLayout, swapchain.drawImage.imageFormat,
                                       swapchain.depthImage.imageFormat);
}

void VulkanEngine::initLightDebugPipeline()
{
    // Reuse mesh vertex shader (push constants + buffer reference)
    VkShaderModule vtx;
    if (!vkutil::load_shader_module(shader_path("mesh.vert.spv").c_str(), ctx.device, &vtx))
    {
        fmt::println("Error when building debug vertex shader module");
    }
    VkShaderModule frag;
    if (!vkutil::load_shader_module(shader_path("debug_light.frag.spv").c_str(), ctx.device, &frag))
    {
        fmt::println("Error when building debug fragment shader module");
    }

    VkPushConstantRange range{};
    range.offset = 0;
    range.size = sizeof(GPUDrawPushConstants);
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayout sets[] = {gpuSceneDataDescriptorLayout};

    VkPipelineLayoutCreateInfo plci = vkinit::pipeline_layout_create_info();
    plci.setLayoutCount = 1;
    plci.pSetLayouts = sets;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &range;

    VK_CHECK(vkCreatePipelineLayout(ctx.device, &plci, nullptr, &_lightDebugPipelineLayout));

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
    pb.set_color_attachment_format(swapchain.drawImage.imageFormat);
    pb.set_depth_format(swapchain.depthImage.imageFormat);
    pb._pipelineLayout = _lightDebugPipelineLayout;

    _lightDebugPipeline = pb.build_pipeline(ctx.device);

    vkDestroyShaderModule(ctx.device, vtx, nullptr);
    vkDestroyShaderModule(ctx.device, frag, nullptr);

    _mainDeletionQueue.push_function(
        [=, this]()
        {
            if (_lightDebugPipeline)
                vkDestroyPipeline(ctx.device, _lightDebugPipeline, nullptr);
            if (_lightDebugPipelineLayout)
                vkDestroyPipelineLayout(ctx.device, _lightDebugPipelineLayout, nullptr);
        });
}

void VulkanEngine::initDebugTexturePipeline()
{
    // Reuse mesh vertex shader (push constants + buffer reference)
    VkShaderModule vtx;
    if (!vkutil::load_shader_module(shader_path("debug_shadow_map.vert.spv").c_str(), ctx.device, &vtx))
    {
        fmt::println("Error when building debug vertex shader module");
    }
    VkShaderModule frag;
    if (!vkutil::load_shader_module(shader_path("debug_shadow_map.frag.spv").c_str(), ctx.device, &frag))
    {
        fmt::println("Error when building debug fragment shader module");
    }

    VkPushConstantRange range{};
    range.offset = 0;
    range.size = sizeof(GPUDrawPushConstants);
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayout sets[] = {gpuSceneDataDescriptorLayout};

    VkPipelineLayoutCreateInfo plci = vkinit::pipeline_layout_create_info();
    plci.setLayoutCount = 1;
    plci.pSetLayouts = sets;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &range;

    VK_CHECK(vkCreatePipelineLayout(ctx.device, &plci, nullptr, &_textureDebugPipelineLayout));

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
    pb.set_color_attachment_format(swapchain.drawImage.imageFormat);
    pb.set_depth_format(swapchain.depthImage.imageFormat);
    pb._pipelineLayout = _textureDebugPipelineLayout;

    _textureDebugPipeline = pb.build_pipeline(ctx.device);

    vkDestroyShaderModule(ctx.device, vtx, nullptr);
    vkDestroyShaderModule(ctx.device, frag, nullptr);

    _mainDeletionQueue.push_function(
        [=, this]()
        {
            if (_textureDebugPipeline)
                vkDestroyPipeline(ctx.device, _textureDebugPipeline, nullptr);
            if (_textureDebugPipelineLayout)
                vkDestroyPipelineLayout(ctx.device, _textureDebugPipelineLayout, nullptr);
        });
}

void VulkanEngine::initDescriptors()
{
    // create a descriptor pool
    std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
    };

    _globalDescriptorAllocator.init_pool(ctx.device, 10, sizes);
    _mainDeletionQueue.push_function(
        [&]() { vkDestroyDescriptorPool(ctx.device, _globalDescriptorAllocator.pool, nullptr); });

    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        _drawImageDescriptorLayout = builder.build(ctx.device, VK_SHADER_STAGE_COMPUTE_BIT);
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
        vkGetPhysicalDeviceProperties(ctx.chosenGPU, &props);

        VkPhysicalDeviceDescriptorIndexingProperties indexingProps{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES};
        VkPhysicalDeviceProperties2 props2{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
                                           .pNext = &indexingProps};
        vkGetPhysicalDeviceProperties2(ctx.chosenGPU, &props2);

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
        ctx.maxSampledImageDescriptors = colorDescriptorCap;
        resources.texCache.set_max(ctx.maxSampledImageDescriptors);

        bindFlags.bindingCount = static_cast<uint32_t>(flagArray.size());
        bindFlags.pBindingFlags = flagArray.data();

        gpuSceneDataDescriptorLayout =
            builder.build(ctx.device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, &bindFlags,
                          VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);
    }

    _mainDeletionQueue.push_function(
        [&]()
        {
            vkDestroyDescriptorSetLayout(ctx.device, _drawImageDescriptorLayout, nullptr);
            vkDestroyDescriptorSetLayout(ctx.device, gpuSceneDataDescriptorLayout, nullptr);
        });

    _drawImageDescriptors = _globalDescriptorAllocator.allocate(ctx.device, _drawImageDescriptorLayout);
    {
        DescriptorWriter writer;
        writer.write_image(0, swapchain.drawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL,
                           VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        writer.update_set(ctx.device, _drawImageDescriptors);
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
        _frames[i]._frameDescriptors.init(ctx.device, 1000, frame_sizes,
                                          VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);
        _mainDeletionQueue.push_function([&, i]() { _frames[i]._frameDescriptors.destroy_pools(ctx.device); });
    }
}
