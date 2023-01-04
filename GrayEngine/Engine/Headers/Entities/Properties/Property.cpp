#include "pch.h"
#include "Engine/Headers/Entities/Entity.h"
#include "Engine/Headers/Entities/DrawableObject.h"
#include "Property.h"

EntityID::EntityID(UINT id, void* parent)
{
	property_value = id;
	property_name = "EntityID";
	property_type = PropertyType::INT;
	string_value = std::to_string(id);
	owner = parent;
}

EntityID::~EntityID()
{

}

const char* EntityID::ValueString()
{
	return string_value.c_str();
}

void EntityID::ParsePropertyValue(const char* value)
{
	property_value = std::atoi(value);
}

void EntityID::SetPropertyValue(UINT value)
{
	property_value = value;
}

UINT EntityID::GetValue()
{
	return property_value;
}

std::any EntityID::GetAnyValue()
{
	return property_value;
}

void* EntityID::GetValueAdress()
{
	return &property_value;
}

/////////////////////////////////////////////////////////////////////////////////

Mass::Mass(int mass, void* parent)
{
	property_value = mass;
	property_name = "Mass";
	property_type = PropertyType::INT;
	string_value = std::to_string(mass);
	owner = parent;
}

Mass::~Mass()
{

}

const char* Mass::ValueString()
{
	string_value = std::to_string(property_value);
	return string_value.c_str();
}

void Mass::ParsePropertyValue(const char* value)
{
	property_value = std::atoi(value);

	if (owner != nullptr)
		static_cast<GrEngine::Entity*>(owner)->recalculatePhysics();
}

void Mass::SetPropertyValue(int value)
{
	property_value = value;

	if (owner != nullptr)
		static_cast<GrEngine::Entity*>(owner)->recalculatePhysics();
}

std::any Mass::GetAnyValue()
{
	return property_value;
}

void* Mass::GetValueAdress()
{
	return &property_value;
}

/////////////////////////////////////////////////////////////////////////////////

EntityPosition::EntityPosition(float x, float y, float z, void* parent)
{
	property_value = { x, y, z };
	property_name = "EntityPosition";
	property_type = PropertyType::VECTOR3;
	owner = parent;
}

EntityPosition::~EntityPosition()
{

}

const char* EntityPosition::ValueString()
{
	property_string = (GrEngine::Globals::FloatToString(property_value.x, 2) + ":" + GrEngine::Globals::FloatToString(property_value.y, 2) + ":" + GrEngine::Globals::FloatToString(property_value.z, 2));
	return property_string.c_str();
}

void EntityPosition::ParsePropertyValue(const char* value)
{
	auto cols = GrEngine::Globals::SeparateString(value, ':');
	if (cols.size() < 3) return;

	property_value = { stof(cols[0]), stof(cols[1]), stof(cols[2]) };

	if (owner != nullptr)
		static_cast<GrEngine::Entity*>(owner)->recalculatePhysics();
}

void EntityPosition::SetPropertyValue(const float& x, const float& y, const float& z)
{
	property_value = { x, y, z };

	if (owner != nullptr)
		static_cast<GrEngine::Entity*>(owner)->recalculatePhysics();
}

void EntityPosition::SetPropertyValue(const glm::vec3& value)
{
	property_value = value;

	if (owner != nullptr)
		static_cast<GrEngine::Entity*>(owner)->recalculatePhysics();
}

std::any EntityPosition::GetAnyValue()
{
	return property_value;
}

void* EntityPosition::GetValueAdress()
{
	return &property_value;
}

/////////////////////////////////////////////////////////////////////////////////

EntityOrientation::EntityOrientation(const float& pitch, const float& yaw, const float& roll, void* parent)
{
	glm::quat qPitch = glm::angleAxis(glm::radians(yaw), glm::vec3(1, 0, 0));
	glm::quat qYaw = glm::angleAxis(glm::radians(pitch), glm::vec3(0, 1, 0));
	glm::quat qRoll = glm::angleAxis(glm::radians(roll), glm::vec3(0, 0, 1));
	property_value = glm::normalize(qPitch * qYaw * qRoll);
	pitch_yaw_roll = { pitch, yaw, roll };
	property_name = "EntityOrientation";
	property_type = PropertyType::VECTOR3;
	owner = parent;
}

EntityOrientation::~EntityOrientation()
{

}

const char* EntityOrientation::ValueString()
{
	property_string = (GrEngine::Globals::FloatToString(pitch_yaw_roll.x, 2) + ":" + GrEngine::Globals::FloatToString(pitch_yaw_roll.y, 2) + ":" + GrEngine::Globals::FloatToString(pitch_yaw_roll.z, 2));
	return property_string.c_str();
}

void EntityOrientation::ParsePropertyValue(const char* value)
{
	auto cols = GrEngine::Globals::SeparateString(value, ':');

	if (cols.size() < 3) return;

	glm::quat qPitch = glm::angleAxis(glm::radians(stof(cols[1])), glm::vec3(1, 0, 0));
	glm::quat qYaw = glm::angleAxis(glm::radians(stof(cols[0])), glm::vec3(0, 1, 0));
	glm::quat qRoll = glm::angleAxis(glm::radians(stof(cols[2])), glm::vec3(0, 0, 1));

	property_value = glm::normalize(qPitch * qYaw * qRoll);
	pitch_yaw_roll = { stof(cols[0]), stof(cols[1]), stof(cols[2]) };
}

void EntityOrientation::SetPropertyValue(const float& pitch, const float& yaw, const float& roll)
{
	glm::quat qPitch = glm::angleAxis(glm::radians(yaw), glm::vec3(1, 0, 0));
	glm::quat qYaw = glm::angleAxis(glm::radians(pitch), glm::vec3(0, 1, 0));
	glm::quat qRoll = glm::angleAxis(glm::radians(roll), glm::vec3(0, 0, 1));
	property_value = glm::normalize(qPitch * qYaw * qRoll);
	pitch_yaw_roll = { pitch, yaw, roll };
}

void EntityOrientation::SetPropertyValue(glm::quat value)
{
	property_value = value;
}

std::any EntityOrientation::GetAnyValue()
{
	return property_value;
}

void* EntityOrientation::GetValueAdress()
{
	return &property_value;
}

/////////////////////////////////////////////////////////////////////////////////

Color::Color(void* parent)
{
	property_value = glm::vec4(1.f, 1.f, 1.f, 1.f);
	property_name = "Color";
	property_type = PropertyType::VECTOR4;
	owner = parent;
}

Color::Color(const float& r, const float& g, const float& b, const float& a, void* parent)
{
	property_value = glm::vec4(r, g, b, a);
	property_name = "Color";
	property_type = PropertyType::VECTOR4;
	owner = parent;
}

Color::Color(const float& r, const float& g, const float& b, void* parent)
{
	property_value = glm::vec4(r, g, b, 1.f);
	property_name = "Color";
	property_type = PropertyType::VECTOR4;
	owner = parent;
}

Color::~Color()
{

}

const char* Color::ValueString()
{
	property_string = (GrEngine::Globals::FloatToString(property_value.x, 2) + ":" + GrEngine::Globals::FloatToString(property_value.y, 2) + ":" + GrEngine::Globals::FloatToString(property_value.z, 2) + ":" + GrEngine::Globals::FloatToString(property_value.w, 2));
	return property_string.c_str();
}

void Color::ParsePropertyValue(const char* value)
{
	auto cols = GrEngine::Globals::SeparateString(value, ':');

	if (cols.size() < 3) return;

	if (cols.size() == 3)
	{
		SetPropertyValue(stof(cols[0]), stof(cols[1]), stof(cols[2]), 1.f);
	}
	else
	{
		SetPropertyValue(stof(cols[0]), stof(cols[1]), stof(cols[2]), stof(cols[3]));
	}
}

void Color::SetPropertyValue(const float& r, const float& g, const float& b, const float& a)
{
	property_value = glm::vec4(r, g, b, a);
}

void Color::SetPropertyValue(glm::vec4 value)
{
	property_value = value;
}

std::any Color::GetAnyValue()
{
	return property_value;
}

void* Color::GetValueAdress()
{
	return &property_value;
}
/////////////////////////////////////////////////////////////////////////////////

Drawable::Drawable(const char* path, void* parent)
{
	property_name = "Drawable";
	property_type = PropertyType::STRING;
	owner = parent;
}

Drawable::~Drawable()
{

}

const char* Drawable::ValueString()
{
	return property_value.c_str();
}

void Drawable::ParsePropertyValue(const char* value)
{
	SetPropertyValue(value);
}

void Drawable::SetPropertyValue(std::string value)
{
	property_value = value;

	if (owner != nullptr)
		static_cast<GrEngine::DrawableObject*>(owner)->LoadMesh(value.c_str(), true, nullptr);
}

std::any Drawable::GetAnyValue()
{
	return property_value;
}

void* Drawable::GetValueAdress()
{
	return &property_value;
}