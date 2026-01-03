#include "scene/camera.h"
#include <glm/gtc/matrix_transform.hpp>

Camera::Camera()
{
    updateCameraVectors();
}

glm::mat4 Camera::getViewMatrix() const
{
    return glm::lookAt(m_position, m_position + m_front, m_up);
}

void Camera::processKeyboard(CameraMovement direction, float deltaTime)
{
    float velocity = m_movementSpeed * deltaTime;
    if (direction == FORWARD)
    {
        m_position += m_front * velocity;
    }
    if (direction == BACKWARD)
    {
        m_position -= m_front * velocity;
    }
    if (direction == LEFT)
    {
        m_position -= m_right * velocity;
    }
    if (direction == RIGHT)
    {
        m_position += m_right * velocity;
    }
}

void Camera::processMouseMovement(float xoffset, float yoffset, GLboolean constrainPitch)
{
    xoffset *= m_mouseSensitivity;
    yoffset *= m_mouseSensitivity;

    m_yaw += xoffset;
    m_pitch += yoffset;

    // make sure that when pitch is out of bounds, screen doesn't get flipped
    if (constrainPitch)
    {
        if (m_pitch > 89.0f)
        {
            m_pitch = 89.0f;
        }
        if (m_pitch < -89.0f)
        {
            m_pitch = -89.0f;
        }
    }

    // update Front, Right and Up Vectors using the updated Euler angles
    updateCameraVectors();
}

void Camera::processMouseScroll(float yoffset)
{
    m_zoom -= (float)yoffset;
    if (m_zoom < 1.0f)
    {
        m_zoom = 1.0f;
    }
    if (m_zoom > kZoom)
    {
        m_zoom = kZoom;
    }
}

void Camera::updateCameraVectors()
{
    // calculate the new Front vector
    glm::vec3 front;
    front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    front.y = sin(glm::radians(m_pitch));
    front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    m_front = glm::normalize(front);
    // also re-calculate the Right and Up vector
    m_right = glm::normalize(
        glm::cross(m_front, m_worldUp)); // normalize the vectors, because their length gets closer to 0 the more you
                                         // look up or down which results in slower movement.
    m_up = glm::normalize(glm::cross(m_right, m_front));
}