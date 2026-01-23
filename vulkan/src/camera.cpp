#include "camera.h"
#include "vk_engine.h"
#include <SDL_events.h>
#include <SDL_scancode.h>
#include <SDL_timer.h>
#include <glm/gtx/quaternion.hpp>

glm::mat4 Camera::getViewMatrix() const
{
    return glm::lookAt(m_position, m_position + m_front, m_up);
}

void Camera::processInput(VulkanEngine* engine)
{
    const Uint8* keys = SDL_GetKeyboardState(NULL);
    float sprint{1.0f};

    if (keys[SDL_SCANCODE_LSHIFT])
    {
        sprint *= 3.0f;
    }

    if (keys[SDL_SCANCODE_W])
    {
        processKeyboard(FORWARD, engine->get_delta_time() * sprint);
    }
    if (keys[SDL_SCANCODE_S])
    {
        processKeyboard(BACKWARD, engine->get_delta_time() * sprint);
    }
    if (keys[SDL_SCANCODE_A])
    {
        processKeyboard(LEFT, engine->get_delta_time() * sprint);
    }
    if (keys[SDL_SCANCODE_D])
    {
        processKeyboard(RIGHT, engine->get_delta_time() * sprint);
    }

    processMouseMovement();
}

void Camera::processKeyboard(CameraMovement direction, float deltaTime)
{
    float velocity = kSpeed * deltaTime;
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

void Camera::processMouseMovement()
{
    int mouseX, mouseY;

    Uint32 buttons = SDL_GetRelativeMouseState(&mouseX, &mouseY);

    m_yaw += (float)mouseX * Camera::kSensitivity;
    m_pitch -= (float)mouseY * Camera::kSensitivity;

    m_pitch = glm::clamp(m_pitch, -89.0f, 89.0f);

    // update Front, Right and Up vectors using the updated Euler angles
    updateCameraVectors();
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
