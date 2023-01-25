#include <stdexcept>

#include "Application.h"

int main()
{
	Application app{ 800, 600, "Vulkan Tutorial - Test" };

	try {
		app.initWindow();
		app.run();
	} catch (const std::exception& e)
	{
		(void)fprintf(stderr, "Exception : %s\n", e.what());
		return EXIT_FAILURE;
	}
	return 0;
}
