#include "core/window.h"
#include "core/application.h"
#include "scene/camera.h"
#include <iostream>

Window::Window() : m_lastX(kScreenWidth / 2.0f), m_lastY(kScreenHeight / 2.0f)
{
    if (!glfwInit())
    {
        throw std::runtime_error("Failed to initialize GLFW");
    }
    // TODO: change to 4.6 for linux/windows
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    m_window = glfwCreateWindow(m_width, m_height, "OpenGL", NULL, NULL);
    if (m_window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }
    glfwMakeContextCurrent(m_window);
    // glfwSwapInterval(1);                      // Enable vsync
    glfwSwapInterval(0);                      // Disable vsync
    glfwSetWindowUserPointer(m_window, this); // Set the user pointer to the instance

    glfwSetFramebufferSizeCallback(m_window, framebufferSizeCallback);
    glfwSetCursorPosCallback(m_window, mouseCallback);
    glfwSetScrollCallback(m_window, scrollCallback);

    // tell GLFW to capture our mouse
    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void Window::setCamera(Camera* camera)
{
    m_camera = camera;
}

void Window::processInput(Application* app)
{
    if (glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(m_window, true);
    }

    float sprint{1.0f};

    if (glfwGetKey(m_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
    {
        sprint *= 3.0f;
    }

    // get WASD keys
    if (glfwGetKey(m_window, GLFW_KEY_W) == GLFW_PRESS)
    {
        m_camera->processKeyboard(FORWARD, app->deltaTime * sprint);
    }
    if (glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS)
    {
        m_camera->processKeyboard(BACKWARD, app->deltaTime * sprint);
    }
    if (glfwGetKey(m_window, GLFW_KEY_A) == GLFW_PRESS)
    {
        m_camera->processKeyboard(LEFT, app->deltaTime * sprint);
    }
    if (glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS)
    {
        m_camera->processKeyboard(RIGHT, app->deltaTime * sprint);
    }

    if (glfwGetKey(m_window, GLFW_KEY_J) == GLFW_PRESS)
    {
        app->numAsteroids -= app->deltaTime * 5000;
        if (app->numAsteroids < kSliderMin)
        {
            app->numAsteroids = 0;
        }
    }
    if (glfwGetKey(m_window, GLFW_KEY_K) == GLFW_PRESS)
    {
        app->numAsteroids += app->deltaTime * 5000;
        if (app->numAsteroids > kSliderMax)
        {
            app->numAsteroids = kSliderMax;
        }
    }

    // if (glfwGetKey(m_window, GLFW_KEY_SPACE) == GLFW_PRESS)
    // {
    //     outSunSpeed *= 10.0f;
    // }
    // if (glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
    // {
    //     spotlightGain = 5.0f;
    // }
}

Window::~Window()
{
    glfwTerminate();
}

void Window::framebufferSizeCallback(GLFWwindow* /*window*/, int width, int height)
{
    glViewport(0, 0, width, height);
}

void Window::mouseCallback(GLFWwindow* window, double xposIn, double yposIn)
{
    Window* instance = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (instance)
    {
        float xpos = static_cast<float>(xposIn);
        float ypos = static_cast<float>(yposIn);

        if (instance->m_firstMouse)
        {
            instance->m_lastX = xpos;
            instance->m_lastY = ypos;
            instance->m_firstMouse = false;
        }

        float xoffset = xpos - instance->m_lastX;
        float yoffset = instance->m_lastY - ypos;

        instance->m_lastX = xpos;
        instance->m_lastY = ypos;

        instance->m_camera->processMouseMovement(xoffset, yoffset);
    }
}

void Window::scrollCallback(GLFWwindow* window, double /*xoffset*/, double yoffset)
{
    Window* instance = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (instance)
    {
        instance->m_camera->processMouseScroll(static_cast<float>(yoffset));
    }
}
