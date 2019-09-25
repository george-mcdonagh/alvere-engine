#pragma once

#include <typeindex>
#include <unordered_map>

#include "alvere/world/drawable_scene_system.hpp"
#include "alvere/world/entity_handle.hpp"
#include "alvere/world/scene_system.hpp"
#include "alvere/world/updated_scene_system.hpp"


#include "alvere/world/entity_component_systems/scene_renderer.hpp"


namespace alvere
{
	class Scene
	{
	public:

		template <typename SceneSystemType>
		void addSystem()
		{
			static_assert(
				std::is_base_of<SceneSystem, SceneSystemType>::value && (
					std::is_base_of<UpdatedSceneSystem, SceneSystemType>::value ||
					std::is_base_of<DrawableSceneSystem, SceneSystemType>::value));

			std::type_index systemTypeIndex = typeid(SceneSystemType);

			if (m_systems.find(systemTypeIndex) != m_systems.end())
				return;

			SceneSystemType * newSceneSystem = new SceneSystemType();

			UpdatedSceneSystem * newUpdatedSceneSystem = dynamic_cast<UpdatedSceneSystem *>(newSceneSystem);

			if (newUpdatedSceneSystem != nullptr)
				m_updatedSystems[systemTypeIndex] = newUpdatedSceneSystem;

			DrawableSceneSystem * newDrawableSceneSystem = dynamic_cast<DrawableSceneSystem *>(newSceneSystem);

			if (newDrawableSceneSystem != nullptr)
				m_drawableSystems[systemTypeIndex] = newDrawableSceneSystem;

			m_systems[systemTypeIndex] = newSceneSystem;
		}

		template <typename SceneSystemType>
		void removeSystem()
		{
			static_assert(
				std::is_base_of<SceneSystem, SceneRenderer>::value && (
					std::is_base_of<UpdatedSceneSystem, SceneRenderer>::value ||
					std::is_base_of<DrawableSceneSystem, SceneRenderer>::value));

			std::type_index systemTypeIndex = typeid(SceneRenderer);

			auto iter = m_systems.find(systemTypeIndex);

			if (iter == m_systems.end())
				return;

			delete iter->second;

			m_systems.erase(iter);

			if (iter != m_updatedSystems.end())
				m_updatedSystems.erase(iter);

			if (iter != m_drawableSystems.end())
				m_drawableSystems.erase(iter);
		}

		EntityHandle createEntity();

		void destroyEntity(EntityHandle & entityHandle);

		void update(float deltaTime);

		void draw();

	private:

		std::unordered_map<std::type_index, SceneSystem *> m_systems;
		std::unordered_map<std::type_index, UpdatedSceneSystem *> m_updatedSystems;
		std::unordered_map<std::type_index, DrawableSceneSystem *> m_drawableSystems;
	};
}