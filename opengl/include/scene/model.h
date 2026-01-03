#ifndef MODEL_H
#define MODEL_H

#include "mesh.h"
#include <assimp/scene.h>
#include <glm/glm.hpp>

class Camera;
class Shader;
class DirectionalLight;
class Spotlight;
class RobotArm;

class Model
{
public:
    Model(std::string const& path);
    ~Model();

    static constexpr glm::vec3 kValleyTint{0.85f, 0.553f, 0.133f};

    void configureShader(Shader& shader, const Camera& camera, const DirectionalLight& sunLight,
                         const Spotlight& spotlight, const RobotArm& robotArm, float spotlightGain) const;
    void draw(Shader& modelShader, const glm::mat4& projection, const glm::mat4& view, const Camera& camera,
              const glm::vec3 sunPos, const glm::vec3 spotlightPos);
    void drawShadowMap(Shader& depthShader, const glm::mat4& lightSpaceMatrix, const glm::mat4& modelMatrix);
    void setModelMatrix(const glm::mat4& modelMatrix) { m_modelMatrix = modelMatrix; }
    glm::mat4 getModelMatrix() const { return m_modelMatrix; }

private:
    std::vector<Texture> m_texturesLoaded;
    std::vector<Mesh> m_meshes;
    std::string m_directory{};
    glm::mat4 m_modelMatrix{1.0f};

    void loadModel(std::string const& path);
    void processNode(aiNode* node, const aiScene* scene);
    Mesh processMesh(aiMesh* mesh, const aiScene* scene);
    std::vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName);
    uint32_t textureFromFile(const char* path, const std::string& directory);
};

#endif
