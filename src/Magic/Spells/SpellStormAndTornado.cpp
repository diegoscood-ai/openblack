/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpellStormAndTornado.h"

#include <algorithm>
#include <string>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "ECS/Components/Spell.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/GUtilsDistance.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellCreator.h"
#include "Magic/Core/SpellEvent.h"
#include "Magic/MagicTables.h"
#include "PSys/ParticleTypes.h"
#include "PSys/PSysManager.h"
#include "SpellClasses.h"
#include "StormDebugHooks.h"

using namespace openblack;
using namespace openblack::magic;
using namespace openblack::ecs::components;
namespace reactions = openblack::ecs::effects::reactions;

namespace
{
/// 0xDA07F8 (head, linked through +0xF4) / 0xDA07FC: the ctor 0x72D9C0 puts each new one first
std::vector<entt::entity> g_StormSpells;

const GMagicStormAndTornadoInfo& StormInfoOf(entt::entity spell)
{
	// SpellStormAndTornado::GetMagicInfo 0x72DB80 = 0xD37D10[spell +0xB4]
	const auto type = Locator::entitiesRegistry::value().Get<const Spell>(spell).magicType;
	return *GetMagicInfoAs<GMagicStormAndTornadoInfo>(Locator::infoConstants::value(), type);
}

SpellStormData& DataOf(entt::entity spell)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* data = registry.TryGet<SpellStormData>(spell); data != nullptr)
	{
		return *data;
	}
	// ctor 0x72D9C0: Spell(type, creator), fn_0072DA10 (+0xEC = +0xF0 = 0), into the list first, the count + 1
	g_StormSpells.insert(g_StormSpells.begin(), spell);
	return registry.Assign<SpellStormData>(spell);
}

/// SpellStormAndTornado::InitWithPos 0x72DAA0
int InitWithPos(entt::entity spell, const glm::vec3& position, SpellCastData* castData, const psys::ProcessInfo& info)
{
	const auto& stormInfo = StormInfoOf(spell);
	auto& data = DataOf(spell);
	// the radius (castData +0) clamped to [minRadius, maxRadius] and written back. The original reads castData +0
	// directly; the fallback for a NULL castData (radius 40, no chants, time -1) is openblack's (inferido)
	SpellCastData fallback {40.0f, 0.0f, -1.0f, -1};
	SpellCastData* cast = castData != nullptr ? castData : &fallback;
	cast->magnitude = ClampStormRadius(stormInfo.minRadius, stormInfo.maxRadius, cast->magnitude);
	const int result = base::InitWithPos(spell, position, cast, info);
	// the swirl at the hand: GJPSysInterface::Create(no spell, PT 106 SF_StormCast, the LHPoint of pos, the
	// PSysProcessInfo's +0x24 (direction), 1.0, 0), then its magnitude is the spell's (vt 0x11C)
	const auto file = psys::ParticleTypeFile(ParticleType::StormCast);
	if (!file.empty())
	{
		data.castPsys = psys::manager::StartForSpell(std::string(file), ToWorld(position), info.direction, 1.0f, nullptr);
		if (auto* effect = psys::manager::Find(data.castPsys); effect != nullptr)
		{
			effect->SetMagnitude(Locator::entitiesRegistry::value().Get<const Spell>(spell).magnitude); // GetSpellMagnitude
		}
	}
	if (TraceEnabled())
	{
		const auto& component = Locator::entitiesRegistry::value().Get<const Spell>(spell);
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Spell trace: spell {} SpellStormAndTornado::InitWithPos at ({:.1f}, {:.1f}): radius {:.1f} "
		                   "(min {:.0f}, max {:.0f}), upkeep {:.2f}/turn, rain {:.0f}, cast psys {}",
		                   static_cast<uint32_t>(spell), position.x, position.z, component.magnitude, stormInfo.minRadius,
		                   stormInfo.maxRadius,
		                   StormCostToMaintain(EffectInfoOf(spell).costPerGameTurn, component.magnitude, stormInfo.radiusForNormalCost),
		                   stormInfo.rainAmount, data.castPsys);
	}
	return result;
}

/// SpellStormAndTornado::Process 0x72DB90
int Process(entt::entity spell)
{
	auto& data = DataOf(spell);
	// +0xF0 that is no longer available (0x72DBA2: vt 0x2C IsAvailable != 1) is forgotten (audit4: it only tested that
	// the reaction still existed; the same check as SpellWater::Process 0x724EF1)
	if (data.waterReaction != 0 && !reactions::IsAvailable(reactions::Find(data.waterReaction)))
	{
		data.waterReaction = 0;
	}
	if (data.castPsys != 0)
	{
		// a PSysProcessInfo of zeros with strength 1 and enabled, filled in by the creator (vt 0x5C UpdateSpellInfo),
		// strength 1 again; no creator, or the effect finished (5): it goes
		const auto& component = Locator::entitiesRegistry::value().Get<const Spell>(spell);
		psys::ProcessInfo info {};
		info.interfacePos = glm::vec3(0.0f);
		info.handPos = glm::vec3(0.0f);
		info.cameraForward = glm::vec3(0.0f);
		info.direction = glm::vec3(0.0f);
		info.power = 1.0f;
		info.curl = 0.0f;
		info.enabled = true;
		const bool hasCreator = component.creator.kind != SpellCreator::Kind::None;
		if (hasCreator)
		{
			creator::UpdateSpellInfo(component.creator, spell, info);
		}
		info.power = 1.0f;
		if (!hasCreator || !psys::manager::ProcessForSpell(data.castPsys, info, static_cast<float>(k_TurnMs) * 0.001f))
		{
			psys::manager::Delete(data.castPsys); // vt 4 (1)
			data.castPsys = 0;
		}
	}
	storm_debug::OnTurn(spell); // OPENBLACK_TEST_STORM_SHOT, OPENBLACK_STORM_TRACE
	return base::Process(spell);
}

/// SpellStormAndTornado::CalculateCostToMaintain 0x72DB50
float CostToMaintain(entt::entity spell)
{
	const auto& component = Locator::entitiesRegistry::value().Get<const Spell>(spell);
	return StormCostToMaintain(base::CalculateCostToMaintain(spell), component.magnitude, StormInfoOf(spell).radiusForNormalCost);
}

/// SpellStormAndTornado::ToBeDeleted 0x72DA20 (the class part): out of the list, then (after Spell::ToBeDeleted) the cast
/// PSys deleted (vt 4 (1))
void StormToBeDeleted(entt::entity spell)
{
	std::erase(g_StormSpells, spell);
	if (auto* data = Locator::entitiesRegistry::value().TryGet<SpellStormData>(spell); data != nullptr)
	{
		if (data->castPsys != 0)
		{
			psys::manager::Delete(data->castPsys);
			data->castPsys = 0;
		}
	}
}
} // namespace

float magic::ClampStormRadius(float minRadius, float maxRadius, float radius)
{
	// 0x72DAB2: fcompp mag, max; test ah, 1 -> keep mag only when below max; 0x72DACC: test ah, 0x41 -> keep it only
	// when above min
	if (!(radius < maxRadius))
	{
		radius = maxRadius;
	}
	if (!(radius > minRadius))
	{
		radius = minRadius;
	}
	return radius;
}

float magic::StormCostToMaintain(float baseCost, float magnitude, float radiusForNormalCost)
{
	// fdiv [info +0x60]; the square times Spell::CalculateCostToMaintain 0x720810
	const float k = magnitude / radiusForNormalCost;
	return baseCost * (k * k);
}

void spell_storm::ReactToRainOnFire(const glm::vec3& objectPosition)
{
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto spell : g_StormSpells)
	{
		if (!registry.Valid(spell) || !registry.AllOf<Spell, SpellStormData>(spell))
		{
			continue;
		}
		auto& data = registry.Get<SpellStormData>(spell);
		if (data.waterReaction != 0)
		{
			continue;
		}
		const auto& component = registry.Get<const Spell>(spell);
		// fn_00605CD0 (the distance to +0x14) against Get2DRadius 0x72D950 (GetSpellMagnitude): `test ah, 0x41; je`
		// -> the radius must be above the distance
		const float distance = gutils::GetDistanceInMetres(objectPosition, component.position);
		if (component.magnitude > distance)
		{
			data.waterReaction = reactions::CreateReaction(spell, Reaction::ReactToMagicWaterPuttingOutFire, component.player, true);
			return;
		}
	}
}

const std::vector<entt::entity>& spell_storm::Spells()
{
	return g_StormSpells;
}

uint32_t spell_storm::CastPSysOf(entt::entity spell)
{
	const auto* data = Locator::entitiesRegistry::value().TryGet<const SpellStormData>(spell);
	return data != nullptr ? data->castPsys : 0;
}

void spell_storm::Clear()
{
	g_StormSpells.clear();
	storm_debug::Reset();
}

void openblack::magic::RegisterStormSpell()
{
	SpellOps ops;
	ops.initWithPos = InitWithPos;
	ops.initWithObject = base::InitWithObject;
	ops.process = Process;
	ops.spellEvent = spell_event::SpellEvent;
	ops.costToMaintain = CostToMaintain;
	ops.closeDown = base::CloseDown; // vt 0x530 is Spell::CloseDown 0x55CE40
	ops.toBeDeleted = StormToBeDeleted;
	ops.hasEnoughChantsForRecast = base::HasEnoughChantsAndLifeForRecast;
	ops.particleType = base::GetParticleType;
	RegisterOps(SpellClass::StormAndTornado, ops);
}
