/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureEmoteAgendas.h"

#include <cmath>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"

using namespace openblack;
using namespace openblack::creature_emote;

float creature_emote::FollowCameraReach(float height)
{
	return (height + 1.0f) * 2.0f;
}

FollowCameraAction creature_emote::FollowCameraTurn(const FollowCameraView& view, uint32_t& turnsLeft, float reach)
{
	if (!view.focus.has_value() || turnsLeft == 0)
	{
		return FollowCameraAction::Done;
	}
	--turnsLeft;
	const glm::vec2 focus {view.focus->x, view.focus->z};
	if (!map_coords::InBounds(map_coords::FromMetres(focus)))
	{
		return FollowCameraAction::Fail;
	}
	if (view.moving)
	{
		return view.focusAhead ? FollowCameraAction::Continue : FollowCameraAction::MoveAgain;
	}
	if (gutils::GetDistanceInMetres(view.position, focus) < reach)
	{
		return FollowCameraAction::Done;
	}
	if (*view.focus == glm::vec3(0.0f))
	{
		return FollowCameraAction::Done;
	}
	return FollowCameraAction::MoveAgain;
}

float creature_emote::FloatRandom(const Random& random, float most)
{
	return static_cast<float>(random(game_random::k_FloatRandRange)) * most * game_random::FloatRandScale();
}

float creature_emote::WholeTurnSeconds(float seconds, float turnsPerSecond)
{
	// Without turns (an agenda made before the game's clock is known) the seconds stay as they are
	if (turnsPerSecond <= 0.0f)
	{
		return seconds;
	}
	return std::trunc(seconds * turnsPerSecond) / turnsPerSecond;
}
