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
#include <cstring>
#include <iterator>

const uint32_t WIDTH  = 800;
const uint32_t HEIGHT = 600;
constexpr bool print_debug = 1;

const std::vector<char const*> validationLayers{
	"VK_LAYER_KHRONOS_validation"	// Standard validation bundle
};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

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
		setupDebugMessenger();
	}

	void setupDebugMessenger()
	{
		if (!enableValidationLayers) return;

		vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);

		vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
			vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
			vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
			vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
		);

		vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{
			.messageSeverity = severityFlags,
			.messageType = messageTypeFlags,
			.pfnUserCallback = &debugCallback
		};

		debugMessenger = instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT );
	}

	void createInstance()
	{
		// Get the required layers
		std::vector<const char*> requiredLayers{};
		if (enableValidationLayers){
			requiredLayers.assign(validationLayers.begin(), validationLayers.end());

					// Check if the required layers is supported by the Vulkan implementation.
			auto layerProperties = context.enumerateInstanceLayerProperties();
			std::vector<decltype(requiredLayers)::value_type> unsupportedLayers{};
			std::ranges::copy_if(
				requiredLayers,
				std::back_inserter(unsupportedLayers),
				[&layerProperties](const auto& requiredLayer){
					return std::ranges::none_of(
						layerProperties,
						[&requiredLayer](const auto& layerProperty){
							return strcmp(layerProperty.layerName, requiredLayer) == 0;
						});
				});

			
			if(!unsupportedLayers.empty()){
				std::cout << "Unsupported Layers: " << std::endl;
				for(const auto& layer : unsupportedLayers){
					std::cout << '\t' << layer << std::endl;
				}

				throw std::runtime_error("Required layer unsupported.");

			} else {
				std::cout << "Supported Layers: " << std::endl;
				for(const auto& layer : requiredLayers){
					std::cout << '\t' << layer << std::endl;
				}
			}
		}

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
		auto glfwRequiredExtensions = getRequiredInstanceExtensions();

		// Check if the required GLFW extensions are supported by the Vulkan implementation.
		auto extensionProperties = context.enumerateInstanceExtensionProperties();
		std::vector<decltype(glfwRequiredExtensions)::value_type> unsupportedExtensions{};
		std::ranges::copy_if(
			glfwRequiredExtensions,
			std::back_inserter(unsupportedExtensions),
			[&extensionProperties](const auto& requiredExtension){
				return std::ranges::none_of(
					extensionProperties,
					[&requiredExtension](const auto& extensionProperty){
						return strcmp(extensionProperty.extensionName, requiredExtension) == 0;
					});
			});

		if(!unsupportedExtensions.empty()){
			std::cout << "Unsupported Extensions: " << std::endl;
			for(const auto& extension : unsupportedExtensions){
				std::cout << '\t' << extension << std::endl;
			}

			throw std::runtime_error("Required extensions unsupported.");

		} else {
			std::cout << "Supported Extensions: "  << std::endl;
			for(const auto& extension : glfwRequiredExtensions){
				std::cout <<'\t' << extension << std::endl;
			}
		}

		vk::InstanceCreateInfo createInfo{
			.pApplicationInfo = &appInfo,
			.enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
			.ppEnabledLayerNames = requiredLayers.data(),
			.enabledExtensionCount = static_cast<uint32_t>(glfwRequiredExtensions.size()),
			.ppEnabledExtensionNames = glfwRequiredExtensions.data()
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
	vk::raii::DebugUtilsMessengerEXT debugMessenger{nullptr};

	public:
	// Helpers
	std::vector<const char*> getRequiredInstanceExtensions()
	{
		uint32_t glfwExtensionCount{};
		auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		if(enableValidationLayers){
			extensions.push_back(vk::EXTDebugUtilsExtensionName);
		}

		return extensions;
	}
 
	static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
		vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
		vk::DebugUtilsMessageTypeFlagsEXT type,
		const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData)
		{
			if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning || 
				severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError){
					std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;
				}
		

			return vk::False;
		}
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
