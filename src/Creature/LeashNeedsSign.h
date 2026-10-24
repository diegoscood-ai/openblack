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

/// A leash tied to one of a village totem's needs signs. A totem shows three signs, how much its village wants food,
/// rest, and how many of its people are hiding. Tied to one of them, the creature is set to help the village's worship
/// site: once as it is tied, and again every turn while the sign is more than 0.6 full. When it is next free to choose,
/// it casts the food miracle for the worshippers if it knows it, or else fishes for them if it knows how. Whichever
/// sign it is tied to, it is the same help.
///
/// Only the rules are here: openblack does not make the totem's signs yet, nor can the creature's mind keep an action
/// for when it is next free, so nothing calls them.
namespace openblack::creature_leash
{

/// Which needs sign: the totem's three, in their order, or a workshop's own, which is never the totem's
enum class NeedsSignRow : uint8_t
{
	Food,
	Rest,
	PeopleHiding,
	Workshop,
};

/// A needs sign the leash is tied to
struct NeedsSign
{
	NeedsSignRow row {NeedsSignRow::Workshop};
	/// The totem it belongs to stands at a worship site
	bool totemHasWorshipSite {false};
};

/// Whether the sign is one of a worship totem's, its totem at a worship site; a workshop's sign never is
[[nodiscard]] bool NeedsSignQualifies(const NeedsSign& sign);

/// How full the sign is: its desire over the most it can show, at most 1
[[nodiscard]] float NeedsSignFill(float desire, float maxNeed);

/// More than this full, the creature tied to it is set to help again every turn
constexpr double k_NeedsSignWants = 0.6;
/// At the tie, a sign that qualifies sets the creature to help, however full it is
[[nodiscard]] bool NeedsSignArmsAtTie(const NeedsSign& sign);
/// Every turn while tied, a sign that qualifies and is more than 0.6 full sets it to help again
[[nodiscard]] bool NeedsSignArmsThisTurn(const NeedsSign& sign, float fill);

/// The actions it helps with, by their rows in the game's action table: the food miracle cast for the worshippers, and
/// fish hauled to the worship site
constexpr uint32_t k_FoodForWorshipAction = 246;
constexpr uint32_t k_FishForWorshipAction = 300;
/// What the creature does when it is next free: the food miracle if it knows it, else fishing if it knows it, else
/// nothing
[[nodiscard]] std::optional<uint32_t> NeedsSignAction(bool knowsFoodMiracle, bool knowsFishing);

} // namespace openblack::creature_leash
