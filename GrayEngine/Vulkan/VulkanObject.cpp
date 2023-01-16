#include <pch.h>
#include "VulkanObject.h"
#include "VulkanAPI.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace GrEngine_Vulkan
{
	glm::vec3 VulkanObject::selected_id{};

	void VulkanObject::initObject(VkDevice device, VmaAllocator allocator, GrEngine::Renderer* owner)
	{
		p_Owner = owner;
		resources = &static_cast<VulkanAPI*>(owner)->GetResourceManager();
		logicalDevice = device;
		memAllocator = allocator;

		UINT id = GetEntityID();
		colorID = { id / 1000000 % 1000, id / 1000 % 1000, id % 1000 };

		auto resource = resources->GetMeshResource("default");
		if (resource == nullptr)
		{
			Mesh* default_mesh = new Mesh();

			default_mesh->vertices = {
				{{{ 0.25, 0.25, 0.25, 1.0f },{ 1.f, 1.0f }}},
				{{{ -0.25, 0.25, -0.25, 1.0f },{ 0.f, 0.0f }}},
				{{{ -0.25, 0.25 , 0.25, 1.0f },{ 0.f, 1.f }}},
				{{{ 0.25, 0.25, -0.25, 1.0f },{ 1.f, 0.0f }}},

				{{{ 0.25, -0.25, 0.25, 1.0f },{ 1.f, 1.0f }}},
				{{{ -0.25, -0.25, -0.25, 1.0f },{ 0.f, 0.0f }}},
				{{{ -0.25, -0.25 , 0.25, 1.0f },{ 0.f, 1.f }}},
				{{{ 0.25, -0.25, -0.25, 1.0f },{ 1.f, 0.0f }}}
			};

			default_mesh->indices = { 0, 1, 2, 0, 3, 1,
				4, 5, 6, 4, 7, 5,
				0, 2, 6, 0, 4, 6,
				0, 3, 4, 3, 7, 4,
				1, 3, 7, 1, 5, 7,
				1, 2, 6, 1, 5, 6,
			};

			default_mesh->mesh_path = "default";

			resource = resources->AddMeshResource("default", default_mesh);
			object_mesh = resource->AddLink();

			VulkanAPI::m_createVkBuffer(logicalDevice, memAllocator, object_mesh->vertices.data(), sizeof(object_mesh->vertices[0]) * object_mesh->vertices.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &object_mesh->vertexBuffer);
			VulkanAPI::m_createVkBuffer(logicalDevice, memAllocator, object_mesh->indices.data(), sizeof(object_mesh->indices[0]) * object_mesh->indices.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, &object_mesh->indexBuffer);
		}
		else
		{
			object_mesh = resource->AddLink();
		}

		createDescriptorLayout();
		createDescriptorPool();
		createDescriptorSet();
		createPipelineLayout();
		createGraphicsPipeline();
	}

	bool VulkanObject::pushConstants(VkCommandBuffer cmd, VkExtent2D extent, UINT32 mode)
	{
		/*orientation relative to the position in a 3D space (?)*/
		ubo.model = glm::translate(glm::mat4(1.f), GetObjectPosition()) * glm::mat4_cast(GetObjectOrientation());
		/*Math for Game Programmers: Understanding Homogeneous Coordinates GDC 2015*/
		ubo.view = glm::translate(glm::mat4_cast(p_Owner->getActiveViewport()->UpdateCameraOrientation(0.2)), -p_Owner->getActiveViewport()->UpdateCameraPosition(0.65)); // [ix iy iz w1( = 0)]-direction [jx jy jz w2( = 0)]-direction [kx ky kz w3( = 0)]-direction [tx ty tz w ( = 1)]-position
		ubo.proj = glm::perspective(glm::radians(60.0f), (float)extent.width / (float)extent.height, near_plane, far_plane); //fov, aspect ratio, near clipping plane, far clipping plane
		ubo.proj[1][1] *= -1; //reverse Y coordinate
		ubo.scale = GetPropertyValue("Scale", glm::vec3(1.f));
		vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(UniformBufferObject), &ubo);

		opo.draw_mode = mode;
		opo.colors = { {GetPropertyValue("Color", glm::vec4(1))}, {colorID.x, colorID.y, colorID.z, 1}, {selected_id.x, selected_id.y, selected_id.z, 1}};
		//opo.colors = glm::mat4(0);

		vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(UniformBufferObject), sizeof(PickingBufferObject), &opo);
		return true;
	}

	void VulkanObject::updateCollisions()
	{
		delete colShape;
		if (colMesh != nullptr)
		{
			delete colMesh;
		}

		colMesh = new btTriangleMesh();
		for (std::vector<Vertex>::iterator itt = object_mesh->vertices.begin(); itt != object_mesh->vertices.end(); ++itt)
		{
			colMesh->findOrAddVertex(btVector3((*itt).pos.x, (*itt).pos.y, (*itt).pos.z), false);
		}

		for (std::vector<uint16_t>::iterator itt = object_mesh->indices.begin(); itt != object_mesh->indices.end(); ++itt)
		{
			colMesh->addTriangleIndices(*(++itt), *(++itt), *itt);
		}

		colShape = new btBvhTriangleMeshShape(colMesh, false);
		recalculatePhysics();
	}

	bool VulkanObject::LoadModel(const char* model_path)
	{
		auto start = std::chrono::steady_clock::now();

		std::string mesh_path = "";
		std::vector<std::string> textures_vector;
		if (!GrEngine::Globals::readGMF(model_path, &mesh_path, &textures_vector)) return false;

		LoadModel(mesh_path.c_str(), textures_vector);

		auto end = std::chrono::steady_clock::now();
		auto time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
		Logger::Out("Model %s loaded in %d ms", OutputColor::Gray, OutputType::Log, model_path, (int)time);

		return true;
	}

	bool VulkanObject::LoadModel(const char* mesh_path, std::vector<std::string> textures_vector)
	{
		VulkanObject* ref_obj = this;

		VulkanAPI* inst = static_cast<VulkanAPI*>(p_Owner);
		std::vector<std::string>* out_materials_collection = &material_names;
		std::map<int, std::future<void>> processes_map;
		std::map<std::string, std::vector<int>> materials_map;

		material_names.clear();
		texture_names.clear();

		processes_map[processes_map.size()] = std::async(std::launch::async, [textures_vector, ref_obj, inst]()
			{
				inst->assignTextures(textures_vector, ref_obj);
			});

		if (object_mesh != nullptr)
		{
			resources->RemoveMesh(object_mesh->mesh_path.c_str(), logicalDevice, memAllocator);
			object_mesh = nullptr;
		}

		auto resource = resources->GetMeshResource(mesh_path);
		if (resource == nullptr)
		{
			processes_map[processes_map.size()] = std::async(std::launch::async, [mesh_path, textures_vector, ref_obj, out_materials_collection, inst]()
				{
					ref_obj->LoadMesh(mesh_path, textures_vector.size() != 0, out_materials_collection);
				});
		}
		else
		{
			object_mesh = resource->AddLink();
			bound = object_mesh->bounds;
		}

		for (int ind = 0; ind < processes_map.size(); ind++)
		{
			if (processes_map[ind].valid())
			{
				processes_map[ind].wait();
			}
		}

		texture_names = textures_vector;
		updateObject();

		return true;
	}

	bool VulkanObject::LoadMesh(const char* mesh_path, bool useTexturing, std::vector<std::string>* out_materials)
	{
		std::unordered_map<Vertex, uint32_t> uniqueVertices{};
		Assimp::Importer importer;
		Mesh* target_mesh = new Mesh();

		auto model = importer.ReadFile(mesh_path, 0);

		if (model == NULL)
		{
			Logger::Out("Could not load the mesh %c%s%c!", OutputColor::Red, OutputType::Error, '"', mesh_path, '"');
			return false;
		}

		target_mesh->mesh_path = mesh_path;

		if (colMesh != nullptr)
		{
			delete colMesh;
			colMesh = nullptr;
		}

		colMesh = new btTriangleMesh();

		float highest_pointx = 0.f;
		float highest_pointy = 0.f;
		float highest_pointz = 0.f;

		for (int mesh_ind = 0; mesh_ind < model->mNumMeshes; mesh_ind++)
		{
			auto num_vert = model->mMeshes[mesh_ind]->mNumVertices;
			auto cur_mesh = model->mMeshes[mesh_ind];
			auto name3 = model->mMeshes[mesh_ind]->mName;
			auto uv_ind = mesh_ind;

			for (int vert_ind = 0; vert_ind < num_vert; vert_ind++)
			{
				auto coord = model->mMeshes[mesh_ind]->mTextureCoords[0];

				GrEngine_Vulkan::Vertex vertex{ {
				{ cur_mesh->mVertices[vert_ind].x, cur_mesh->mVertices[vert_ind].y, cur_mesh->mVertices[vert_ind].z, 1.0f },
				{ coord[vert_ind].x, 1.0f - coord[vert_ind].y },
				getColorID(),
				(uint32_t)uv_ind,
				useTexturing} };


				if (uniqueVertices.count(vertex) == 0)
				{
					uniqueVertices[vertex] = static_cast<uint32_t>(target_mesh->vertices.size());
					target_mesh->vertices.push_back(vertex);
					colMesh->findOrAddVertex(btVector3(vertex.pos.x, vertex.pos.y, vertex.pos.z), false);
				}

				if (highest_pointx < cur_mesh->mVertices[vert_ind].x)
					highest_pointx = cur_mesh->mVertices[vert_ind].x;
				if (highest_pointy < cur_mesh->mVertices[vert_ind].y)
					highest_pointy = cur_mesh->mVertices[vert_ind].y;
				if (highest_pointz < cur_mesh->mVertices[vert_ind].z)
					highest_pointz = cur_mesh->mVertices[vert_ind].z;

				int index = uniqueVertices[vertex];
				target_mesh->indices.push_back(index);
			}

			aiString material;
			aiGetMaterialString(model->mMaterials[model->mMeshes[mesh_ind]->mMaterialIndex], AI_MATKEY_NAME, &material);

			if (out_materials)
			{
				out_materials->push_back(material.C_Str());
			}
		}

		VulkanAPI::m_createVkBuffer(logicalDevice, memAllocator, target_mesh->vertices.data(), sizeof(target_mesh->vertices[0]) * target_mesh->vertices.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &target_mesh->vertexBuffer);
		VulkanAPI::m_createVkBuffer(logicalDevice, memAllocator, target_mesh->indices.data(), sizeof(target_mesh->indices[0]) * target_mesh->indices.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, &target_mesh->indexBuffer);

		target_mesh->bounds = glm::uvec3(highest_pointx, highest_pointy, highest_pointz);
		bound = target_mesh->bounds;

		for (std::vector<uint16_t>::iterator itt = target_mesh->indices.begin(); itt != target_mesh->indices.end(); ++itt)
		{
			colMesh->addTriangleIndices(*(++itt), *(++itt), *itt);
		}

		auto resource = resources->AddMeshResource(mesh_path, target_mesh);
		object_mesh = resource->AddLink();
		delete colShape;
		colShape = new btBvhTriangleMeshShape(colMesh, false);
		recalculatePhysics();
	}
};