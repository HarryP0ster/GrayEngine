#pragma once
#include "Core.h"

namespace GrEngine
{
	class Entity
	{
	public:
		Entity() {};
		virtual ~Entity() {};

		virtual void Rotate(const float& pitch, const float& yaw, const float& roll)
		{
			pitch_yaw_roll += glm::vec3{ pitch * Globals::delta_time, yaw * Globals::delta_time, roll * Globals::delta_time };
			glm::quat qPitch = glm::angleAxis(glm::radians(pitch_yaw_roll.y), glm::vec3(1, 0, 0));
			glm::quat qYaw = glm::angleAxis(glm::radians(pitch_yaw_roll.x), glm::vec3(0, 1, 0));
			glm::quat qRoll = glm::angleAxis(glm::radians(pitch_yaw_roll.z), glm::vec3(0, 0, 1));
			obj_orientation_target = glm::normalize(qPitch * qYaw * qRoll);
		}

		virtual void Rotate(const glm::vec3& angle)
		{
			pitch_yaw_roll += angle * glm::vec3(Globals::delta_time, Globals::delta_time, Globals::delta_time);
			glm::quat qPitch = glm::angleAxis(glm::radians(pitch_yaw_roll.y), glm::vec3(1, 0, 0));
			glm::quat qYaw = glm::angleAxis(glm::radians(pitch_yaw_roll.x), glm::vec3(0, 1, 0));
			glm::quat qRoll = glm::angleAxis(glm::radians(pitch_yaw_roll.z), glm::vec3(0, 0, 1));
			obj_orientation_target = glm::normalize(qPitch * qYaw * qRoll);
		}

		virtual void Rotate(const glm::quat& angle)
		{
			pitch_yaw_roll += glm::eulerAngles(angle) * glm::vec3(Globals::delta_time, Globals::delta_time, Globals::delta_time);
			obj_orientation_target += angle;
		}

		virtual void SetRotation(const float& pitch, const float& yaw, const float& roll)
		{
			pitch_yaw_roll = glm::vec3{ pitch, yaw, roll };
			glm::quat qPitch = glm::angleAxis(glm::radians(pitch_yaw_roll.y), glm::vec3(1, 0, 0));
			glm::quat qYaw = glm::angleAxis(glm::radians(pitch_yaw_roll.x), glm::vec3(0, 1, 0));
			glm::quat qRoll = glm::angleAxis(glm::radians(pitch_yaw_roll.z), glm::vec3(0, 0, 1));
			obj_orientation = glm::normalize(qPitch * qYaw * qRoll);
			obj_orientation_target = glm::normalize(qPitch * qYaw * qRoll);
		}

		virtual void SetRotation(const glm::vec3& angle)
		{
			pitch_yaw_roll = angle;
			glm::quat qPitch = glm::angleAxis(glm::radians(pitch_yaw_roll.y), glm::vec3(1, 0, 0));
			glm::quat qYaw = glm::angleAxis(glm::radians(pitch_yaw_roll.x), glm::vec3(0, 1, 0));
			glm::quat qRoll = glm::angleAxis(glm::radians(pitch_yaw_roll.z), glm::vec3(0, 0, 1));
			obj_orientation = glm::normalize(qPitch * qYaw * qRoll);
			obj_orientation_target = glm::normalize(qPitch * qYaw * qRoll);
		}

		virtual void SetRotation(const glm::quat& angle)
		{
			pitch_yaw_roll = glm::eulerAngles(angle);
			obj_orientation = angle;
			obj_orientation_target = angle;
		}

		virtual void MoveObject(const float& x, const float& y, const float& z)
		{
			object_position_target += glm::vec3(x * Globals::delta_time, y * Globals::delta_time, z * Globals::delta_time);
		}

		virtual void MoveObject(const glm::vec3& vector)
		{
			object_position_target += vector * glm::vec3(Globals::delta_time, Globals::delta_time, Globals::delta_time);
		}

		virtual void PositionObjectAt(const float& x, const float& y, const float& z)
		{
			object_position_target = glm::vec3(x, y, z);
			object_position = glm::vec3(x, y, z);
		}

		virtual void PositionObjectAt(const glm::vec3& vector)
		{
			object_position_target = vector;
			object_position = vector;
		}

		glm::vec3& UpdateObjectPosition(float smoothing_factor = 0.f)
		{
			smoothing_factor = smoothing_factor == 1.f ? FLT_EPSILON : smoothing_factor;
			object_position = object_position + (object_position_target - object_position) * (1 - smoothing_factor);
			return object_position;
		}

		glm::quat& UpdateObjectOrientation(float smoothing_factor = 0.f)
		{
			smoothing_factor = smoothing_factor == 1.f ? FLT_EPSILON : smoothing_factor;
			obj_orientation = obj_orientation + (obj_orientation_target - obj_orientation) * (1 - smoothing_factor);
			return obj_orientation;
		}

		glm::vec3& GetObjectPosition()
		{
			return object_position;
		}

		glm::quat& GetObjectOrientation()
		{
			return obj_orientation;
		}

	protected:
		glm::quat obj_orientation = { 0.f, 0.f, 0.f, 0.f };
		glm::quat obj_orientation_target = { 0.f, 0.f, 0.f, 0.f };
		glm::vec3 object_position = { 0.f, 0.f, 0.f };
		glm::vec3 object_position_target = { 0.f, 0.f, 0.f };
		glm::vec3 pitch_yaw_roll = { 0.f, 0.f, 0.f };
	};
}