/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>

#include "Enums.h"

// How much a miracle impresses a villager who reacts to it. A miracle that impresses gives belief in its caster to
// the watcher's town; a closer watcher is impressed more, along the steep curve of the distance over the reaction's
// reach (gutils::DistanceChangeToBelief), and a town that has seen the same kind of reaction again and again is
// impressed less each time. Each impression counts by the villager's share of its town, so that a small town is
// impressed as much as a large one. Pure rules, tested with made-up values.

namespace openblack::magic
{

/// What goes into how much a miracle impresses one watcher
struct ImpressionInputs
{
	/// The land's balance of impressiveness, which scripts raise on later lands
	float landBalance {1.0f};
	/// The miracle's own impressiveness, from its effect's table
	float impressiveValue {0.0f};
	/// The reaction's multiplier
	float reactionMultiplier {1.0f};
	float distance {0.0f};
	float maxDistance {1.0f};
	/// The miracle's power; every miracle's is 1
	float power {1.0f};
	/// The watcher's town's weariness of this kind of reaction, 1 when it hasn't seen it before
	float boredom {1.0f};
};

/// How much a miracle impresses one watcher. The factors are multiplied in the original's order, which matters for
/// the last bit of a float: the weariness by the power and by the distance's share, times the reaction's multiplier
/// by the miracle's own value and by the land's balance
[[nodiscard]] float ImpressiveValue(const ImpressionInputs& inputs);

/// What one watcher needs to be impressed by an object's reaction
struct ObjectImpressionInputs
{
	/// The land's balance of an object's impressiveness (its second balance), which scripts raise on later lands
	float landBalance {1.0f};
	/// The object's own impressiveness, from its info
	float impressiveValue {0.0f};
	float reactionMultiplier {1.0f};
	float distance {0.0f};
	float maxDistance {1.0f};
	/// The object's power; every object that makes a reaction has 1
	float power {1.0f};
	float boredom {1.0f};
	/// How many times as impressive the object is for being an artefact
	float artefactModifier {1.0f};
};

/// How much an object's reaction (a pot, a thrown thing, a fire, a villager...) impresses one watcher. The factors
/// are multiplied in the original's order, which is not a miracle's: the artefact modifier by the weariness, by the
/// power and by the distance's share, times the land's balance by the object's own value and by the reaction's
/// multiplier
[[nodiscard]] float ObjectImpressiveValue(const ObjectImpressionInputs& inputs);

/// How much a scaffold's reaction impresses one watcher: how many scaffolds it stands for, by its value as an object,
/// by what its site's town wants of the building (1 without a site), in the original's order
[[nodiscard]] float ScaffoldImpressiveValue(uint32_t scaffolds, float objectValue, float sitePart);

struct CreatureImpressionInputs
{
	/// The size the creature is drawn at, and its species' addition to it and multiplier of it for impressiveness
	float drawnSize {1.0f};
	float sizeAddition {0.0f};
	float sizeMultiplier {1.0f};
	float boredom {1.0f};
	/// A creature's power; every creature has 1
	float power {1.0f};
	float distance {0.0f};
	float maxDistance {1.0f};
	/// What it did, to the watcher's town: the reaction's multiplier by how impressive its last action carried out
	/// to its end is, or, when the town has not yet judged it, the town rules' one-off belief for a creature
	float done {0.0f};
	/// The land's balance of an object's impressiveness (its second balance)
	float landBalance {1.0f};
};
/// How much a creature impresses one watcher: its size share (its drawn size with its species' addition, held between
/// 1 and 4, by its species' multiplier), by the weariness, by the power, by the distance's share, by what it did, by
/// the land's balance, in the original's order
[[nodiscard]] float CreatureImpressiveValue(const CreatureImpressionInputs& inputs);
/// What a scaffold's reaction adds to its watcher's town's boredom multiplier for the kind, before the villager's share:
/// the town tires of a scaffold at once, where everything else takes the belief rules' small default
constexpr float k_ScaffoldBoredomUpdate = -1.0f;

/// The multiplier a reaction impresses one watcher by. A reaction to food or wood impresses by how much the watcher's
/// town wants what its table names (its desire, plus the boosts the game and the scripts give it), or by exactly 1
/// when the watcher has no town or the table names nothing. Every other reaction impresses by its table's multiplier.
[[nodiscard]] float ReactionMultiplier(Reaction type, float tableMultiplier, std::optional<float> townDesire);

/// A shield's impression on a villager of a town. While the shield stands or is struck, it impresses nothing the
/// people of a town whose last attacker is the shield's own player; as it is destroyed it impresses four times as
/// much as another miracle would; otherwise as much
inline constexpr float k_ShieldDestroyedImpressiveness = 4.0f;
[[nodiscard]] float ShieldImpressiveValue(Reaction type, bool townAttackedByShieldPlayer, float spellValue);

/// The added to the town's population and to the population whose impressions count unchanged, so that an empty town
/// divides by no zero
inline constexpr float k_TownShareSmall = 0.001f;
/// A villager's share of its town's impression: the population whose impressions count unchanged over the town's
/// people (adults and children), so a smaller town's people count for more each
[[nodiscard]] float TownShare(float populationForUnmodifiedBelief, uint32_t people);

/// How much a creature's size impresses: its size plus its species' addition, held between 1 and 4, times its
/// species' multiplier
[[nodiscard]] float CreatureImpressiveSize(float size, float addition, float multiplier);

/// How much an impression moves the alignment of the player whose reaction it is, before the alignment's own
/// weighing: the kind's own amount, times how much the town wants what it shows when the kind looks to a desire
[[nodiscard]] float ImpressionAlignment(float alignmentModifier, std::optional<float> townRawDesire);

} // namespace openblack::magic
