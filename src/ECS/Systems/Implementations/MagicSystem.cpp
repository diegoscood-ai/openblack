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

#include "3D/LandIslandInterface.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/PlayerMagic.h"
#include "ECS/Components/Poisoned.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/CreatureMimic.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Influence/Influence.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/PotResource.h"
#include "ECS/Registry.h"
#include "ECS/ResourceStores.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/Implementations/HandGrain.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/TownEmergency.h"
#include "GameClock.h"
#include "Locator.h"
#include "Magic/CastRules.h"
#include "Magic/Core/Chants.h"
#include "Magic/Core/OneOffSpellSeed.h"
#include "Magic/Core/Players.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellCreator.h"
#include "Magic/Core/SpellWithObjects.h"
#include "Magic/DispenserRules.h"
#include "Magic/EffectSourceRules.h"
#include "Magic/HandMotion.h"
#include "Magic/MagicLoop.h"
#include "Magic/MagicTables.h"
#include "Magic/MiracleDeeds.h"
#include "Magic/Spells/SpellForest.h"
#include "Magic/Spells/SpellStormAndTornado.h"
#include "Magic/Spells/SpellWater.h"
#include "Particles/Rules/Explosion.h"
#include "Worship/InterfaceStatus.h"
#include "Worship/PlayerSpellIcons.h"
#include "Worship/SpellDispenser.h"
#include "Worship/WorshipSite.h"
#include "Worship/WorshipSpellIcon.h"

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

/// The level a seed is summoned at: its plain miracle. The worship is only ever asked for that; a seed's power-ups are
/// charged onto it once it is in the hand.
constexpr int k_SummonedPowerUp = -1;
/// A summon asks an icon only while its site has prayer power to charge it with, or the icon holds some already
constexpr bool k_SummonNeedsChants = true;

/// A request made of the player's worship, and the seed it put into the player's hand: one only when the icon asked was
/// already full. Otherwise the icon starts charging, and its seed comes into the hand turns later, once it is full.
template <typename Request>
entt::entity SeedComingIntoHand(PlayerNames player, Request&& request)
{
	const auto before = worship::interface::HeldSpellSeed(player);
	std::forward<Request>(request)();
	const auto after = worship::interface::HeldSpellSeed(player);
	return after != before ? after : entt::entity {entt::null};
}
} // namespace

entt::entity MagicSystem::CastAtPoint(MagicType type, PlayerNames player, glm::vec3 point, const magic::SpellCastData& cast,
                                      const particles::ProcessInfo& info)
{
	// the spells take a map position: x, z on the map and y above the land, as the hand's and the scripts' casts do
	auto data = cast;
	entt::entity spell = entt::null;
	magic::CastAtPos(type, magic::creator::OfPlayer(player), magic::ToMap(point), &spell, &data, info);
	return spell;
}

entt::entity MagicSystem::CastOnObject(MagicType type, PlayerNames player, entt::entity target,
                                       const magic::SpellCastData& cast, const particles::ProcessInfo& info)
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

bool MagicSystem::SpellEvent(entt::entity spell, const particles::SpellEventInfo& event)
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
	// The building the Magic window starts with, as nothing in the game makes a dispenser through the service yet
	const auto abode = magic::DefaultDispenserAbode(magic::DispenserAbodes(Locator::infoConstants::value()));
	if (!abode.has_value())
	{
		return entt::null;
	}
	// On the land, with no town (the first player's oldest), at full size; then its miracle, a bubble at once and its
	// building's own period, as the map script's dispenser has them (a period of 0 leaves it inactive)
	const auto ground = magic::ToWorld(glm::vec3(position.x, 0.0f, position.z));
	const auto dispenser = worship::dispenser::Create(ground, *abode, entt::null, yAngleRadians, 1.0f);
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

void MagicSystem::SetDispenserActive(entt::entity dispenser, bool active)
{
	// turned on, the dispenser floats its bubble at once and counts again from it
	worship::dispenser::SetActive(dispenser, active);
}

void MagicSystem::SetDispenserTurns(entt::entity dispenser, uint32_t turns)
{
	worship::dispenser::SetPeriodTurns(dispenser, turns);
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

entt::entity MagicSystem::SummonSeed(PlayerNames player, SpellSeedType seed, int powerUp)
{
	// As the miracle selection asks the player's worship for a seed: only for its plain miracle, and only when some site
	// has an icon of it that can be asked; then the best of those icons is asked
	if (powerUp != k_SummonedPowerUp || !worship::player::IconValidForRequest(player, seed))
	{
		return entt::null;
	}
	return SeedComingIntoHand(player, [player, seed]() { worship::player::RequestSpell(player, seed); });
}

entt::entity MagicSystem::SummonSeedAtSite(entt::entity site, SpellSeedType seed, int powerUp)
{
	// The same request, made of this site's icon of the seed for the site's player
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* worshipSite = registry.Valid(site) ? registry.TryGet<const ecs::components::WorshipSite>(site) : nullptr;
	if (worshipSite == nullptr || powerUp != k_SummonedPowerUp)
	{
		return entt::null;
	}
	const auto player = worshipSite->player;
	const auto icon = worship::site::GetSpellIconFromSeedType(site, seed);
	if (icon == entt::null || !worship::icon::ValidForRequestSpell(icon, player, k_SummonedPowerUp, k_SummonNeedsChants))
	{
		return entt::null;
	}
	return SeedComingIntoHand(
	    player, [icon, player]() { worship::icon::RequestSpell(icon, player, k_SummonedPowerUp, k_SummonNeedsChants); });
}

void MagicSystem::DiscardHeldSeed()
{
	// the hand's own forced drop, as a shake has it: the next turn the seed goes back to its worship site
	if (worship::interface::HeldSpellSeed(k_HandPlayer) != entt::null)
	{
		Locator::handSystem::value().ForceDropHeld();
		_lastHandResult = HandResult::Discarded;
	}
}

bool MagicSystem::TapAction()
{
	// the hand taps a bubble under it itself, through its own press and tap; nothing is tapped through the service
	return false;
}

bool MagicSystem::PressAction()
{
	// with no seed in the hand the press is the hand's own, for the creatures and things
	if (!IsHandBusy())
	{
		return false;
	}
	_pressHeld = true;
	RecordHandResult(Locator::handSystem::value().PressSeedAction());
	return true;
}

void MagicSystem::ReleaseAction()
{
	// the button comes up on the hand's seed release when a seed is held, and also when the press was the miracles' and
	// the seed has gone from the hand while the button was down (a held miracle that used up its prayer): the hand's
	// release ends the press it holds, whatever it then has to let go of
	if (IsHandBusy() || _pressHeld)
	{
		RecordHandResult(Locator::handSystem::value().ReleaseSeedAction());
	}
	_pressHeld = false;
}

void MagicSystem::RecordHandResult(HandResult result)
{
	if (result != HandResult::None)
	{
		_lastHandResult = result;
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

bool MagicSystem::SendSpellEvent(entt::entity spell, const particles::SpellEventInfo& event)
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
	_lastHandResult = HandResult::None;
}

float MagicSystem::GetTribalPower(PlayerNames player, Tribe tribe) const
{
	using ecs::components::PlayerMagic;
	if (tribe == Tribe::NONE || static_cast<size_t>(tribe) >= PlayerMagic::k_Tribes)
	{
		return 1.0f;
	}
	// read through the const registry, so that no storage is made
	return magic::players::ReadMagicOf(player).tribalPower.at(static_cast<size_t>(tribe));
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
		    .effect = component->effect,
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

// ---- The miracles' world in the game --------------------------------------------------------------------------------

float GameMagicWorld::LandHeight(glm::vec2 xz) const
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(xz) : 0.0f;
}

bool GameMagicWorld::InBounds(glm::vec3 point) const
{
	return magic::cast_rules::InBounds(point);
}

bool GameMagicWorld::IsDryLand(glm::vec3 point) const
{
	return ecs::pot_resource::IsDryLand(point);
}

bool GameMagicWorld::IsLand(glm::vec3 point) const
{
	return magic::cast_rules::IsLand(point);
}

bool GameMagicWorld::InInfluence(PlayerNames player, glm::vec3 point) const
{
	// with the allies, as the cast rules ask it
	return influence::CalculatePlayerInfluence(player, magic::ToWorld(point), influence::CalcType::Default, true) > 0.0f;
}

std::optional<glm::vec3> GameMagicWorld::PositionOf(entt::entity object) const
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (!registry.Valid(object))
	{
		return std::nullopt;
	}
	const auto* transform = registry.TryGet<const ecs::components::Transform>(object);
	return transform != nullptr ? std::optional<glm::vec3> {transform->position} : std::nullopt;
}

bool GameMagicWorld::ApplyEffect(entt::entity object, const magic::EffectValues& values,
                                 [[maybe_unused]] const magic::EffectSource& source)
{
	// who applies it is in our values; an object takes it while it is there at all: whether it takes effects is for the
	// caller to ask, as the game's own callers ask (the spell event does, a building struck does not)
	if (!std::as_const(Locator::entitiesRegistry::value()).Valid(object))
	{
		return false;
	}
	auto applied = values;
	applied.blow = source.blow;
	ecs::effects::ApplyEffect(object, applied);
	return true;
}

std::vector<entt::entity> GameMagicWorld::ApplyEffectAt(glm::vec3 point, const magic::EffectValues& values,
                                                        [[maybe_unused]] const magic::EffectSource& source)
{
	// ours takes the point's altitude above the land
	const glm::vec3 position(point.x, point.y - LandHeight(glm::vec2(point.x, point.z)), point.z);
	auto applied = values;
	std::vector<entt::entity> hits;
	applied.ApplyEffectToMapPos(position, hits);
	return hits;
}

void GameMagicWorld::Water(const magic::WaterDrop& drop)
{
	// the miracle's own kind says whether it is the power-up's water, and its component who cast it
	if (!Locator::entitiesRegistry::value().Valid(drop.spell))
	{
		return;
	}
	magic::water::LandDrop(drop.spell, drop.position, drop.reach, drop.ringGrowth);
}

std::vector<entt::entity> GameMagicWorld::HealTargets(glm::vec3 point, float radius, size_t maximum) const
{
	return magic::cast_rules::HealTargetsAround(point, radius, maximum);
}

void GameMagicWorld::CurePoison(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(object) && registry.AllOf<ecs::components::Poisoned>(object))
	{
		registry.Remove<ecs::components::Poisoned>(object);
	}
}

bool GameMagicWorld::AddResource(ResourceType type, glm::vec3 point, uint32_t amount, bool speedUp, PlayerNames player)
{
	// put down by the player's interface, which only a human player has: into the stores and piles about there, or onto
	// a new pile; a miracle's resource is never poisoned
	return ecs::resource_stores::Service().PourAt(type, point, amount, speedUp, player, false);
}

namespace
{
/// The values, with who applied them taken from the source when they name nobody themselves
magic::EffectValues WithApplier(const magic::EffectValues& values, const magic::EffectSource& source)
{
	if (!magic::NamesNoApplier(values))
	{
		return values;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto struckBy = source.appliedBy.value_or(entt::null);
	const bool struck = struckBy != entt::null && registry.Valid(struckBy);
	return magic::ApplierFrom(values, source, struck, struck && registry.AllOf<ecs::components::Creature>(struckBy),
	                          struck ? ecs::town_emergency::HitterPlayer(struckBy) : std::nullopt);
}
} // namespace

void MagicSystem::ApplyEffectAt(glm::vec3 point, const magic::EffectValues& values, PlayerNames player)
{
	const magic::EffectSource source {.player = player};
	_world.ApplyEffectAt(point, WithApplier(values, source), source);
}

bool MagicSystem::ApplyEffectToObject(entt::entity object, const magic::EffectValues& values, PlayerNames player)
{
	const magic::EffectSource source {.player = player};
	return _world.ApplyEffect(object, WithApplier(values, source), source);
}

bool MagicSystem::ApplyEffectToObject(entt::entity object, const magic::EffectValues& values, const magic::EffectSource& source)
{
	return _world.ApplyEffect(object, WithApplier(values, source), source);
}

void GameMagicWorld::PlayerAffected(entt::entity object, MagicType type, PlayerNames player)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (!registry.Valid(object))
	{
		return;
	}
	// A miracle meant to anger shows no wish to impress a town; any other near another player's town does
	const bool meantToAnger =
	    magic::GetMagicEffectInfo(Locator::infoConstants::value(), type).perceivedPlayerDesire[0] == CreatureDesires::Anger;
	const auto at = PositionOf(object).value_or(glm::vec3(0.0f));
	const auto town = ecs::map_cells::GetNearestTown(map_coords::FromMetres(glm::vec2(at.x, at.z)), magic::k_DeedTownReach);
	const auto* owned = town != entt::null ? registry.TryGet<const ecs::components::Town>(town) : nullptr;
	const bool anotherPlayersTown = owned != nullptr && owned->owner != player;
	// What it reached: a storage pit once built, any worship site, and a building going up (its percent built below one)
	// or damaged and not yet repaired
	const magic::DeedTarget target {
	    .field = registry.AllOf<ecs::components::Field>(object),
	    .onFire = ecs::fire::IsOnFire(object),
	    .storagePit = registry.AllOf<ecs::components::StoragePit>(object) && ecs::abodes::IsBuilt(object),
	    .worshipSite = registry.AllOf<ecs::components::WorshipSite>(object),
	    .buildingBeingBuilt = ecs::abodes::IsBeingBuilt(object),
	};
	const auto deed = magic::MiracleDeed(type, meantToAnger, anotherPlayersTown, target);
	if (deed != magic::k_NoDeed)
	{
		ecs::creature_mimic::Consider(player, static_cast<creature_watching::Deed>(deed), object, type);
	}
}

bool GameMagicWorld::CanBeDestroyedBySpell(entt::entity object, entt::entity spell) const
{
	return particles::explosion::CanBeDestroyedBySpell(object, spell);
}
