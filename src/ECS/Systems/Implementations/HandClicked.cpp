/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The interface's memory of the last thing tapped or clicked, the one slot that GAME_THING_CLICKED and POSITION_CLICKED
// read: the object with its turn, the land tap with its turn. It is the hand's HandClicked component, and the rules
// are hand_click's. See docs/wiki/hand/picking.md#tap-memory.

#define LOCATOR_IMPLEMENTATIONS

#include "ECS/Components/HandClicked.h"

#include "ECS/Registry.h"
#include "GameClock.h"
#include "Hand/HandClickRules.h"
#include "HandSystem.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using openblack::ecs::components::HandClicked;

HandClicked& HandSystem::Clicked() noexcept
{
	return Locator::entitiesRegistry::value().Get<HandClicked>(_hands[static_cast<size_t>(Side::Left)]);
}

entt::entity HandSystem::GetClickedObject() const noexcept
{
	// null when there is no hand yet, nothing is stored or the object no longer exists
	const auto& registry = Locator::entitiesRegistry::value();
	const auto hand = _hands[static_cast<size_t>(Side::Left)];
	const auto* clicked = hand != entt::null && registry.Valid(hand) ? registry.TryGet<HandClicked>(hand) : nullptr;
	if (clicked == nullptr || clicked->thing == entt::null || !registry.Valid(clicked->thing))
	{
		return entt::null;
	}
	return clicked->thing;
}

void HandSystem::RememberTapped(entt::entity object) noexcept
{
	// the object (null clears it) and the game turn
	hand_click::ClickThing(Clicked(), object, game_clock::Turn());
}

void HandSystem::UpdateTapMemory(bool actionReleased) noexcept
{
	// (before the action states) the action button released in action state 0 with nothing in the hand and the game
	// not paused remembers the collided object, else the land point with its turn. (not ported) a Reward under the
	// leash is not remembered. Otherwise the two 15 s expiries
	auto& clicked = Clicked();
	const bool idle = !_held && !_tug && !_pickSource && !_releaseArmed && !_pendingPick && !_gripPoint && !_leashPull;
	if (actionReleased && idle && !game_clock::IsPaused())
	{
		const auto object = _hovered ? *_hovered : (_cursorObject ? *_cursorObject : entt::null);
		const auto under = object != entt::null ? std::optional(object) : std::nullopt;
		const auto place = _interactionPoint.has_value()
		                       ? std::optional(map_coords::FromMetres({_interactionPoint->x, _interactionPoint->z}))
		                       : std::nullopt;
		hand_click::ClickReleased(clicked, under, true, place, game_clock::Turn());
		return;
	}
	hand_click::Forget(clicked, game_clock::Turn(), game_clock::MsPerTurn());
}
