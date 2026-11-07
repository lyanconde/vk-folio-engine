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
#include <cstdint>
#include <limits>


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
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();
		createSwapchain();
		createImageViews();
	}

	void createImageViews()
	{
		assert(swapchainImageViews.empty());

		vk::ImageViewCreateInfo imageViewCreateInfo{
			.viewType = vk::ImageViewType::e2D,
			.format = swapchainSurfaceFormat.format,
			.subresourceRange = {
				vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
		};

		swapchainImageViews.reserve(swapchainImages.size());

		std::ranges::for_each(swapchainImages,
			[&](auto& image){
				imageViewCreateInfo.image = image;
				swapchainImageViews.emplace_back(device, imageViewCreateInfo);
			});
	}

	void createSwapchain()
	{
		vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);
		swapchainExtent = chooseSwapchainExtent(surfaceCapabilities);
		uint32_t minImageCount = chooseSwapchainMinImageCount(surfaceCapabilities);

		std::vector<vk::SurfaceFormatKHR> availableFormats = physicalDevice.getSurfaceFormatsKHR(*surface);
		swapchainSurfaceFormat = chooseSwapchainSurfaceFormat(availableFormats);

		std::vector<vk::PresentModeKHR> availablePresentModes = physicalDevice.getSurfacePresentModesKHR(*surface);
		vk::PresentModeKHR presentMode = chooseSwapchainPresentMode(availablePresentModes);

		vk::SwapchainCreateInfoKHR swapchainCreateInfo{
			.surface = *surface,
			.minImageCount = minImageCount,
			.imageFormat = swapchainSurfaceFormat.format,
			.imageColorSpace = swapchainSurfaceFormat.colorSpace,
			.imageExtent = swapchainExtent,
			.imageArrayLayers = 1,
			.imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
			.imageSharingMode = vk::SharingMode::eExclusive,
			.preTransform = surfaceCapabilities.currentTransform,
			.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
			.presentMode = presentMode,
			.clipped = true,
			.oldSwapchain = nullptr
		};

		swapchain = vk::raii::SwapchainKHR(device, swapchainCreateInfo);
		swapchainImages = swapchain.getImages();
	}

	void createSurface()
	{
		VkSurfaceKHR _surface{};
		if(glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0){
			throw std::runtime_error("failed to create window surface!");
		}

		surface = vk::raii::SurfaceKHR(instance, _surface);
	}

	void createLogicalDevice()
	{
		std::vector<vk::QueueFamilyProperties> queueFamilyProperties{physicalDevice.getQueueFamilyProperties()};
		if(enableValidationLayers){
			std::cout << "Queue Family: " << std::endl;
			for(const auto& family : queueFamilyProperties){
				std::cout << '\t' << vk::to_string(family.queueFlags) << std::endl;
			}
		}

		// Get the first index into queueFamilyProperties which supports both graphics and present.
		auto it = std::ranges::find_if(
			queueFamilyProperties,
			[&](const auto& property){
				auto index = static_cast<uint32_t>(&property - queueFamilyProperties.data());
				return (property.queueFlags & vk::QueueFlagBits::eGraphics) && 
						physicalDevice.getSurfaceSupportKHR(index, *surface);
			}
		);

		if(it == queueFamilyProperties.end()){
			throw std::runtime_error("failed to find queue family that supports graphics and present!");
		}

		uint32_t queueIndex = static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), it));

		float queuePriority = 0.5f;

		vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
			.queueFamilyIndex = queueIndex,
			.queueCount = 1,
			.pQueuePriorities = &queuePriority
		};

		vk::StructureChain<
			vk::PhysicalDeviceFeatures2,
			vk::PhysicalDeviceVulkan11Features,
			vk::PhysicalDeviceVulkan13Features,
			vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain = {
				{},
				{.shaderDrawParameters = true},
				{.dynamicRendering = true},
				{.extendedDynamicState = true}
			};
		
		vk::DeviceCreateInfo deviceCreateInfo{
			.pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
			.queueCreateInfoCount = 1,
			.pQueueCreateInfos = &deviceQueueCreateInfo,
			.enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtensions.size()),
			.ppEnabledExtensionNames = requiredDeviceExtensions.data()
		};

		device = vk::raii::Device(physicalDevice, deviceCreateInfo);
		graphicsQueue = vk::raii::Queue(device, queueIndex, 0);
	}

	void pickPhysicalDevice()
	{
		auto physicalDevices = instance.enumeratePhysicalDevices();

		// Find the a suitable device
		auto const devIter = std::ranges::find_if(
			physicalDevices,
			[&](const auto& candidate){
				return isDeviceSuitable(candidate);
			});
		
		if(devIter == physicalDevices.end()){
			throw std::runtime_error("failed to find Vulkan supported GPU!");
		}

		physicalDevice = *devIter;
		if(enableValidationLayers){
			std::cout << "Selected GPU: " << std::endl;
			auto deviceProperties = physicalDevice.getProperties();
			std::cout << '\t' << deviceProperties.deviceName << std::endl;
		}
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
	vk::raii::PhysicalDevice physicalDevice{nullptr};
	vk::raii::Device device{nullptr};
	vk::raii::Queue graphicsQueue{nullptr};
	vk::raii::SurfaceKHR surface{nullptr};
	
	// Present components
	vk::raii::SwapchainKHR swapchain{nullptr};
	std::vector<vk::Image> swapchainImages{};
	vk::SurfaceFormatKHR swapchainSurfaceFormat{};
	vk::Extent2D swapchainExtent{};

	std::vector<vk::raii::ImageView> swapchainImageViews{};

	std::vector<const char*> requiredDeviceExtensions{
		vk::KHRSwapchainExtensionName
	};

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

	bool isDeviceSuitable(const vk::raii::PhysicalDevice& physicalDevice)
	{
		// Check if the Physical Device supports Vulkan 1.3 API version
		bool supportsVulkan1_3 = physicalDevice.getProperties().apiVersion >= vk::ApiVersion13;

		// Check if any of the queue families support graphics operations
		auto queueFamilies = physicalDevice.getQueueFamilyProperties();
		bool supportsGraphics = std::ranges::any_of(queueFamilies, [](const auto& queueFamily){
			// queueFlags are bitmasks so test the graphics bit with a
			// bitwise operator '&' instead of a comparator '=='.
			// The '!!' operator forces the bitmask to return a boolean
			return !!(queueFamily.queueFlags & vk::QueueFlagBits::eGraphics);
		});

		// Check if all required physicalDevice extensions are available
		auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
		bool supportsAllRequiredExtensions = 
			std::ranges::all_of(
				requiredDeviceExtensions,
				[&availableDeviceExtensions](const auto& requiredExtension){
					return std::ranges::any_of(
						availableDeviceExtensions,
						[&requiredExtension](const auto& availableExtension){
							return strcmp(availableExtension.extensionName, requiredExtension) == 0;}
					);
				});
		
		// Check if Physical Device supports the required features 
		// (shader draw parameters, dynamic rendering, and extended dynamic state)
		auto features = physicalDevice.getFeatures2<
				vk::PhysicalDeviceFeatures2,
				vk::PhysicalDeviceVulkan11Features,
				vk::PhysicalDeviceVulkan13Features,
				vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
		
				bool supportsRequiredFeatures = 
					features.get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
					features.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
					features.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

		// Return 'true' if the Physical Device supports all the required 
		// criteria
		return supportsVulkan1_3 && supportsGraphics && supportsAllRequiredExtensions && supportsRequiredFeatures;
	}

	vk::SurfaceFormatKHR chooseSwapchainSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats )
	{
		assert(!availableFormats.empty());

		const auto formatIt = std::ranges::find_if(
			availableFormats,
			[](const auto& format) {
				 return format.format == vk::Format::eB8G8R8A8Srgb &&
				 	format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
			});
		
		return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
	}

	vk::PresentModeKHR chooseSwapchainPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes)
	{
		assert(std::ranges::any_of(availablePresentModes,
			[](const auto& presentMode){
				return presentMode == vk::PresentModeKHR::eFifo;
			}));
	
		return std::ranges::any_of(availablePresentModes,
		[](const auto& presentMode){
			return vk::PresentModeKHR::eMailbox == presentMode;
		}) ? vk::PresentModeKHR::eMailbox : vk::PresentModeKHR::eFifo;
	}

	vk::Extent2D chooseSwapchainExtent(const vk::SurfaceCapabilitiesKHR& capabilities)
	{
		// currentExtent is only set to the special "undefined" value described above
		// when the window manager lets us choose the extent ourselves; any other value
		// means the surface already dictates a fixed extent that we must use as-is.
		if(capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			return capabilities.currentExtent;
		}

		int width{}, height{};
		glfwGetFramebufferSize(window, &width, &height);

		return {
			std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
			std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
		};


	}

	uint32_t chooseSwapchainMinImageCount(const vk::SurfaceCapabilitiesKHR& surfaceCapabilities)
	{
		auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
		if((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount))
		{
			minImageCount = surfaceCapabilities.maxImageCount;
		}

		return minImageCount;
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
