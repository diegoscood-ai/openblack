/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "MagicSystem.h"

#include <optional>
#include <utility>
#include <vector>

#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/Implementations/HandGrain.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "GameClock.h"
#include "Locator.h"
#include "Magic/CastRules.h"
#include "Magic/Core/Chants.h"
#include "Magic/Core/OneOffSpellSeed.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellCreator.h"
#include "Magic/Core/SpellWithObjects.h"
#include "Magic/DispenserRules.h"
#include "Magic/HandMotion.h"
#include "Magic/MagicLoop.h"
#include "Magic/Spells/SpellForest.h"
#include "Magic/Spells/SpellStormAndTornado.h"
#include "Worship/InterfaceStatus.h"
#include "Worship/SpellDispenser.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
/// Whether an entity is a miracle still running
bool IsSpell(entt::entity spell)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	return registry.Valid(spell) && registry.AllOf<ecs::components::Spell>(spell);
}

/// The miracle pays a cost through its chant rules, its caster asked for the whole shortfall when forced: its strength
/// after, 0 once it has gone
float PaySpell(entt::entity spell, float cost, bool force)
{
	if (!IsSpell(spell))
	{
		return 0.0f;
	}
	auto& component = Locator::entitiesRegistry::value().Get<ecs::components::Spell>(spell);
	return magic::chants::PayFor(component, magic::ChantContextOf(spell), cost, force);
}

/// Whether working out a miracle's upkeep leaves everything as it was. A forest's upkeep counts its trees and a flock's
/// its animals, and each first makes the record it counts from when the miracle has none yet; every other kind's upkeep
/// only reads the tables and the miracle.
bool UpkeepOnlyReads(const ecs::Registry& registry, entt::entity spell, ecs::components::SpellClass spellClass)
{
	using ecs::components::SpellClass;
	switch (spellClass)
	{
	case SpellClass::Forest:
		return registry.AllOf<magic::SpellForestData>(spell);
	case SpellClass::FlockFlying:
	case SpellClass::FlockGround:
		return registry.AllOf<ecs::components::SpellObjects>(spell);
	default:
		return true;
	}
}

/// The player whose hand the miracles' service looks after
constexpr PlayerNames k_HandPlayer = PlayerNames::PLAYER_ONE;
} // namespace

entt::entity MagicSystem::CastAtPoint(MagicType type, PlayerNames player, glm::vec3 point, const magic::SpellCastData& cast,
                                      const psys::ProcessInfo& info)
{
	// the spells take a map position: x, z on the map and y above the land, as the hand's and the scripts' casts do
	auto data = cast;
	entt::entity spell = entt::null;
	magic::CastAtPos(type, magic::creator::OfPlayer(player), magic::ToMap(point), &spell, &data, info);
	return spell;
}

entt::entity MagicSystem::CastOnObject(MagicType type, PlayerNames player, entt::entity target,
                                       const magic::SpellCastData& cast, const psys::ProcessInfo& info)
{
	auto data = cast;
	entt::entity spell = entt::null;
	magic::CastAtObject(type, magic::creator::OfPlayer(player), target, &spell, &data, info);
	return spell;
}

void MagicSystem::CloseDown(entt::entity spell)
{
	if (IsSpell(spell))
	{
		magic::CloseDown(spell);
	}
}

bool MagicSystem::CanCastAt(MagicType type, [[maybe_unused]] PlayerNames player, glm::vec3 point)
{
	return magic::cast_rules::CanCastAt(type, magic::ToMap(point));
}

bool MagicSystem::SpellEvent(entt::entity spell, const psys::SpellEventInfo& event)
{
	// the class's own event, as the miracle's particles send it
	if (!IsSpell(spell))
	{
		return false;
	}
	const auto spellClass = Locator::entitiesRegistry::value().Get<const ecs::components::Spell>(spell).spellClass;
	return magic::OpsOf(spellClass).spellEvent(spell, event) != 0;
}

void MagicSystem::PayForSpell(entt::entity spell, float cost)
{
	// not forced: as a resource miracle pays for each load it drops
	PaySpell(spell, cost, false);
}

entt::entity MagicSystem::CreateDispenser(glm::vec3 position, MagicType type, float yAngleRadians)
{
	if (!Locator::infoConstants::has_value())
	{
		return entt::null;
	}
	// The building the Miracles window starts with, as nothing in the game makes a dispenser through the service yet
	const auto abode = magic::DefaultDispenserAbode(magic::DispenserAbodes(Locator::infoConstants::value()));
	if (!abode.has_value())
	{
		return entt::null;
	}
	// On the land, for the nearest town, at full size; then its miracle, a bubble at once and its building's own period,
	// as the map script's dispenser has them (a period of 0 leaves it inactive)
	constexpr int k_NearestTown = -1;
	const auto ground = magic::ToWorld(glm::vec3(position.x, 0.0f, position.z));
	const auto dispenser = worship::dispenser::Create(ground, *abode, k_NearestTown, yAngleRadians, 1.0f);
	if (dispenser == entt::null)
	{
		return entt::null;
	}
	const auto period = Locator::entitiesRegistry::value().Get<const ecs::components::SpellDispenser>(dispenser).timer.period;
	worship::dispenser::SetMagicAndPeriod(dispenser, type, period);
	return dispenser;
}

void MagicSystem::ChargeDispenser(entt::entity dispenser)
{
	if (!worship::dispenser::IsDispenser(dispenser))
	{
		return;
	}
	const auto& component = Locator::entitiesRegistry::value().Get<const ecs::components::SpellDispenser>(dispenser);
	if (component.orb == entt::null && component.magicType != MagicType::None)
	{
		worship::dispenser::CreateOneOffSpellSeed(dispenser);
	}
}

void MagicSystem::SetDispenserPeriod(entt::entity dispenser, float seconds)
{
	worship::dispenser::SetTimerTime(dispenser, seconds);
}

entt::entity MagicSystem::CreateOneOffSeed(glm::vec3 position, SpellSeedType seed, int powerUp, float multiplier)
{
	return magic::one_off::Create(position, seed, powerUp, multiplier);
}

entt::entity MagicSystem::CreateOneOffSeedFor(glm::vec3 position, MagicType type)
{
	return magic::one_off::CreateFor(position, type);
}

bool MagicSystem::Remove(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity))
	{
		return false;
	}
	if (const auto* dispenser = registry.TryGet<const ecs::components::SpellDispenser>(entity))
	{
		// its swirl, its bubble, then the building itself at once, out of its town
		if (dispenser->effect != ParticleSystemInterface::k_NoEffect && Locator::particleSystem::has_value())
		{
			Locator::particleSystem::value().Delete(dispenser->effect);
		}
		magic::one_off::Destroy(dispenser->orb);
		ecs::ToBeDeleted(entity, true);
		return true;
	}
	if (registry.AllOf<ecs::components::OneOffSpellSeed>(entity))
	{
		magic::one_off::Destroy(entity);
		return true;
	}
	return false;
}

entt::entity MagicSystem::GiveSeedToHand(PlayerNames player, SpellSeedType seed, int powerUp, float multiplier)
{
	return magic::one_off::CreateSpellIntoHand(player, seed, powerUp, multiplier);
}

entt::entity MagicSystem::SummonSeed([[maybe_unused]] PlayerNames player, [[maybe_unused]] SpellSeedType seed,
                                     [[maybe_unused]] int powerUp)
{
	// the player's seeds are charged at the worship sites' icons; none comes straight from the player's prayer power
	return entt::null;
}

void MagicSystem::DiscardHeldSeed()
{
	// the hand's own forced drop, as a shake has it: the next turn the seed goes back to its worship site
	if (worship::interface::HeldSpellSeed(k_HandPlayer) != entt::null)
	{
		Locator::handSystem::value().ForceDropHeld();
	}
}

bool MagicSystem::IsHandBusy() const
{
	return worship::interface::HeldSpellSeed(k_HandPlayer) != entt::null;
}

magic::PourPose MagicSystem::GetHandPour(float fraction) const
{
	// at rest when the locator has no miracles' service, whose hand magic state the grain pours from
	return Locator::magicSystem::has_value() ? hand_grain::PoseAt(fraction) : magic::PourPose {};
}

std::optional<entt::entity> MagicSystem::GetHeldSeed() const
{
	const auto seed = worship::interface::HeldSpellSeed(k_HandPlayer);
	return seed != entt::null ? std::optional<entt::entity> {seed} : std::nullopt;
}

bool MagicSystem::SendSpellEvent(entt::entity spell, const psys::SpellEventInfo& event)
{
	return SpellEvent(spell, event);
}

float MagicSystem::ForcePayForSpell(entt::entity spell, float cost)
{
	return PaySpell(spell, cost, true);
}

float MagicSystem::SpellStrength(entt::entity spell)
{
	return IsSpell(spell) ? magic::GetSpellStrength(spell) : 0.0f;
}

void MagicSystem::ProcessTurn()
{
	// the turn the game loop started: only its start moves the clock's turn on
	magic::ProcessTurn(game_clock::Turn());
}

void MagicSystem::Update(float seconds)
{
	magic::Update(seconds);
}

void MagicSystem::Reset()
{
	magic::OnLoadMap();
}

std::vector<MagicSystemInterface::SpellInfo> MagicSystem::GetSpells() const
{
	std::vector<SpellInfo> result;
	if (!Locator::infoConstants::has_value())
	{
		return result;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	for (const auto spell : magic::Spells())
	{
		if (!registry.Valid(spell))
		{
			continue;
		}
		const auto* component = registry.TryGet<const ecs::components::Spell>(spell);
		if (component == nullptr)
		{
			continue;
		}
		// the strength and the upkeep both work out the upkeep: 0 for a miracle whose upkeep would make a record
		const bool readable = UpkeepOnlyReads(registry, spell, component->spellClass);
		result.push_back({
		    .entity = spell,
		    .magicType = component->magicType,
		    .player = component->player,
		    .age = component->age,
		    .duration = component->duration,
		    .chants = component->chants,
		    .initialChants = component->initialChants,
		    .strength = readable ? magic::GetSpellStrength(spell) : 0.0f,
		    .upkeep = readable ? magic::ChantContextOf(spell).costToMaintain : 0.0f,
		    .closing = component->closedDown,
		    .fromHand = magic::IsCastFromHand(spell),
		    .effect = component->psys,
		    .position = magic::ToWorld(component->position),
		});
	}
	return result;
}

std::optional<entt::entity> MagicSystem::SpellAt(MagicType type, glm::vec3 point, float radius) const
{
	// the miracles keep map positions, as a script's point is taken
	const auto spell = magic::FindSpellAt(type, magic::ToMap(point), radius);
	return spell != entt::null ? std::optional<entt::entity> {spell} : std::nullopt;
}

void MagicSystem::RainOnFire(const glm::vec3& point)
{
	magic::spell_storm::ReactToRainOnFire(point);
}

std::vector<MagicSystemInterface::DispenserInfo> MagicSystem::GetDispensers() const
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	std::vector<DispenserInfo> result;
	registry.Each<const ecs::components::SpellDispenser, const ecs::components::Transform>(
	    [&registry, &result](entt::entity entity, const ecs::components::SpellDispenser& dispenser,
	                         const ecs::components::Transform& transform) {
		    result.push_back({
		        .entity = entity,
		        .magicType = dispenser.magicType,
		        .position = transform.position,
		        .hasOrb = dispenser.orb != entt::null && registry.Valid(dispenser.orb),
		        .tick = dispenser.timer.tick,
		        .period = dispenser.timer.period,
		        .active = dispenser.timer.active,
		    });
	    });
	return result;
}

void MagicSystem::ProcessGameInputs()
{
	magic::ProcessGameInputs();
}

void MagicSystem::ProcessTurnStart(uint32_t turn)
{
	magic::ProcessTurnStart(turn);
}

void MagicSystem::ProcessForests(uint32_t turn)
{
	magic::ProcessForests(turn);
}

void MagicSystem::RunDebugHooks()
{
	magic::RunDebugHooks();
}

void MagicSystem::ProcessSpellParticlesEndOfLoop()
{
	magic::ProcessSpellParticlesEndOfLoop();
}

void MagicSystem::ProcessHandTurn()
{
	magic::ProcessHandTurn();
}
