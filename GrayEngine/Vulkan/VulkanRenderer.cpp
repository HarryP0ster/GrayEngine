#include <pch.h>
#define GLFW_INCLUDE_VULKAN
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "VulkanRenderer.h"

namespace GrEngine_Vulkan
{
	void VulkanRenderer::destroy()
	{
		Initialized = false;

		waitForRenderer();

		vmaUnmapMemory(memAllocator, pickingBuffer.Allocation);
		VulkanAPI::m_destroyShaderBuffer(logicalDevice, memAllocator, &pickingBuffer);

		while (entities.size() > 0)
		{
			std::map<UINT, GrEngine::Entity*>::iterator pos = entities.begin();
			dynamic_cast<VulkanDrawable*>((*pos).second)->destroyObject();
			delete (*pos).second;
			entities.erase(pos);
		}

		cleanupSwapChain();
		resources.Clean(logicalDevice, memAllocator);

		VulkanAPI::FreeCommandBuffers(commandBuffers.data(), commandBuffers.size());
		VulkanAPI::Destroy(logicalDevice, memAllocator);

		vkDestroySurfaceKHR(_vulkan, surface, nullptr);
		vkDestroyInstance(_vulkan, nullptr);
	}

	bool VulkanRenderer::init(void* window) //Vulkan integration done with a help of vulkan-tutorial.com
	{
		bool res = true;
		pParentWindow = reinterpret_cast<GLFWwindow*>(window);

		if (!createVKInstance())
			throw std::runtime_error("Failed to create vulkan instance!");

		uint32_t deviceCount = 0;

		vkEnumeratePhysicalDevices(_vulkan, &deviceCount, nullptr);
		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(_vulkan, &deviceCount, devices.data());

		if (deviceCount == 0)
			throw std::runtime_error("Failed to find GPUs with Vulkan support!");

		if (!glfwCreateWindowSurface(_vulkan, pParentWindow, nullptr, &surface) == VK_SUCCESS)
			Logger::Out("[Vk] Failed to create presentation surface", OutputColor::Red, OutputType::Error);

		for (const auto& device : devices)
		{
			if (isDeviceSuitable(device))
			{
				physicalDevice = device;
				vkGetPhysicalDeviceProperties(physicalDevice, &deviceProps);
				//msaaSamples = VulkanAPI::GetMaxSampleCount(physicalDevice);
				msaaSamples = VK_SAMPLE_COUNT_1_BIT;
				break;
			}
		}

		if (physicalDevice == VK_NULL_HANDLE)
			throw std::exception("Failed to find suitable Vulkan device");
		else
			Logger::Out("Presentation device: %s", OutputColor::Green, OutputType::Log, deviceProps.deviceName);


		float queuePriority = 1.f;
		VkPhysicalDeviceFeatures deviceFeatures = VulkanAPI::StructPhysicalDeviceFeatures();
		std::vector<VkDeviceQueueCreateInfo> deviceQueues = VulkanAPI::StructQueueCreateInfo(physicalDevice, surface, { VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_COMPUTE_BIT }, &queuePriority);
		compute_bit = deviceQueues[1].queueFamilyIndex;
		VkDeviceCreateInfo deviceInfo = VulkanAPI::StructDeviceCreateInfo(physicalDevice, &deviceFeatures, deviceQueues.data(), deviceQueues.size(), deviceExtensions.data(), deviceExtensions.size());

		if ((res = VulkanAPI::CreateLogicalDevice(physicalDevice, &deviceInfo, &logicalDevice) & res) == false)
			Logger::Out("[Vk] Failed to create logical device", OutputColor::Red, OutputType::Error);

		if ((res = VulkanAPI::GetDeviceQueue(logicalDevice, deviceQueues[0].queueFamilyIndex, &graphicsQueue) & res) == false)
			Logger::Out("[Vk] Failed to get graphics queue!", OutputColor::Red, OutputType::Error);

		if ((res = VulkanAPI::GetDeviceQueue(logicalDevice, deviceQueues[0].queueFamilyIndex, &presentQueue) & res) == false)
			Logger::Out("[Vk] Failed to get present queue!", OutputColor::Red, OutputType::Error);

		if ((res = VulkanAPI::CreateVulkanMemoryAllocator(_vulkan, physicalDevice, logicalDevice, &memAllocator) & res) == false)
			Logger::Out("[Vk] Failed to create memory allocator", OutputColor::Red, OutputType::Error);

		if ((res = VulkanAPI::CreateVkSwapchain(physicalDevice, logicalDevice, pParentWindow, surface, &swapChain) & createSwapChainImages() & res) == false)
			Logger::Out("[Vk] Failed to create swap chain", OutputColor::Red, OutputType::Error);

		if ((res = VulkanAPI::CreateRenderPass(logicalDevice, swapChainImageFormat, depthFormat, msaaSamples, &renderPass) & res) == false)
			Logger::Out("[Vk] Failed to create render pass", OutputColor::Red, OutputType::Error);

		createAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &position);
		createAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &normal);
		createAttachment(swapChainImageFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &albedo);


		swapChainFramebuffers.resize(swapChainImageViews.size());
		for (std::size_t i = 0; i < swapChainImageViews.size(); i++)
		{
			VkImageView attachments[] = { swapChainImageViews[i], position.textureImageView, normal.textureImageView, albedo.textureImageView, depthImageView, };

			if ((res = VulkanAPI::CreateFrameBuffer(logicalDevice, renderPass, attachments, 5, swapChainExtent, &swapChainFramebuffers[i]) & res) == false)
				Logger::Out("[Vk] Failed to create framebuffer", OutputColor::Red, OutputType::Error);
		}

		if ((res = VulkanAPI::CreateCommandPool(logicalDevice, deviceQueues[0].queueFamilyIndex, &commandPool) & res) == false)
			Logger::Out("[Vk] Failed to create command pool", OutputColor::Red, OutputType::Error);

		commandBuffers.resize(swapChainFramebuffers.size());
		if ((res = VulkanAPI::AllocateCommandBuffers(logicalDevice, commandPool, commandBuffers.data(), commandBuffers.size()) & res) == false)
			Logger::Out("[Vk] Failed to create command buffer", OutputColor::Red, OutputType::Error);


		if ((res = VulkanAPI::CreateVkSemaphore(logicalDevice, &renderFinishedSemaphore) & res) == false)
			Logger::Out("[Vk] Failed to create semaphores", OutputColor::Red, OutputType::Error);

		if ((res = VulkanAPI::CreateVkSemaphore(logicalDevice, &imageAvailableSemaphore) & res) == false)
			Logger::Out("[Vk] Failed to create semaphores", OutputColor::Red, OutputType::Error);

		if ((res = VulkanAPI::CreateVkFence(logicalDevice, &graphicsFence) & res) == false)
			Logger::Out("[Vk] Failed to create fences", OutputColor::Red, OutputType::Error);


		prepareCompositionPass();
		prepareTransparencyPass();

		sky = new VulkanSkybox(1000000000);
		sky->initObject(logicalDevice, memAllocator, this);
		sky->UpdateNameTag("Sky");
		sky->MakeStatic();
		entities[sky->GetEntityID()] = sky;

		pickingMem = calloc(1, sizeof(PickingInfo));
		VulkanAPI::m_createVkBuffer(logicalDevice, memAllocator, nullptr, sizeof(PickingInfo), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &pickingBuffer);
		vmaMapMemory(memAllocator, pickingBuffer.Allocation, (void**)&pickingMem);

		vkDeviceWaitIdle(logicalDevice);

		return Initialized = res;
	}

	void VulkanRenderer::createAttachment(VkFormat format, VkImageUsageFlags usage, Texture* attachment)
	{
		VkImageAspectFlags aspectMask = 0;
		VkImageLayout imageLayout;

		VulkanAPI::DestroyImage(attachment->newImage.allocatedImage);
		VulkanAPI::DestroyImageView(attachment->textureImageView);
		attachment->initialized = false;

		if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
		{
			aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}
		if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
			imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}

		VkImageCreateInfo imageInfo{};
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.format = format;
		imageInfo.extent.width = swapChainExtent.width;
		imageInfo.extent.height = swapChainExtent.height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.usage = usage | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VulkanAPI::CreateImage(memAllocator, &imageInfo, &attachment->newImage.allocatedImage, &attachment->newImage.allocation);

		VkImageSubresourceRange imageView{};
		imageView.aspectMask = aspectMask;
		imageView.baseMipLevel = 0;
		imageView.levelCount = 1;
		imageView.baseArrayLayer = 0;
		imageView.layerCount = 1;
		VulkanAPI::CreateImageView(logicalDevice, format, attachment->newImage.allocatedImage, imageView, &attachment->textureImageView);

		attachment->initialized = true;
		attachment->texInfo.mipLevels = 1;
		attachment->texInfo.format = format;
		attachment->texInfo.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachment->texInfo.descriptor.imageView = attachment->textureImageView;
		attachment->texInfo.descriptor.sampler = VK_NULL_HANDLE;
	}

	void VulkanRenderer::createSkybox(const char* East, const char* West, const char* Top, const char* Bottom, const char* North, const char* South)
	{
		Initialized = false;

		std::array<std::string, 6> mat_vector = { East, West, Top, Bottom, North, South };
		static_cast<CubemapProperty*>(sky->GetProperty("CubemapProperty"))->SetPropertyValue(mat_vector);

		Initialized = true;
	}

	void VulkanRenderer::RenderFrame()
	{
		drawFrame(swapChainExtent);
	}

	void VulkanRenderer::drawFrame(VkExtent2D extent)
	{
		if (!Initialized) return;

		currentImageIndex = 0;
		vkAcquireNextImageKHR(logicalDevice, swapChain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &currentImageIndex);

		if (!updateDrawables(currentImageIndex, DrawMode::NORMAL, extent))
			throw std::runtime_error("Logical device was lost!");

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = { imageAvailableSemaphore };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[currentImageIndex];
		VkSemaphore signalSemaphores[] = { renderFinishedSemaphore };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		VkResult res;
		vkResetFences(logicalDevice, 1, &graphicsFence);
		res = vkQueueSubmit(graphicsQueue, 1, &submitInfo, graphicsFence);

		if (res == VK_ERROR_DEVICE_LOST)
		{
			Logger::Out("Logical device was lost!", OutputColor::Red, OutputType::Error);
			throw std::runtime_error("Logical device was lost!");
		}

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = { swapChain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &currentImageIndex;
		presentInfo.pResults = nullptr; // Optional

		vkQueuePresentKHR(presentQueue, &presentInfo);
		vkWaitForFences(logicalDevice, 1, &graphicsFence, TRUE, UINT64_MAX);
	}
	void VulkanRenderer::SaveScreenshot(const char* filepath)
	{
		VkImage srcImage = swapChainImages[currentImageIndex];
		VkImage dstImage;
		VkCommandBuffer cmd;
		VkImageCreateInfo dstInf{};
		dstInf.imageType = VK_IMAGE_TYPE_2D;
		dstInf.format = VK_FORMAT_B8G8R8A8_SRGB;
		dstInf.extent.width = swapChainExtent.width;
		dstInf.extent.height = swapChainExtent.height;
		dstInf.extent.depth = 1;
		dstInf.arrayLayers = 1;
		dstInf.mipLevels = 1;
		dstInf.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		dstInf.samples = VK_SAMPLE_COUNT_1_BIT;
		dstInf.tiling = VK_IMAGE_TILING_LINEAR;
		dstInf.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		vkCreateImage(logicalDevice, &dstInf, nullptr, &dstImage);

		VkMemoryRequirements memRequirements;
		VkMemoryAllocateInfo memAllocInfo{};
		VkDeviceMemory dstImageMemory;
		vkGetImageMemoryRequirements(logicalDevice, dstImage, &memRequirements);
		memAllocInfo.allocationSize = memRequirements.size;
		VmaAllocationCreateInfo allocInfo = {};
		allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
		allocInfo.flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		vmaFindMemoryTypeIndex(memAllocator, memRequirements.memoryTypeBits, &allocInfo, &memAllocInfo.memoryTypeIndex);
		vkAllocateMemory(logicalDevice, &memAllocInfo, nullptr, &dstImageMemory);
		vkBindImageMemory(logicalDevice, dstImage, dstImageMemory, 0);

		VulkanAPI::AllocateCommandBuffers(logicalDevice, commandPool, &cmd, 1);
		VulkanAPI::BeginCommandBuffer(cmd, VK_COMMAND_BUFFER_USAGE_FLAG_BITS_MAX_ENUM);
		VulkanAPI::TransitionImageLayout(dstImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VulkanAPI::StructSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT), cmd);
		VulkanAPI::TransitionImageLayout(srcImage, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VulkanAPI::StructSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT), cmd);

		VkImageCopy imageCopyRegion{};
		imageCopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageCopyRegion.srcSubresource.layerCount = 1;
		imageCopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageCopyRegion.dstSubresource.layerCount = 1;
		imageCopyRegion.extent.width = swapChainExtent.width;
		imageCopyRegion.extent.height = swapChainExtent.height;
		imageCopyRegion.extent.depth = 1;

		vkCmdCopyImage(cmd, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopyRegion);

		VulkanAPI::TransitionImageLayout(dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VulkanAPI::StructSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT), cmd);
		VulkanAPI::TransitionImageLayout(srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VulkanAPI::StructSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT), cmd);
		VulkanAPI::EndAndSubmitCommandBuffer(logicalDevice, commandPool, cmd, graphicsQueue, graphicsFence);

		VkImageSubresource subResource{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
		VkSubresourceLayout subResourceLayout;
		vkGetImageSubresourceLayout(logicalDevice, dstImage, &subResource, &subResourceLayout);

		if (filepath != "")
		{
			unsigned char* data;
			vkMapMemory(logicalDevice, dstImageMemory, 0, VK_WHOLE_SIZE, 0, (void**)&data);
			unsigned char* pixels = (unsigned char*)malloc(swapChainExtent.width * swapChainExtent.height * 4);

			int offset = 0;

			//BGRA to RGBA
			for (int y = 0; y < swapChainExtent.height; y++) 
			{
				for (int x = 0; x < swapChainExtent.width; x++) 
				{
					pixels[offset] = data[offset + 2];
					pixels[offset + 1] = data[offset + 1];
					pixels[offset + 2] = data[offset];
					pixels[offset + 3] = 255;

					offset += 4;
				}
			}
			int i = stbi_write_png(filepath, swapChainExtent.width, swapChainExtent.height, 4, pixels, swapChainExtent.width * 4);
			free(pixels);
			vkUnmapMemory(logicalDevice, dstImageMemory);
		}


		vkFreeMemory(logicalDevice, dstImageMemory, nullptr);
		vkDestroyImage(logicalDevice, dstImage, nullptr);
	}

	void VulkanRenderer::SelectEntityAtCursor()
	{
		if (pickingMem == nullptr || pickingBuffer.initialized == false) return;

		POINTFLOAT cur = GrEngine::Engine::GetContext()->GetCursorPosition();
		PickingInfo id{ cur.x, cur.y };


		memcpy_s(pickingMem, sizeof(PickingInfo), &id, sizeof(PickingInfo));

		drawFrame(swapChainExtent);

		memcpy_s(&id, sizeof(PickingInfo), pickingMem, sizeof(PickingInfo));
		int selection = 0;
		for (int i = 0; i < 32; i++)
		{
			if (id.id[i] != 0)
			{
				selection = id.id[i];
				break;
			}
		}
		selectEntity(selection);
	}

	float VulkanRenderer::DistanceToFragment(float x, float y)
	{
		if (pickingMem == nullptr || pickingBuffer.initialized == false) return 0.f;

		PickingInfo out{ x, y };

		memcpy_s(pickingMem, sizeof(PickingInfo) , &out, sizeof(PickingInfo));

		drawFrame(swapChainExtent);

		memcpy_s(&out, sizeof(PickingInfo), pickingMem, sizeof(PickingInfo));
		float depth = 0.f;
		for (int i = 0; i < 32; i++)
		{
			if (out.depth[i] != 0)
			{
				depth = out.depth[i];
				break;
			}
		}

		return depth;
	}

	float VulkanRenderer::DistanceToFragment(float x, float y, UINT id)
	{
		if (pickingMem == nullptr || pickingBuffer.initialized == false) return 0.f;

		PickingInfo out{ x, y };

		memcpy_s(pickingMem, sizeof(PickingInfo), &out, sizeof(PickingInfo));

		drawFrame(swapChainExtent);

		memcpy_s(&out, sizeof(PickingInfo), pickingMem, sizeof(PickingInfo));
		float depth = 0.f;
		for (int i = 0; i < 32; i++)
		{
			if (out.depth[i] != 0 && id == out.id[i])
			{
				depth = out.depth[i];
				break;
			}
		}

		return depth;
	}

	std::array<byte, 3> VulkanRenderer::GetPixelColorAtCursor()
	{
		drawFrame(swapChainExtent);

		VkImage srcImage = swapChainImages[currentImageIndex];
		VkImage dstImage;
		VkCommandBuffer cmd;
		VkImageCreateInfo dstInf = VulkanAPI::StructImageCreateInfo({ swapChainExtent.width, swapChainExtent.height, 1 }, VK_FORMAT_B8G8R8A8_SRGB, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_TRANSFER_DST_BIT);
		dstInf.tiling = VK_IMAGE_TILING_LINEAR;
		vkCreateImage(logicalDevice, &dstInf, nullptr, &dstImage);

		VkMemoryRequirements memRequirements;
		VkMemoryAllocateInfo memAllocInfo{};
		VkDeviceMemory dstImageMemory;
		vkGetImageMemoryRequirements(logicalDevice, dstImage, &memRequirements);
		memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memAllocInfo.allocationSize = memRequirements.size;
		VmaAllocationCreateInfo allocInfo = {};
		allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
		allocInfo.flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		vmaFindMemoryTypeIndex(memAllocator, memRequirements.memoryTypeBits, &allocInfo, &memAllocInfo.memoryTypeIndex);
		vkAllocateMemory(logicalDevice, &memAllocInfo, nullptr, &dstImageMemory);
		vkBindImageMemory(logicalDevice, dstImage, dstImageMemory, 0);

		VulkanAPI::AllocateCommandBuffers(logicalDevice, commandPool, &cmd, 1);
		VulkanAPI::BeginCommandBuffer(cmd, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		VulkanAPI::TransitionImageLayout(dstImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VulkanAPI::StructSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT), cmd);
		VulkanAPI::TransitionImageLayout(srcImage, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VulkanAPI::StructSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT), cmd);

		VkImageCopy imageCopyRegion{};
		imageCopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageCopyRegion.srcSubresource.layerCount = 1;
		imageCopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageCopyRegion.dstSubresource.layerCount = 1;
		imageCopyRegion.extent.width = swapChainExtent.width;
		imageCopyRegion.extent.height = swapChainExtent.height;
		imageCopyRegion.extent.depth = 1;

		vkCmdCopyImage(cmd, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopyRegion);

		VulkanAPI::TransitionImageLayout(dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VulkanAPI::StructSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT), cmd);
		VulkanAPI::TransitionImageLayout(srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VulkanAPI::StructSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT), cmd);
		VulkanAPI::EndAndSubmitCommandBuffer(logicalDevice, commandPool, cmd, graphicsQueue, graphicsFence);

		VkImageSubresource subResource{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
		VkSubresourceLayout subResourceLayout;
		vkGetImageSubresourceLayout(logicalDevice, dstImage, &subResource, &subResourceLayout);

		const char* data;
		vkMapMemory(logicalDevice, dstImageMemory, 0, VK_WHOLE_SIZE, 0, (void**)&data);
		data += subResourceLayout.offset;
		double xpos, ypos;
		glfwGetCursorPos(pParentWindow, &xpos, &ypos);
		data += subResourceLayout.rowPitch * (int)(ypos);
		unsigned int* row = (unsigned int*)data + (int)(xpos);

		double r = ((unsigned char*)row + 2)[0];
		double g = ((unsigned char*)row + 1)[0];
		double b = ((unsigned char*)row)[0];

		vkUnmapMemory(logicalDevice, dstImageMemory);
		vkFreeMemory(logicalDevice, dstImageMemory, nullptr);
		vkDestroyImage(logicalDevice, dstImage, nullptr);

		return { (byte)r, (byte)g, (byte)b };
	}

	void VulkanRenderer::prepareCompositionPass()
	{
		std::vector<VkDescriptorPoolSize> poolSizes;
		poolSizes.resize(2);
		poolSizes[0].descriptorCount = 4;
		poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[1].descriptorCount = 4;
		poolSizes[1].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;

		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;
		setLayoutBindings.resize(3);
		setLayoutBindings[0].binding = 0;
		setLayoutBindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		setLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		setLayoutBindings[0].descriptorCount = 1;
		setLayoutBindings[1].binding = 1;
		setLayoutBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		setLayoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		setLayoutBindings[1].descriptorCount = 1;
		setLayoutBindings[2].binding = 2;
		setLayoutBindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		setLayoutBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		setLayoutBindings[2].descriptorCount = 1;

		VulkanAPI::CreateDescriptorPool(logicalDevice, poolSizes, &compositionSetPool);
		VulkanAPI::CreateDescriptorSetLayout(logicalDevice, setLayoutBindings, &compositionSetLayout);
		VulkanAPI::CreatePipelineLayout(logicalDevice, {}, { compositionSetLayout }, &compositionPipelineLayout);
		VulkanAPI::AllocateDescriptorSet(logicalDevice, compositionSetPool, compositionSetLayout, &compositionSet);

		std::vector<VkWriteDescriptorSet> writeDescriptorSets;
		writeDescriptorSets.resize(3);
		writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSets[0].dstBinding = 0;
		writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		writeDescriptorSets[0].dstSet = compositionSet;
		writeDescriptorSets[0].pImageInfo = &position.texInfo.descriptor;
		writeDescriptorSets[0].descriptorCount = 1;
		writeDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSets[1].dstBinding = 1;
		writeDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		writeDescriptorSets[1].dstSet = compositionSet;
		writeDescriptorSets[1].pImageInfo = &normal.texInfo.descriptor;
		writeDescriptorSets[1].descriptorCount = 1;
		writeDescriptorSets[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSets[2].dstBinding = 2;
		writeDescriptorSets[2].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		writeDescriptorSets[2].dstSet = compositionSet;
		writeDescriptorSets[2].pImageInfo = &albedo.texInfo.descriptor;
		writeDescriptorSets[2].descriptorCount = 1;

		vkUpdateDescriptorSets(logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

		// Pipeline
		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;
		VkPipelineRasterizationStateCreateInfo rasterizationState{};
		rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizationState.depthClampEnable = VK_FALSE;
		rasterizationState.rasterizerDiscardEnable = VK_FALSE;
		rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizationState.lineWidth = 1.0f;
		rasterizationState.cullMode = VK_CULL_MODE_NONE;
		rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizationState.depthBiasEnable = VK_FALSE;
		rasterizationState.depthBiasConstantFactor = 0.0f;
		rasterizationState.depthBiasClamp = 0.0f;
		rasterizationState.depthBiasSlopeFactor = 0.0f;
		VkPipelineColorBlendAttachmentState blendAttachmentState{};
		blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		blendAttachmentState.blendEnable = VK_FALSE;

		VkPipelineColorBlendStateCreateInfo colorBlendState{};
		colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlendState.attachmentCount = 1;
		colorBlendState.pAttachments = &blendAttachmentState;

		VkPipelineDepthStencilStateCreateInfo depthStencilState{};
		depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencilState.pNext = nullptr;
		depthStencilState.depthTestEnable = VK_TRUE;
		depthStencilState.depthWriteEnable = VK_TRUE;
		depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		depthStencilState.depthBoundsTestEnable = VK_FALSE;
		depthStencilState.minDepthBounds = 0.0f; // Optional
		depthStencilState.maxDepthBounds = 1.0f; // Optional
		depthStencilState.stencilTestEnable = VK_FALSE;

		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = nullptr;
		viewportState.scissorCount = 1;
		viewportState.pScissors = nullptr;

		VkPipelineMultisampleStateCreateInfo multisampleState{};
		multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampleState.sampleShadingEnable = VK_TRUE;
		multisampleState.minSampleShading = 0;

		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.pNext = nullptr;
		dynamicState.flags = 0;
		dynamicState.dynamicStateCount = (uint32_t)dynamicStates.size();
		dynamicState.pDynamicStates = dynamicStates.data();

		std::string solution_path = GrEngine::Globals::getExecutablePath();
		std::vector<char> vertShaderCode = GrEngine::Globals::readFile(solution_path + "Shaders//composition_vert.spv");
		std::vector<char> fragShaderCode = GrEngine::Globals::readFile(solution_path + "Shaders//composition_frag.spv");
		VkShaderModule shaders[2] = { VulkanAPI::m_createShaderModule(logicalDevice, vertShaderCode) , VulkanAPI::m_createShaderModule(logicalDevice, fragShaderCode) };

		VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = shaders[0];
		vertShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = shaders[1];
		fragShaderStageInfo.pName = "main";


		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = { vertShaderStageInfo , fragShaderStageInfo };

		VkSpecializationMapEntry specializationEntry{};
		specializationEntry.constantID = 0;
		specializationEntry.offset = 0;
		specializationEntry.size = sizeof(uint32_t);

		uint32_t specializationData = 0;

		VkSpecializationInfo specializationInfo;
		specializationInfo.mapEntryCount = 1;
		specializationInfo.pMapEntries = &specializationEntry;
		specializationInfo.dataSize = sizeof(specializationData);
		specializationInfo.pData = &specializationData;

		shaderStages[1].pSpecializationInfo = &specializationInfo;

		VkGraphicsPipelineCreateInfo pipelineCI{};

		VkPipelineVertexInputStateCreateInfo emptyInputState{};
		emptyInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineCI.layout = compositionPipelineLayout;
		pipelineCI.renderPass = renderPass;
		pipelineCI.basePipelineIndex = 0;
		pipelineCI.pVertexInputState = &emptyInputState;
		pipelineCI.pInputAssemblyState = &inputAssembly;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;
		pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCI.pStages = shaderStages.data();
		pipelineCI.subpass = 1;
		depthStencilState.depthWriteEnable = VK_FALSE;

		VulkanAPI::CreateGraphicsPipeline(logicalDevice, &pipelineCI, &compositionPipeline);

		vkDestroyShaderModule(logicalDevice, shaders[0], nullptr);
		vkDestroyShaderModule(logicalDevice, shaders[1], nullptr);
	}

	void VulkanRenderer::prepareTransparencyPass()
	{
		VkCommandBuffer cmd;

		const int node_count = 20;
		geometrySBO.count = 0;
		geometrySBO.maxNodeCount = node_count * swapChainExtent.width * swapChainExtent.height;
		VulkanAPI::m_createVkBuffer(logicalDevice, memAllocator, &geometrySBO, sizeof(geometrySBO), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, &transBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VulkanAPI::m_createVkBuffer(logicalDevice, memAllocator, nullptr, sizeof(Node) * node_count* swapChainExtent.width* swapChainExtent.height, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &nodeBfffer);

		VkImageCreateInfo imageInfo = VulkanAPI::StructImageCreateInfo({ swapChainExtent.width,  swapChainExtent.height, 1 }, VK_FORMAT_R32_UINT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
		VulkanAPI::CreateImage(memAllocator, &imageInfo, &headIndex.newImage.allocatedImage, &headIndex.newImage.allocation);

		VkImageSubresourceRange viewRange{};
		viewRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewRange.baseMipLevel = 0;
		viewRange.levelCount = 1;
		viewRange.baseArrayLayer = 0;
		viewRange.layerCount = 1;
		VulkanAPI::CreateImageView(logicalDevice, VK_FORMAT_R32_UINT, headIndex.newImage.allocatedImage, viewRange, &headIndex.textureImageView);

		headIndex.srcInfo.channels = 1;
		headIndex.srcInfo.width = swapChainExtent.width;
		headIndex.srcInfo.height = swapChainExtent.height;
		headIndex.textureSampler = VK_NULL_HANDLE;
		headIndex.texInfo.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		headIndex.texInfo.descriptor.imageView = headIndex.textureImageView;
		headIndex.texInfo.mipLevels = 1;
		headIndex.texInfo.format = VK_FORMAT_R32_UINT;
		headIndex.initialized = true;

		VulkanAPI::AllocateCommandBuffers(logicalDevice, commandPool, &cmd, 1);
		VulkanAPI::BeginCommandBuffer(cmd, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		VulkanAPI::TransitionImageLayout(headIndex.newImage.allocatedImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VulkanAPI::StructSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT), cmd);
		VulkanAPI::EndAndSubmitCommandBuffer(logicalDevice, commandPool, cmd, graphicsQueue, graphicsFence);

		std::vector<VkDescriptorPoolSize> poolSizes;
		poolSizes.resize(2);
		poolSizes[0].descriptorCount = 1;
		poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		poolSizes[1].descriptorCount = 1;
		poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;
		setLayoutBindings.resize(2);
		setLayoutBindings[0].binding = 0;
		setLayoutBindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		setLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		setLayoutBindings[0].descriptorCount = 1;
		setLayoutBindings[1].binding = 1;
		setLayoutBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		setLayoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		setLayoutBindings[1].descriptorCount = 1;

		VulkanAPI::CreateDescriptorPool(logicalDevice, poolSizes, &transparencySetPool);
		VulkanAPI::CreateDescriptorSetLayout(logicalDevice, setLayoutBindings, &transparencySetLayout);
		VulkanAPI::CreatePipelineLayout(logicalDevice, {}, { transparencySetLayout }, &transparencyPipelineLayout);
		VulkanAPI::AllocateDescriptorSet(logicalDevice, transparencySetPool, transparencySetLayout, &transparencySet);

		// Image descriptors for the offscreen color attachments
		VkDescriptorImageInfo texDescriptorHead{};
		texDescriptorHead.imageView = headIndex.textureImageView;
		texDescriptorHead.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		texDescriptorHead.sampler = VK_NULL_HANDLE;

		std::vector<VkWriteDescriptorSet> writeDescriptorSets;
		writeDescriptorSets.resize(2);
		writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSets[0].dstBinding = 0;
		writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writeDescriptorSets[0].dstSet = transparencySet;
		writeDescriptorSets[0].pImageInfo = &texDescriptorHead;
		writeDescriptorSets[0].descriptorCount = 1;
		writeDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSets[1].dstBinding = 1;
		writeDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writeDescriptorSets[1].dstSet = transparencySet;
		writeDescriptorSets[1].pBufferInfo = &nodeBfffer.BufferInfo;
		writeDescriptorSets[1].descriptorCount = 1;

		vkUpdateDescriptorSets(logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

		// Pipeline
		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;
		VkPipelineRasterizationStateCreateInfo rasterizationState{};
		rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizationState.depthClampEnable = VK_FALSE;
		rasterizationState.rasterizerDiscardEnable = VK_FALSE;
		rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizationState.lineWidth = 1.0f;
		rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
		rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizationState.depthBiasEnable = VK_FALSE;
		rasterizationState.depthBiasConstantFactor = 0.0f;
		rasterizationState.depthBiasClamp = 0.0f;
		rasterizationState.depthBiasSlopeFactor = 0.0f;
		VkPipelineColorBlendAttachmentState blendAttachmentState{};
		blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		blendAttachmentState.blendEnable = VK_FALSE;

		VkPipelineColorBlendStateCreateInfo colorBlendState{};
		colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlendState.attachmentCount = 1;
		colorBlendState.pAttachments = &blendAttachmentState;

		VkPipelineDepthStencilStateCreateInfo depthStencilState{};
		depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencilState.pNext = nullptr;
		depthStencilState.depthTestEnable = VK_FALSE;
		depthStencilState.depthWriteEnable = VK_FALSE;
		depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		depthStencilState.depthBoundsTestEnable = VK_FALSE;
		depthStencilState.minDepthBounds = 0.0f; // Optional
		depthStencilState.maxDepthBounds = 1.0f; // Optional
		depthStencilState.stencilTestEnable = VK_FALSE;

		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = nullptr;
		viewportState.scissorCount = 1;
		viewportState.pScissors = nullptr;

		VkPipelineMultisampleStateCreateInfo multisampleState{};
		multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampleState.sampleShadingEnable = VK_TRUE;
		multisampleState.minSampleShading = 0;

		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.pNext = nullptr;
		dynamicState.flags = 0;
		dynamicState.dynamicStateCount = (uint32_t)dynamicStates.size();
		dynamicState.pDynamicStates = dynamicStates.data();

		std::string solution_path = GrEngine::Globals::getExecutablePath();
		std::vector<char> vertShaderCode = GrEngine::Globals::readFile(solution_path + "Shaders//composition_vert_transparent.spv");
		std::vector<char> fragShaderCode = GrEngine::Globals::readFile(solution_path + "Shaders//composition_frag_transparent.spv");
		VkShaderModule shaders[2] = { VulkanAPI::m_createShaderModule(logicalDevice, vertShaderCode) , VulkanAPI::m_createShaderModule(logicalDevice, fragShaderCode) };

		VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = shaders[0];
		vertShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = shaders[1];
		fragShaderStageInfo.pName = "main";


		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = { vertShaderStageInfo , fragShaderStageInfo };

		std::vector<VkPipelineColorBlendAttachmentState> blendAttachmentStates;
		blendAttachmentStates.resize(1);
		blendAttachmentStates[0].blendEnable = VK_TRUE;
		blendAttachmentStates[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		blendAttachmentStates[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendAttachmentStates[0].colorBlendOp = VK_BLEND_OP_ADD;
		blendAttachmentStates[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttachmentStates[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		blendAttachmentStates[0].alphaBlendOp = VK_BLEND_OP_ADD;
		blendAttachmentStates[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		colorBlendState.attachmentCount = static_cast<uint32_t>(blendAttachmentStates.size());
		colorBlendState.pAttachments = blendAttachmentStates.data();

		// Use specialization constants to pass number of lights to the shader
		VkSpecializationMapEntry specializationEntry{};
		specializationEntry.constantID = 0;
		specializationEntry.offset = 0;
		specializationEntry.size = sizeof(uint32_t);

		uint32_t specializationData = 0;

		VkSpecializationInfo specializationInfo;
		specializationInfo.mapEntryCount = 1;
		specializationInfo.pMapEntries = &specializationEntry;
		specializationInfo.dataSize = sizeof(specializationData);
		specializationInfo.pData = &specializationData;

		shaderStages[1].pSpecializationInfo = &specializationInfo;

		VkGraphicsPipelineCreateInfo pipelineCI{};

		VkPipelineVertexInputStateCreateInfo emptyInputState{};
		emptyInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

		pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineCI.layout = transparencyPipelineLayout;
		pipelineCI.renderPass = renderPass;
		pipelineCI.basePipelineIndex = 0;
		pipelineCI.pVertexInputState = &emptyInputState;
		pipelineCI.pInputAssemblyState = &inputAssembly;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;
		pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCI.pStages = shaderStages.data();
		// Index of the subpass that this pipeline will be used in
		pipelineCI.subpass = 3;

		depthStencilState.depthWriteEnable = VK_FALSE;

		VulkanAPI::CreateGraphicsPipeline(logicalDevice, &pipelineCI, &transparencyPipeline);

		vkDestroyShaderModule(logicalDevice, shaders[0], nullptr);
		vkDestroyShaderModule(logicalDevice, shaders[1], nullptr);
	}

	bool VulkanRenderer::updateDrawables(uint32_t index, DrawMode mode, VkExtent2D extent)
	{
		VkClearValue clearValues[5];
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[2].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[3].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[4].depthStencil = { 1.0f, 0 };


		VkRenderPassBeginInfo renderPassInfo{};
		VkViewport viewport{};
		VkRect2D scissor{};

		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.framebuffer = swapChainFramebuffers[index];
		renderPassInfo.renderPass = renderPass;
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = extent;
		renderPassInfo.clearValueCount = 5;
		renderPassInfo.pClearValues = clearValues;

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		if (vkBeginCommandBuffer(commandBuffers[index], &beginInfo) != VK_SUCCESS)
			return false;

		VkClearColorValue clearColor;
		clearColor.uint32[0] = 0xffffffff;

		VkImageSubresourceRange subresRange = {};
		subresRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresRange.levelCount = 1;
		subresRange.layerCount = 1;
		vkCmdClearColorImage(commandBuffers[index], headIndex.newImage.allocatedImage, VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &subresRange);

		vkCmdFillBuffer(commandBuffers[index], transBuffer.Buffer, 0, sizeof(uint32_t), 0);

		// We need a barrier to make sure all writes are finished before starting to write again
		VkMemoryBarrier memoryBarrier{};
		memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		vkCmdPipelineBarrier(commandBuffers[index], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

		vkCmdBeginRenderPass(commandBuffers[index], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		viewport.x = 0;
		viewport.y = 0;
		viewport.width = (float)extent.width;
		viewport.height = (float)extent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffers[index], 0, 1, &viewport);
		scissor.offset = { 0, 0 };
		scissor.extent = swapChainExtent;
		vkCmdSetScissor(commandBuffers[index], 0, 1, &scissor);

		for (std::map<UINT, GrEngine::Entity*>::iterator itt = entities.begin(); itt != entities.end(); ++itt)
		{
			if ((*itt).second->GetEntityType() == "Object" && dynamic_cast<VulkanObject*>((*itt).second)->IsVisible())
			{
				static_cast<VulkanObject*>((*itt).second)->recordCommandBuffer(commandBuffers[index], swapChainExtent, DrawMode::NORMAL);
			}
			else if ((*itt).second->GetEntityType() == "Skybox")
			{
				static_cast<VulkanSkybox*>((*itt).second)->recordCommandBuffer(commandBuffers[index], swapChainExtent, DrawMode::NORMAL);
			}
			else if ((*itt).second->GetEntityType() == "Terrain")
			{
				static_cast<VulkanTerrain*>((*itt).second)->recordCommandBuffer(commandBuffers[index], swapChainExtent, DrawMode::NORMAL);
			}
		}

		vkCmdNextSubpass(commandBuffers[index], VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(commandBuffers[index], VK_PIPELINE_BIND_POINT_GRAPHICS, compositionPipeline);
		vkCmdBindDescriptorSets(commandBuffers[index], VK_PIPELINE_BIND_POINT_GRAPHICS, compositionPipelineLayout, 0, 1, &compositionSet, 0, NULL);
		vkCmdDraw(commandBuffers[index], 3, 1, 0, 0);

		vkCmdNextSubpass(commandBuffers[index], VK_SUBPASS_CONTENTS_INLINE);

		for (std::map<UINT, GrEngine::Entity*>::iterator itt = entities.begin(); itt != entities.end(); ++itt)
		{
			if ((*itt).second->GetEntityType() == "Object" && dynamic_cast<VulkanObject*>((*itt).second)->IsVisible())
			{
				static_cast<VulkanObject*>((*itt).second)->recordCommandBuffer(commandBuffers[index], swapChainExtent, DrawMode::TRANSPARENCY);
			}
		}

		vkCmdNextSubpass(commandBuffers[index], VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(commandBuffers[index], VK_PIPELINE_BIND_POINT_GRAPHICS, transparencyPipeline);
		vkCmdBindDescriptorSets(commandBuffers[index], VK_PIPELINE_BIND_POINT_GRAPHICS, transparencyPipelineLayout, 0, 1, &transparencySet, 0, nullptr);
		vkCmdDraw(commandBuffers[index], 3, 1, 0, 0);

		vkCmdEndRenderPass(commandBuffers[index]);

		return vkEndCommandBuffer(commandBuffers[index]) == VK_SUCCESS;
	}

	bool VulkanRenderer::createVKInstance()
	{
#ifndef _DEBUG
		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Gray Engine App";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "Gray Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledLayerCount = 0;

		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;

		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		createInfo.enabledExtensionCount = glfwExtensionCount;
		createInfo.ppEnabledExtensionNames = glfwExtensions;
		createInfo.enabledLayerCount = 0;
#else
		if (enableValidationLayers && !checkValidationLayerSupport()) {
			throw std::runtime_error("validation layers requested, but not available!");
		}

		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hello Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		auto extensions = getRequiredExtensions();
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
		if (enableValidationLayers) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();

			populateDebugMessengerCreateInfo(debugCreateInfo);
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
		}
		else {
			createInfo.enabledLayerCount = 0;

			createInfo.pNext = nullptr;
		}
#endif // DEBUG

		return vkCreateInstance(&createInfo, nullptr, &_vulkan) == VK_SUCCESS;
	}

	bool VulkanRenderer::isDeviceSuitable(const VkPhysicalDevice device)
	{
		float queuePriority = 1.f;
		std::vector<VkDeviceQueueCreateInfo> deviceQueues = VulkanAPI::StructQueueCreateInfo(device, surface, { VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_COMPUTE_BIT }, &queuePriority);
		deviceQueues.insert(deviceQueues.begin(), deviceQueues[0]);

		bool extensionsSupported = VulkanAPI::CheckDeviceExtensionSupport(device, deviceExtensions);

		bool swapChainAdequate = false;
		if (extensionsSupported) {
			SwapChainSupportDetails swapChainSupport = VulkanAPI::QuerySwapChainSupport(surface, device);
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}

		bool is_complete = true;
		for (VkDeviceQueueCreateInfo info : deviceQueues)
		{
			if (info.queueFamilyIndex < 0)
			{
				is_complete = false;
			}
		}

		return is_complete && extensionsSupported && swapChainAdequate;
	}

	bool VulkanRenderer::createSwapChainImages()
	{
		bool res = true;

		SwapChainSupportDetails swapChainSupport = VulkanAPI::QuerySwapChainSupport(surface, physicalDevice);
		VkSurfaceFormatKHR surfaceFormat = VulkanAPI::ChooseSwapSurfaceFormat(swapChainSupport.formats);
		VkExtent2D extent = VulkanAPI::ChooseSwapExtent(pParentWindow, swapChainSupport.capabilities);

		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
		if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}

		vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount, nullptr);
		swapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount, swapChainImages.data());
		swapChainImageFormat = surfaceFormat.format;
		swapChainExtent = extent;

		VkImageCreateInfo samplingImageInfo = VulkanAPI::StructImageCreateInfo({ extent.width, extent.height, 1 }, swapChainImageFormat, msaaSamples, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
		res = VulkanAPI::CreateImage(memAllocator, &samplingImageInfo, &samplingImage.allocatedImage, &samplingImage.allocation) & res;
		res = VulkanAPI::CreateImageView(logicalDevice, swapChainImageFormat, samplingImage.allocatedImage, VulkanAPI::StructSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT), &colorImageView) & res;

		depthFormat = VK_FORMAT_D32_SFLOAT;
		VkImageCreateInfo depthImageCreateInfo = VulkanAPI::StructImageCreateInfo({ extent.width, extent.height, 1 }, depthFormat, msaaSamples, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
		res = VulkanAPI::CreateImage(memAllocator, &depthImageCreateInfo, &depthImage.allocatedImage, &depthImage.allocation) & res;
		res = VulkanAPI::CreateImageView(logicalDevice, depthFormat, depthImage.allocatedImage, VulkanAPI::StructSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT), &depthImageView) & res;

		swapChainImageViews.resize(swapChainImages.size());
		for (std::size_t i = 0; i < swapChainImages.size(); i++) {
			VkImageSubresourceRange subresourceRange = VulkanAPI::StructSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
			if ((res = VulkanAPI::CreateImageView(logicalDevice, swapChainImageFormat, swapChainImages[i], subresourceRange, &swapChainImageViews[i]) & res) == false)
				Logger::Out("[Vk] Failed to create image views", OutputColor::Red, OutputType::Error);
		}

		return res;
	}

	void VulkanRenderer::Update()
	{
		recreateSwapChain();
	}

	void VulkanRenderer::recreateSwapChain()
	{
		Initialized = false; //Block rendering for a time it takes to recreate swapchain

		vkDeviceWaitIdle(logicalDevice);
		vkQueueWaitIdle(graphicsQueue);

		cleanupSwapChain();

		VulkanAPI::CreateVkSwapchain(physicalDevice, logicalDevice, pParentWindow, surface, &swapChain);
		createSwapChainImages();

		swapChainImageViews.resize(swapChainImages.size());

		for (std::size_t i = 0; i < swapChainImages.size(); i++)
		{
			VkImageSubresourceRange subresourceRange = VulkanAPI::StructSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
			VulkanAPI::CreateImageView(logicalDevice, swapChainImageFormat, swapChainImages[i], subresourceRange, &swapChainImageViews[i]);
		}

		createAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &position);
		createAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &normal);
		createAttachment(swapChainImageFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &albedo);

		VulkanAPI::CreateRenderPass(logicalDevice, swapChainImageFormat, depthFormat, msaaSamples, &renderPass);
		prepareCompositionPass();
		prepareTransparencyPass();

		swapChainFramebuffers.resize(swapChainImageViews.size());
		for (std::size_t i = 0; i < swapChainImageViews.size(); i++) {
			VkImageView attachments[] = { swapChainImageViews[i], position.textureImageView, normal.textureImageView, albedo.textureImageView, depthImageView, };


			VulkanAPI::CreateFrameBuffer(logicalDevice, renderPass, attachments, 5, swapChainExtent, &swapChainFramebuffers[i]);
		}

		commandBuffers.resize(swapChainFramebuffers.size());
		VulkanAPI::AllocateCommandBuffers(logicalDevice, commandPool, commandBuffers.data(), commandBuffers.size());



		Initialized = swapChainExtent.height != 0;
	}

	void VulkanRenderer::cleanupSwapChain()
	{
		for (std::size_t i = 0; i < swapChainFramebuffers.size(); i++)
		{
			VulkanAPI::DestroyFramebuffer(swapChainFramebuffers[i]);
		}
		swapChainFramebuffers.resize(0);

		VulkanAPI::FreeDescriptorSet(compositionSet);
		VulkanAPI::DestroyDescriptorLayout(compositionSetLayout);
		VulkanAPI::DestroyDescriptorPool(compositionSetPool);
		VulkanAPI::DestroyPipeline(compositionPipeline);
		VulkanAPI::DestroyPipelineLayout(compositionPipelineLayout);

		VulkanAPI::FreeDescriptorSet(transparencySet);
		VulkanAPI::DestroyDescriptorLayout(transparencySetLayout);
		VulkanAPI::DestroyDescriptorPool(transparencySetPool);
		VulkanAPI::DestroyPipeline(transparencyPipeline);
		VulkanAPI::DestroyPipelineLayout(transparencyPipelineLayout);

		VulkanAPI::DestroyImageView(headIndex.textureImageView);
		VulkanAPI::DestroyImage(headIndex.newImage.allocatedImage);

		if (transBuffer.initialized == true)
		{
			VulkanAPI::m_destroyShaderBuffer(logicalDevice, memAllocator, &transBuffer);
		}

		if (nodeBfffer.initialized == true)
		{
			VulkanAPI::m_destroyShaderBuffer(logicalDevice, memAllocator, &nodeBfffer);
			nodeBfffer.Buffer = nullptr;
		}

		VulkanAPI::FreeCommandBuffers(commandBuffers.data(), commandBuffers.size());
		commandBuffers.resize(0);

		VulkanAPI::DestroyRenderPass(renderPass);

		for (std::size_t i = 0; i < swapChainImageViews.size(); i++)
		{
			VulkanAPI::DestroyImageView(swapChainImageViews[i]);
		}
		swapChainImageViews.resize(0);

		if (colorImageView != nullptr)
		{
			VulkanAPI::DestroyImageView(colorImageView);
			VulkanAPI::DestroyImage(samplingImage.allocatedImage);
		}

		if (depthImageView != nullptr)
		{
			VulkanAPI::DestroyImageView(depthImageView);
			VulkanAPI::DestroyImage(depthImage.allocatedImage);
		}

		VulkanAPI::DestroySwapchainKHR(swapChain);

		depthImageView = nullptr;
		swapChain = nullptr;
	}

	void VulkanRenderer::clearDrawables()
	{
		waitForRenderer();

		int offset = 0;
		while (entities.size() != offset)
		{
			std::map<UINT, GrEngine::Entity*>::iterator pos = entities.begin();
			std::advance(pos, offset);

			if ((*pos).second->IsStatic() == false)
			{
				dynamic_cast<VulkanDrawable*>((*pos).second)->destroyObject();
				delete (*pos).second;
				entities.erase((*pos).first);
			}
			else
			{
				offset++;
			}
		}

		Logger::Out("The scene was cleared", OutputColor::Green, OutputType::Log);
	}

	bool VulkanRenderer::loadModel(UINT id, const char* mesh_path, std::vector<std::string> textures_vector)
	{
		Initialized = false;
		VulkanObject* ref_obj = dynamic_cast<VulkanObject*>(entities[id]);
		ref_obj->LoadModel(mesh_path, textures_vector);

		Initialized = true;
		return true;
	}

	bool VulkanRenderer::loadModel(UINT id, const char* model_path)
	{
		Initialized = false;
		VulkanObject* ref_obj = static_cast<VulkanObject*>(entities[id]);

		if (ref_obj->HasProperty("Drawable"))
		{
			ref_obj->ParsePropertyValue("Drawable", model_path);
		}
		else
		{
			ref_obj->AddNewProperty("Drawable");
			ref_obj->ParsePropertyValue("Drawable", model_path);
		}

		Initialized = true;
		return true;
	}

	bool VulkanRenderer::assignTextures(std::vector<std::string> textures, GrEngine::Entity* target)
	{
		if (target->GetEntityType() == "Object")
		{
			VulkanObject* object = static_cast<VulkanObject*>(target);
			if (object->object_texture != nullptr)
			{
				resources.RemoveTexture(object->object_texture->texture_collection, logicalDevice, memAllocator);
				object->object_texture = nullptr;
			}

			object->object_texture = loadTexture(textures, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_TYPE_2D)->AddLink();
			object->updateObject();
		}
		else if (target->GetEntityType() == "Skybox")
		{
			VulkanSkybox* object = static_cast<VulkanSkybox*>(target);
			if (object->object_texture != nullptr)
			{
				resources.RemoveTexture(object->object_texture->texture_collection, logicalDevice, memAllocator);
				object->object_texture = nullptr;
			}

			object->object_texture = loadTexture(textures, VK_IMAGE_VIEW_TYPE_CUBE, VK_IMAGE_TYPE_2D)->AddLink();
			object->updateObject();
		}

		else if (target->GetEntityType() == "Terrain")
		{
			VulkanTerrain* object = static_cast<VulkanTerrain*>(target);
			if (object->object_texture != nullptr)
			{
				resources.RemoveTexture(object->object_texture->texture_collection, logicalDevice, memAllocator);
				object->object_texture = nullptr;
			}

			object->object_texture = loadTexture(textures, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_TYPE_2D)->AddLink();
			object->updateObject();
		}

		return true;
	}

	Resource<Texture*>* VulkanRenderer::loadTexture(std::vector<std::string> texture_path, VkImageViewType type_view, VkImageType type_img)
	{
		Resource<Texture*>* resource = resources.GetTextureResource(texture_path);

		if (resource == nullptr)
		{
			int maxW = 1;
			int maxH = 1;
			int channels = 4;
			unsigned char* data = nullptr;
			ShaderBuffer stagingBuffer;
			Texture* new_texture = new Texture();

			if (texture_path.size() == 0)
			{
				int size = type_view == VK_IMAGE_VIEW_TYPE_CUBE ? 6 : 1;
				for (int i = 0; i < size; i++)
				{
					texture_path.push_back("empty_texture");
				}


				data = (unsigned char*)malloc(channels * maxW * maxH * size);
				memset(data, (byte)255, channels * maxW * maxH * size);
			}
			else
			{
				for (std::vector<std::string>::iterator itt = texture_path.begin(); itt != texture_path.end(); ++itt)
				{
					int width;
					int height;

					stbi_info((*itt).c_str(), &width, &height, &channels);

					maxW = width > maxW ? width : maxW;
					maxH = height > maxH ? height : maxH;
				}

				std::map<int, std::future<void>> processes_map;
				data = (unsigned char*)malloc(maxW * maxH * channels * texture_path.size());

				//TBD: fix loading same image file on different surfaces
				int procNum = 0;
				int image_size = maxW * maxH * channels;
				for (std::vector<std::string>::iterator itt = texture_path.begin(); itt != texture_path.end(); ++itt)
				{
					const char* texture = (*itt).c_str();
					if ((*itt) != "")
					{
						processes_map[procNum] = std::async(std::launch::async, [texture, data, maxW, maxH, image_size, procNum]()
							{
								int width;
								int height;
								int channels;
								stbi_uc* pixels = stbi_load(texture, &width, &height, &channels, STBI_rgb_alpha);

								if (!pixels)
								{
									Logger::Out("An error occurred while loading the texture: %s", OutputColor::Green, OutputType::Error, texture);
									free(data);
									return;
								}

								if (width < maxW || height < maxH)
								{
									unsigned char* output = (unsigned char*)malloc(image_size);
									stbir_resize_uint8(pixels, width, height, 0, output, maxW, maxH, 0, channels);
									memcpy_s(data + image_size * procNum, image_size, output, image_size);
									stbi_image_free(pixels);
									free(output);
								}
								else
								{
									memcpy_s(data + image_size * procNum, image_size, pixels, image_size);
									stbi_image_free(pixels);
								}
							});
					}
					else
					{
						unsigned char* pixels = (unsigned char*)malloc(image_size);
						memset(pixels, (byte)255, image_size);
						memcpy_s(data + image_size * procNum, image_size, pixels, image_size);
						free(pixels);
						texture_path[procNum] = "empty_texture";
					}

					procNum++;
				}

				for (int ind = 0; ind < processes_map.size(); ind++)
				{
					if (processes_map[ind].valid())
					{
						processes_map[ind].wait();
					}
				}
			}

			VulkanAPI::m_createVkBuffer(logicalDevice, memAllocator, data, sizeof(byte) * maxW * maxH * channels * texture_path.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &stagingBuffer);
			free(data);

			uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(maxW, maxH)))) + 1;

			VkExtent3D imageExtent;
			imageExtent.width = static_cast<uint32_t>(maxW);
			imageExtent.height = static_cast<uint32_t>(maxH);
			imageExtent.depth = 1;

			VkImageCreateInfo dimg_info = VulkanAPI::StructImageCreateInfo(imageExtent, VK_FORMAT_R8G8B8A8_SRGB, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, type_img);
			dimg_info.arrayLayers = texture_path.size();
			dimg_info.mipLevels = mipLevels;
			dimg_info.flags = type_view == VK_IMAGE_VIEW_TYPE_CUBE ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;

			VulkanAPI::CreateImage(memAllocator, &dimg_info, &new_texture->newImage.allocatedImage, &new_texture->newImage.allocation);

			VkCommandBuffer commandBuffer;
			VkImageSubresourceRange subresourceRange = VulkanAPI::StructSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, texture_path.size());

			VulkanAPI::AllocateCommandBuffers(logicalDevice, commandPool, &commandBuffer, 1);
			VulkanAPI::BeginCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
			VulkanAPI::TransitionImageLayout(new_texture->newImage.allocatedImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange, commandBuffer);
			VulkanAPI::EndAndSubmitCommandBuffer(logicalDevice, commandPool, commandBuffer, graphicsQueue, graphicsFence);

			VulkanAPI::CopyBufferToImage(logicalDevice, commandPool, stagingBuffer.Buffer, new_texture->newImage.allocatedImage, graphicsQueue, graphicsFence, { (uint32_t)maxW, (uint32_t)maxH, (uint32_t)channels }, texture_path.size());

			VulkanAPI::AllocateCommandBuffers(logicalDevice, commandPool, &commandBuffer, 1);
			VulkanAPI::BeginCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
			VulkanAPI::TransitionImageLayout(new_texture->newImage.allocatedImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange, commandBuffer);
			VulkanAPI::EndAndSubmitCommandBuffer(logicalDevice, commandPool, commandBuffer, graphicsQueue, graphicsFence);


			generateMipmaps(new_texture->newImage.allocatedImage, maxW, maxH, mipLevels, texture_path.size());


			VulkanAPI::CreateImageView(logicalDevice, VK_FORMAT_R8G8B8A8_SRGB, new_texture->newImage.allocatedImage, subresourceRange, &new_texture->textureImageView, type_view);
			VulkanAPI::m_destroyShaderBuffer(logicalDevice, memAllocator, &stagingBuffer);
			VulkanAPI::CreateSampler(physicalDevice, logicalDevice, &new_texture->textureSampler, (float)mipLevels);

			new_texture->texture_collection = texture_path;
			new_texture->srcInfo.width = maxW;
			new_texture->srcInfo.height = maxH;
			new_texture->srcInfo.channels = channels;
			new_texture->texInfo.mipLevels = mipLevels;
			new_texture->texInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
			new_texture->texInfo.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			new_texture->texInfo.descriptor.sampler = new_texture->textureSampler;
			new_texture->texInfo.descriptor.imageView = new_texture->textureImageView;
			new_texture->initialized = true;

			if (resource != nullptr)
			{
				resources.UpdateTexture(texture_path, logicalDevice, memAllocator, new_texture);
			}
			else
			{
				resource = resources.AddTextureResource(new_texture->texture_collection, new_texture);
			}
		}

		return resource;
	}

	bool VulkanRenderer::updateTexture(GrEngine::Entity* target, int textureIndex)
	{
		VulkanDrawable* object = nullptr;
		if (target->GetEntityType() == "Object")
		{
			object = static_cast<VulkanObject*>(target);
		}
		else if (target->GetEntityType() == "Skybox")
		{
			object = static_cast<VulkanSkybox*>(target);
		}

		else if (target->GetEntityType() == "Terrain")
		{
			object = static_cast<VulkanTerrain*>(target);
		}
		else
		{
			return false;
		}

		Resource<Texture*>* resource = resources.GetTextureResource(object->object_texture->texture_collection);

		if (resource != nullptr)
		{
			int maxW = object->object_texture->srcInfo.width;
			int maxH = object->object_texture->srcInfo.height;
			int channels = object->object_texture->srcInfo.channels;
			uint32_t mipLevels = object->object_texture->texInfo.mipLevels;

			VkCommandBuffer commandBuffer;
			std::vector<VkBufferImageCopy> regions;
			uint32_t offset = 0;

			for (int i = 0; i < object->object_texture->texture_collection.size(); i++)
			{
				VkBufferImageCopy region{};
				region.bufferOffset = offset;

				region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				region.imageSubresource.mipLevel = 0;
				region.imageSubresource.baseArrayLayer = i;
				region.imageSubresource.layerCount = 1;

				region.imageOffset = { 0, 0, 0 };
				region.imageExtent = {
					(uint32_t)maxW,
					(uint32_t)maxH,
					1
				};

				regions.push_back(region);
				offset += maxW * maxH * channels;
			}

			ShaderBuffer stagingBuffer;
			VulkanAPI::m_createVkBuffer(logicalDevice, memAllocator, nullptr, sizeof(byte) * maxW * maxH * channels * object->object_texture->texture_collection.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &stagingBuffer);
			VkImageSubresourceRange subresourceRange = VulkanAPI::StructSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, object->object_texture->texture_collection.size());

			VulkanAPI::AllocateCommandBuffers(logicalDevice, commandPool, &commandBuffer, 1);
			VulkanAPI::BeginCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
			VulkanAPI::TransitionImageLayout(object->object_texture->newImage.allocatedImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, subresourceRange, commandBuffer);
			vkCmdCopyImageToBuffer(commandBuffer, object->object_texture->newImage.allocatedImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer.Buffer, regions.size(), regions.data());
			VulkanAPI::EndAndSubmitCommandBuffer(logicalDevice, commandPool, commandBuffer, graphicsQueue, graphicsFence);

			size_t alloc_size = sizeof(byte) * maxW * maxH * channels * object->object_texture->texture_collection.size();
			unsigned char* data;
			vmaMapMemory(memAllocator, stagingBuffer.Allocation, (void**)&data);

			uint32_t buf_offset = sizeof(byte) * maxW * maxH * channels * textureIndex;
			int width;
			int height;
			stbi_uc* pixels = stbi_load(object->object_texture->texture_collection[textureIndex].c_str(), &width, &height, &channels, STBI_rgb_alpha);

			if (!pixels)
			{
				Logger::Out("An error occurred while loading the texture: %s", OutputColor::Green, OutputType::Error, object->object_texture->texture_collection[textureIndex].c_str());
				vmaUnmapMemory(memAllocator, stagingBuffer.Allocation);
				return false;
			}

			if (width < maxW || height < maxH)
			{
				auto output = (unsigned char*)malloc(sizeof(byte) * maxW * maxH * channels);
				stbir_resize_uint8(pixels, width, height, 0, output, maxW, maxH, 0, channels);
				memcpy_s(data + buf_offset, maxW * maxH * channels, output, maxW * maxH * channels); //sizeof(byte)
				free(pixels);
				free(output);
			}
			else
			{
				memcpy_s(data + buf_offset, width * height * channels, pixels, width * height * channels); //sizeof(byte)
				stbi_image_free(pixels);
			}

			vmaUnmapMemory(memAllocator, stagingBuffer.Allocation);

			VulkanAPI::DestroyImageView(object->object_texture->textureImageView);
			VulkanAPI::DestroySampler(object->object_texture->textureSampler);
			VulkanAPI::DestroyImage(object->object_texture->newImage.allocatedImage);
			object->object_texture->initialized = false;

			VkExtent3D imageExtent;
			imageExtent.width = static_cast<uint32_t>(maxW);
			imageExtent.height = static_cast<uint32_t>(maxH);
			imageExtent.depth = 1;

			VkImageCreateInfo dimg_info = VulkanAPI::StructImageCreateInfo(imageExtent, VK_FORMAT_R8G8B8A8_SRGB, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
			dimg_info.arrayLayers = object->object_texture->texture_collection.size();
			dimg_info.mipLevels = mipLevels;
			dimg_info.flags = 0;

			VulkanAPI::CreateImage(memAllocator, &dimg_info, &object->object_texture->newImage.allocatedImage, &object->object_texture->newImage.allocation);

			VulkanAPI::AllocateCommandBuffers(logicalDevice, commandPool, &commandBuffer, 1);
			VulkanAPI::BeginCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
			VulkanAPI::TransitionImageLayout(object->object_texture->newImage.allocatedImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange, commandBuffer);
			VulkanAPI::EndAndSubmitCommandBuffer(logicalDevice, commandPool, commandBuffer, graphicsQueue, graphicsFence);


			VulkanAPI::CopyBufferToImage(logicalDevice, commandPool, stagingBuffer.Buffer, object->object_texture->newImage.allocatedImage, graphicsQueue, graphicsFence, { (uint32_t)maxW, (uint32_t)maxH, (uint32_t)channels }, object->object_texture->texture_collection.size());


			generateMipmaps(object->object_texture->newImage.allocatedImage, maxW, maxH, mipLevels, object->object_texture->texture_collection.size());


			subresourceRange.levelCount = mipLevels;
			VulkanAPI::AllocateCommandBuffers(logicalDevice, commandPool, &commandBuffer, 1);
			VulkanAPI::BeginCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
			VulkanAPI::TransitionImageLayout(object->object_texture->newImage.allocatedImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange, commandBuffer);
			VulkanAPI::EndAndSubmitCommandBuffer(logicalDevice, commandPool, commandBuffer, graphicsQueue, graphicsFence);

			VulkanAPI::CreateImageView(logicalDevice, VK_FORMAT_R8G8B8A8_SRGB, object->object_texture->newImage.allocatedImage, subresourceRange, &object->object_texture->textureImageView, VK_IMAGE_VIEW_TYPE_2D_ARRAY);
			VulkanAPI::CreateSampler(physicalDevice, logicalDevice, &object->object_texture->textureSampler, (float)mipLevels);

			object->object_texture->texture_collection = object->object_texture->texture_collection;
			object->object_texture->srcInfo.width = maxW;
			object->object_texture->srcInfo.height = maxH;
			object->object_texture->srcInfo.channels = channels;
			object->object_texture->texInfo.mipLevels = mipLevels;
			object->object_texture->texInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
			object->object_texture->texInfo.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			object->object_texture->texInfo.descriptor.sampler = object->object_texture->textureSampler;
			object->object_texture->texInfo.descriptor.imageView = object->object_texture->textureImageView;
			object->object_texture->initialized = true;
			object->updateObject();

			VulkanAPI::m_destroyShaderBuffer(logicalDevice, memAllocator, &stagingBuffer);
			return true;
		}
		else
		{
			return false;
		}
	}

	bool VulkanRenderer::updateTexture(GrEngine::Entity* target, void* pixels, int textureIndex)
	{
		VulkanDrawable* object = nullptr;
		if (target->GetEntityType() == "Object")
		{
			object = static_cast<VulkanObject*>(target);
		}
		else if (target->GetEntityType() == "Skybox")
		{
			object = static_cast<VulkanSkybox*>(target);
		}

		else if (target->GetEntityType() == "Terrain")
		{
			object = static_cast<VulkanTerrain*>(target);
		}
		else
		{
			return false;
		}

		Resource<Texture*>* resource = resources.GetTextureResource(object->object_texture->texture_collection);

		if (resource != nullptr)
		{
			int maxW = object->object_texture->srcInfo.width;
			int maxH = object->object_texture->srcInfo.height;
			int channels = object->object_texture->srcInfo.channels;
			uint32_t mipLevels = object->object_texture->texInfo.mipLevels;

			VkCommandBuffer commandBuffer;
			std::vector<VkBufferImageCopy> regions;
			uint32_t offset = 0;

			for (int i = 0; i < object->object_texture->texture_collection.size(); i++)
			{
				VkBufferImageCopy region{};
				region.bufferOffset = offset;

				region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				region.imageSubresource.mipLevel = 0;
				region.imageSubresource.baseArrayLayer = i;
				region.imageSubresource.layerCount = 1;

				region.imageOffset = { 0, 0, 0 };
				region.imageExtent = {
					(uint32_t)maxW,
					(uint32_t)maxH,
					1
				};

				regions.push_back(region);
				offset += maxW * maxH * channels;
			}

			ShaderBuffer stagingBuffer;
			VulkanAPI::m_createVkBuffer(logicalDevice, memAllocator, nullptr, sizeof(byte) * maxW * maxH * channels * object->object_texture->texture_collection.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &stagingBuffer);
			VkImageSubresourceRange subresourceRange = VulkanAPI::StructSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, object->object_texture->texture_collection.size());

			VulkanAPI::AllocateCommandBuffers(logicalDevice, commandPool, &commandBuffer, 1);
			VulkanAPI::BeginCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
			VulkanAPI::TransitionImageLayout(object->object_texture->newImage.allocatedImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, subresourceRange, commandBuffer);
			vkCmdCopyImageToBuffer(commandBuffer, object->object_texture->newImage.allocatedImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer.Buffer, regions.size(), regions.data());
			VulkanAPI::EndAndSubmitCommandBuffer(logicalDevice, commandPool, commandBuffer, graphicsQueue, graphicsFence);

			unsigned char* data;
			vmaMapMemory(memAllocator, stagingBuffer.Allocation, (void**)&data);

			uint32_t buf_offset = sizeof(byte) * maxW * maxH * channels * textureIndex;
			memcpy_s(data + buf_offset, maxW * maxH * channels, pixels, maxW * maxH * channels); //sizeof(byte)
			vmaUnmapMemory(memAllocator, stagingBuffer.Allocation);

			VulkanAPI::DestroyImageView(object->object_texture->textureImageView);
			VulkanAPI::DestroySampler(object->object_texture->textureSampler);
			VulkanAPI::DestroyImage(object->object_texture->newImage.allocatedImage);
			object->object_texture->initialized = false;

			VkExtent3D imageExtent;
			imageExtent.width = static_cast<uint32_t>(maxW);
			imageExtent.height = static_cast<uint32_t>(maxH);
			imageExtent.depth = 1;

			VkImageCreateInfo dimg_info = VulkanAPI::StructImageCreateInfo(imageExtent, VK_FORMAT_R8G8B8A8_SRGB, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
			dimg_info.arrayLayers = object->object_texture->texture_collection.size();
			dimg_info.mipLevels = mipLevels;
			dimg_info.flags = 0;

			VulkanAPI::CreateImage(memAllocator, &dimg_info, &object->object_texture->newImage.allocatedImage, &object->object_texture->newImage.allocation);

			VulkanAPI::AllocateCommandBuffers(logicalDevice, commandPool, &commandBuffer, 1);
			VulkanAPI::BeginCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
			VulkanAPI::TransitionImageLayout(object->object_texture->newImage.allocatedImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange, commandBuffer);
			VulkanAPI::EndAndSubmitCommandBuffer(logicalDevice, commandPool, commandBuffer, graphicsQueue, graphicsFence);


			VulkanAPI::CopyBufferToImage(logicalDevice, commandPool, stagingBuffer.Buffer, object->object_texture->newImage.allocatedImage, graphicsQueue, graphicsFence, { (uint32_t)maxW, (uint32_t)maxH, (uint32_t)channels }, object->object_texture->texture_collection.size());


			generateMipmaps(object->object_texture->newImage.allocatedImage, maxW, maxH, mipLevels, object->object_texture->texture_collection.size());


			subresourceRange.levelCount = mipLevels;
			VulkanAPI::AllocateCommandBuffers(logicalDevice, commandPool, &commandBuffer, 1);
			VulkanAPI::BeginCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
			VulkanAPI::TransitionImageLayout(object->object_texture->newImage.allocatedImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange, commandBuffer);
			VulkanAPI::EndAndSubmitCommandBuffer(logicalDevice, commandPool, commandBuffer, graphicsQueue, graphicsFence);

			VulkanAPI::CreateImageView(logicalDevice, VK_FORMAT_R8G8B8A8_SRGB, object->object_texture->newImage.allocatedImage, subresourceRange, &object->object_texture->textureImageView, VK_IMAGE_VIEW_TYPE_2D_ARRAY);
			VulkanAPI::CreateSampler(physicalDevice, logicalDevice, &object->object_texture->textureSampler, (float)mipLevels);

			object->object_texture->texture_collection = object->object_texture->texture_collection;
			object->object_texture->srcInfo.width = maxW;
			object->object_texture->srcInfo.height = maxH;
			object->object_texture->srcInfo.channels = channels;
			object->object_texture->texInfo.mipLevels = mipLevels;
			object->object_texture->texInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
			object->object_texture->texInfo.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			object->object_texture->texInfo.descriptor.sampler = object->object_texture->textureSampler;
			object->object_texture->texInfo.descriptor.imageView = object->object_texture->textureImageView;
			object->object_texture->initialized = true;
			object->updateObject();

			VulkanAPI::m_destroyShaderBuffer(logicalDevice, memAllocator, &stagingBuffer);
			return true;
		}
		else
		{
			return false;
		}
	}

	bool VulkanRenderer::updateResource(Texture* target, byte* pixels)
	{
		ShaderBuffer stagingBuffer;
		VkCommandBuffer commandBuffer;
		VulkanAPI::m_createVkBuffer(logicalDevice, memAllocator, pixels, sizeof(byte) * target->srcInfo.width * target->srcInfo.height * target->srcInfo.channels * target->texture_collection.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &stagingBuffer);
		VkImageSubresourceRange subresourceRange = VulkanAPI::StructSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, target->texture_collection.size());

		VulkanAPI::AllocateCommandBuffers(logicalDevice, commandPool, &commandBuffer, 1);
		VulkanAPI::BeginCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		VulkanAPI::TransitionImageLayout(target->newImage.allocatedImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange, commandBuffer);
		VulkanAPI::EndAndSubmitCommandBuffer(logicalDevice, commandPool, commandBuffer, graphicsQueue, graphicsFence);


		VulkanAPI::CopyBufferToImage(logicalDevice, commandPool, stagingBuffer.Buffer, target->newImage.allocatedImage, graphicsQueue, graphicsFence, target->srcInfo, target->texture_collection.size());


		generateMipmaps(target->newImage.allocatedImage, target->srcInfo.width, target->srcInfo.height, target->texInfo.mipLevels, target->texture_collection.size());


		subresourceRange.levelCount = target->texInfo.mipLevels;
		VulkanAPI::AllocateCommandBuffers(logicalDevice, commandPool, &commandBuffer, 1);
		VulkanAPI::BeginCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		VulkanAPI::TransitionImageLayout(target->newImage.allocatedImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange, commandBuffer);
		VulkanAPI::EndAndSubmitCommandBuffer(logicalDevice, commandPool, commandBuffer, graphicsQueue, graphicsFence);

		VulkanAPI::m_destroyShaderBuffer(logicalDevice, memAllocator, &stagingBuffer);
		return true;
	}


	bool VulkanRenderer::updateResource(Texture* target, byte* pixels, uint32_t width, uint32_t height, uint32_t offset_x, uint32_t offset_y)
	{
		byte* data;
		ShaderBuffer stagingBuffer;
		VkCommandBuffer commandBuffer;
		VkImageSubresourceRange subresourceRange = VulkanAPI::StructSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, target->texture_collection.size());

		VulkanAPI::m_createVkBuffer(logicalDevice, memAllocator, nullptr, sizeof(byte) * width * target->srcInfo.channels * height, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &stagingBuffer);
		VulkanAPI::AllocateCommandBuffers(logicalDevice, commandPool, &commandBuffer, 1);
		VulkanAPI::BeginCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		VulkanAPI::TransitionImageLayout(target->newImage.allocatedImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange, commandBuffer);
		VulkanAPI::EndAndSubmitCommandBuffer(logicalDevice, commandPool, commandBuffer, graphicsQueue, graphicsFence);

		std::vector<VkBufferImageCopy> regions;
		VkBufferImageCopy region{};
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		int whole_size = target->srcInfo.width * target->srcInfo.height * target->srcInfo.channels;
		int index = 0;

		vmaMapMemory(memAllocator, stagingBuffer.Allocation, (void**)&data);
		memset(data, (byte)0, width * target->srcInfo.channels * height);
		for (int i = 0; i < height; i++)
		{
			uint32_t offset = (offset_x + (offset_y + i) * target->srcInfo.width) * target->srcInfo.channels;
			if (offset > whole_size)
				continue;

			memcpy_s(data + width * target->srcInfo.channels * index, width * target->srcInfo.channels, pixels + offset,  width * target->srcInfo.channels); //sizeof(byte)

			region.bufferOffset = width * target->srcInfo.channels * index;
			region.imageOffset = { (int)offset_x, (int)offset_y + i, 0 };
			region.imageExtent = {
				width,
				1,
				1
			};

			regions.push_back(region);
			index++;
		}
		vmaUnmapMemory(memAllocator, stagingBuffer.Allocation);


		VulkanAPI::AllocateCommandBuffers(logicalDevice, commandPool, &commandBuffer, 1);
		VulkanAPI::BeginCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.Buffer, target->newImage.allocatedImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, regions.size(), regions.data());
		VulkanAPI::EndAndSubmitCommandBuffer(logicalDevice, commandPool, commandBuffer, graphicsQueue, graphicsFence);


		generateMipmaps(target->newImage.allocatedImage, target->srcInfo.width, target->srcInfo.height, target->texInfo.mipLevels, target->texture_collection.size());


		subresourceRange.levelCount = target->texInfo.mipLevels;
		VulkanAPI::AllocateCommandBuffers(logicalDevice, commandPool, &commandBuffer, 1);
		VulkanAPI::BeginCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		VulkanAPI::TransitionImageLayout(target->newImage.allocatedImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange, commandBuffer);
		VulkanAPI::EndAndSubmitCommandBuffer(logicalDevice, commandPool, commandBuffer, graphicsQueue, graphicsFence);
		VulkanAPI::m_destroyShaderBuffer(logicalDevice, memAllocator, &stagingBuffer);

		return true;
	}

	void VulkanRenderer::generateMipmaps(VkImage image, int32_t texWidth, int32_t texHeight, uint32_t mipLevels, uint32_t arrayLevels)
	{
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties(physicalDevice, VK_FORMAT_R8G8B8A8_SRGB, &formatProperties);

		if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) 
		{
			throw std::runtime_error("texture image format does not support linear blitting!");
		}

		VkCommandBuffer commandBuffer;

		for (int k = 0; k < arrayLevels; k++)
		{
			VulkanAPI::AllocateCommandBuffers(logicalDevice, commandPool, &commandBuffer, 1);
			VulkanAPI::BeginCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

			VkImageMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.image = image;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseArrayLayer = k;
			barrier.subresourceRange.layerCount = arrayLevels;
			barrier.subresourceRange.levelCount = 1;

			int32_t mipWidth = texWidth;
			int32_t mipHeight = texHeight;

			for (uint32_t i = 1; i < mipLevels; i++) 
			{


				barrier.subresourceRange.baseMipLevel = i - 1;
				barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

				vkCmdPipelineBarrier(commandBuffer,
					VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
					0, nullptr,
					0, nullptr,
					1, &barrier);

				VkImageBlit blit{};
				blit.srcOffsets[0] = { 0, 0, 0 };
				blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
				blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				blit.srcSubresource.mipLevel = i - 1;
				blit.srcSubresource.baseArrayLayer = k;
				blit.srcSubresource.layerCount = 1;
				blit.dstOffsets[0] = { 0, 0, 0 };
				blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
				blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				blit.dstSubresource.mipLevel = i;
				blit.dstSubresource.baseArrayLayer = k;
				blit.dstSubresource.layerCount = 1;

				vkCmdBlitImage(commandBuffer,
					image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1, &blit,
					VK_FILTER_LINEAR);

				barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

				vkCmdPipelineBarrier(commandBuffer,
					VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
					0, nullptr,
					0, nullptr,
					1, &barrier);

				if (mipWidth > 1) mipWidth /= 2;
				if (mipHeight > 1) mipHeight /= 2;
			}

			barrier.subresourceRange.baseMipLevel = mipLevels - 1;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier(commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			VulkanAPI::EndAndSubmitCommandBuffer(logicalDevice, commandPool, commandBuffer, graphicsQueue, graphicsFence);
		}
	}

	void VulkanRenderer::LoadTerrain(int resolution, int width, int height, int depth, std::array<std::string, 6> maps)
	{
		if (terrain != nullptr)
		{
			terrain->destroyObject();
			entities.erase(terrain->GetEntityID());
			delete terrain;
			terrain = nullptr;
		}

		terrain = new VulkanTerrain(100);
		terrain->initObject(logicalDevice, memAllocator, this);
		entities[terrain->GetEntityID()] = terrain;

		terrain->LinkExternalStorageBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, &pickingBuffer);

		terrain->GenerateTerrain(resolution, width, height, depth, maps);
	}

	GrEngine::Entity* VulkanRenderer::addEntity()
	{
		UINT id;
		if (free_ids.size() > 0)
		{
			id = free_ids.front();
			free_ids.erase(free_ids.begin());
		}
		else
		{
			next_id[2]++;
			next_id[1] += next_id[2] / 255;
			next_id[0] += next_id[1] / 255;
			next_id[2] %= 255;
			char _buf[11];
			std::snprintf(_buf, sizeof(_buf), "1%03d%03d%03d", next_id[0], next_id[1], next_id[2]);
			id = std::atoi(_buf);
		}

		GrEngine::Entity* ent = new VulkanObject(id);

		static_cast<VulkanObject*>(ent)->LinkExternalStorageBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, &pickingBuffer);

		dynamic_cast<VulkanObject*>(ent)->initObject(logicalDevice, memAllocator, this);
		std::string new_name = std::string("Entity") + std::to_string(entities.size());
		ent->UpdateNameTag(new_name.c_str());
		entities[id] = ent;

		return ent;
	}

	GrEngine::Entity* VulkanRenderer::addEntity(UINT id)
	{
		char _buf[11];
		std::snprintf(_buf, sizeof(_buf), "1%03d%03d%03d", next_id[0], next_id[1], next_id[2]);

		if (id > std::atoi(_buf))
		{
			next_id = { id / 1000000 % 1000, id / 1000 % 1000, id % 1000 };
			next_id[2]++;
			next_id[1] += next_id[2] / 255;
			next_id[0] += next_id[1] / 255;
			next_id[2] %= 255;
		}

		GrEngine::Entity* ent = new VulkanObject(id);

		static_cast<VulkanObject*>(ent)->LinkExternalStorageBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, &pickingBuffer);

		static_cast<VulkanObject*>(ent)->initObject(logicalDevice, memAllocator, this);
		std::string new_name = std::string("Entity") + std::to_string(entities.size() + 1);
		ent->UpdateNameTag(new_name.c_str());
		entities[ent->GetEntityID()] = ent;

		return ent;
	}

	void VulkanRenderer::addEntity(GrEngine::Entity* entity)
	{

		static_cast<VulkanObject*>(entity)->LinkExternalStorageBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, &pickingBuffer);

		static_cast<VulkanObject*>(entity)->initObject(logicalDevice, memAllocator, this);
		std::string new_name = std::string("Entity") + std::to_string(entities.size() + 1);
		entity->UpdateNameTag(new_name.c_str());
		entities[entity->GetEntityID()] = entity;
	}

	GrEngine::Entity* VulkanRenderer::selectEntity(UINT ID)
	{
		selected_entity = ID;
		std::vector<double> para = { (double)selected_entity };
		listener->registerEvent(EventType::SelectionChanged, para);

		if (entities.contains(ID))
		{
			if (highlight_selection)
				VulkanObject::selected_id = ID;
			else
				VulkanObject::selected_id = 0;

			return entities.at(ID);
		}

		selected_entity = 0;
		VulkanObject::selected_id = 0;
		return nullptr;
	}

	void VulkanRenderer::DeleteEntity(UINT id)
	{
		waitForRenderer();

		auto object = entities.at(id);
		if (object != nullptr)
		{
			dynamic_cast<VulkanObject*>(object)->destroyObject();
			entities.erase(id);
			free_ids.push_back(id);
		}
	}

	void VulkanRenderer::SetHighlightingMode(bool enabled)
	{
		highlight_selection = enabled;
	}

	void VulkanRenderer::waitForRenderer()
	{
		vkDeviceWaitIdle(logicalDevice);
		vkQueueWaitIdle(graphicsQueue);
	}

	void VulkanRenderer::SaveScene(const char* path)
	{
		std::fstream new_file;
		new_file.open(path, std::fstream::out | std::ios::trunc);

		if (!new_file)
		{
			Logger::Out("Couldn't create file for saving!", OutputColor::Red, OutputType::Error);
			return;
		}

		new_file << "viewport\n{\n";
		std::vector<EntityProperty*> cur_props = getActiveViewport()->GetProperties();
		for (std::vector<EntityProperty*>::iterator itt = cur_props.begin(); itt != cur_props.end(); ++itt)
		{
			new_file << "    " << (*itt)->PrpertyNameString() << " " << (*itt)->ValueString() << "\n";
		}
		new_file << "}\n";

		for (std::map<UINT, GrEngine::Entity*>::iterator itt = entities.begin(); itt != entities.end(); ++itt)
		{
			if ((*itt).second->IsStatic() == false || (*itt).second == sky)
			{
				new_file << (*itt).second->GetEntityType() << " " << (*itt).second->GetEntityID() << "\n{\n";
				cur_props = (*itt).second->GetProperties();
				for (std::vector<EntityProperty*>::iterator itt = cur_props.begin(); itt != cur_props.end(); ++itt)
				{
					new_file << "   " << (*itt)->PrpertyNameString() << " " << (*itt)->ValueString() << "\n";
				}
				new_file << "}\n";
			}
		}

		new_file << '\0';
		new_file.close();
	}

	void VulkanRenderer::LoadScene(const char* path)
	{
		Initialized = false;
		clearDrawables();

		std::ifstream file(path, std::ios::ate | std::ios::binary);

		if (!file.is_open())
		{
			file.close();
			file.open(path, std::ios::ate | std::ios::binary);
		}

		file.seekg(0);
		std::string stream;
		std::string cur_property;
		bool block_open = false;
		bool value = false;
		GrEngine::Entity* cur_ent = nullptr;

		while (file >> stream)
		{
			if (stream == "{")
			{
				block_open = true;
			}
			else if (stream == "}")
			{
				block_open = false;
				value = false;
			}
			else if (block_open && cur_ent != nullptr)
			{
				if (!value)
				{
					cur_property = stream;
					cur_ent->AddNewProperty(cur_property.c_str());
					value = true;
				}
				else
				{
					cur_ent->ParsePropertyValue(cur_property.c_str(), stream.c_str());
					value = false;
				}
			}
			else if (!block_open && stream == "Object")
			{
				file >> stream;
				cur_ent = addEntity(std::atoi(stream.c_str()));
			}
			else if (!block_open && stream == "viewport")
			{
				cur_ent = &viewport_camera;
			}
			else if (!block_open && stream == "Skybox")
			{
				cur_ent = sky;
				entities[sky->GetEntityID()] = sky;
			}
		}

		file.close();
		Initialized = true;
	}
};