/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GroundMarks.h"

#include <algorithm>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "3D/AllMeshes.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/SmokyStuff.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
struct Mark
{
	entt::entity entity {entt::null}; ///< +4
	int32_t life {0};                 ///< +8, ms
};

/// 0xEB9A00, the newest first
std::vector<Mark> g_Marks;
/// (aproximado) openblack's frame delta is a float: the fraction waits for the next frame so the sum stays in whole ms
/// like g_game_time_inc ([0xEA9EC0], an integer the original takes off as is, 0x8253B1..0x8253C0, with no carry)
float g_Carry = 0.0f;
} // namespace

entt::entity ecs::ground_marks::Create(const glm::vec3& position, const glm::mat3& rotation, float scale)
{
	// 0x8252EB: SmokyStuff::Create(point, 1, 1.0f, 0xFFFFFFFF), the dust of mode 1 (0x823DA7), made whether the mark's
	// object could be drawn or not (both makers: the explosion's crater and the uprooted tree's)
	smoky_stuff::Create(position, 1, 1.0f, 0xFFFFFFFFu);
	auto& meshes = Locator::resources::value().GetMeshes();
	const auto mesh = resources::HashIdentifier(MeshId::TreeRootsPile); // MeshPack 0x251
	if (!meshes.Contains(mesh))
	{
		return entt::null;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, position, rotation, glm::vec3(scale));
	registry.Assign<Mesh>(entity, mesh, static_cast<int8_t>(0), static_cast<int8_t>(-1));
	// LH3DObject::Create(ecx = 1) 0x82527D, a morphable object whose deltas UpdateMelting takes once (vt+0x1E8, 0x8252C3)
	registry.Assign<MorphWithTerrain>(entity, land_morph::Melting::Snapshot);
	g_Marks.insert(g_Marks.begin(), Mark {entity, k_LifeMs});
	return entity;
}

entt::entity ecs::ground_marks::CreateExplosionMark(const glm::vec3& position, float angle)
{
	// SetAngleY's rows are glm::rotate(-angle, Y) (3D/Billboard.h's conventions)
	const auto rotation = glm::mat3(glm::rotate(glm::mat4(1.0f), -angle, glm::vec3(0.0f, 1.0f, 0.0f)));
	return Create(position, rotation, k_ExplosionScale);
}

void ecs::ground_marks::Clear()
{
	// ClearAllStuff 0x82AEE8..0x82AF0D: fn_00825300 and delete on every node; the entities go with the registry's reset
	g_Marks.clear();
	g_Carry = 0.0f;
}

void ecs::ground_marks::Update(float gameMilliseconds)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	g_Carry += std::max(gameMilliseconds, 0.0f);
	const auto step = static_cast<int32_t>(g_Carry);
	g_Carry -= static_cast<float>(step);
	bool dirty = false;
	for (auto& mark : g_Marks)
	{
		if (!registry.Valid(mark.entity))
		{
			mark.entity = entt::null;
			continue;
		}
		if (mark.life <= k_FadeMs)
		{
			// 0x825387..0x8253AE: the colour's alpha byte = ftol(life x 0.255), from 255 at 1000 ms
			const auto alpha = static_cast<int32_t>(static_cast<float>(mark.life) * k_FadeAlphaPerMs) & 0xFF;
			registry.AssignOrReplace<Alpha>(mark.entity, static_cast<float>(alpha) / 255.0f);
			dirty = true;
		}
		mark.life -= step;
		if (mark.life <= 0)
		{
			registry.Destroy(mark.entity); // fn_00825300
			mark.entity = entt::null;
			dirty = true;
		}
	}
	std::erase_if(g_Marks, [](const Mark& mark) { return mark.entity == entt::null; });
	if (dirty)
	{
		registry.SetDirty();
	}
}
