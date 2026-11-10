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
#include <fstream>
#include <span>


const uint32_t WIDTH  = 800;
const uint32_t HEIGHT = 600;
constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;


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
		createGraphicsPipeline();
		createCommandPool();
		createCommandBuffer();
		createSyncObjects();
	}

	void createSyncObjects()
	{
		imageAcquiredSemaphores.reserve(swapchainImages.size());
		renderFinishedSemaphores.reserve(swapchainImages.size());
		for(const auto& image : swapchainImages){
			imageAcquiredSemaphores.emplace_back(vk::raii::Semaphore(device, vk::SemaphoreCreateInfo()));
			renderFinishedSemaphores.emplace_back(vk::raii::Semaphore(device, vk::SemaphoreCreateInfo()));
		}
		drawFence = vk::raii::Fence(device, {.flags = vk::FenceCreateFlagBits::eSignaled});
	}

	void recordCommandBuffer(uint32_t imageIndex)
	{
		commandBuffer.begin({});

			transitionImageLayout(
				imageIndex, 
				vk::ImageLayout::eUndefined,
				vk::ImageLayout::eColorAttachmentOptimal,
				{},
				vk::AccessFlagBits2::eColorAttachmentWrite,
				vk::PipelineStageFlagBits2::eColorAttachmentOutput,
				vk::PipelineStageFlagBits2::eColorAttachmentOutput
			);

			vk::ClearValue clearColor  = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 0.0f);
			vk::RenderingAttachmentInfo attachmentInfo{
				.imageView = swapchainImageViews[imageIndex],
				.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
				.loadOp = vk::AttachmentLoadOp::eClear,
				.storeOp = vk::AttachmentStoreOp::eStore,
				.clearValue = clearColor
			};
			vk::RenderingInfo renderingInfo {
				.renderArea = {.offset = {0, 0}, .extent = swapchainExtent},
				.layerCount = 1,
				.colorAttachmentCount  = 1,
				.pColorAttachments = &attachmentInfo
			};

			commandBuffer.beginRendering(renderingInfo);

				commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
				commandBuffer.setViewport(0, vk::Viewport(
					0.0f, 0.0f, 
					static_cast<float>(swapchainExtent.width), static_cast<float>(swapchainExtent.height), 
					0.0f, 1.0f));

				commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapchainExtent));
				commandBuffer.draw(3, 1, 0, 0);
			
			commandBuffer.endRendering();

			transitionImageLayout(
				imageIndex,
				vk::ImageLayout::eColorAttachmentOptimal,
				vk::ImageLayout::ePresentSrcKHR,
				vk::AccessFlagBits2::eColorAttachmentWrite,
				{},
				vk::PipelineStageFlagBits2::eColorAttachmentOutput,
				vk::PipelineStageFlagBits2::eBottomOfPipe
			);

		commandBuffer.end();
	}

	void createCommandBuffer()
	{
		vk::CommandBufferAllocateInfo allocInfo {
			.commandPool = commandPool,
			.level = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = 1
		};

		commandBuffer = std::move(vk::raii::CommandBuffers(device, allocInfo).front());
	}

	void createCommandPool()
	{
		vk::CommandPoolCreateInfo poolInfo{
			.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
			.queueFamilyIndex = queueIndex
		};

		commandPool = vk::raii::CommandPool(device, poolInfo);
	}

	void createGraphicsPipeline()
	{
		vk::raii::ShaderModule shaderModule = createShaderModule(readFile("shaders/triangle.spv"));

		vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
			.stage = vk::ShaderStageFlagBits::eVertex,
			.module = shaderModule,
			.pName = "vertMain"
		};

		vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
			.stage = vk::ShaderStageFlagBits::eFragment,
			.module = shaderModule,
			.pName = "fragMain"
		};

		vk::PipelineShaderStageCreateInfo shaderStages[] = {
			vertShaderStageInfo,
			fragShaderStageInfo
		};

		std::vector<vk::DynamicState> dynamicStates = {
			 vk::DynamicState::eViewport,
			 vk::DynamicState::eScissor
		};

		vk::PipelineDynamicStateCreateInfo dynamicState {
			.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
			.pDynamicStates = dynamicStates.data()
		};

		vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
		vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
			.topology = vk::PrimitiveTopology::eTriangleList
		};

		vk::Viewport viewport(0.0f, 0.0f, 
			static_cast<float>(swapchainExtent.width), static_cast<float>(swapchainExtent.height),
			0.0f, 1.0f);

		vk::Rect2D scissor{vk::Offset2D{0,0}, swapchainExtent};

		vk::PipelineViewportStateCreateInfo viewportState{
			.viewportCount = 1,
			.pViewports = &viewport,
			.scissorCount = 1,
			.pScissors = &scissor
		};

		vk::PipelineRasterizationStateCreateInfo rasterizer{
			.depthClampEnable = vk::False,
			.rasterizerDiscardEnable = vk::False,
			.polygonMode = vk::PolygonMode::eFill,
			.cullMode = vk::CullModeFlagBits::eBack,
			.frontFace = vk::FrontFace::eClockwise,
			.depthBiasEnable = vk::False,
			.lineWidth = 1.0f
		};

		vk::PipelineMultisampleStateCreateInfo multisampling{
			.rasterizationSamples = vk::SampleCountFlagBits::e1,
			.sampleShadingEnable = vk::False
		};

		vk::PipelineColorBlendAttachmentState colorBlendAttachment{
			.blendEnable = vk::False,
			.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
			.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
			.colorBlendOp = vk::BlendOp::eAdd,
			.srcAlphaBlendFactor = vk::BlendFactor::eOne,
			.dstAlphaBlendFactor = vk::BlendFactor::eZero,
			.alphaBlendOp = vk::BlendOp::eAdd,
			.colorWriteMask = 
				vk::ColorComponentFlagBits::eR | 
				vk::ColorComponentFlagBits::eG | 
				vk::ColorComponentFlagBits::eB |
				vk::ColorComponentFlagBits::eA
		};

		vk::PipelineColorBlendStateCreateInfo colorBlending{
			.logicOpEnable = vk::False,
			.logicOp = vk::LogicOp::eCopy,
			.attachmentCount = 1,
			.pAttachments = &colorBlendAttachment
		};

		vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
			.setLayoutCount = 0,
			.pushConstantRangeCount = 0
		};

		pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

		vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo{
			.colorAttachmentCount = 1,
			.pColorAttachmentFormats = &swapchainSurfaceFormat.format
		};

		vk::StructureChain<
			vk::GraphicsPipelineCreateInfo,
			vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain{
				{
					.stageCount = 2,
					.pStages = shaderStages,
					.pVertexInputState = &vertexInputInfo,
					.pInputAssemblyState = &inputAssembly,
					.pViewportState = &viewportState,
					.pRasterizationState = &rasterizer,
					.pMultisampleState = &multisampling,
					.pColorBlendState =&colorBlending,
					.pDynamicState = &dynamicState,
					.layout = pipelineLayout,
					.renderPass = nullptr
				},
				{
					.colorAttachmentCount = 1,
					.pColorAttachmentFormats = &swapchainSurfaceFormat.format
				}};

		graphicsPipeline = vk::raii::Pipeline(device, nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
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

		queueIndex = static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), it));

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
				{.synchronization2 = true, .dynamicRendering = true,},
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
			drawFrame();
		}

		device.waitIdle();
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
	vk::raii::PipelineLayout pipelineLayout{nullptr};
	vk::raii::Pipeline graphicsPipeline{nullptr};
	
	// Present components
	vk::raii::SwapchainKHR swapchain{nullptr};
	std::vector<vk::Image> swapchainImages{};
	vk::SurfaceFormatKHR swapchainSurfaceFormat{};
	vk::Extent2D swapchainExtent{};

	std::vector<vk::raii::ImageView> swapchainImageViews{};

	// Drawing components
	uint32_t queueIndex{};
	vk::raii::CommandPool commandPool{nullptr};
	vk::raii::CommandBuffer commandBuffer{nullptr};

	// Synchronization components
	std::vector<vk::raii::Semaphore> imageAcquiredSemaphores{};
	std::vector<vk::raii::Semaphore> renderFinishedSemaphores{};
	vk::raii::Fence drawFence{nullptr};
	uint32_t frameIndex = 0;

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
		auto minImageCount = std::max(MAX_FRAMES_IN_FLIGHT, surfaceCapabilities.minImageCount);
		if((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount))
		{
			minImageCount = surfaceCapabilities.maxImageCount;
		}

		return minImageCount;
	}

	static std::vector<char> readFile(const std::string& filename)
	{
		std::ifstream file(filename, std::ios::ate | std::ios::binary);

		if(!file.is_open()){
			throw std::runtime_error("failed to open file: " + filename);
		}

		std::vector<char> buffer(file.tellg());
		file.seekg(0, std::ios::beg);
		file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

		file.close();

		return buffer;
	}

	[[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const
	{
		std::span codeView{code};
		vk::ShaderModuleCreateInfo createInfo{
			.codeSize = codeView.size_bytes(),
			.pCode = reinterpret_cast<const uint32_t*>(codeView.data())
		};

		return device.createShaderModule(createInfo);
	}

	void transitionImageLayout(
		uint32_t imageIndex,
		vk::ImageLayout oldLayout,
		vk::ImageLayout newLayout,
		vk::AccessFlags2 srcAccessMask,
		vk::AccessFlags2 dstAccessMask,
		vk::PipelineStageFlags2 srcStageMask,
		vk::PipelineStageFlags2 dstStageMask)
	{
		vk::ImageMemoryBarrier2 barrier{
			.srcStageMask = srcStageMask,
			.srcAccessMask = srcAccessMask,
			.dstStageMask = dstStageMask,
			.dstAccessMask = dstAccessMask,
			.oldLayout = oldLayout,
			.newLayout = newLayout,
			.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
			.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
			.image = swapchainImages[imageIndex],
			.subresourceRange = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			}};

		vk::DependencyInfo dependencyInfo {
			.dependencyFlags = {},
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &barrier
		};

		commandBuffer.pipelineBarrier2(dependencyInfo);
	}

	void drawFrame()
	{
		// At the start of the frame, we want to wait until the previous
		// frame has finished, so that the command buffer and semaphores are 
		// available to use.
		auto fenceResult = device.waitForFences(*drawFence, vk::True, UINT64_MAX);
		if(fenceResult != vk::Result::eSuccess){
			throw std::runtime_error("failed to wait for fence!");
		}

		device.resetFences(*drawFence);
		// Acquire an image from the swapchain, signal the semaphore that 
		// the image is safe to use.
		auto [result, imageIndex] = swapchain.acquireNextImage(UINT64_MAX, *imageAcquiredSemaphores[frameIndex], nullptr);

		// Record a command buffer which draws the scene into that image.
		recordCommandBuffer(imageIndex);

		// Submit the recorded command buffer.
		vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
		const vk::SubmitInfo submitInfo{
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &*imageAcquiredSemaphores[frameIndex],
			.pWaitDstStageMask = &waitDestinationStageMask,
			.commandBufferCount = 1,
			.pCommandBuffers = &*commandBuffer,
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &*renderFinishedSemaphores[frameIndex]
		};

		graphicsQueue.submit(submitInfo, *drawFence);

		// Present the swapchain image.
		const vk::PresentInfoKHR presentInfoKHR{
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &*renderFinishedSemaphores[frameIndex],
			.swapchainCount = 1,
			.pSwapchains = &*swapchain,
			.pImageIndices = &imageIndex
		};

		result = graphicsQueue.presentKHR(presentInfoKHR);
		frameIndex = (frameIndex + 1) % static_cast<uint32_t>(swapchainImages.size());
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
