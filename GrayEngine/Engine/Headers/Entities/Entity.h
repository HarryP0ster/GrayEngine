#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Core/Globals.h"
#include "Core/Globals.h"
#include "Engine/Headers/Virtual/PhysicsObject.h"
#include "Properties/Property.h"

namespace GrEngine
{
	enum EntityType
	{
		DefaultEntity = 0,
		ObjectEntity = 1,
		SkyboxEntity = 2,
		TerrainEntity = 4,
		CameraEntity = 8,
		SpotlightEntity = 16,
		CascadeLightEntity = 32,
		PointLightEntity = 64,
		OmniLightEntity = 128,
	};

	inline EntityType operator|(EntityType a, EntityType b)
	{
		return static_cast<EntityType>(static_cast<int>(a) | static_cast<int>(b));
	}

	inline EntityType operator&(EntityType a, EntityType b)
	{
		return static_cast<EntityType>(static_cast<int>(a) & static_cast<int>(b));
	}

	inline void operator&=(EntityType& a, EntityType b)
	{
		a = static_cast<EntityType>(static_cast<int>(a) & static_cast<int>(b));
	}

	inline void operator|=(EntityType& a, EntityType b)
	{
		a = static_cast<EntityType>(static_cast<int>(a) | static_cast<int>(b));
	}

	inline bool operator==(EntityType a, EntityType b)
	{
		return (a & b) != 0 || (static_cast<int>(a) == 0 && static_cast<int>(b) == 0);
	}

	class DllExport Entity
	{
	public:

		Entity()
		{
			char _buf[11];
			std::snprintf(_buf, sizeof(_buf), "1%03d%03d%03d", std::rand() % 255 + 1, std::rand() % 255 + 1, std::rand() % 255 + 1);

			//Don't shuffle
			properties.push_back(new EntityNameProperty("Object", this));
			properties.push_back(new EntityIDProperty(atoi(_buf), this));
			properties.push_back(new EntityPositionProperty(0.f, 0.f, 0.f, this));
			properties.push_back(new EntityOrientationProperty(0.f, 0.f, 0.f, this));
			//Don't shuffle

			obj_name = static_cast<std::string*>(properties[0]->GetValueAdress());
			obj_id = static_cast<UINT*>(properties[1]->GetValueAdress());
			object_origin = static_cast<glm::vec3*>(properties[2]->GetValueAdress());
			obj_orientation = static_cast<glm::quat*>(properties[3]->GetValueAdress());
		};

		Entity(UINT id)
		{
			//Don't shuffle
			properties.push_back(new EntityNameProperty("Object", this));
			properties.push_back(new EntityIDProperty(id, this));
			properties.push_back(new EntityPositionProperty(0.f, 0.f, 0.f, this));
			properties.push_back(new EntityOrientationProperty(0.f, 0.f, 0.f, this));
			//Don't shuffle

			obj_name = static_cast<std::string*>(properties[0]->GetValueAdress());
			obj_id = static_cast<UINT*>(properties[1]->GetValueAdress());
			object_origin = static_cast<glm::vec3*>(properties[2]->GetValueAdress());
			obj_orientation = static_cast<glm::quat*>(properties[3]->GetValueAdress());
		};

		virtual ~Entity()
		{
			obj_name = nullptr;
			obj_id = nullptr;
			object_origin = nullptr;
			obj_orientation = nullptr;

			while (properties.size() > 0)
			{
				std::vector<EntityProperty*>::iterator itt = properties.begin();
				delete (*itt);
				properties.erase(itt);
			}
		};

		virtual void Rotate(const float& pitch, const float& yaw, const float& roll)
		{
			pitch_yaw_roll += glm::vec3{ pitch, yaw, roll };
			static_cast<EntityOrientationProperty*>(properties[3])->SetPropertyValue(pitch_yaw_roll);
		};

		virtual void Rotate(const glm::vec3& angle)
		{
			pitch_yaw_roll += angle;
			static_cast<EntityOrientationProperty*>(properties[3])->SetPropertyValue(pitch_yaw_roll);
		};

		virtual void Rotate(const glm::quat& angle)
		{
			pitch_yaw_roll += glm::degrees(glm::eulerAngles(angle));
			static_cast<EntityOrientationProperty*>(properties[3])->SetPropertyValue(angle);
		};

		virtual void SetRotation(const float& pitch, const float& yaw, const float& roll)
		{
			pitch_yaw_roll = glm::vec3{ pitch, yaw, roll };
			static_cast<EntityOrientationProperty*>(properties[3])->SetPropertyValue(pitch_yaw_roll);
		};

		virtual void SetRotation(const glm::vec3& angle)
		{
			pitch_yaw_roll = angle;
			static_cast<EntityOrientationProperty*>(properties[3])->SetPropertyValue(pitch_yaw_roll);
		};

		virtual void SetRotation(const glm::quat& angle)
		{
			pitch_yaw_roll = glm::degrees(glm::eulerAngles(angle));
			static_cast<EntityOrientationProperty*>(properties[3])->SetPropertyValue(angle);
		};

		virtual void MoveObject(const float& x, const float& y, const float& z)
		{
			static_cast<EntityPositionProperty*>(properties[2])->SetPropertyValue(*object_origin + glm::vec3(x, y, z));
		};

		virtual void MoveObject(const glm::vec3& vector)
		{
			static_cast<EntityPositionProperty*>(properties[2])->SetPropertyValue(*object_origin + vector);
		};

		virtual void PositionObjectAt(const float& x, const float& y, const float& z)
		{
			static_cast<EntityPositionProperty*>(properties[2])->SetPropertyValue(x, y, z);
		};

		virtual void PositionObjectAt(const glm::vec3& vector)
		{
			static_cast<EntityPositionProperty*>(properties[2])->SetPropertyValue(vector);
		};

		virtual glm::vec3 GetObjectPosition()
		{
			if (physComp != nullptr && physComp->HasValue())
			{
				auto pos = physComp->GetPhysPosition();
				return pos;
			}
			else
			{
				if (parent == nullptr)
				{
					return *object_origin;
				}
				else
				{
					return *object_origin + parent->GetObjectPosition();
				}
			}
		};

		virtual glm::quat GetObjectOrientation()
		{
			if (physComp != nullptr && physComp->HasValue())
			{
				auto ori = physComp->GetPhysOrientation();
				return ori;
			}
			else
			{
				return *obj_orientation;
			}
		};

		virtual glm::mat4 GetObjectTransformation()
		{
			return glm::translate(glm::mat4(1.f), GetObjectPosition()) * glm::mat4_cast(GetObjectOrientation());
		}

		template<typename T>
		T GetPropertyValue(PropertyType type, T default_value)
		{
			for (std::vector<EntityProperty*>::iterator itt = properties.begin(); itt != properties.end(); ++itt)
			{
				if ((*itt)->GetPropertyType() == type)
				{
					return std::any_cast<T>((*itt)->GetAnyValue());
				}
			}

			return default_value;
		};

		template<typename T>
		T GetPropertyValue(const char* property_name, T default_value)
		{
			return GetPropertyValue(EntityProperty::StringToType(property_name), default_value);
		};

		inline EntityProperty* GetProperty(PropertyType type)
		{
			for (std::vector<EntityProperty*>::iterator itt = properties.begin(); itt != properties.end(); ++itt)
			{
				if ((*itt)->GetPropertyType() == type)
				{
					return (*itt);
				}
			}

			return nullptr;
		};

		inline EntityProperty* GetProperty(const char* property_name)
		{
			return GetProperty(EntityProperty::StringToType(property_name));
		};

		bool HasProperty(PropertyType type)
		{
			for (std::vector<EntityProperty*>::iterator itt = properties.begin(); itt != properties.end(); ++itt)
			{
				if (type == (*itt)->GetPropertyType())
				{
					return true;
				}
			}

			return false;
		};

		bool HasProperty(const char* property_name)
		{
			return HasProperty(EntityProperty::StringToType(property_name));
		};

		virtual void ParsePropertyValue(PropertyType type, const char* property_value)
		{
			for (std::vector<EntityProperty*>::iterator itt = properties.begin(); itt != properties.end(); ++itt)
			{
				if ((*itt)->GetPropertyType() == type)
				{
					(*itt)->ParsePropertyValue(property_value);
					break;
				}
			}
		};

		virtual void ParsePropertyValue(const char* property_name, const char* property_value)
		{
			ParsePropertyValue(EntityProperty::StringToType(property_name), property_value);
		};

		void* FindPropertyAdress(const char* property_name)
		{
			std::string prop_str = std::string(property_name);
			for (std::vector<EntityProperty*>::iterator itt = properties.begin(); itt != properties.end(); ++itt)
			{
				std::string cur_str = (*itt)->PrpertyNameString();
				if (cur_str == prop_str)
				{
					return (*itt)->GetValueAdress();
				}
			}

			return nullptr;
		};

		inline void UpdateNameTag(std::string new_name)
		{
			static_cast<EntityNameProperty*>(properties[0])->SetPropertyValue(new_name.c_str());
		};

		inline std::string& GetObjectName()
		{
			return *obj_name;
		};

		inline UINT& GetEntityID()
		{
			return *obj_id;
		};

		EntityProperty* AddNewProperty(PropertyType type)
		{
			if (!HasProperty(type))
			{
				switch (type)
				{
				case PropertyType::Error:
					break;
				case PropertyType::EntityID:
					break;
				case PropertyType::Mass:
					properties.push_back(new MassProperty(0.f, this));
					return properties.back();
				case PropertyType::EntityName:
					break;
				case PropertyType::Scale:
					properties.push_back(new ScaleProperty(1.f, 1.f, 1.f, this));
					return properties.back();
				case PropertyType::EntityPosition:
					break;
				case PropertyType::EntityOrientation:
					break;
				case PropertyType::Color:
					properties.push_back(new ColorProperty(1.f, 1.f, 1.f, this));
					return properties.back();
				case PropertyType::PhysComponent:
					properties.push_back(new PhysComponentProperty(this));
					physComp = static_cast<PhysicsObject*>(properties.back()->GetValueAdress());
					return properties.back();
				case PropertyType::Drawable:
					properties.push_back(new DrawableProperty("nil", this));
					Type |= EntityType::ObjectEntity;
					return properties.back();
				case PropertyType::Spotlight:
					properties.push_back(new SpotLightProperty(this));
					Type |= EntityType::SpotlightEntity;
					return properties.back();
				case PropertyType::CascadeLight:
					properties.push_back(new CascadeProperty(this));
					Type |= EntityType::CascadeLightEntity;
					return properties.back();
				case PropertyType::PointLight:
					properties.push_back(new PointLightPropery(this));
					Type |= EntityType::PointLightEntity;
					return properties.back();
				case PropertyType::OmniLight:
					properties.push_back(new OmniLightPropery(this));
					Type |= EntityType::OmniLightEntity;
					return properties.back();
				case PropertyType::Cubemap:
					break;
				case PropertyType::Shader:
					properties.push_back(new ShaderProperty("Shaders//default", this));
					return properties.back();
				case PropertyType::Transparency:
					properties.push_back(new TransparencyProperty(0, this));
					return properties.back();
				case PropertyType::DoubleSided:
					properties.push_back(new DoubleSidedProperty(0, this));
					return properties.back();
				case PropertyType::CastShadow:
					properties.push_back(new CastShadowProperty(1, this));
					return properties.back();
				case PropertyType::CollisionType:
					properties.push_back(new CollisionTypeProperty(this));
					return properties.back();
				default:
					break;
				}
			}
			else
			{
				return GetProperty(type);
			}
		};

		EntityProperty* AddNewProperty(const char* property_name)
		{
			return AddNewProperty(EntityProperty::StringToType(property_name));
		};

		inline const std::vector<EntityProperty*>& GetProperties()
		{
			return properties;
		}

		inline EntityType& GetEntityType()
		{
			return Type;
		};

		const std::string GetTypeString()
		{
			switch (Type)
			{
			case 2:
				return "Skybox";
				break;
			case 4:
				return "Terrain";
				break;
			case 8:
				return "Camera";
				break;
			default:
				return "Entity";
			}

			return "";
		};

		inline const char* GetEntityNameTag() 
		{ 
			return (*obj_name).c_str(); 
		};

		inline void MakeStatic()
		{
			isPrivated = true;
		}

		inline bool& IsStatic()
		{
			return isPrivated;
		}

		void SetParentEntity(Entity* parent_ent)
		{
			parent = parent_ent;
		}

		std::string drawable_path = "";
		std::string collision_path = "";

		//////////////////////////////////////////////////////////////////
		//void SetType(EntityType t)
		//{
		//	Type = t;
		//}
		//////////////////////////////////////////////////////////////////

	protected:
		std::vector<EntityProperty*> properties;

		std::string* obj_name;
		UINT* obj_id;
		glm::quat* obj_orientation;
		glm::vec3* object_origin;
		EntityType Type = EntityType::DefaultEntity;

		glm::vec3 pitch_yaw_roll = { 0.f, 0.f, 0.f };
		bool isPrivated = false;
		PhysicsObject* physComp = nullptr;
		Entity* parent;
	};
}