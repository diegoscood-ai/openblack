/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The interface's memory of the last thing tapped or clicked, the one slot that GAME_THING_CLICKED and POSITION_CLICKED
// read: GInterface +0x45C (a BaseInfo) with its turn +0x468, the land tap +0x46C with its turn +0x478; RememberTapped
// fn_005D36D0 and fn_005D3700. Research: dev\documentacion\hand\clicked\README.md.

#define LOCATOR_IMPLEMENTATIONS

#include "HandSystem.h"

#include "ECS/GUtilsDistance.h"
#include "ECS/Registry.h"
#include "GameClock.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
/// fn_005D3700: (turn - stored turn) x msPerTurn [0xD01A38] x 0.001 > 15.0 s of game time
bool Expired(uint32_t storedTurn)
{
	const float seconds = static_cast<float>(game_clock::Turn() - storedTurn) * game_clock::k_TurnSeconds;
	return seconds > 15.0f;
}
} // namespace

entt::entity HandSystem::GetClickedObject() const noexcept
{
	// BaseInfo GetBase 0x436B80: null when nothing is stored or the object no longer exists
	if (_clickedObject == entt::null || !Locator::entitiesRegistry::value().Valid(_clickedObject))
	{
		return entt::null;
	}
	return _clickedObject;
}

void HandSystem::ClearClicked() noexcept
{
	// +0x460 = +0x464 = 0 (GameThingClicked 0x70AF84, ClearClickedObject 0x70B0E0); the turn +0x468 stays
	_clickedObject = entt::null;
}

void HandSystem::RememberTapped(entt::entity object) noexcept
{
	// fn_005D36D0: BaseInfo::Set(+0x45C, obj) 0x436BB0 (null clears it) and +0x468 = the game turn
	_clickedObject = object;
	_clickedTurn = game_clock::Turn();
}

bool HandSystem::PositionClicked(const glm::vec3& position, float radius) const noexcept
{
	// fn_005D0460: GUtils::GetDistanceInMetres 0x74CD70 (2D) from +0x46C, at most the radius
	return gutils::GetDistanceInMetres(_clickedPosition, position) <= radius;
}

void HandSystem::ClearClickedPosition() noexcept
{
	_clickedPosition = glm::vec3(0.0f); // ClearClickedPosition 0x70B100
}

void HandSystem::UpdateTapMemory(bool actionReleased) noexcept
{
	// fn_005D3700 (InterfaceActionProcess fn_005D1120, before the action states): the action button released
	// (m_Buttons & 0x4000) in action state 0 with nothing in the hand and the game not paused (g_game +0x14 & 4)
	// remembers the collided object (+0x400), else the land point (+0x3F0) with its turn. (not ported) a Reward under the
	// leash is not remembered. Otherwise the two 15 s expiries
	const bool idle = !_held && !_tug && !_pickSource && !_releaseArmed && !_pendingPick && !_gripPoint;
	if (actionReleased && idle && !game_clock::IsPaused())
	{
		const auto object = _hovered ? *_hovered : (_cursorObject ? *_cursorObject : entt::null);
		if (object != entt::null)
		{
			RememberTapped(object);
			return;
		}
		_clickedObject = entt::null;
		_clickedPosition = _interactionPoint.value_or(glm::vec3(0.0f));
		_clickedPositionTurn = game_clock::Turn();
		return;
	}
	if (Expired(_clickedPositionTurn))
	{
		_clickedPosition = glm::vec3(0.0f);
	}
	if (_clickedObject != entt::null && Expired(_clickedTurn))
	{
		_clickedObject = entt::null;
	}
}
