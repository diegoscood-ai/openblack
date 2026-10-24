/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PlayerRivalry.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
/// How far the average goes towards the power each time
constexpr float k_AverageShare = 0.5f;
} // namespace

void player_rivalry::AddHarm(components::PlayerRivalry& rivalry, PlayerNames by, float harm)
{
	const auto index = static_cast<size_t>(by);
	if (index >= rivalry.harmFrom.size())
	{
		return;
	}
	rivalry.harmFrom.at(index) = harm + rivalry.harmFrom.at(index);
}

void player_rivalry::NoteInfluencePower(components::PlayerRivalry& rivalry, float power)
{
	rivalry.averageInfluencePower = (rivalry.averageInfluencePower + power) * k_AverageShare;
}
