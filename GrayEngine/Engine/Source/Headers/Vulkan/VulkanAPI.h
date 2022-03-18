#pragma once
#include <pch.h>
#include <glm/glm.hpp>
#include <tiny_obj_loader.h>
#include <vk_mem_alloc.h>
#include "Engine/Source/Engine.h"
#include "DrawableObj.h"

struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;

	bool isComplete() {
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

struct AllocatedImage {
	VkImage allocatedImage;
	VmaAllocation allocation;
};

class VulkanAPI
{
public:
	bool initVulkan(GLFWwindow* window, VulkanAPI* apiInstance);
	void destroy();
	void drawFrame();
	VkDevice logicalDevice;

	inline VkExtent2D getExtent() { return swapChainExtent; };
	inline static VulkanAPI* m_getRenderer() { return pInstance; };
	inline VmaAllocator getMemAllocator() { return memAllocator; };
	inline VkRenderPass getRenderPass() { return renderPass; };
	bool updateDrawables(uint32_t index);
	static VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code);
	bool Initialized = false;
private:
	GLFWwindow* pParentWindow;
	static VulkanAPI* pInstance;
	VmaAllocator memAllocator;

	VkInstance _vulkan;
	VkPhysicalDeviceProperties deviceProps;
	VkQueue presentQueue;
	VkSurfaceKHR surface;
	VkPhysicalDevice physicalDevice = nullptr;
	const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	VkSwapchainKHR swapChain;
	std::vector<VkImage> swapChainImages;
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;
	std::vector<VkImageView> swapChainImageViews;
	VkRenderPass renderPass;
	std::vector<VkFramebuffer> swapChainFramebuffers;
	VkCommandPool commandPool;
	std::vector<VkCommandBuffer> commandBuffers;
	VkSemaphore imageAvailableSemaphore;
	VkSemaphore renderFinishedSemaphore;
	VkQueue graphicsQueue;
	VkFence drawFence;

	VkMemoryRequirements memRequirements;

	std::vector<DrawableObj> drawables;

	bool createVKInstance();
	bool createMemoryAllocator();
	bool createLogicalDevice();
	bool isDeviceSuitable(VkPhysicalDevice device);
	bool checkDeviceExtensionSupport(VkPhysicalDevice device);

	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

	bool createSwapChain();
	bool createImageViews();
	bool createGraphicsPipeline();

	bool createRenderPass();
	bool createFramebuffers();
	bool createCommandPool();
	bool createCommandBuffers();
	bool createSemaphores();

	static void callSwapChainUpdate(std::vector<double> para);
	void recreateSwapChain();
	void cleanupSwapChain();
	bool loadModel(const char* model_path);

	void clearDrawables();

	VkImageView depthImageView;
	AllocatedImage depthImage;
	VkFormat depthFormat;
};