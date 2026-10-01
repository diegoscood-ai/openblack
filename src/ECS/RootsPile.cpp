/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "RootsPile.h"

#include <cmath>

#include <algorithm>
#include <optional>
#include <vector>

#include <glm/mat3x3.hpp>

#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
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
using namespace openblack::ecs;

namespace
{
struct Pile
{
	entt::entity entity {entt::null}; ///< +4: the LH3DObject
	int32_t milliseconds {RootsPile::k_LifeMilliseconds}; ///< +8
};

/// The list 0xEB9A00 (newest first in the original; the order changes nothing that is seen)
std::vector<Pile> g_piles;
/// [0xEB9A04]: MeshPack[mesh] of the first pile ever made (never reset, not even by ClearAllStuff)
std::optional<int32_t> g_mesh;

/// LH3DObject::SetPosition 0x423140 from the identity: rows X = (cos, 0, sin), Z = (-sin, 0, cos), times the scale
glm::mat3 TurnedAboutY(float angle)
{
	const float c = std::cos(angle);
	const float s = std::sin(angle);
	return glm::mat3(glm::vec3(c, 0.0f, s), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(-s, 0.0f, c));
}
} // namespace

int RootsPile::AlphaFor(int32_t millisecondsLeft)
{
	// 0x825387..0x8253A3: fild ms, x 0.255, __ftol (truncation), into the colour's top byte
	return static_cast<int>(static_cast<float>(millisecondsLeft) * k_AlphaPerMillisecond);
}

entt::entity RootsPile::Create(const glm::vec3& position, float angle, float scale, int32_t mesh)
{
	if (!g_mesh.has_value())
	{
		// 0x825251..0x82525F: an index out of the pack is 0
		g_mesh = mesh >= 0 && mesh < static_cast<int32_t>(MeshId::_COUNT) ? mesh : 0;
	}
	// 0x8252EB: SmokyStuff::Create(point, 1, 1.0f, 0xFFFFFFFF), made whether the object could be drawn or not
	smoky_stuff::Create(position, 1, 1.0f, 0xFFFFFFFFu);
	// (port guard) without the registry or the pack (unit tests) the pile still counts down, with nothing drawn
	const auto meshId = resources::HashIdentifier(static_cast<MeshId>(*g_mesh));
	if (!Locator::entitiesRegistry::has_value() || !Locator::resources::has_value() ||
	    !Locator::resources::value().GetMeshes().Contains(meshId))
	{
		g_piles.push_back({entt::null, k_LifeMilliseconds});
		return entt::null;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.Assign<components::Transform>(entity, position, TurnedAboutY(angle), glm::vec3(scale));
	registry.Assign<components::Mesh>(entity, meshId, static_cast<int8_t>(0), static_cast<int8_t>(-1));
	// LH3DObject::Create(1) is a morphable object and UpdateMelting (vt 0x1E8) drapes it on the land under it.
	// (aproximado) the original drapes it once (0x8252C3); openblack's MorphWithTerrain follows the land every frame (the
	// same unless the land under it changes in its 15 s)
	registry.Assign<components::MorphWithTerrain>(entity);
	registry.SetDirty();
	g_piles.push_back({entity, k_LifeMilliseconds});
	return entity;
}

void RootsPile::DrawAll(int32_t milliseconds)
{
	if (g_piles.empty())
	{
		return;
	}
	auto* registry = Locator::entitiesRegistry::has_value() ? &Locator::entitiesRegistry::value() : nullptr;
	bool dirty = false;
	for (auto& pile : g_piles)
	{
		const bool alive = registry != nullptr && pile.entity != entt::null && registry->Valid(pile.entity);
		// 0x825376..0x8253AE: the fade, before the time goes down. (aproximado) the land light of fn_00801C90 (0x825371,
		// the colour's RGB at the pile) is left to openblack's mesh lighting
		if (alive && pile.milliseconds <= k_FadeMilliseconds)
		{
			const int alpha = AlphaFor(pile.milliseconds) & 0xFF;
			registry->AssignOrReplace<components::Alpha>(pile.entity, static_cast<float>(alpha) / 255.0f);
			dirty = true;
		}
		// 0x8253B1..0x8253C3: g_game_time_inc off; at <= 0 fn_00825300 deletes it
		pile.milliseconds -= milliseconds;
		if (pile.milliseconds <= 0)
		{
			if (alive)
			{
				registry->Destroy(pile.entity);
				dirty = true;
			}
			pile.entity = entt::null;
		}
	}
	std::erase_if(g_piles, [](const Pile& pile) { return pile.milliseconds <= 0; });
	if (dirty)
	{
		registry->SetDirty();
	}
}

void RootsPile::Clear()
{
	if (Locator::entitiesRegistry::has_value())
	{
		auto& registry = Locator::entitiesRegistry::value();
		for (const auto& pile : g_piles)
		{
			if (pile.entity != entt::null && registry.Valid(pile.entity))
			{
				registry.Destroy(pile.entity);
			}
		}
	}
	g_piles.clear();
}

size_t RootsPile::Count()
{
	return g_piles.size();
}
