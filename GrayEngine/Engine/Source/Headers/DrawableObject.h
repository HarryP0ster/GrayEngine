#pragma once
#include <pch.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "Core.h"

namespace GrEngine
{
	struct Texture
	{
		const char* texture_path;
	};

	struct Vertex
	{
		glm::vec4 pos;
		glm::vec4 color;
		glm::vec2 uv;
		uint32_t uv_index;

		bool operator==(const Vertex& other) const
		{
			return pos == other.pos && uv == other.uv;
		}
	};

	struct Mesh
	{
		const char* mesh_path;
	};

	class DllExport DrawableObject
	{
	public:
		DrawableObject() {};
		virtual ~DrawableObject() {};

		void Rotate(float pitch, float yaw, float roll)
		{
			pitch_yaw_roll += glm::vec3{pitch, yaw, roll};
			glm::quat qPitch = glm::angleAxis(glm::radians(pitch_yaw_roll.y), glm::vec3(1, 0, 0));
			glm::quat qYaw = glm::angleAxis(glm::radians(pitch_yaw_roll.x), glm::vec3(0, 1, 0));
			glm::quat qRoll = glm::angleAxis(glm::radians(pitch_yaw_roll.z), glm::vec3(0, 0, 1));
			obj_orientation = glm::normalize(qPitch * qYaw * qRoll);
		}

		void Rotate(glm::vec3 angle)
		{
			pitch_yaw_roll += angle;
			glm::quat qPitch = glm::angleAxis(glm::radians(pitch_yaw_roll.y), glm::vec3(1, 0, 0));
			glm::quat qYaw = glm::angleAxis(glm::radians(pitch_yaw_roll.x), glm::vec3(0, 1, 0));
			glm::quat qRoll = glm::angleAxis(glm::radians(pitch_yaw_roll.z), glm::vec3(0, 0, 1));
			obj_orientation = glm::normalize(qPitch * qYaw * qRoll);
		}

		void Rotate(glm::quat angle)
		{
			pitch_yaw_roll += glm::eulerAngles(angle);
			obj_orientation += angle;
		}

		void SetRotation(float pitch, float yaw, float roll)
		{
			pitch_yaw_roll = glm::vec3{ pitch, yaw, roll };
			glm::quat qPitch = glm::angleAxis(glm::radians(pitch_yaw_roll.y), glm::vec3(1, 0, 0));
			glm::quat qYaw = glm::angleAxis(glm::radians(pitch_yaw_roll.x), glm::vec3(0, 1, 0));
			glm::quat qRoll = glm::angleAxis(glm::radians(pitch_yaw_roll.z), glm::vec3(0, 0, 1));
			obj_orientation = glm::normalize(qPitch * qYaw * qRoll);
		}

		void SetRotation(glm::vec3 angle)
		{
			pitch_yaw_roll = angle;
			glm::quat qPitch = glm::angleAxis(glm::radians(pitch_yaw_roll.y), glm::vec3(1, 0, 0));
			glm::quat qYaw = glm::angleAxis(glm::radians(pitch_yaw_roll.x), glm::vec3(0, 1, 0));
			glm::quat qRoll = glm::angleAxis(glm::radians(pitch_yaw_roll.z), glm::vec3(0, 0, 1));
			obj_orientation = glm::normalize(qPitch * qYaw * qRoll);
		}

		void SetRotation(glm::quat angle)
		{
			pitch_yaw_roll = glm::eulerAngles(angle);
			obj_orientation = angle;
		}

		glm::vec3& GetObjectBounds()
		{
			return bound;
		}

		glm::quat& GetObjectOrientation()
		{
			return obj_orientation;
		}

		void SetObjectBounds(glm::vec3 new_bounds)
		{
			bound = new_bounds;
		}

		void MoveObjectBy(float x, float y, float z)
		{
			object_position += glm::vec3(x, y, z);
		}

		void MoveObjectBy(glm::vec3 vector)
		{
			object_position += vector;
		}

		void PositionObjectAt(float x, float y, float z)
		{
			object_position = glm::vec3(x, y, z);
		}

		void PositionObjectAt(glm::vec3 position)
		{
			object_position = position;
		}

		glm::vec3& GetObjectPosition()
		{
			return object_position;
		}

	private:
		glm::quat obj_orientation = { 0.f, 0.f, 0.f, 0.f };
		glm::vec3 object_position = { 0.f, 0.f, 0.f };
		glm::vec3 pitch_yaw_roll = { 0.f, 0.f, 0.f };
		glm::vec3 bound = { 0.f, 0.f, 0.f };
	};
}
