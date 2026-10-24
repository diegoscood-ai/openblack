/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "ECS/Components/Alignment.h"
#include "Enums.h"

// The players' alignment, good (+1) to evil (-1): what applied effects and trees add to the change pending this turn,
// and the player's turn that folds it in. The one API for it. Wiki:
// docs/wiki/magic/influence-and-alignment.md#player-alignment, docs/wiki/world/trees.md#alignment.

namespace openblack
{
struct GPlayerInfo;
namespace v120
{
struct InfoConstants;
}
using InfoConstants = v120::InfoConstants;
}

namespace openblack::ecs::effects
{
struct EffectValues;

namespace alignment
{
/// A change v scaled by the current alignment A: v of A's sign (0 counts as positive) -> v (1 - |A| / 2),
/// of the opposite sign -> v (1 + |A| / 2)
[[nodiscard]] float ScaleChange(const components::Alignment& alignment, float change);

/// The alignment info's row `effect` (0 burn .. 4 fly away), column `type`; 0 past the table's rows
[[nodiscard]] float AlignmentFactor(const InfoConstants& info, size_t effect, AlignmentType type);

/// One turn's fold of a pending change: the value moves by changePerTurn x the pending change clamped to -1..1, the
/// result is clamped to -1..1, and the pending change goes back to 0
[[nodiscard]] float StepPending(float value, float& pending, float changePerTurn);

/// The change of an applied effect (object, values, life before): nothing unless the life changed. With
/// K = |life0 - life| + GPlayerInfo.applyEffectAlignmentChangeAddition (player 0's) and col = the object's info
/// alignmentType, `pending` gets ScaleChange(values[i] x alignmentInfo[i][col] x K) for crush, hit, heal and fly away,
/// and ScaleChange(ConvertTemperatureToDamage(burn) x alignmentInfo[0][col] x K). The alignment history is not kept.
void Update(components::Alignment& alignment, entt::entity object, const EffectValues& values, float lifeBefore);
/// What that change reads of the thing the effect reached: its life after the effect, its info's alignment type
/// (Unimportant without an info) and the effect's burn converted to damage to it (ConvertTemperatureToDamage)
struct EffectTarget
{
	float life {0.0f};
	AlignmentType type {AlignmentType::Unimportant};
	float burnDamage {0.0f};
};
/// The thing's EffectTarget as it now stands
[[nodiscard]] EffectTarget TargetOf(entt::entity object, const EffectValues& values);
/// The same change from what was read of the thing: a thing its destruction has deleted is read before it goes, with
/// the life its destruction left it
void Update(components::Alignment& alignment, const EffectTarget& target, const EffectValues& values, float lifeBefore);

/// The player's alignment: the player entity's components::Alignment (Magic/Core/Players)
[[nodiscard]] components::Alignment& Of(PlayerNames player);
/// The alignment value, -1..1 (0 for a new game: the game's init takes the profile's)
[[nodiscard]] float Get(PlayerNames player);
/// The alignment = the value clamped to -1..1
void SetClamped(PlayerNames player, float value);
/// The alignment += the change, clamped to -1..1 at once (SET_ALIGNMENT, the network packets)
void AddClamped(PlayerNames player, float change);
/// The change of a tree: +-GPlayerInfo::treePullPutAlignmentChange, ScaleChange-d, into `pending`. Uprooting with
/// the hand is evil (the tree put in the hand), planting good (the tree's EndPhysics). TODO: the alignment history.
void UpdateForTree(PlayerNames player, bool good);
/// The change of resources given to or taken from an abode (n, delta): k = n > 0 ? the abode's town info
/// giveResourceAligmnetChangeMultiplier (0.5) : take (0.5), `pending` += ScaleChange(delta x k). The alignment
/// history, the only reader of the type, is not kept, as everywhere here: no type argument
void UpdateForResource(PlayerNames player, entt::entity abode, int32_t amount, float change);
/// The change of a villager impressed by one of the player's miracles (the reaction's own amount, times the town's
/// want where the reaction looks to one): ScaleChange-d, into `pending`. The alignment history is not kept
void UpdateForImpression(PlayerNames player, float change);
/// The change of a disciple made by the player's hand (disciple::AlignmentForMaking): ScaleChange-d, into `pending`. The
/// alignment history is not kept
void UpdateForDisciple(PlayerNames player, float change);
/// The change of a death with reason r: v = GPlayerInfo dealthReason[r]; a child v + v; an animal v x 0.5. No
/// ScaleChange
[[nodiscard]] float DeathAlignmentChange(const GPlayerInfo& info, DeathReason reason, bool child, bool animal);
/// A villager's death, on the owner's alignment: `pending` += DeathAlignmentChange, no clamp here (ProcessForPlayer
/// folds it in)
void UpdateForDeath(PlayerNames owner, DeathReason reason, bool child, bool animal);
/// The pending change clamped to -1..1, times
/// GPlayerInfo::maxAlignmentChangePerGameTurn, goes through AddClamped and the pending change goes back to 0
void ProcessForPlayer(PlayerNames player);
/// Once per turn for every player (Magic/MagicLoop.cpp, slot 3, the players' process)
void ProcessPlayers();

/// Once per turn for every creature, at the end of the creature systems (ECS/CreatureLoop.cpp): the plan it carries out
/// adds its change to the creature's pending alignment (creature_deed_alignment::TurnChange, with its species' balance
/// scale), then StepPending folds it in by its species' alignment change per turn. Nothing without the info constants.
/// Wiki: docs/wiki/magic/influence-and-alignment.md#creature-alignment
void ProcessCreatures();

/// The most influential player: the first player (in player order) whose CalculatePlayerInfluence(pos, player, 0,
/// type 0, allies 1) is above every earlier one and above 0; the neutral player when none is. Players that do not exist
/// are skipped.
[[nodiscard]] PlayerNames MostInfluentialPlayer(const glm::vec3& position);
/// The land's own alignment at a point. Every existing player (in player order) adds CalculatePlayerInfluence(pos,
/// player, 0, type 0, allies 1) x the player's alignment value, and the sum is clamped to -1..1. Trees grow faster on
/// good land (the tree's process).
[[nodiscard]] float LandAlignmentAt(const glm::vec3& position);
/// The sky's input: x = clamp((alignment of the most influential player at the interface's position + 1) / 2, 0, 1)
/// for the -1 evil .. 1 good alignment. It is worked out once a turn at the end of the players' process; the
/// interface's position is the camera's position (the interface status takes the camera forward as focus - position).
/// 0.5 until the first turn (the sky starts neutral); the multiplayer citadel forces 0.5 (no multiplayer in
/// openblack).
[[nodiscard]] float GetInterfaceAlignment();
/// The interface alignment now (MagicLoop slot 3, after ProcessPlayers)
void UpdateInterfaceAlignment();
/// The interface alignment of a point (the same formula for any position; tests)
[[nodiscard]] float InterfaceAlignmentAt(const glm::vec3& position);
/// Back to 0.5 (tests; a new land does not reset it: opening the land only reads it, and the next turn writes it
/// again)
void ResetInterfaceAlignment();
} // namespace alignment
} // namespace openblack::ecs::effects
