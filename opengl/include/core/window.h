#ifndef WINDOW_H
#define WINDOW_H

// clang-format off
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <cstdint>
// clang-format on

class Camera;
class Application;

class Window
{
public:
    Window();
    ~Window();

    static constexpr uint32_t kScreenWidth{1920};
    static constexpr uint32_t kScreenHeight{1080};
    static constexpr ImS32 kSliderMin{0};
    static constexpr ImS32 kSliderMax{30000};

    void setCamera(Camera* camera);
    void processInput(Application* app);
    void updateFrame();

    GLFWwindow* getGlfwWindow() const { return m_window; }

private:
    GLFWwindow* m_window{};
    uint32_t m_width{kScreenWidth};
    uint32_t m_height{kScreenHeight};
    float m_lastX{0.0f};
    float m_lastY{0.0f};
    bool m_firstMouse{true};
    Camera* m_camera{nullptr};
    bool m_iPressedLastFrame{false};

    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void mouseCallback(GLFWwindow* window, double xpos, double ypos);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
};

#endif // WINDOW_H