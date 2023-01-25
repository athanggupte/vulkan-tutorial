#include "Application.h"
#include <cstdio>

namespace
{
	bool s_GlfwInitialized = false;
}

Application::Application(int width, int height, const char* name)
	: m_AppName(name), m_Width(width), m_Height(height)
{
}

void Application::run()
{
	m_VkContext.initContext(m_AppName, m_Window);
	mainLoop();
	shutdown();
}

void Application::initWindow()
{
	if (!s_GlfwInitialized)
		glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	m_Window = glfwCreateWindow(m_Width, m_Height, m_AppName, nullptr, nullptr);
}

void Application::mainLoop()
{
	double previous = glfwGetTime();
	double lastFrameTime = 0.0;
	double lastLogTime = previous;
	size_t numFramesTillLastLog = 0;

	while (!glfwWindowShouldClose(m_Window))
	{
		glfwPollEvents();
		m_VkContext.drawFrame();

		double now = glfwGetTime();
		m_FrameTime = now - previous;
		previous = now;

		m_NumFramesRendered++;
		
		m_AverageFrameTime += 1.0 / static_cast<double>(m_NumFramesRendered) * (m_FrameTime - m_AverageFrameTime);
		lastFrameTime = m_FrameTime;

		double deltaLogTime = now - lastLogTime;
		size_t numFramesSizeLastLog = m_NumFramesRendered - numFramesTillLastLog;
		if (deltaLogTime > 1.0)
		{
			printf("Frame Time (latest)       : %lf\n", m_FrameTime);
			printf("Frame Time (running mean) : %lf\n", m_AverageFrameTime);
			printf("# Frames (total)          : %lld\n", m_NumFramesRendered);
			printf("# Frames (since last log) : %lld\n", numFramesSizeLastLog);
			printf("Frames per second         : %lf\n", static_cast<double>(numFramesSizeLastLog) / deltaLogTime);
			printf("-----------------------------------------------\n");
			lastLogTime = now;
			numFramesTillLastLog = m_NumFramesRendered;
		}

	}
}

void Application::shutdown()
{
	m_VkContext.shutdownContext();
	glfwDestroyWindow(m_Window);
	glfwTerminate();
}
