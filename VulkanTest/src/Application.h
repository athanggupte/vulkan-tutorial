#pragma once

#include "VulkanContext.h"

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

class Application
{
public:
	Application(int width, int height, const char* name);

	void run();
	void initWindow();

private:
	void mainLoop();
	void shutdown();

	const char* m_AppName = nullptr;
	int m_Width, m_Height;
	GLFWwindow* m_Window{};
	VulkanContext m_VkContext{};

	size_t m_NumFramesRendered = 0;
	double m_FrameTime = 0;
	double m_AverageFrameTime = 0.0;
};
