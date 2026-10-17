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

#include <array>
#include <optional>

#include <entt/entity/entity.hpp>

#include "Enums.h"
#include "Help/ToolTips.h"

// The decisions of the hand's tooltips (HandSystem::SubmitToolTips) that need no game state: what a branch submits,
// from what the caller read off the object. Wiki: docs/wiki/interface/tooltips.md#tooltips.
namespace openblack::ecs::systems::hand_tooltips
{

/// The BINDABLE_ACTION whose button the tooltip's icon shows: none, the left button, the right (action) button
constexpr int32_t k_ActionNone = -1;
constexpr int32_t k_ActionLeft = 1;
constexpr int32_t k_ActionRight = 2;

/// One submit: help::tooltips::Submit(text, action, align, force, numbers)
struct ToolTip
{
	uint32_t text {0};
	int32_t action {k_ActionNone};
	uint32_t align {0};
	bool force {false};
	help::tooltips::Numbers numbers {};
};

/// What a branch shows: nothing, the land's tooltips, or its own tooltip
enum class Shows
{
	Nothing,
	Land,
	Own,
};

struct Choice
{
	Shows shows {Shows::Nothing};
	ToolTip tip {}; ///< With Shows::Own
};

/// A seed in the hand: out of the influence the land's tooltips; with no seed nothing. Over an object, the seed valid
/// to apply to it gives 0xE81 "Cast" on the action button, else the land's (the land under it is not tried). Over the
/// land, the seed valid to apply there gives 0xEF1 for the storms, the tornado and the shields, 0xE81 for any other
/// magic, else the land's
[[nodiscard]] Choice SeedToolTip(bool inInfluence, bool hasSeed, bool hasTarget, bool validToObject, bool validToMap,
                                 MagicType magic);

/// Over a one-shot orb that can be picked up: only its own pick-up text, with no button, forced
[[nodiscard]] ToolTip OrbPickUpToolTip(uint32_t text);

/// Over a spell icon in the influence: its seed's tooltip, with no button, forced (a seed with no tooltip shows nothing,
/// as help::tooltips::Submit drops a text before k_First)
[[nodiscard]] ToolTip SpellIconToolTip(uint32_t seedToolTip);

/// Over a workshop in the influence: only a built one shows 0xEF3, with no button and no numbers, so its text shows
/// as it is written, with its "%3.0f%%"
[[nodiscard]] std::optional<ToolTip> WorkshopToolTip(bool built);

/// Over a storage pit: only a built one shows 0xEF9 "Food Stored: %3.0f Wood: %3.0f", with no button, its food (as
/// a float) then its wood (the whole number as it is)
[[nodiscard]] std::optional<ToolTip> StoragePitToolTip(bool built, uint32_t food, uint32_t wood);

/// Over a town desire flag that shows: 0xE88 "Need" in the flag's own words (0xECE on, one for each desire), with no
/// button, its town's raw desire in hundredths, never more than 100; none for a desire without its own words
[[nodiscard]] std::optional<ToolTip> TownDesireFlagToolTip(TownDesireInfo desire, float rawDesire);

/// The town centre whose town's texts an object shows: a town centre its own; a village totem its town centre's
/// (entt::null when it has none); nullopt for any other object
[[nodiscard]] std::optional<entt::entity> TownCentreForToolTips(entt::entity object, bool isTownCentre, bool isTotem,
                                                                entt::entity totemsTownCentre);

/// Over a town centre: 0xECC with the lead in believers (0xEE0 "Believers Needed" when it is below 0), then 0xEFC
/// with the population and "/" the places, both with no button
[[nodiscard]] std::array<ToolTip, 2> TownToolTips(float lead, uint32_t population, uint32_t places);

/// Over a creature that can be interacted with: 0xE85 "Interact", with no button
[[nodiscard]] ToolTip CreatureInteractToolTip();

/// What the interface knows of the object under an empty hand
struct ObjectFacts
{
	bool interactable {false};      ///< Available (an abode: partly built)
	bool inInfluence {false};       ///< The hand is in the player's influence
	bool mustBeInInfluence {false}; ///< Interaction with it needs the influence (every object but a script highlight)
	bool validForLockedSelect {false};
	bool validForPlaceInHand {false};
	bool cannotBePickedUp {false};
};

/// The hand state over an object, with nothing in the hand, in the original's order: not interactable 3; out of the
/// influence when it needs it 18; valid for a locked select 13; valid to place in the hand and not cannot-be-picked-up
/// 9; else 18. (not ported) the locked apply's 15 / 16 between 13 and 9
[[nodiscard]] int32_t ObjectHandState(const ObjectFacts& facts);

} // namespace openblack::ecs::systems::hand_tooltips
