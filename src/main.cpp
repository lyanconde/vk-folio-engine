#include <memory>
#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#	include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <algorithm>

const uint32_t WIDTH  = 800;
const uint32_t HEIGHT = 600;
constexpr bool print_debug = 1;

class HelloTriangleApplication
{
  public:
	void run()
	{
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}

  private:
	GLFWwindow *window = nullptr;

	void initWindow()
	{
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
	}

	void initVulkan()
	{
		// Initialize Vulkan library by creating an instance.
		createInstance();
	}

	void createInstance()
	{
		constexpr vk::ApplicationInfo appInfo{
			.pApplicationName = "Hello Triangle",
			.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
			.pEngineName = "Folio Engine",
			.engineVersion = VK_MAKE_VERSION(1, 0, 0),
			.apiVersion = vk::ApiVersion14
		};

		// Since Vulkan is a platform agnostic API, an extension is required
		// to interface with the window system. GLFW has a built-in function
		// that returns the extension(s) required which we can pass to the struct.

		// Get the required instance extensions from GLFW
		uint32_t glfwExtensionCount = 0;
		auto glfwRequiredExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		// Check if the required GLFW extensions are supported by the Vulkan implementation.
		auto extensionProperties = context.enumerateInstanceExtensionProperties();

		if(print_debug) std::cout << "Supported GLFW Requried Extensions: " << std::endl;
		for (uint32_t i = 0; i < glfwExtensionCount; i++){
			if(std::ranges::none_of(extensionProperties,
				[glfwExtension = glfwRequiredExtensions[i]](auto const& extensionProperty)
			{ return strcmp(extensionProperty.extensionName, glfwExtension) == 0; }))
			{
				throw std::runtime_error("Required GLFW extension not supported: " + std::string(glfwRequiredExtensions[i])); 
			} else if(print_debug){
				std::cout << '\t' << std::string(glfwRequiredExtensions[i]) << std::endl;
			}
		}

		vk::InstanceCreateInfo createInfo{
			.pApplicationInfo = &appInfo,
			.enabledExtensionCount = glfwExtensionCount,
			.ppEnabledExtensionNames = glfwRequiredExtensions
			// TODO: Enable Validation Layer
		};

		instance = vk::raii::Instance(context, createInfo);
	}

	void mainLoop()
	{
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
		}
	}

	void cleanup()
	{
		glfwDestroyWindow(window);

		glfwTerminate();
	}

	private:
	vk::raii::Context context{};
	vk::raii::Instance instance{nullptr};

};

int main()
{
	try
	{
		HelloTriangleApplication app;
		app.run();
	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
