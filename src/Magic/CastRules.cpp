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
#include "ECS/Influence/Influence.h"
#include "ECS/Map.h"
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
/// GUtils::Spiral 0x74D7E0 (table 0xDA59FC: +x, +z, -x, -z): the next cell step
glm::ivec2 Spiral(int& direction, int& count)
{
	static constexpr glm::ivec2 k_Steps[4] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
	if (--count == 0)
	{
		++direction;
		count = direction / 2;
	}
	return k_Steps[direction & 3];
}

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
} // namespace

bool cast_rules::InBounds(const glm::vec3& position)
{
	// (inferido: 512 cells per side when there is no terrain, an openblack fallback)
	const uint16_t side = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetCellsPerSide() : 512;
	// the MapCoords' high words (the 10 m cells) as unsigned: a negative coordinate is out. 6553.6 = 65536 / 10 m, the
	// MapCoords fixed point (castPos = (int)(x * 6553.6) in Spell::ProcessMaintainRequest 0x7204D0)
	const auto cellX = static_cast<uint16_t>(static_cast<int32_t>(std::floor(position.x * 6553.6f)) >> 16);
	const auto cellZ = static_cast<uint16_t>(static_cast<int32_t>(std::floor(position.z * 6553.6f)) >> 16);
	return cellX < side && cellZ < side;
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
	auto cell = glm::ivec2(ecs::MapInterface::GetGridCell(glm::vec2(position.x, position.z)));
	int direction = 1;
	int count = 1;
	int healed = 0;
	int looked = 0;
	while (cells != 0 && healed < maximum)
	{
		if (InBounds(glm::vec3(static_cast<float>(cell.x) * 10.0f, 0.0f, static_cast<float>(cell.y) * 10.0f)))
		{
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
				const glm::vec2 d(transform->position.x - position.x, transform->position.z - position.z);
				// GetDistanceInMetres < R, available, IsEffectReceiver, a Living that can be healed, dx^2 + dz^2 < R^2
				if (!(glm::length(d) < radius) || !ecs::effects::IsEffectReceiver(object, values) ||
				    !CanBeHealedByHealSpell(object) || !(glm::dot(d, d) < radius * radius))
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
		cell += Spiral(direction, count);
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
