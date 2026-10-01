/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CastRules.h"

#include <cmath>

#include <algorithm>
#include <vector>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "Core/Spell.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/GUtilsDistance.h"
#include "ECS/Influence/Influence.h"
#include "ECS/Map.h"
#include "ECS/MapCoords.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/HandSystemDetail.h"
#include "Magic/Objects/MagicTeleport.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "MagicTables.h"
#include "Spells/SpellForest.h"
#include "PSys/PSysManager.h"

using namespace openblack;
using namespace openblack::magic;

namespace
{
/// Living::CanBeHealedByHealSpell 0x5EE550 (!IsDead; the Dove 0x41EAB0 answers 0)
bool CanBeHealedByHealSpell(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<ecs::components::Villager>(object))
	{
		return true; // (inferido: dead villagers taken as gone from openblack's world; no IsDead test)
	}
	// TODO(M4c): the Dove (Animal SpellDove) answers 0. (inferido: no IsDead test for animals either)
	return registry.AllOf<ecs::components::Animal>(object);
}

/// The map's cells per side, 512 in the original (map_coords::k_MapCells)
/// (inferido: 512 when there is no terrain, an openblack fallback)
uint16_t MapSide()
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetCellsPerSide()
	                                           : static_cast<uint16_t>(ecs::map_coords::k_MapCells);
}
} // namespace

bool cast_rules::InBounds(const glm::vec3& position)
{
	// the MapCoords' high words (the 10 m cells) as unsigned: a negative coordinate is out. castPos = (int)(x * 6553.6)
	// in Spell::ProcessMaintainRequest 0x7204D0 (truncated, __ftol)
	return ecs::map_coords::InBounds(position, MapSide());
}

bool cast_rules::IsLand(const glm::vec3& position)
{
	return ecs::systems::hand_detail::IsLand(position);
}

bool cast_rules::CanCastRule(const GMagicInfo& info, const glm::vec3& position, PlayerNames player)
{
	if (!InBounds(position))
	{
		return false;
	}
	const auto inInfluence = [&]() {
		return influence::CalculatePlayerInfluence(player, magic::ToWorld(position), influence::CalcType::Default, true) > 0.0f;
	};
	switch (info.castRuleType)
	{
	case CastRuleType::Anywhere:
		return true;
	case CastRuleType::OnLand:
		return IsLand(position);
	case CastRuleType::InInfluence:
		return inInfluence();
	case CastRuleType::OnLandInInfluence:
		return IsLand(position) && inInfluence();
	default:
		return true; // ja: > 3 returns the 1 already in eax
	}
}

bool cast_rules::CanCastAt(MagicType type, const glm::vec3& position)
{
	switch (ClassOf(type))
	{
	case SpellClass::Heal:
		return (type == MagicType::Heal || type == MagicType::HealPowerUpOne) && FindHealTargets(position, entt::null) > 0;
	case SpellClass::Resource:
		return IsLand(position);
	case SpellClass::Creature:
		return false;
	case SpellClass::Forest:
		return spell_forest::CanCastAt(position); // 0x5FAE80 (Spells/SpellForest.cpp)
	case SpellClass::Teleport:
		// GMagicTeleportInfo 0x5FBE50: no MultiMapFixed (another stone, a building...) within fn_005FCCA0 = 6 m
		// (fn_00604C30), then GMagicInfo 0x5FB420 = 1
		return !teleport::AnyMultiMapFixedNear(position, teleport::k_Radius);
	default:
		return true;
	}
}

bool cast_rules::CanCastOn(MagicType type, entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const ecs::components::Transform>(object);
	const glm::vec3 position = transform != nullptr ? ToMap(transform->position) : glm::vec3(0.0f);
	switch (ClassOf(type))
	{
	case SpellClass::Resource:
		return IsLand(position);
	case SpellClass::Forest:
	case SpellClass::Creature:
		return false; // TODO(M8): a Creature whose mind allows it
	default:
		return CanCastAt(type, position);
	}
}

int cast_rules::FindHealTargets(const glm::vec3& position, entt::entity spell)
{
	const auto& tables = Locator::infoConstants::value();
	auto& registry = Locator::entitiesRegistry::value();
	const auto magicType =
	    spell != entt::null ? registry.Get<ecs::components::Spell>(spell).magicType : MagicType::Heal;
	const auto* info = GetMagicInfoAs<GMagicHealInfo>(tables, magicType);
	if (info == nullptr || !Locator::entitiesMap::has_value())
	{
		return 0;
	}
	float radius = info->dummyVar;
	int maximum = static_cast<int>(info->maxToHeal);
	if (spell != entt::null)
	{
		const float power = GetTribalPower(spell);
		radius *= power;
		maximum = static_cast<int>(std::lrint(static_cast<float>(maximum) * power)); // fistp
	}
	const int side = static_cast<int>(std::ceil(2.0f * radius / 10.0f));
	int cells = side * side;
	const auto values = ecs::effects::EffectValues::FromEffectInfo(GetMagicEffectInfo(tables, magicType));
	const auto& map = Locator::entitiesMap::value();
	// the spiral walks the MapCoords itself (a copy of the cast position, 0x5FBB6B..0x5FBB82): InBounds 0x6042C0 at
	// 0x5FBBBA and ToMap 0x603430 at 0x5FBBCB on it, and operator+= 0x605470 at 0x5FBCF1 adds the step to the high words
	// only, so the fraction stays and the 16-bit add wraps (from cell 0xFFFF, left of the map, a +1 step enters cell 0)
	auto coords = ecs::map_coords::FromMetres(glm::vec2(position.x, position.z));
	ecs::map_coords::Spiral spiral; // GUtils::Spiral 0x74D7E0, dir = count = 1 (0x5FBB9A..0x5FBBA5)
	int healed = 0;
	int looked = 0;
	while (cells != 0 && healed < maximum)
	{
		if (ecs::map_coords::InBounds(coords, MapSide()))
		{
			const auto cell = ecs::map_coords::Cell(coords);
			const ecs::MapInterface::CellId id(static_cast<uint16_t>(cell.x), static_cast<uint16_t>(cell.y));
			// the cell's mobile list, in entity order (inf: the original's list order)
			std::vector<entt::entity> objects(map.GetMobileInGridCell(id).begin(), map.GetMobileInGridCell(id).end());
			std::sort(objects.begin(), objects.end());
			for (const auto object : objects)
			{
				if (healed >= maximum)
				{
					break;
				}
				if (!registry.Valid(object))
				{
					continue; // openblack only: the original walks the cell's list
				}
				const auto* transform = registry.TryGet<const ecs::components::Transform>(object);
				if (transform == nullptr)
				{
					continue;
				}
				++looked;
				// both tests measure from the spiral's MapCoords ([ebp-0x24], the one operator+= moves), not from the
				// cast position: GetDistanceInMetres 0x74CD70 (coords, the object's +0x14) at 0x5FBBEE < R, available,
				// IsEffectReceiver, a Living that can be healed, and then dx^2 + dz^2 < R^2 with dx = coords - object
				// in metres (fild; fmul 10 [0x92C100]; fmul 2^-16 [0x8AC41C], 0x5FBC54..0x5FBCA3; test ah, 0x41: an
				// exact square, not the first test again). (aproximado) the object's MapCoords is its float position
				const auto at = ecs::map_coords::FromMetres(glm::vec2(transform->position.x, transform->position.z));
				const float dx = ecs::map_coords::ToMetres(coords.x) - ecs::map_coords::ToMetres(at.x);
				const float dz = ecs::map_coords::ToMetres(coords.z) - ecs::map_coords::ToMetres(at.z);
				const float squared = dz * dz + dx * dx;
				if (!(gutils::GetDistanceInMetres(coords, at) < radius) || !ecs::effects::IsEffectReceiver(object, values) ||
				    !CanBeHealedByHealSpell(object) || !(radius * radius > squared))
				{
					continue;
				}
				++healed;
				if (spell != entt::null)
				{
					// fn_00720140: psys->AddTarget (vt 0x114)
					if (auto* effect = psys::manager::Find(registry.Get<ecs::components::Spell>(spell).psys); effect != nullptr)
					{
						effect->AddTarget(object);
					}
				}
			}
		}
		--cells;
		ecs::map_coords::AddCells(coords, spiral.Next()); // 0x5FBCE5 Spiral, 0x5FBCF1 MapCoords += JustMapXZ
	}
	if (TraceEnabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Spell trace: FindTargets at ({:.1f}, {:.1f}) R {:.1f} max {}: {} cells, {} mobile objects looked at, {} "
		                   "to heal",
		                   position.x, position.z, radius, maximum, side * side, looked, healed);
	}
	return healed;
}
