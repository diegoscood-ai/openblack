/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "EffectValues.h"

#include <cmath>

#include <utility>
#include <vector>

#include <glm/geometric.hpp>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Alignment.h"
#include "Common/GUtilsDistance.h"
#include "Creature/CreatureMarks.h"
#include "ECS/Abodes.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/LandscapeVortex.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownArtefact.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/CreaturePose.h"
#include "ECS/Fields.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/FishFarms.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectResources.h"
#include "ECS/PlayerRivalry.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TempleDestructionSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownEmergency.h"
#include "ECS/TownAggression.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerScript.h"
#include "Hand/HandGrabRules.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Physics/TempleHeart.h"
#include "Reactions.h"
#include "Worship/Citadel.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::effects;
using namespace openblack::ecs::components;

namespace
{
float LandAt(float x, float z)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)) : 0.0f;
}

/// The info's defenceMultiplier*; a creature's are its species row's, its burn, crush, hit and fly away divided by the
/// size it is drawn at, in its temple's pen the pen size (CreatureDefenceDivisor). Without an info: 1 (inferred)
std::array<float, static_cast<size_t>(EffectValues::Number::_COUNT)> DefenceMultipliers(entt::entity object)
{
	const auto* info = fire::traits::InfoOf(object); // the physics' infos plus abodes, fields, features, fireballs
	if (info == nullptr)
	{
		std::array<float, static_cast<size_t>(EffectValues::Number::_COUNT)> ones {};
		ones.fill(1.0f);
		return ones;
	}
	std::array<float, static_cast<size_t>(EffectValues::Number::_COUNT)> multipliers {
	    info->defenceMultiplierBurn,
	    info->defenceMultiplierCrush,
	    info->defenceMultiplierHit,
	    info->defenceMultiplierHeal,
	    info->defenceMultiplierFlyAway,
	    info->defenceMultiplierAlignmentModification,
	    info->defenceMultiplierBeliefModification};
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (registry.AllOf<Creature>(object))
	{
		const float divisor = CreatureDefenceDivisor(ecs::creature_pose::DrawnSize(registry, object));
		for (const auto number : {EffectValues::Number::Burn, EffectValues::Number::Crush, EffectValues::Number::Hit,
		                          EffectValues::Number::FlyAway})
		{
			multipliers.at(static_cast<size_t>(number)) /= divisor;
		}
	}
	return multipliers;
}

/// The burn goes to the fire first (fire::ApplyEffectToFireEffectIfNecessary, ECS/Fire), then the positive crush and
/// hit x their multipliers
float DamageEffect(entt::entity object, const EffectValues& values)
{
	fire::ApplyEffectToFireEffectIfNecessary(object, values);
	if (!ecs::IsAvailable(object))
	{
		return 0.0f;
	}
	const auto multipliers = DefenceMultipliers(object);
	float damage = 0.0f;
	for (const size_t i : {static_cast<size_t>(EffectValues::Number::Crush), static_cast<size_t>(EffectValues::Number::Hit)})
	{
		const float amount = values.numbers[i] * multipliers[i];
		if (amount > 0.0f)
		{
			damage += amount;
		}
	}
	return damage;
}

/// The positive heal x its multiplier
float HealEffect(entt::entity object, const EffectValues& values)
{
	constexpr auto k_Heal = static_cast<size_t>(EffectValues::Number::Heal);
	const float amount = values.numbers[k_Heal] * DefenceMultipliers(object)[k_Heal];
	return amount > 0.0f ? amount : 0.0f;
}

} // namespace

/// The player whose thing it is, as the harm each player does the others is kept and a crushed thing's reaction is
/// started: a villager's town's owner, none without a town; a creature's owner; a building's town's owner, the neutral
/// player without a town (a field none); a fish farm's town's owner, none without a town; the player of a reward, an
/// artefact, a pile or a miracle's animal; none for the football; the neutral player for anything else
std::optional<PlayerNames> effects::HarmedPlayerOf(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<Villager>(object))
	{
		return villager::GetPlayerOf(object);
	}
	if (const auto* creature = registry.TryGet<const Creature>(object))
	{
		return creature->owner;
	}
	if (registry.AllOf<Abode>(object))
	{
		const auto town = abode_villagers::TownOf(object);
		if (const auto* owner = town != entt::null ? registry.TryGet<const Town>(town) : nullptr)
		{
			return owner->owner;
		}
		return fields::IsField(object) ? std::nullopt : std::optional<PlayerNames> {PlayerNames::NEUTRAL};
	}
	if (registry.AllOf<FishFarm>(object))
	{
		return fish_farms::PlayerOf(object);
	}
	if (registry.AllOf<Ball>(object))
	{
		return std::nullopt;
	}
	if (const auto* reward = registry.TryGet<const Reward>(object))
	{
		return reward->player;
	}
	if (const auto* artefact = registry.TryGet<const TownArtefact>(object))
	{
		return artefact->player;
	}
	// a pile of a store answers with its store's player, else its own (object_resources::PlayerOfPile)
	if (registry.AllOf<Pot>(object))
	{
		return object_resources::PlayerOfPile(object);
	}
	if (const auto* animal = registry.TryGet<const Animal>(object); animal != nullptr && animal->player >= 0)
	{
		return static_cast<PlayerNames>(animal->player);
	}
	// a rock, a dead tree: the player it was made for
	if (const auto* statics = registry.TryGet<const MobileStatic>(object))
	{
		return statics->owner;
	}
	if (const auto* dead = registry.TryGet<const DeadTree>(object))
	{
		return dead->owner;
	}
	// a spell icon: a town's is its town's player's, no one's once it has no town; a site's is its site's player's (kept
	// on the icon), the neutral player's without a site
	if (const auto* icon = registry.TryGet<const SpellIcon>(object))
	{
		if (const auto* ofTown = registry.TryGet<const TownCentreSpellIcon>(object); ofTown != nullptr)
		{
			const bool hasTown = ofTown->town != entt::null && registry.Valid(ofTown->town);
			return hasTown ? std::optional<PlayerNames> {icon->player} : std::nullopt;
		}
		if (const auto* ofSite = registry.TryGet<const WorshipSpellIcon>(object); ofSite != nullptr)
		{
			const bool hasSite = ofSite->site != entt::null && registry.Valid(ofSite->site);
			return hasSite ? icon->player : PlayerNames::NEUTRAL;
		}
		return icon->player;
	}
	return PlayerNames::NEUTRAL;
}

entt::entity effects::ActualObjectToEffect(entt::entity object, bool beam)
{
	if (!Locator::templeDestructionSystem::has_value())
	{
		return object;
	}
	auto& temples = Locator::templeDestructionSystem::value();
	if (Locator::entitiesRegistry::value().AllOf<CitadelHeart>(object))
	{
		return temples.ActualObjectToEffect(object, beam);
	}
	const auto heart = openblack::worship::citadel::PartHeart(object);
	if (heart == entt::null)
	{
		return object;
	}
	return openblack::physics::temple_heart::PartTaker(temples.ActualObjectToEffect(heart, beam), heart, object,
	                                                   life::LifeOf(object));
}

namespace
{
/// A kill by an effect, with the effect's player and the life it had: a villager -> VillagerDead(2 SPELL)
/// (ECS/Villager/VillagerDeath.h); an animal -> dying (ECS/AnimalAI); a creature that can die faints, one that can't
/// gets all its life back; a temple's heart starts its destruction, from nothing again if it was under way, and stays;
/// a building (any abode but a field, whose life an effect never takes) leaves its ghost and goes, or is built again
/// by its site's scaffold (abodes::DestroyedByEffect); a fish farm is deleted, its fishermen sent away first. TODO: the
/// other classes.
void DestroyedByEffect(entt::entity object, const EffectValues& values, float lifeBefore)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<Villager>(object))
	{
		villager::DestroyedByEffect(object, values.player, lifeBefore);
	}
	else if (registry.AllOf<Animal>(object))
	{
		ecs::animal_ai::DestroyedByEffect(object);
	}
	else if (const auto* creature = std::as_const(registry).TryGet<const Creature>(object))
	{
		if (!creature->canDie)
		{
			life::SetLife(object, 1.0f);
		}
		else if (Locator::creatureFightSystem::has_value())
		{
			Locator::creatureFightSystem::value().ForceFaint(object);
		}
	}
	else if (registry.AllOf<CitadelHeart>(object))
	{
		if (Locator::templeDestructionSystem::has_value())
		{
			Locator::templeDestructionSystem::value().Start(object);
		}
	}
	else if (registry.AllOf<Abode>(object) && !fields::IsField(object))
	{
		abodes::DestroyedByEffect(object);
	}
	else if (registry.AllOf<FishFarm>(object))
	{
		ecs::ToBeDeleted(object);
	}
}

/// Whose thing it is (HarmedPlayerOf) once its destruction has deleted it: a building has left its town by then, so it
/// is the neutral player's; anything else keeps its player
std::optional<PlayerNames> HarmedPlayerOnceGone(entt::entity object)
{
	if (Locator::entitiesRegistry::value().AllOf<Abode>(object))
	{
		return PlayerNames::NEUTRAL;
	}
	return HarmedPlayerOf(object);
}

/// Whether the effect comes from the creature's own player: applied by that player itself, not by an object of its, nor
/// by a spell whose caster has gone
bool AppliedByItsPlayer(entt::entity creature, const EffectValues& values)
{
	const auto& body = std::as_const(Locator::entitiesRegistry::value()).Get<const Creature>(creature);
	return values.appliedByPlayer && values.player.has_value() && *values.player == body.owner;
}

/// Whether the creature has an opponent: from the start of its duel until its fight has ended for it. Before the duel
/// (walking up, taunting, waiting) and after it (showing off, answering the end) it has none
bool HasOpponent(entt::entity creature)
{
	const auto* fighting = std::as_const(Locator::entitiesRegistry::value()).TryGet<const CreatureFighting>(creature);
	return fighting != nullptr && fighting->stage == CreatureFighting::Stage::Duel && !fighting->ended;
}
} // namespace

EffectValues EffectValues::FromEffectInfo(const GEffectInfo& info)
{
	EffectValues values;
	values.numbers = {info.effectBurn,
	                  info.effectCrush,
	                  info.effectHit,
	                  info.effectHeal,
	                  info.effectFlyAway,
	                  info.effectAlignmentModification,
	                  info.effectBeliefModification};
	values.radius = info.radius;
	return values;
}

void EffectValues::Scale(float factor)
{
	if (factor == 1.0f)
	{
		return;
	}
	for (auto& number : numbers)
	{
		number *= factor;
	}
}

bool EffectValues::IsDestructive() const
{
	return numbers[static_cast<size_t>(Number::Burn)] > 0.0f || numbers[static_cast<size_t>(Number::Crush)] > 0.0f ||
	       numbers[static_cast<size_t>(Number::Hit)] > 0.0f || numbers[static_cast<size_t>(Number::FlyAway)] > 0.0f;
}

bool effects::IsEffectReceiver(entt::entity object, const EffectValues& values)
{
	// A village totem, its plinth or its icon, takes no effect, nor does a town desire flag, a teleport stone, a spell
	// seed, a one-off seed, a worship site's spell icon or totem, a landscape vortex or a script's highlight
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (registry.Valid(object) && registry.AnyOf<VillageTotem, TownDesireFlag, TeleportStone, SpellSeed, OneOffSpellSeed,
	                                             WorshipSpellIcon, WorshipTotem, LandscapeVortex, ScriptHighlight>(object))
	{
		return false;
	}
	// a spiritual shield takes none; a physical shield takes one that does not burn
	if (const auto* shield = registry.Valid(object) ? registry.TryGet<const MagicShield>(object) : nullptr; shield != nullptr)
	{
		return shield->kind == MagicShield::Kind::Physical &&
		       values.numbers[static_cast<size_t>(EffectValues::Number::Burn)] == 0.0f;
	}
	// a field takes a burning effect only while a hand could take from it (grown, with food in it)
	if (const auto* field = registry.Valid(object) ? registry.TryGet<const Field>(object) : nullptr; field != nullptr)
	{
		return hand_grab::FieldValidForPlaceInHand(field->growth, field->food) ||
		       !(values.numbers[static_cast<size_t>(EffectValues::Number::Burn)] > 0.0f);
	}
	// a pot or a pile only while it holds something
	if (const auto* pot = registry.Valid(object) ? registry.TryGet<const Pot>(object) : nullptr; pot != nullptr)
	{
		if (pot->amount == 0)
		{
			return false;
		}
	}
	// nor does a totem building
	if (registry.Valid(object) && registry.AllOf<Abode>(object) && abodes::TypeOf(object) == AbodeType::Totem)
	{
		return false;
	}
	bool totemIcon = false;
	registry.Each<const VillageTotem>([object, &totemIcon](entt::entity /*plinth*/, const VillageTotem& totem) {
		totemIcon = totemIcon || totem.icon == object;
	});
	if (totemIcon)
	{
		return false;
	}
	// A villager takes a heal while it is not dead; any other effect only while it can be reached (available, not at
	// home, not in a hand, not hiding in a building)
	if (registry.Valid(object) && registry.AllOf<Villager>(object))
	{
		if (values.numbers[static_cast<size_t>(EffectValues::Number::Heal)] > 0.0f)
		{
			return !villager::IsDead(object);
		}
		return villager::IsReachable(object);
	}
	// Any other object: whether it is available
	return ecs::IsAvailable(object);
}

std::optional<PlayerNames> effects::EffectPlayer(bool hasApplier, std::optional<PlayerNames> applierPlayer,
                                                 std::optional<PlayerNames> causedPlayer)
{
	// an applier answers for itself, even with no player; the caused player only stands in when nothing applied it
	if (hasApplier)
	{
		return applierPlayer;
	}
	return causedPlayer.value_or(PlayerNames::NEUTRAL);
}

float effects::ConvertTemperatureToDamage(entt::entity object, float temperature)
{
	const auto* info = fire::traits::InfoOf(object);
	if (info == nullptr)
	{
		return 0.0f;
	}
	// the combustion temperature
	const float combustion = info->combustionTemperature;
	if (temperature < combustion)
	{
		return 0.0f;
	}
	return (temperature - combustion) / combustion * info->defenceMultiplierBurn * 0.1f;
}

float effects::CreatureDefenceDivisor(float size)
{
	// a size below the least, or one that compares with nothing, is the least; above the most, the most
	float kept = size;
	if (!(kept >= k_CreatureDefenceLeastSize))
	{
		kept = k_CreatureDefenceLeastSize;
	}
	else if (kept > k_CreatureDefenceMostSize)
	{
		kept = k_CreatureDefenceMostSize;
	}
	return kept * k_CreatureDefencePerSize + 1.0f;
}

std::array<float, static_cast<size_t>(EffectValues::Number::_COUNT)> effects::GetDefenseMultiplier(entt::entity object)
{
	return DefenceMultipliers(object);
}

namespace
{
/// An object taking an effect, as every class but the creature does
float ObjectApplyEffect(entt::entity object, EffectValues& values)
{
	const float life0 = life::LifeOf(object);
	// every abode class (houses, storage pit, town centre, creche, workshop ...)
	const bool isAbode = Locator::entitiesRegistry::value().AllOf<Abode>(object);
	const float damage = DamageEffect(object, values);
	const float heal = HealEffect(object, values);
	float result = 0.0f;
	if (heal > 0.0f)
	{
		result = (1.0f - life0) / heal;
		// an abode's own increase (ecs::abodes: RestartBeingFunctional when it crosses the threshold); the generic one for
		// the rest
		if (isAbode)
		{
			abodes::IncreaseLife(object, heal);
		}
		else
		{
			life::IncreaseLife(object, heal);
		}
	}
	if (damage > 0.0f)
	{
		result += life0 / damage;
		// With the effect's player: an abode's own reduction (ecs::abodes: the multi-map fixed part, the building site,
		// StopBeingFunctional, the town's emergency); a field's (no change, no site), as fields carry an Abode too:
		// abodes::ReduceLife handles both; the generic one for the rest, a creature's too (its body's life). TODO: a
		// creature with an opponent loses its fight health as well
		if (isAbode)
		{
			abodes::ReduceLife(object, damage, values.player);
		}
		else
		{
			life::ReduceLife(object, damage);
		}
	}
	auto& registry = Locator::entitiesRegistry::value();
	constexpr auto k_Burn = static_cast<size_t>(EffectValues::Number::Burn);
	const float burn = values.numbers[k_Burn];
	// the harm: the burn converted to damage + the damage
	const float harm = ConvertTemperatureToDamage(object, burn) + damage;
	// A life brought from some to none is destroyed by the effect, before anything else is done with it, with the
	// effect's player and the life it had. Everything after it sees the thing as its destruction left it. A thing the
	// destruction deletes at once (a building) is read before it goes: its life then (none), its alignment type and
	// whose it is once out of its town. TODO: an applier that is a creature counts one more object destroyed;
	// openblack's creatures keep no such count yet
	std::optional<alignment::EffectTarget> gone;
	std::optional<PlayerNames> goneHarmed;
	// a fish farm keeps its town as it goes, so the town is still attacked after the farm's deletion: its town and
	// its kind's aggressor value are read before
	entt::entity goneTown = entt::null;
	float goneAggressorValue = 0.0f;
	if (ecs::IsAvailable(object) && life::LifeOf(object) == 0.0f && life0 != 0.0f)
	{
		gone = alignment::TargetOf(object, values);
		goneHarmed = HarmedPlayerOnceGone(object);
		if (registry.AllOf<FishFarm>(object))
		{
			goneTown = fish_farms::TownOf(object);
			const auto* info = fire::traits::InfoOf(object);
			goneAggressorValue = info != nullptr ? info->aggressorValue : 0.0f;
		}
		DestroyedByEffect(object, values, life0);
	}
	const bool deleted = !registry.Valid(object);
	// Crushed: REACT_TO_OBJECT_CRUSHED from the applier (or the object), if the object started none. Every living thing
	// can be crushed (villagers, animals and creatures), nothing else: none is deleted by its destruction. Its player is
	// read after the destruction: a villager killed has left its town
	if (!deleted && values.numbers[static_cast<size_t>(EffectValues::Number::Crush)] > 0.01f &&
	    registry.AnyOf<Villager, Animal, Creature>(object) && reactions::GetReactionInitiatedBy(object) == 0)
	{
		const auto initiator = values.appliedBy != entt::null ? values.appliedBy : object;
		// a crushed reaction for the object's player
		reactions::CreateReaction(initiator, Reaction::ReactToObjectCrushed, HarmedPlayerOf(object), true);
	}
	// A destructive effect on an object of a town, with an applier and a non-zero harm attacks the town with the caused
	// player: the harm times the object's aggressor value (town_emergency::AttackTown). A thing deleted by its
	// destruction has no town left (a building leaves its town as it goes), but for a fish farm, which keeps its own
	if (!deleted && values.IsDestructive() && values.appliedBy != entt::null && registry.Valid(values.appliedBy) &&
	    harm != 0.0f)
	{
		const auto caused = values.causedPlayer.has_value() ? values.causedPlayer : values.player;
		// the effect's amounts: the town attack sound reads them, and the town keeps their burn, crush and hit as the
		// aggressor's last attack
		town_emergency::AttackTown(object, harm, caused, values.appliedBy, values.numbers);
	}
	else if (deleted && goneTown != entt::null && registry.Valid(goneTown) && values.IsDestructive() &&
	         values.appliedBy != entt::null && registry.Valid(values.appliedBy) && harm != 0.0f)
	{
		const auto caused = values.causedPlayer.has_value() ? values.causedPlayer : values.player;
		town_emergency::UpdateAggressor(goneTown, caused, town_aggression::AggressionFromDamage(harm, goneAggressorValue),
		                                values.appliedBy, values.numbers);
	}
	// Last, whose alignment moves: the creature's or the caster player's, from the life lost or won, read after the
	// destruction. TODO: the creature's own alignment when a creature applies the effect: today a creature-applied
	// effect moves no alignment
	if (!values.appliedByCreature && values.player.has_value())
	{
		// the player whose thing it is keeps the harm against the caster
		const auto harmed = deleted ? goneHarmed : HarmedPlayerOf(object);
		if (harmed.has_value() && Locator::playerSystem::has_value())
		{
			auto& players = Locator::playerSystem::value();
			auto rivalry = players.Rivalry(*harmed);
			player_rivalry::AddHarm(rivalry, *values.player, harm);
			players.SetRivalry(*harmed, rivalry);
		}
		auto& casterAlignment = magic::players::AlignmentOf(*values.player);
		if (deleted)
		{
			if (gone.has_value())
			{
				alignment::Update(casterAlignment, *gone, values, life0);
			}
		}
		else
		{
			alignment::Update(casterAlignment, object, values, life0);
		}
	}
	return result;
}

/// A heal mends the creature's marks by the heal's counts for each of its strength (truncated)
void MendMarks(entt::entity creature, float heal)
{
	if (heal > 0.0f && Locator::creatureSkinSystem::has_value())
	{
		const auto counts = static_cast<uint32_t>(heal * static_cast<float>(creature_marks::k_HealEffectCounts));
		Locator::creatureSkinSystem::value().Heal(creature, counts);
	}
}

/// A creature taking an effect: the damage's first pass (which gives the fire its burn), no harm from itself, in a fight
/// nothing harmless from another but its own player. In a fight, unless a thing struck it in the physics, the fight's
/// part: blocking, it takes a tenth of the effect (but the harm, worked out before); it reels; the heal mends its marks
/// and, with the harm, goes to its fight health, which can knock it out. Then a heal mends its marks (a second time in
/// a fight), and once out of the fight, as a knock-out leaves it, the object's effect; still in it, its life does not
/// change. Not ported yet: its cuts and scars, its opinion of a creature that applies it
float CreatureApplyEffect(entt::entity creature, EffectValues& values)
{
	const float damage = DamageEffect(creature, values);
	if (damage > 0.0f)
	{
		// harm from itself does nothing
		if (values.appliedBy == creature)
		{
			return 0.0f;
		}
	}
	else if (values.appliedBy != creature && HasOpponent(creature) && !AppliedByItsPlayer(creature, values))
	{
		// in a fight, a harmless effect from anything but itself or its own player does nothing either
		return 0.0f;
	}
	if (HasOpponent(creature) && !values.blow && Locator::creatureFightSystem::has_value())
	{
		auto& fights = Locator::creatureFightSystem::value();
		if (fights.IsBlocking(creature))
		{
			values.Scale(creature_fight::k_BlockedMiracleShare);
		}
		fights.Recoil(creature);
		const float fightHeal = HealEffect(creature, values);
		MendMarks(creature, fightHeal);
		fights.TakeMiracle(creature, fightHeal, damage);
	}
	MendMarks(creature, HealEffect(creature, values));
	// a knock-out has ended the fight for it
	if (HasOpponent(creature))
	{
		return 1.0f;
	}
	return ObjectApplyEffect(creature, values);
}
} // namespace

float effects::ApplyEffect(entt::entity object, EffectValues& values)
{
	if (std::as_const(Locator::entitiesRegistry::value()).AllOf<Creature>(object))
	{
		return CreatureApplyEffect(object, values);
	}
	return ObjectApplyEffect(object, values);
}

entt::entity EffectValues::ApplyEffectToMapPos(const glm::vec3& position)
{
	std::vector<entt::entity> hits;
	ApplyEffectToMapPos(position, hits);
	return hits.empty() ? entt::null : hits.back();
}

void EffectValues::ApplyEffectToMapPos(const glm::vec3& position, std::vector<entt::entity>& hits)
{
	auto& registry = Locator::entitiesRegistry::value();
	// the corners: the metres -/+ radius, back to map coords; the cells are their signed high words and each one is
	// tested in bounds, so a corner off the map does not wrap
	const auto corner = [](float metres, float offset) {
		return static_cast<int32_t>(map_coords::SignedCellOf(map_coords::ToFixedGUtils(map_coords::Quantise(metres) + offset)));
	};
	const glm::ivec2 low(corner(position.x, -radius), corner(position.z, -radius));
	const glm::ivec2 high(corner(position.x, radius), corner(position.z, radius));
	const float altitude = LandAt(position.x, position.z) + position.y;
	for (int32_t x = low.x; x <= high.x; ++x)
	{
		for (int32_t z = low.y; z <= high.y; ++z)
		{
			if (!map_coords::InBounds(glm::ivec2(x, z)))
			{
				continue;
			}
			// the cell's fixed list, then its mobile one (ecs::map_cells). No "done" set and no own-cell test: a multi-cell
			// object is offered the effect once per cell of the square it is in (inferred: unless ApplyEffect limits it
			// itself)
			for (const auto object : map_cells::ObjectsInCell(glm::ivec2(x, z)))
			{
				// available (not being deleted, ecs::IsAvailable; a villager whose final state is not DYING), then an effect
				// receiver
				if (!ecs::IsAvailable(object) || (registry.AllOf<Villager>(object) && !villager::IsAvailable(object)) ||
				    !IsEffectReceiver(object, *this))
				{
					continue;
				}
				const auto* transform = registry.TryGet<const Transform>(object);
				if (transform == nullptr)
				{
					continue;
				}
				// the fire centre (the position; a dead tree's mesh centre) and fire radius (the 2D radius; a dead tree's 0.35
				// x its height), ECS/Fire/FireObjectTraits
				const glm::vec3 centre = fire::traits::FireCentre(object);
				// the distance of the position and that centre against the radius sum
				const float distance = gutils::GetDistanceInMetres(position, centre);
				if (fire::traits::DefaultFireRadius(object) + radius < distance)
				{
					continue;
				}
				// the height + the radius against the altitude difference
				if (object::GetHeight(object) + radius < std::abs(altitude - (LandAt(centre.x, centre.z) + centre.y)))
				{
					continue;
				}
				// what the object hands the effect to takes it: a temple's heart, and a part of its temple through it, give
				// it to a building or a homeless villager of the heart's player, beaming at it for a destructive effect
				const auto taker = ActualObjectToEffect(object, IsDestructive());
				ApplyEffect(taker, *this);
				hits.push_back(taker);
			}
		}
	}
}
