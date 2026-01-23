#include "stb_image.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <filesystem>
#include <fmt/format.h>
#include <fmt/printf.h>
#include <iostream>
#include <limits>
#include <vk_loader.h>

#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_types.h"
#include <glm/gtx/quaternion.hpp>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/util.hpp>

namespace
{
glm::mat4 ai_to_glm(const aiMatrix4x4& matrix)
{
    glm::mat4 result{1.0f};
    result[0][0] = matrix.a1;
    result[0][1] = matrix.a2;
    result[0][2] = matrix.a3;
    result[0][3] = matrix.a4;

    result[1][0] = matrix.b1;
    result[1][1] = matrix.b2;
    result[1][2] = matrix.b3;
    result[1][3] = matrix.b4;

    result[2][0] = matrix.c1;
    result[2][1] = matrix.c2;
    result[2][2] = matrix.c3;
    result[2][3] = matrix.c4;

    result[3][0] = matrix.d1;
    result[3][1] = matrix.d2;
    result[3][2] = matrix.d3;
    result[3][3] = matrix.d4;

    return result;
}

std::optional<AllocatedImage> load_texture_from_disk(VulkanEngine* engine, const std::filesystem::path& texturePath)
{
    int width = 0;
    int height = 0;
    int channels = 0;

    stbi_uc* pixels = stbi_load(texturePath.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels)
    {
        fmt::print("Failed to load texture '{}': {}\n", texturePath.string(), stbi_failure_reason());
        return std::nullopt;
    }

    VkExtent3D size{static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};

    // Generate mipmaps for smoother minification and to avoid grainy aliasing
    AllocatedImage image =
        engine->create_image(pixels, size, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, true);
    stbi_image_free(pixels);

    return image;
}

std::shared_ptr<Node> build_assimp_node(const aiNode* ainode, LoadedGLTF& file,
                                        const std::vector<std::shared_ptr<MeshAsset>>& meshAssets,
                                        size_t& unnamedCounter)
{
    auto node = std::make_shared<Node>();
    node->localTransform = ai_to_glm(ainode->mTransformation);

    std::string nodeName = ainode->mName.C_Str();
    if (nodeName.empty())
    {
        nodeName = fmt::format("assimp_node_{}", unnamedCounter++);
    }
    file.nodes[nodeName] = node;

    for (unsigned int meshIdx = 0; meshIdx < ainode->mNumMeshes; ++meshIdx)
    {
        uint32_t sceneMeshIndex = ainode->mMeshes[meshIdx];
        if (sceneMeshIndex >= meshAssets.size())
        {
            continue;
        }

        auto meshPtr = meshAssets[sceneMeshIndex];
        if (!meshPtr)
        {
            continue;
        }

        auto meshNode = std::make_shared<MeshNode>();
        meshNode->mesh = meshPtr;
        meshNode->localTransform = glm::mat4{1.0f};
        meshNode->parent = node;

        std::string meshNodeName = fmt::format("{}_mesh_{}", nodeName, meshIdx);
        file.nodes[meshNodeName] = meshNode;

        node->children.push_back(meshNode);
    }

    for (unsigned int childIdx = 0; childIdx < ainode->mNumChildren; ++childIdx)
    {
        auto childNode = build_assimp_node(ainode->mChildren[childIdx], file, meshAssets, unnamedCounter);
        childNode->parent = node;
        node->children.push_back(childNode);
    }

    return node;
}
} // namespace

std::optional<std::shared_ptr<LoadedGLTF>> loadAssimpAssets(VulkanEngine* engine, std::string_view filePath)
{
    fmt::print("Loading Assimp scene: {}\n", filePath);

    Assimp::Importer importer;
    const unsigned int importFlags = aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_ImproveCacheLocality |
                                     aiProcess_JoinIdenticalVertices | aiProcess_CalcTangentSpace |
                                     aiProcess_GenBoundingBoxes | aiProcess_SortByPType | aiProcess_FlipUVs |
                                     aiProcess_OptimizeMeshes | aiProcess_OptimizeGraph;
    const aiScene* scene = importer.ReadFile(std::string(filePath), importFlags);

    if (!scene || !scene->mRootNode || !scene->HasMeshes())
    {
        fmt::print("Failed to load scene '{}': {}\n", filePath, importer.GetErrorString());
        return std::nullopt;
    }

    auto loaded = std::make_shared<LoadedGLTF>();
    loaded->creator = engine;

    const std::filesystem::path scenePath{filePath};
    const std::filesystem::path sceneDirectory = scenePath.parent_path();

    size_t materialCount = std::max<size_t>(scene->mNumMaterials, 1);

    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> poolSizes = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
                                                                         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
                                                                         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}};
    loaded->descriptorPool.init(engine->device, static_cast<uint32_t>(materialCount), poolSizes);

    loaded->materialDataBuffer =
        engine->create_buffer(sizeof(GLTFMetallic_Roughness::MaterialConstants) * materialCount,
                              VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    auto* materialConstants =
        reinterpret_cast<GLTFMetallic_Roughness::MaterialConstants*>(loaded->materialDataBuffer.info.pMappedData);

    std::vector<std::shared_ptr<GLTFMaterial>> materials;
    materials.reserve(materialCount);

    for (size_t i = 0; i < materialCount; ++i)
    {
        const aiMaterial* aiMat = (i < scene->mNumMaterials) ? scene->mMaterials[i] : nullptr;

        auto mat = std::make_shared<GLTFMaterial>();
        materials.push_back(mat);

        std::string materialName =
            aiMat && aiMat->GetName().length ? aiMat->GetName().C_Str() : fmt::format("assimp_mat_{}", i);
        loaded->materials[materialName] = mat;

        GLTFMetallic_Roughness::MaterialConstants constants{};
        constants.colorFactors = glm::vec4(1.0f);
        constants.metal_rough_factors = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);

        GLTFMetallic_Roughness::MaterialResources resources{};
        resources.colorImage = engine->whiteImage;
        resources.colorSampler = engine->defaultSamplerLinear;
        resources.metalRoughImage = engine->whiteImage;
        resources.metalRoughSampler = engine->defaultSamplerLinear;
        resources.dataBuffer = loaded->materialDataBuffer.buffer;
        resources.dataBufferOffset = static_cast<uint32_t>(i * sizeof(GLTFMetallic_Roughness::MaterialConstants));

        if (aiMat)
        {
            aiColor4D diffuse{};
            if (AI_SUCCESS == aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse))
            {
                constants.colorFactors = glm::vec4(diffuse.r, diffuse.g, diffuse.b, diffuse.a);
            }

            aiString texPath;
            if (AI_SUCCESS == aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath))
            {
                std::filesystem::path resolved = sceneDirectory / texPath.C_Str();
                resolved = resolved.lexically_normal();

                auto it = loaded->images.find(resolved.string());
                if (it == loaded->images.end())
                {
                    auto img = load_texture_from_disk(engine, resolved);
                    if (img.has_value())
                    {
                        it = loaded->images.emplace(resolved.string(), *img).first;
                    }
                    else
                    {
                        fmt::print("Warning: failed to load texture '{}'. Using fallback.\n", resolved.string());
                    }
                }

                if (it != loaded->images.end())
                {
                    resources.colorImage = it->second;
                }
            }
        }

        constants.colorTexID =
            engine->texCache.AddTexture(resources.colorImage.imageView, resources.colorSampler).Index;
        constants.metalRoughTexID =
            engine->texCache.AddTexture(resources.metalRoughImage.imageView, resources.metalRoughSampler).Index;

        materialConstants[i] = constants;
        mat->data = engine->metalRoughMaterial.write_material(engine->device, MaterialPass::MainColor, resources,
                                                              loaded->descriptorPool);
    }

    std::vector<std::shared_ptr<MeshAsset>> meshAssets(scene->mNumMeshes);

    for (unsigned int meshIdx = 0; meshIdx < scene->mNumMeshes; ++meshIdx)
    {
        const aiMesh* mesh = scene->mMeshes[meshIdx];
        if (!mesh)
        {
            continue;
        }

        auto asset = std::make_shared<MeshAsset>();
        asset->name = mesh->mName.length ? mesh->mName.C_Str() : fmt::format("assimp_mesh_{}", meshIdx);

        std::vector<Vertex> vertices(mesh->mNumVertices);
        glm::vec3 minPos(std::numeric_limits<float>::max());
        glm::vec3 maxPos(-std::numeric_limits<float>::max());

        for (unsigned int v = 0; v < mesh->mNumVertices; ++v)
        {
            Vertex vertex{};
            const aiVector3D& pos = mesh->mVertices[v];
            vertex.position = glm::vec3(pos.x, pos.y, pos.z);
            vertex.normal = mesh->HasNormals()
                                ? glm::vec3(mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z)
                                : glm::vec3(0.0f, 1.0f, 0.0f);

            if (mesh->HasTextureCoords(0))
            {
                vertex.uv_x = mesh->mTextureCoords[0][v].x;
                vertex.uv_y = mesh->mTextureCoords[0][v].y;
            }
            else
            {
                vertex.uv_x = 0.0f;
                vertex.uv_y = 0.0f;
            }

            if (mesh->HasVertexColors(0))
            {
                const aiColor4D& c = mesh->mColors[0][v];
                vertex.color = glm::vec4(c.r, c.g, c.b, c.a);
            }
            else
            {
                vertex.color = glm::vec4(1.0f);
            }

            minPos = glm::min(minPos, vertex.position);
            maxPos = glm::max(maxPos, vertex.position);

            vertices[v] = vertex;
        }

        std::vector<uint32_t> indices;
        indices.reserve(mesh->mNumFaces * 3);
        for (unsigned int f = 0; f < mesh->mNumFaces; ++f)
        {
            const aiFace& face = mesh->mFaces[f];
            if (face.mNumIndices != 3)
            {
                continue;
            }
            indices.push_back(face.mIndices[0]);
            indices.push_back(face.mIndices[1]);
            indices.push_back(face.mIndices[2]);
        }

        if (vertices.empty() || indices.empty())
        {
            fmt::print("Warning: mesh '{}' has no triangles after processing. Skipping.\n", asset->name);
            continue;
        }

        GeoSurface surface{};
        surface.startIndex = 0;
        surface.count = static_cast<uint32_t>(indices.size());
        surface.bounds.origin = (maxPos + minPos) * 0.5f;
        surface.bounds.extents = (maxPos - minPos) * 0.5f;
        surface.bounds.sphereRadius = glm::length(surface.bounds.extents);

        if (!materials.empty())
        {
            uint32_t matIndex = std::min<uint32_t>(mesh->mMaterialIndex, static_cast<uint32_t>(materials.size() - 1));
            surface.material = materials[matIndex];
        }

        asset->surfaces.push_back(surface);
        asset->meshBuffers = engine->uploadMesh(indices, vertices);

        loaded->meshes[asset->name] = asset;
        meshAssets[meshIdx] = asset;
    }

    if (scene->mRootNode)
    {
        size_t unnamedCounter = 0;
        auto root = build_assimp_node(scene->mRootNode, *loaded, meshAssets, unnamedCounter);
        loaded->topNodes.push_back(root);
        root->refreshTransform(glm::mat4{1.0f});
    }

    return loaded;
}

std::optional<AllocatedImage> load_image(VulkanEngine* engine, fastgltf::Asset& asset, fastgltf::Image& image)
{
    AllocatedImage newImage{};

    int width, height, nrChannels;

    std::visit(
        fastgltf::visitor{
            [](auto& arg) {},
            [&](fastgltf::sources::URI& filePath)
            {
                assert(filePath.fileByteOffset == 0); // We don't support offsets with stbi.
                assert(filePath.uri.isLocalPath());   // We're only capable of loading
                                                      // local files.

                const std::string path(filePath.uri.path().begin(),
                                       filePath.uri.path().end()); // Thanks C++.
                unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 4);
                if (data)
                {
                    VkExtent3D imagesize;
                    imagesize.width = width;
                    imagesize.height = height;
                    imagesize.depth = 1;

                    // Enable mipmaps when loading glTF textures
                    newImage = engine->create_image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
                                                    VK_IMAGE_USAGE_SAMPLED_BIT, true);

                    stbi_image_free(data);
                }
            },
            [&](fastgltf::sources::Vector& vector)
            {
                unsigned char* data = stbi_load_from_memory(vector.bytes.data(), static_cast<int>(vector.bytes.size()),
                                                            &width, &height, &nrChannels, 4);
                if (data)
                {
                    VkExtent3D imagesize;
                    imagesize.width = width;
                    imagesize.height = height;
                    imagesize.depth = 1;

                    newImage = engine->create_image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
                                                    VK_IMAGE_USAGE_SAMPLED_BIT, true);

                    stbi_image_free(data);
                }
            },
            [&](fastgltf::sources::BufferView& view)
            {
                auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                auto& buffer = asset.buffers[bufferView.bufferIndex];

                std::visit(
                    fastgltf::visitor{// We only care about VectorWithMime here, because we
                                      // specify LoadExternalBuffers, meaning all buffers
                                      // are already loaded into a vector.
                                      [](auto& arg) {},
                                      [&](fastgltf::sources::Vector& vector)
                                      {
                                          unsigned char* data = stbi_load_from_memory(
                                              vector.bytes.data() + bufferView.byteOffset,
                                              static_cast<int>(bufferView.byteLength), &width, &height, &nrChannels, 4);
                                          if (data)
                                          {
                                              VkExtent3D imagesize;
                                              imagesize.width = width;
                                              imagesize.height = height;
                                              imagesize.depth = 1;

                                              newImage = engine->create_image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
                                                                              VK_IMAGE_USAGE_SAMPLED_BIT, true);

                                              stbi_image_free(data);
                                          }
                                      }},
                    buffer.data);
            },
        },
        image.data);

    // if any of the attempts to load the data failed, we havent written the image
    // so handle is null
    if (newImage.image == VK_NULL_HANDLE)
    {
        return {};
    }
    else
    {
        return newImage;
    }
}

VkFilter extract_filter(fastgltf::Filter filter)
{
    switch (filter)
    {
        // nearest samplers
        case fastgltf::Filter::Nearest:
        case fastgltf::Filter::NearestMipMapNearest:
        case fastgltf::Filter::NearestMipMapLinear:
            return VK_FILTER_NEAREST;

        // linear samplers
        case fastgltf::Filter::Linear:
        case fastgltf::Filter::LinearMipMapNearest:
        case fastgltf::Filter::LinearMipMapLinear:
        default:
            return VK_FILTER_LINEAR;
    }
}

VkSamplerMipmapMode extract_mipmap_mode(fastgltf::Filter filter)
{
    switch (filter)
    {
        case fastgltf::Filter::NearestMipMapNearest:
        case fastgltf::Filter::LinearMipMapNearest:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;

        case fastgltf::Filter::NearestMipMapLinear:
        case fastgltf::Filter::LinearMipMapLinear:
        default:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
}

std::optional<std::shared_ptr<LoadedGLTF>> loadGltf(VulkanEngine* engine, std::string_view filePath)
{
    fmt::print("Loading GLTF: {}\n", filePath);

    std::shared_ptr<LoadedGLTF> scene = std::make_shared<LoadedGLTF>();
    scene->creator = engine;
    LoadedGLTF& file = *scene.get();

    fastgltf::Parser parser{};

    constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble |
                                 fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;
    // fastgltf::Options::LoadExternalImages;

    fastgltf::GltfDataBuffer data;

    std::filesystem::path path = filePath;
    if (!data.loadFromFile(path))
    {
        fmt::print("Failed to open glTF file '{}'.\n", path.string());
        return {};
    }

    fastgltf::Asset gltf;

    auto type = fastgltf::determineGltfFileType(&data);
    if (type == fastgltf::GltfType::glTF)
    {
        auto load = parser.loadGLTF(&data, path.parent_path(), gltfOptions);
        if (load)
        {
            gltf = std::move(load.get());
        }
        else
        {
            std::cerr << "Failed to load glTF: " << fastgltf::to_underlying(load.error()) << std::endl;
            return {};
        }
    }
    else if (type == fastgltf::GltfType::GLB)
    {
        auto load = parser.loadBinaryGLTF(&data, path.parent_path(), gltfOptions);
        if (load)
        {
            gltf = std::move(load.get());
        }
        else
        {
            std::cerr << "Failed to load glTF: " << fastgltf::to_underlying(load.error()) << std::endl;
            return {};
        }
    }
    else
    {
        std::cerr << "Failed to determine glTF container" << std::endl;
        return {};
    }
    // we can stimate the descriptors we will need accurately
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
                                                                     {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
                                                                     {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}};

    file.descriptorPool.init(engine->device, gltf.materials.size(), sizes);

    // load samplers
    for (fastgltf::Sampler& sampler : gltf.samplers)
    {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(engine->chosenGPU, &props);

        VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr};
        sampl.maxLod = VK_LOD_CLAMP_NONE;
        sampl.minLod = 0;
        sampl.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampl.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampl.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

        sampl.magFilter = extract_filter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
        sampl.minFilter = extract_filter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        sampl.mipmapMode = extract_mipmap_mode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        // Enable anisotropy when available (feature enabled at device init)
        sampl.anisotropyEnable = VK_TRUE;
        sampl.maxAnisotropy = std::min(16.0f, props.limits.maxSamplerAnisotropy);

        VkSampler newSampler;
        vkCreateSampler(engine->device, &sampl, nullptr, &newSampler);

        file.samplers.push_back(newSampler);
    }

    // temporal arrays for all the objects to use while creating the GLTF data
    std::vector<std::shared_ptr<MeshAsset>> meshes;
    std::vector<std::shared_ptr<Node>> nodes;
    std::vector<AllocatedImage> images;
    std::vector<TextureID> imageIDs;
    std::vector<std::shared_ptr<GLTFMaterial>> materials;

    // load all textures
    for (fastgltf::Image& image : gltf.images)
    {
        std::optional<AllocatedImage> img = load_image(engine, gltf, image);

        if (img.has_value())
        {
            images.push_back(*img);
            file.images[image.name.c_str()] = *img;
            // imageIDs.push_back( engine->texCache.AddTexture(materialResources.colorImage.imageView,
            // materialResources.colorSampler, );
        }
        else
        {
            // we failed to load, so lets give the slot a default white texture to not
            // completely break loading
            images.push_back(engine->errorCheckerboardImage);
            std::cout << "gltf failed to load texture " << image.name << std::endl;
        }
    }

    //> load_buffer
    // create buffer to hold the material data
    file.materialDataBuffer =
        engine->create_buffer(sizeof(GLTFMetallic_Roughness::MaterialConstants) * gltf.materials.size(),
                              VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    int data_index = 0;
    GLTFMetallic_Roughness::MaterialConstants* sceneMaterialConstants =
        (GLTFMetallic_Roughness::MaterialConstants*)file.materialDataBuffer.info.pMappedData;
    //< load_buffer
    //
    //> load_material
    for (fastgltf::Material& mat : gltf.materials)
    {
        std::shared_ptr<GLTFMaterial> newMat = std::make_shared<GLTFMaterial>();
        materials.push_back(newMat);
        file.materials[mat.name.c_str()] = newMat;

        GLTFMetallic_Roughness::MaterialConstants constants;
        constants.colorFactors.x = mat.pbrData.baseColorFactor[0];
        constants.colorFactors.y = mat.pbrData.baseColorFactor[1];
        constants.colorFactors.z = mat.pbrData.baseColorFactor[2];
        constants.colorFactors.w = mat.pbrData.baseColorFactor[3];

        constants.metal_rough_factors.x = mat.pbrData.metallicFactor;
        constants.metal_rough_factors.y = mat.pbrData.roughnessFactor;

        MaterialPass passType = MaterialPass::MainColor;
        if (mat.alphaMode == fastgltf::AlphaMode::Blend)
        {
            passType = MaterialPass::Transparent;
        }

        GLTFMetallic_Roughness::MaterialResources materialResources;
        // default the material textures
        materialResources.colorImage = engine->whiteImage;
        materialResources.colorSampler = engine->defaultSamplerLinear;
        materialResources.metalRoughImage = engine->whiteImage;
        materialResources.metalRoughSampler = engine->defaultSamplerLinear;

        // set the uniform buffer for the material data
        materialResources.dataBuffer = file.materialDataBuffer.buffer;
        materialResources.dataBufferOffset = data_index * sizeof(GLTFMetallic_Roughness::MaterialConstants);
        // grab textures from gltf file
        if (mat.pbrData.baseColorTexture.has_value())
        {
            size_t img = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
            size_t sampler = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();

            materialResources.colorImage = images[img];
            materialResources.colorSampler = file.samplers[sampler];
        }

        constants.colorTexID =
            engine->texCache.AddTexture(materialResources.colorImage.imageView, materialResources.colorSampler).Index;
        constants.metalRoughTexID =
            engine->texCache
                .AddTexture(materialResources.metalRoughImage.imageView, materialResources.metalRoughSampler)
                .Index;

        // write material parameters to buffer
        sceneMaterialConstants[data_index] = constants;
        // build material
        newMat->data =
            engine->metalRoughMaterial.write_material(engine->device, passType, materialResources, file.descriptorPool);

        data_index++;
    }

    // use the same vectors for all meshes so that the memory doesnt reallocate as
    // often
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;

    for (fastgltf::Mesh& mesh : gltf.meshes)
    {
        std::shared_ptr<MeshAsset> newmesh = std::make_shared<MeshAsset>();
        meshes.push_back(newmesh);
        file.meshes[mesh.name.c_str()] = newmesh;
        newmesh->name = mesh.name;

        // clear the mesh arrays each mesh, we dont want to merge them by error
        indices.clear();
        vertices.clear();

        for (auto&& p : mesh.primitives)
        {
            GeoSurface newSurface;
            newSurface.startIndex = (uint32_t)indices.size();
            newSurface.count = (uint32_t)gltf.accessors[p.indicesAccessor.value()].count;

            size_t initial_vtx = vertices.size();

            // load indexes
            {
                fastgltf::Accessor& indexaccessor = gltf.accessors[p.indicesAccessor.value()];
                indices.reserve(indices.size() + indexaccessor.count);

                fastgltf::iterateAccessor<std::uint32_t>(
                    gltf, indexaccessor, [&](std::uint32_t idx) { indices.push_back(idx + initial_vtx); });
            }

            // load vertex positions
            {
                fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->second];
                vertices.resize(vertices.size() + posAccessor.count);

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
                                                              [&](glm::vec3 v, size_t index)
                                                              {
                                                                  Vertex newvtx;
                                                                  newvtx.position = v;
                                                                  newvtx.normal = {1, 0, 0};
                                                                  newvtx.color = glm::vec4{1.f};
                                                                  newvtx.uv_x = 0;
                                                                  newvtx.uv_y = 0;
                                                                  vertices[initial_vtx + index] = newvtx;
                                                              });
            }

            // load vertex normals
            auto normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end())
            {

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[(*normals).second],
                                                              [&](glm::vec3 v, size_t index)
                                                              { vertices[initial_vtx + index].normal = v; });
            }

            // load UVs
            auto uv = p.findAttribute("TEXCOORD_0");
            if (uv != p.attributes.end())
            {

                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uv).second],
                                                              [&](glm::vec2 v, size_t index)
                                                              {
                                                                  vertices[initial_vtx + index].uv_x = v.x;
                                                                  vertices[initial_vtx + index].uv_y = v.y;
                                                              });
            }

            // load vertex colors
            auto colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end())
            {

                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*colors).second],
                                                              [&](glm::vec4 v, size_t index)
                                                              { vertices[initial_vtx + index].color = v; });
            }

            if (p.materialIndex.has_value())
            {
                newSurface.material = materials[p.materialIndex.value()];
            }
            else
            {
                newSurface.material = materials[0];
            }

            glm::vec3 minpos = vertices[initial_vtx].position;
            glm::vec3 maxpos = vertices[initial_vtx].position;
            for (int i = initial_vtx; i < vertices.size(); i++)
            {
                minpos = glm::min(minpos, vertices[i].position);
                maxpos = glm::max(maxpos, vertices[i].position);
            }

            newSurface.bounds.origin = (maxpos + minpos) / 2.f;
            newSurface.bounds.extents = (maxpos - minpos) / 2.f;
            newSurface.bounds.sphereRadius = glm::length(newSurface.bounds.extents);
            newmesh->surfaces.push_back(newSurface);
        }

        newmesh->meshBuffers = engine->uploadMesh(indices, vertices);
    }
    //> load_nodes
    // load all nodes and their meshes
    for (fastgltf::Node& node : gltf.nodes)
    {
        std::shared_ptr<Node> newNode;

        // find if the node has a mesh, and if it does hook it to the mesh pointer and allocate it with the meshnode
        // class
        if (node.meshIndex.has_value())
        {
            newNode = std::make_shared<MeshNode>();
            static_cast<MeshNode*>(newNode.get())->mesh = meshes[*node.meshIndex];
        }
        else
        {
            newNode = std::make_shared<Node>();
        }

        nodes.push_back(newNode);
        file.nodes[node.name.c_str()];

        std::visit(fastgltf::visitor{[&](fastgltf::Node::TransformMatrix matrix)
                                     { memcpy(&newNode->localTransform, matrix.data(), sizeof(matrix)); },
                                     [&](fastgltf::Node::TRS transform)
                                     {
                                         glm::vec3 tl(transform.translation[0], transform.translation[1],
                                                      transform.translation[2]);
                                         glm::quat rot(transform.rotation[3], transform.rotation[0],
                                                       transform.rotation[1], transform.rotation[2]);
                                         glm::vec3 sc(transform.scale[0], transform.scale[1], transform.scale[2]);

                                         glm::mat4 tm = glm::translate(glm::mat4(1.f), tl);
                                         glm::mat4 rm = glm::toMat4(rot);
                                         glm::mat4 sm = glm::scale(glm::mat4(1.f), sc);

                                         newNode->localTransform = tm * rm * sm;
                                     }},
                   node.transform);
    }
    //< load_nodes
    //> load_graph
    // run loop again to setup transform hierarchy
    for (int i = 0; i < gltf.nodes.size(); i++)
    {
        fastgltf::Node& node = gltf.nodes[i];
        std::shared_ptr<Node>& sceneNode = nodes[i];

        for (auto& c : node.children)
        {
            sceneNode->children.push_back(nodes[c]);
            nodes[c]->parent = sceneNode;
        }
    }

    // find the top nodes, with no parents
    for (auto& node : nodes)
    {
        if (node->parent.lock() == nullptr)
        {
            file.topNodes.push_back(node);
            node->refreshTransform(glm::mat4{1.f});
        }
    }
    return scene;
    //< load_graph
}

void LoadedGLTF::Draw(const glm::mat4& topMatrix, DrawContext& ctx)
{
    // create renderables from the scenenodes
    for (auto& n : topNodes)
    {
        n->Draw(topMatrix, ctx);
    }
}

void LoadedGLTF::clearAll()
{
    VkDevice dv = creator->device;

    for (auto& [k, v] : meshes)
    {

        creator->destroy_buffer(v->meshBuffers.indexBuffer);
        creator->destroy_buffer(v->meshBuffers.vertexBuffer);
    }

    for (auto& [k, v] : images)
    {

        if (v.image == creator->errorCheckerboardImage.image)
        {
            // dont destroy the default images
            continue;
        }
        creator->destroy_image(v);
    }

    for (auto& sampler : samplers)
    {
        vkDestroySampler(dv, sampler, nullptr);
    }

    auto materialBuffer = materialDataBuffer;
    auto samplersToDestroy = samplers;

    descriptorPool.destroy_pools(dv);

    creator->destroy_buffer(materialBuffer);
}