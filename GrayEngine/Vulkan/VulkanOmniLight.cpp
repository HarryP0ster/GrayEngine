#include "pch.h"
#include "VulkanOmniLight.h"

namespace GrEngine_Vulkan
{
	void VulkanOmniLight::initLight(VkDevice device, VmaAllocator allocator)
	{
		logicalDevice = device;
		memAllocator = allocator;
		type = LightType::Omni;
		UpdateLight();
	}

	void VulkanOmniLight::destroyLight()
	{

	}

	void VulkanOmniLight::UpdateLight()
	{
		max_distance = ownerEntity->GetPropertyValue(PropertyType::MaximumDistance, 15.f);
		max_distance = max_distance < 0 ? 15.f : max_distance;
		brightness = ownerEntity->GetPropertyValue(PropertyType::Brightness, 1.f);
	}

	std::array<VulkanOmniLight::OmniProjection, 6>& VulkanOmniLight::getLightUBO()
	{
		for (int i = 0; i < 6; i++)
		{
			lightPerspective[i].model = ownerEntity->GetObjectTransformation();
			glm::vec3 dir;
			glm::vec3 up;
			glm::mat4 proj = glm::perspective(glm::radians(90.f), 1.f, 0.01f, max_distance);
			switch (i)
			{
			case 0:
				dir = glm::vec3(1, 0, 0);
				up = glm::vec3(0, 1, 0);
				break;
			case 1:
				dir = glm::vec3(-1, 0, 0);
				up = glm::vec3(0, 1, 0);
				break;
			case 2:
				dir = glm::vec3(0, -1, 0);
				up = glm::vec3(0, 0, 1);
				break;
			case 3:
				dir = glm::vec3(0, 1, 0);
				up = glm::vec3(0, 0, 1);
				break;
			case 4:
				dir = glm::vec3(0, 0, 1);
				up = glm::vec3(0, 1, 0);
				break;
			case 5:
				dir = glm::vec3(0, 0, -1);
				up = glm::vec3(0, 1, 0);
				break;
			}
			glm::vec3 pos = ownerEntity->GetObjectPosition();
			glm::vec3 target = pos + dir;
			glm::mat4 view = glm::lookAt(pos, target, up);
			lightPerspective[i].viewproj = proj * view;
			lightPerspective[i].color = ownerEntity->GetPropertyValue(PropertyType::Color, glm::vec4(1.f));
			lightPerspective[i].spec.x = type;
			lightPerspective[i].spec.y = max_distance;
			lightPerspective[i].spec.z = brightness;
		}

		return lightPerspective;
	}
}
