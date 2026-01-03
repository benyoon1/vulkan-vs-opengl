#ifndef CAMERA_H
#define CAMERA_H

#include <glad/glad.h>
#include <glm/glm.hpp>

enum CameraMovement
{
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT
};

class Camera
{
public:
    Camera();

    static constexpr glm::vec3 kPosition{5.f, 5.f, 10.f};
    static constexpr float kYaw = -90.0f;
    static constexpr float kPitch = 0.0f;
    static constexpr float kSpeed = 10.0f;
    static constexpr float kSensitivity = 0.1f;
    static constexpr float kZoom = 60.0f;

    glm::mat4 getViewMatrix() const;
    glm::vec3 getPosition() const { return m_position; }
    float getZoom() const { return m_zoom; }
    void processKeyboard(CameraMovement direction, float deltaTime);
    void processMouseMovement(float xoffset, float yoffset, GLboolean constrainPitch = true);
    void processMouseScroll(float yoffset);

private:
    glm::vec3 m_position{kPosition};
    glm::vec3 m_front{0.0f, 0.0f, -1.0f};
    glm::vec3 m_up{0.0f, 1.0f, 0.0f};
    glm::vec3 m_right{1.0f, 0.0f, 0.0f};
    glm::vec3 m_worldUp{0.0f, 1.0f, 0.0f};
    float m_yaw{kYaw};
    float m_pitch{kPitch};
    float m_movementSpeed{kSpeed};
    float m_mouseSensitivity{kSensitivity};
    float m_zoom{kZoom};
    void updateCameraVectors();
};

#endif
