/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand's part of tying the leash: the Action button's double click, and the tap with the leash in the hand on a
// thing or on the land. The leash decides and sends its packets (ecs::leash_tie). Wiki:
// docs/wiki/creature/leash.md#tying-the-leash-with-the-hand.

#define LOCATOR_IMPLEMENTATIONS

#include "ECS/HandLeash.h"
#include "ECS/LeashHandTie.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "Hand/HandClickRules.h"
#include "HandSystem.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
/// Whether the thing is a reward. openblack has no rewards yet
bool IsReward(entt::entity)
{
	return false;
}

/// The player's leash is on and not tied to anything
bool LeashOnUntied()
{
	return Locator::leashSystem::has_value() &&
	       ecs::leash_tie::CheckTap(Locator::leashSystem::value(), PlayerNames::PLAYER_ONE).leashOnUntied;
}
} // namespace

bool HandSystem::Tap(entt::entity object) noexcept
{
	// the tap's own start: the memory of the last thing tapped, whatever happens next
	const bool reward = IsReward(object);
	if (hand_click::MarksThing(reward && LeashOnUntied(), reward))
	{
		RememberTapped(object);
	}
	// the leash's act on the thing takes the tap: the press ends and the action state goes back to idle, which every
	// caller's state already does as it taps
	if (ecs::leash_tie::OnHandTap(PlayerNames::PLAYER_ONE, object))
	{
		return true;
	}
	return SendTap(object);
}

bool HandSystem::LeashIsOnUntied() const noexcept
{
	return LeashOnUntied();
}

void HandSystem::TapLand(const glm::vec3& point) noexcept
{
	// ActOnPoint: the press is cleared and the action state kept, as the press on the land leaves it already. Nothing
	// (the origin): nothing at all. NotLeash: (not ported) the hand's own click on the land
	(void)ecs::leash_tie::OnHandTap(PlayerNames::PLAYER_ONE, point);
}

void HandSystem::LeashDoubleClick() noexcept
{
	// the double click ends what the press started: the demo mouse's buttons go out
	_litEnded = true;
	// the thing under the hand, else the nearest within 5 of the action's point (the map's corner when it is cleared)
	std::optional<entt::entity> underHand = _hovered;
	if (!underHand && _cursorObject && ecs::IsAvailable(*_cursorObject) && *_cursorObject != _hands[0] &&
	    *_cursorObject != _hands[1])
	{
		underHand = _cursorObject;
	}
	const auto target = ecs::hand_leash::TieTarget(underHand, [this]() -> std::optional<entt::entity> {
		return FindObjectNearMapCoord(_interactionPoint.value_or(glm::vec3(0.0f)));
	});
	// a one-off spell seed is tapped through the tap packet alone: not remembered, and not the leash's act
	if (ecs::leash_tie::OnHandDoubleClick(PlayerNames::PLAYER_ONE, target) == creature_leash::HandTie::Tap)
	{
		(void)SendTap(target);
	}
	// whatever the result, the press ends and the action state is reset: the idle hand stays idle (Update drops the
	// press)
}
