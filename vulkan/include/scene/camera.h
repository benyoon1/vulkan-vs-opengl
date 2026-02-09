#ifndef CAMERA_H
#define CAMERA_H

#include <vk_types.h>

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
    static constexpr glm::vec3 kPosition{5.f, 0.f, 23.f};
    static constexpr float kYaw{-70.0f};
    static constexpr float kPitch{0.0f};
    static constexpr float kSpeed{5.0f};
    static constexpr float kSensitivity{0.1f};
    static constexpr float kZoom{60.0f};

    glm::mat4 getViewMatrix() const;
    glm::vec3 getPosition() const { return m_position; }
    float getZoom() const { return m_zoom; }
    float getFOV() const { return m_FOV; }
    void processKeyboard(CameraMovement direction, float deltaTime);
    void processMouseMovement();
    void processInput(float deltaTime);
    void update();
    void updateFrame();

private:
    glm::vec3 m_position{kPosition};
    glm::vec3 m_front{0.0f, 0.0f, -1.0f};
    glm::vec3 m_up{0.0f, 1.0f, 0.0f};
    glm::vec3 m_right{1.0f, 0.0f, 0.0f};
    glm::vec3 m_worldUp{0.0f, 1.0f, 0.0f};
    float m_yaw{kYaw};
    float m_pitch{kPitch};
    float m_mouseSensitivity{kSensitivity};
    float m_zoom{kZoom};
    float m_FOV{80.0f};

    void updateCameraVectors();
};

#endif // CAMERA_H
