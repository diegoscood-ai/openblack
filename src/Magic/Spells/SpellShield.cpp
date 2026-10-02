/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpellShield.h"

#include <algorithm>
#include <limits>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "ECS/Components/Spell.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/GUtilsDistance.h"
#include "ECS/Influence/Influence.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellEvent.h"
#include "Magic/Core/SpellWithObjects.h"
#include "Magic/MagicTables.h"
#include "Magic/Objects/MapShield.h"
#include "SpellClasses.h"

using namespace openblack;
using namespace openblack::magic;
using namespace openblack::ecs::components;
namespace reactions = openblack::ecs::effects::reactions;

namespace
{
/// 0xDA07F0 (head, linked through +0x100) / 0xDA07F4: the newest first (fn_0072B3C0)
std::vector<entt::entity> g_ShieldSpells;

constexpr float k_ReactionRadiusAdd = 30.0f; ///< 0x8BF51C
constexpr float k_TownRadius = 250.0f;       ///< 0x43FA0000

const GMagicShieldInfo& ShieldInfoOf(entt::entity spell)
{
	// SpellShield::GetMagicInfo 0x72B820
	const auto type = Locator::entitiesRegistry::value().Get<const Spell>(spell).magicType;
	return *GetMagicInfoAs<GMagicShieldInfo>(Locator::infoConstants::value(), type);
}

SpellShieldData& DataOf(entt::entity spell)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* data = registry.TryGet<SpellShieldData>(spell); data != nullptr)
	{
		return *data;
	}
	// the ctor 0x72B4B0 -> fn_0072B3C0: into the list, +0xF4 / +0xF8 / +0xFC = 0
	g_ShieldSpells.insert(g_ShieldSpells.begin(), spell);
	return registry.Assign<SpellShieldData>(spell);
}

/// MapCoords::GetNearestTown 0x6020E0 (r): every player's (and the neutral one's) towns, the nearest closer than r
entt::entity NearestTown(const glm::vec3& position, float radius)
{
	entt::entity best = entt::null;
	float bestDistance = radius;
	Locator::entitiesRegistry::value().Each<const Town, const Transform>(
	    [&](entt::entity town, const Town& /*unused*/, const Transform& transform) {
		    // fn_00605CD0 = GUtils::GetDistanceInMetres 0x74CD70 (0x602112, 0x602193)
		    const float distance = gutils::GetDistanceInMetres(position, transform.position);
		    if (distance < bestDistance)
		    {
			    bestDistance = distance;
			    best = town;
		    }
	    });
	return best;
}

/// SpellShield::InitWithPos 0x72B5F0
int InitWithPos(entt::entity spell, const glm::vec3& position, SpellCastData* castData, const psys::ProcessInfo& info)
{
	const auto& shieldInfo = ShieldInfoOf(spell);
	auto& data = DataOf(spell);
	// the radius (castData +0) clamped to [minRadius, maxRadius] and written back. 0x72B5F0 reads castData +0
	// directly; the fallback for a NULL castData (radius 40, no chants, time -1) is openblack's (inferido)
	SpellCastData fallback {40.0f, 0.0f, -1.0f, -1};
	SpellCastData* cast = castData != nullptr ? castData : &fallback;
	const float radius = ClampShieldRadius(shieldInfo, cast->magnitude);
	cast->magnitude = radius;
	const int result = base::InitWithPos(spell, position, cast, info);
	auto& component = Locator::entitiesRegistry::value().Get<Spell>(spell);
	// REACTION_REACT_TO_MAGIC_SHIELD (13) by the spell's player, radius r + 30
	data.shieldReaction = reactions::CreateReaction(spell, Reaction::ReactToMagicShield, component.player, false);
	reactions::SetRadius(data.shieldReaction, radius + k_ReactionRadiusAdd);
	data.town = NearestTown(position, k_TownRadius);
	// an anti-influence ring of the spell's magnitude for every other active player (GGame::GetNextActivePlayer
	// 0x5508D0: the seven players' slots in use; aproximado: here the players the land made an entity for)
	const float magnitude = component.magnitude; // GetSpellMagnitude 0x7202C0
	for (uint8_t p = 0; p < static_cast<uint8_t>(PlayerNames::NEUTRAL); ++p)
	{
		const auto player = static_cast<PlayerNames>(p);
		if (players::EntityOf(player) == entt::null || (component.hasPlayer && component.player == player))
		{
			continue;
		}
		const auto ring = influence::CreateRing(position, player, magnitude, true); // InfluenceRing::Create 0x5CD9D0, flag 1
		if (ring != entt::null)
		{
			data.rings.insert(data.rings.begin(), ring);
		}
	}
	// MapShield::Create 0x72BE20 onto the SpellWithObjects list
	if (const auto shield = map_shield::Create(position, spell, radius); shield != entt::null)
	{
		spell_objects::Add(spell, shield);
	}
	if (TraceEnabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Spell trace: spell {} SpellShield::InitWithPos at ({:.1f}, {:.1f}): radius {:.1f}, {} anti rings, "
		                   "reaction {}, town {}, upkeep {:.2f}/turn",
		                   static_cast<uint32_t>(spell), position.x, position.z, radius, data.rings.size(),
		                   data.shieldReaction, data.town == entt::null ? -1 : static_cast<int>(data.town),
		                   ShieldCostToMaintain(EffectInfoOf(spell).costPerGameTurn, magnitude, shieldInfo.radiusForNormalCost));
	}
	return result;
}

/// SpellShield::Process 0x72B750: the struck reaction goes once it is not available, then SpellWithObjects::Process
int Process(entt::entity spell)
{
	auto& data = DataOf(spell);
	if (data.struckReaction != 0 && reactions::Find(data.struckReaction) == nullptr)
	{
		data.struckReaction = 0;
	}
	return spell_objects::Process(spell);
}

/// SpellShield::CalculateCostToMaintain 0x72B7F0
float CostToMaintain(entt::entity spell)
{
	const auto& component = Locator::entitiesRegistry::value().Get<const Spell>(spell);
	return ShieldCostToMaintain(base::CalculateCostToMaintain(spell), component.magnitude, ShieldInfoOf(spell).radiusForNormalCost);
}

/// SpellShield::CloseDown 0x72B840 -> SpellWithObjects::CloseDown 0x721300: CoreCloseDown, then (GetSetObjectsDying
/// OnCloseDown 0x55CF50 = 1) SetDying (vt 0x6A4) on every object not already going (+0xA bit 0)
void ShieldCloseDown(entt::entity spell)
{
	base::CloseDown(spell);
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto object : std::vector<entt::entity>(spell_objects::Objects(spell)))
	{
		if (registry.Valid(object))
		{
			map_shield::SetDying(object);
		}
	}
}

/// SpellShield::ToBeDeleted 0x72B500 (the class part; the base Spell::ToBeDeleted 0x71FD90 runs after it)
void ToBeDeleted(entt::entity spell)
{
	std::erase(g_ShieldSpells, spell);
	reactions::RemoveAllReactionsInitiatedByObject(spell);
	auto* data = Locator::entitiesRegistry::value().TryGet<SpellShieldData>(spell);
	if (data != nullptr)
	{
		data->shieldReaction = 0;
	}
	// SpellWithObjects::ToBeDeleted 0x720FD0: CloseDown (vt 0x530), the object list emptied
	magic::CloseDown(spell);
	for (const auto object : std::vector<entt::entity>(spell_objects::Objects(spell)))
	{
		spell_objects::Remove(spell, object);
	}
	// then every InfluenceRing's ToBeDeleted
	if (data != nullptr)
	{
		for (const auto ring : data->rings)
		{
			influence::DeleteRing(ring);
		}
		data->rings.clear();
	}
}
} // namespace

float magic::ClampShieldRadius(const GMagicShieldInfo& info, float radius)
{
	// 0x72B603: fcomp; test ah, 0x41 -> r = max unless max > r; then r = min unless min < r
	if (!(info.maxRadius > radius))
	{
		radius = info.maxRadius;
	}
	if (!(info.minRadius < radius))
	{
		radius = info.minRadius;
	}
	return radius;
}

float magic::ShieldCostToMaintain(float baseCost, float magnitude, float radiusForNormalCost)
{
	const float k = magnitude / radiusForNormalCost;
	return baseCost * (k * k);
}

void spell_shield::UpdateStruckReaction(entt::entity spell)
{
	auto& data = DataOf(spell);
	if (data.struckReaction == 0)
	{
		const auto& component = Locator::entitiesRegistry::value().Get<const Spell>(spell);
		data.struckReaction = reactions::CreateReaction(spell, Reaction::ReactToMagicShieldStruck, component.player, false);
		return;
	}
	reactions::Stamp(data.struckReaction); // +0x2C = the game turn
}

void spell_shield::SetUpDestroyedReaction(entt::entity spell)
{
	auto& data = DataOf(spell);
	reactions::RemoveAllReactionsOfTypeInitiatedBy(spell, Reaction::ReactToMagicShield);
	data.shieldReaction = 0;
	const auto& component = Locator::entitiesRegistry::value().Get<const Spell>(spell);
	reactions::CreateReaction(spell, Reaction::ReactToMagicShieldDestroyed, component.player, false);
}

bool spell_shield::IsUnder(entt::entity spell, const glm::vec3& point, float margin)
{
	// GetRadius (vt 0x60 -> Get2DRadius 0x72B440: the magnitude) - margin, against the distance to castPos (+0xCC)
	const auto& component = Locator::entitiesRegistry::value().Get<const Spell>(spell);
	const float distance = gutils::GetDistanceInMetres(point, component.castPos); // 0x74CD70 (0x72BD3C)
	return distance < component.magnitude - margin;
}

entt::entity spell_shield::FindShieldAt(const glm::vec3& point, uint32_t mask)
{
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto spell : g_ShieldSpells)
	{
		if (!registry.Valid(spell) || !registry.AllOf<Spell>(spell))
		{
			continue;
		}
		const auto& component = registry.Get<const Spell>(spell);
		const uint32_t bit = component.magicType == MagicType::Shield ? 2u : (component.magicType == MagicType::PhysicalShield ? 1u : 0u);
		if ((bit | mask) == 0)
		{
			continue;
		}
		const float distance = gutils::GetDistanceInMetres(point, component.castPos); // 0x74CD70 (0x72BA4B)
		if (distance <= component.magnitude)
		{
			return spell;
		}
	}
	return entt::null;
}

entt::entity spell_shield::TownOf(entt::entity spell)
{
	const auto* data = Locator::entitiesRegistry::value().TryGet<const SpellShieldData>(spell);
	return data != nullptr ? data->town : entt::null;
}

const std::vector<entt::entity>& spell_shield::Spells()
{
	return g_ShieldSpells;
}

void spell_shield::Clear()
{
	g_ShieldSpells.clear();
}

void openblack::magic::RegisterShieldSpell()
{
	SpellOps ops;
	ops.initWithPos = InitWithPos;
	ops.initWithObject = base::InitWithObject;
	ops.process = Process;
	ops.spellEvent = spell_event::SpellEvent;
	ops.costToMaintain = CostToMaintain;
	ops.closeDown = ShieldCloseDown; // (a plain CloseDown here would find magic::CloseDown: endless recursion)
	ops.toBeDeleted = ToBeDeleted;
	ops.hasEnoughChantsForRecast = base::HasEnoughChantsAndLifeForRecast;
	ops.particleType = base::GetParticleType;
	ops.updateStruckReaction = spell_shield::UpdateStruckReaction;
	ops.setUpDestroyedReaction = spell_shield::SetUpDestroyedReaction;
	RegisterOps(SpellClass::Shield, ops);
}
