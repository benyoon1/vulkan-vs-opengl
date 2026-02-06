#include "scene/model.h"
#include "render/shader.h"
#include "render/shadowMap.h"
#include "scene/camera.h"
#include "scene/lights/directionalLight.h"
#include "scene/lights/spotlight.h"
#include <core/utils.h>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <stb_image.h>

Model::Model(std::string const& path)
{
    loadModel(Utils::getPath(path));

    // set valley model matrix (position/scale)
    m_modelMatrix = glm::mat4(1.0f);
    m_modelMatrix = glm::translate(m_modelMatrix, glm::vec3(0.0f, 0.0f, 0.0f));
    m_modelMatrix = glm::scale(m_modelMatrix, glm::vec3(50.0f));
}

Model::~Model()
{
    for (const auto& tex : m_texturesLoaded)
    {
        if (tex.id)
            glDeleteTextures(1, &tex.id);
    }
    m_texturesLoaded.clear();
}

void Model::configureShader(Shader& shader, const Camera& camera, const DirectionalLight& sunLight,
                            const Spotlight& spotlight, float spotlightGain) const
{
    shader.use();
    shader.setInt("sunShadowMapTextureNum", ShadowMap::kSunShadowTextureNum);
    shader.setInt("spotlightShadowMapTextureNum", ShadowMap::kSpotShadowTextureNum);

    shader.setMat4("sunLightSpaceMatrix", sunLight.getLightSpaceMatrix());
    shader.setMat4("spotLightSpaceMatrix", spotlight.getSpotLightSpaceMatrix());

    shader.setVec3("sunPos", sunLight.getSunPosition());
    shader.setVec3("sunColor", DirectionalLight::kSunColor);
    shader.setVec3("viewPos", camera.getPosition());
    shader.setMat4("model", this->getModelMatrix());
    shader.setVec3("sunColor", glm::vec3(1.0f, 1.0f, 1.0f)); // white light

    shader.setInt("spotEnabled", 1);
    shader.setVec3("spotColor", Spotlight::kSpotColor);
    shader.setFloat("spotInnerCutoff", glm::cos(glm::radians(Spotlight::kInnerCutDeg)));
    shader.setFloat("spotOuterCutoff", glm::cos(glm::radians(Spotlight::kOuterCutDeg)));
    shader.setFloat("spotIntensity", Spotlight::kIntensity * spotlightGain);

    shader.setInt("receiveShadow", 1);

    // orange tint to valley model
    shader.setVec3("objectColor", Model::kValleyTint);
}

void Model::draw(Shader& modelShader, const glm::mat4& projection, const glm::mat4& view, const Camera& camera,
                 const glm::vec3 sunPos, const glm::vec3 spotlightPos)
{
    modelShader.use();
    modelShader.setMat4("projection", projection);
    modelShader.setMat4("view", view);
    modelShader.setVec3("viewPos", camera.getPosition());
    modelShader.setVec3("sunPos", sunPos);
    modelShader.setVec3("spotlightPos", spotlightPos);

    for (uint32_t i = 0; i < m_meshes.size(); i++)
    {
        m_meshes[i].draw(modelShader);
    }
}

void Model::drawShadowMap(Shader& depthShader, const glm::mat4& lightSpaceMatrix, const glm::mat4& modelMatrix)
{
    depthShader.use();
    depthShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
    depthShader.setMat4("model", modelMatrix);

    for (uint32_t i = 0; i < m_meshes.size(); i++)
    {
        m_meshes[i].draw(depthShader);
    }
}

void Model::loadModel(std::string const& path)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        path, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        std::cout << "ERROR::ASSIMP:: " << importer.GetErrorString() << std::endl;
        return;
    }
    m_directory = path.substr(0, path.find_last_of('/'));
    processNode(scene->mRootNode, scene);
}

void Model::processNode(aiNode* node, const aiScene* scene)
{
    for (uint32_t i = 0; i < node->mNumMeshes; i++)
    {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        m_meshes.push_back(processMesh(mesh, scene));
    }
    for (uint32_t i = 0; i < node->mNumChildren; i++)
    {
        processNode(node->mChildren[i], scene);
    }
}
Mesh Model::processMesh(aiMesh* mesh, const aiScene* scene)
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Texture> textures;

    for (uint32_t i = 0; i < mesh->mNumVertices; i++)
    {
        Vertex vertex;
        glm::vec3 vector;
        vector.x = mesh->mVertices[i].x;
        vector.y = mesh->mVertices[i].y;
        vector.z = mesh->mVertices[i].z;
        vertex.position = vector;

        if (mesh->HasNormals())
        {
            vector.x = mesh->mNormals[i].x;
            vector.y = mesh->mNormals[i].y;
            vector.z = mesh->mNormals[i].z;
            vertex.normal = vector;
        }

        if (mesh->mTextureCoords[0])
        {
            glm::vec2 vec;
            vec.x = mesh->mTextureCoords[0][i].x;
            vec.y = mesh->mTextureCoords[0][i].y;
            vertex.texCoords = vec;

            vector.x = mesh->mTangents[i].x;
            vector.y = mesh->mTangents[i].y;
            vector.z = mesh->mTangents[i].z;
            vertex.tangent = vector;

            vector.x = mesh->mBitangents[i].x;
            vector.y = mesh->mBitangents[i].y;
            vector.z = mesh->mBitangents[i].z;
            vertex.bitangent = vector;
        }
        else
        {
            vertex.texCoords = glm::vec2(0.0f, 0.0f);
        }

        vertices.push_back(vertex);
    }

    for (uint32_t i = 0; i < mesh->mNumFaces; i++)
    {
        aiFace face = mesh->mFaces[i];
        for (uint32_t j = 0; j < face.mNumIndices; j++)
            indices.push_back(face.mIndices[j]);
    }

    aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
    std::vector<Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse");
    textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
    std::vector<Texture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
    textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
    std::vector<Texture> normalMaps = loadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal");
    textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
    std::vector<Texture> heightMaps = loadMaterialTextures(material, aiTextureType_AMBIENT, "texture_height");
    textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());

    return Mesh(vertices, indices, textures);
}

std::vector<Texture> Model::loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName)
{
    std::vector<Texture> textures;
    for (uint32_t i = 0; i < mat->GetTextureCount(type); i++)
    {
        aiString str;
        mat->GetTexture(type, i, &str);
        bool skip = false;
        for (uint32_t j = 0; j < m_texturesLoaded.size(); j++)
        {
            if (std::strcmp(m_texturesLoaded[j].path.data(), str.C_Str()) == 0)
            {
                textures.push_back(m_texturesLoaded[j]);
                skip = true;
                break;
            }
        }
        if (!skip)
        {
            Texture texture;
            texture.id = textureFromFile(str.C_Str(), this->m_directory);
            texture.type = typeName;
            texture.path = str.C_Str();
            textures.push_back(texture);
            m_texturesLoaded.push_back(texture);
        }
    }
    return textures;
}

uint32_t Model::textureFromFile(const char* path, const std::string& directory)
{
    std::string filename = std::string(path);
    filename = directory + '/' + filename;

    uint32_t textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char* data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;
        else
        {
            std::cerr << "Unknown texture format: " << nrComponents << " components" << std::endl;
            stbi_image_free(data);
            if (textureID)
            {
                glDeleteTextures(1, &textureID);
            }
            return 0;
        }

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}
