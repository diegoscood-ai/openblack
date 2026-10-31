/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <optional>

#include <entt/entity/entity.hpp>

#include "Audio/Services/Guidance.h"
#include "ECS/TownAggression.h"
#include "Enums.h"

// The town's emergency: a storage pit or a town centre knocked below its functional threshold (abodes::ReduceLife),
// one of them on fire or the town attacked while in the emergency (ProcessTownEmergency) start it; the housed
// villagers are called at once to congregate in the town (242) and the worship stops for its 1200 turns (the town
// info's emergency duration). town_queries::IsInStateOfEmergency tells whether it is running. Fields:
// components::Town::emergencyTurn, aggression.lastTurn, savedWorshipPercentage; the worship percentage through
// worship::percentage.
// The attack refresh reads the town's aggression record (ECS/TownAggression.h), written by UpdateAggressor: a
// destructive effect on one of its things, its things burning, a villager of it hurt by a physics impact and a
// physical shield of it struck.

namespace openblack::ecs::town_emergency
{
/// Each abode of the town, each inhabitant: when its final state's row allows it ->
/// SetTopState(242 GOTO_CONGREGATE_IN_TOWN_AFTER_EMERGENCY). The per-villager part is
/// villager::CallToTownEmergency. Only housed villagers are called
void CallAllVillagersToTownEmergency(entt::entity town);
/// No emergency running -> CallAllVillagersToTownEmergency; then the start = the turn (also when it was set: the start
/// is refreshed, the villagers are not called again)
void SetInStateOfEmergency(entt::entity town);
/// TownProcess step 15: while in the emergency the attack of this turn refreshes it and a worship percentage != 0 is
/// saved and set to 0; else the storage pit or the town centre on fire starts it; else the saved percentage comes back
/// (when the current one is 0) and the saved percentage and the start are cleared
void ProcessTownEmergency(entt::entity town);
/// The town is attacked by the caused player (`causedPlayer`; the neutral player without one) with `aggression`: the
/// town's record takes it (town_aggression::Attacked, with the town info's first-time addition, as the town's own
/// player's harm when the aggressor owns it) and keeps the aggressor and the turn, which ProcessTownEmergency's
/// refresh while attacked reads, and what the attack did (the burn, the crush and the hit of `effects`) as the
/// aggressor's last attack. Before the record takes it, the town attack sound plays (guidance::
/// PlayTownAttackRemark, from TownAttackOf with the attack's seven effect amounts `effects`); after it, the advisors
/// may remark on it (AttackRemarkFor, `appliedBy` the thing that applied the harm, none for a burn)
void UpdateAggressor(entt::entity town, std::optional<PlayerNames> causedPlayer, float aggression,
                     entt::entity appliedBy = entt::null, const std::array<float, 7>& effects = {});
/// An object of a town is harmed by `damage`: its town (town_queries::GetTown; none: nothing) is attacked with the
/// damage times the aggressor value of the object's info (0 when openblack has no info for its kind), and the effect
/// amounts of the harm
void AttackTown(entt::entity object, float damage, std::optional<PlayerNames> causedPlayer, entt::entity appliedBy = entt::null,
                const std::array<float, 7>& effects = {});

/// What the town attack sound reads of an attack: the town's people, position, this turn's want of protection and the
/// aggression it held against each player before this attack; the effect amounts; the kind of the thing that applied
/// it (none: no applier); the caused player as the attack names it (none: no player; not yet the neutral one); the
/// town's owner and the local player. The town is the local player's when its owner is
[[nodiscard]] audio::guidance::TownAttack
TownAttackOf(uint32_t population, glm::vec3 position, float protection, const std::array<float, 8>& aggression,
             const std::array<float, 7>& effects, std::optional<audio::guidance::Attacker> attacker,
             std::optional<PlayerNames> causedPlayer, PlayerNames townOwner, PlayerNames local);
/// The kind of a thing that applied harm, as its attack sample sees it: a creature; a spell, a lightning bolt's or
/// another's; a rock, a fragment, a felled or dead tree or a bonfire; anything else a plain thing. None for no thing
[[nodiscard]] std::optional<audio::guidance::Attacker> AttackerOf(entt::entity appliedBy);

/// A creature that harmed a town, as the advisors' remark sees it
struct AttackingCreature
{
	PlayerNames owner {PlayerNames::NEUTRAL};
	/// It is its owner's own creature, the one they lead
	bool ownersCreature {false};
};
/// The advisors' remark on a town attacked
enum class AttackRemark : uint8_t
{
	None,
	/// The local player is attacking a town (guidance::RemarkAttackingTown)
	AttackingTown,
	/// The local player's creature, not the town player's own, is attacking it (guidance::RemarkCreatureAttacking)
	CreatureAttackingThem,
};
/// Which remark a town attack makes for the local player: harm applied by a creature of the local player that is not
/// the town player's own creature makes CreatureAttackingThem, any other creature's none; else the aggressor (the
/// neutral player without a caused player) being the local player makes AttackingTown
[[nodiscard]] constexpr AttackRemark AttackRemarkFor(PlayerNames aggressor, std::optional<AttackingCreature> creature,
                                                     PlayerNames townOwner, PlayerNames local)
{
	if (creature.has_value())
	{
		const bool townPlayersOwn = creature->owner == townOwner && creature->ownersCreature;
		return creature->owner == local && !townPlayersOwn ? AttackRemark::CreatureAttackingThem : AttackRemark::None;
	}
	return aggressor == local ? AttackRemark::AttackingTown : AttackRemark::None;
}
/// The player a thing that hit something answers for itself, for an attack that no hand threw: a villager its town's
/// owner (none without a town), a creature its owner, a standing tree none, and every other thing that flies in the
/// physics (rocks, fragments, felled trees, mobile objects and statics, animals, pots, seeds) the neutral player
[[nodiscard]] std::optional<PlayerNames> HitterPlayer(entt::entity hitter);
} // namespace openblack::ecs::town_emergency
