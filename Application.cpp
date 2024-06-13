#include "Application.hpp"

#include <GLFW/glfw3.h>

#include <iostream>
#include <ratio>
#include <charconv>

Application::Application(const std::string& title)
    : _title(title)
{
}

Application::~Application()
{
    Application::Cleanup();
}

bool Application::Initialize()
{
    if (!glfwInit())
    {
        std::cerr << "GLFW: Failed to initialize\n";
        return false;
    }

    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* videoMode = glfwGetVideoMode(primaryMonitor);
    _width = static_cast<int32_t>(videoMode->width * 0.5f);
    _height = static_cast<int32_t>(videoMode->height * 0.75f);

    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_FALSE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    _window = glfwCreateWindow(
        _width,
        _height,
        _title.data(),
        nullptr,
        nullptr);

    if (_window == nullptr)
    {
        std::cerr << "GLFW: Failed to create a window\n";
        Cleanup();
        return false;
    }

    const int32_t windowLeft = videoMode->width / 2 - _width / 2;
    const int32_t windowTop = videoMode->height / 2 - _height / 2;
    glfwSetWindowPos(_window, windowLeft, windowTop);

    glfwSetWindowUserPointer(_window, this);
    glfwSetFramebufferSizeCallback(_window, HandleResize);

    _currentTime = std::chrono::high_resolution_clock::now();
    return true;
}

void Application::OnResize(
    const int32_t width,
    const int32_t height)
{
    _width = width;
    _height = height;
}

void Application::Cleanup()
{
    glfwDestroyWindow(_window);
    glfwTerminate();
}

void Application::Run()
{
    if (!Initialize())
    {
        return;
    }

    if (!Load())
    {
        return;
    }

    while (!glfwWindowShouldClose(_window))
    {
        Update();
        Render();
    }
}

void Application::HandleResize(
    GLFWwindow* window,
    const int32_t width,
    const int32_t height)
{
    Application* application = static_cast<Application*>(glfwGetWindowUserPointer(window));
    application->OnResize(width, height);
}

GLFWwindow* Application::GetWindow() const
{
    return _window;
}

int32_t Application::GetWindowWidth() const
{
    return _width;
}

int32_t Application::GetWindowHeight() const
{
    return _height;
}

void Application::Update()
{
    const auto oldTime = _currentTime;
    _totalFrames += 1;
    _currentTime = std::chrono::high_resolution_clock::now();
    if (_totalFrames > 100)
    {
        _totalFrames = 1;
        _totalTime = 0;
    }

    std::chrono::duration<double, std::milli> timeSpan = (_currentTime - oldTime);
    _deltaTime = static_cast<float>(timeSpan.count());
    _totalTime += _deltaTime;
    const float avgTime = _totalTime / _totalFrames;
    const int fps = static_cast<int>(1000.0 / avgTime);
    // will insert into the array later, init with string first
    char timeStr[59] = "last ms: ....... | last 100 frame avg ms: .......fps: ....";
    constexpr std::chars_format fmt = std::chars_format::fixed;
    constexpr int precision = 2;
    std::to_chars(timeStr + 9, timeStr + 22, _deltaTime, fmt, precision); // even if there is an error,
    std::to_chars(timeStr + 42, timeStr + 48, avgTime  , fmt, precision); // we can still change dont care
    std::to_chars(timeStr + 55, timeStr + 58, fps);

    glfwSetWindowTitle(_window, timeStr);
    glfwPollEvents();
}