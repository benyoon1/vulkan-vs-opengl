#include "vk_material.h"

#include "vk_context.h"
#include "vk_initializers.h"
#include "vk_pipelines.h"

void GLTFMetallic_Roughness::build_pipelines(VkDevice device, VkDescriptorSetLayout sceneDataLayout,
                                             VkFormat drawImageFormat, VkFormat depthImageFormat)
{
    VkShaderModule meshFragShader;
    if (!vkutil::load_shader_module(shader_path("basic_phong.frag.spv").c_str(), device, &meshFragShader))
    {
        fmt::println("Error when building the triangle fragment shader module");
    }

    VkShaderModule meshVertexShader;
    if (!vkutil::load_shader_module(shader_path("mesh.vert.spv").c_str(), device, &meshVertexShader))
    {
        fmt::println("Error when building the triangle vertex shader module");
    }

    VkPushConstantRange matrixRange{};
    matrixRange.offset = 0;
    matrixRange.size = sizeof(GPUDrawPushConstants);
    matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    materialLayout = layoutBuilder.build(device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    VkDescriptorSetLayout layouts[] = {sceneDataLayout, materialLayout};

    VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
    mesh_layout_info.setLayoutCount = 2;
    mesh_layout_info.pSetLayouts = layouts;
    mesh_layout_info.pPushConstantRanges = &matrixRange;
    mesh_layout_info.pushConstantRangeCount = 1;

    VkPipelineLayout newLayout;
    VK_CHECK(vkCreatePipelineLayout(device, &mesh_layout_info, nullptr, &newLayout));

    opaquePipeline.layout = newLayout;
    transparentPipeline.layout = newLayout;

    PipelineBuilder pipelineBuilder;

    pipelineBuilder.set_shaders(meshVertexShader, meshFragShader);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.disable_blending();
    pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

    pipelineBuilder.set_color_attachment_format(drawImageFormat);
    pipelineBuilder.set_depth_format(depthImageFormat);

    pipelineBuilder._pipelineLayout = newLayout;

    opaquePipeline.pipeline = pipelineBuilder.build_pipeline(device);

    pipelineBuilder.enable_blending_additive();
    pipelineBuilder.enable_depthtest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);

    transparentPipeline.pipeline = pipelineBuilder.build_pipeline(device);

    vkDestroyShaderModule(device, meshFragShader, nullptr);
    vkDestroyShaderModule(device, meshVertexShader, nullptr);
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
