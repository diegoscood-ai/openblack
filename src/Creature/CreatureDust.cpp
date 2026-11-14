/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureDust.h"

using namespace openblack;
using namespace openblack::creature_dust;

std::optional<int32_t> creature_dust::PuffsFor(audio::SoundAction action)
{
	switch (action)
	{
	case audio::SoundAction::FootstepLight:
		return 3;
	case audio::SoundAction::FootstepNormal:
		return 6;
	case audio::SoundAction::FootStamp:
		return 12;
	default:
		return std::nullopt;
	}
}

Puff creature_dust::Make(float drawnSize, const std::function<uint32_t(int32_t)>& localRand)
{
	const float spread = drawnSize * 0.05f;
	// The three draws in their order, each taken about 0
	const float z = static_cast<float>(static_cast<int32_t>(localRand(201)) - 100) * spread;
	const float y = static_cast<float>(static_cast<int32_t>(localRand(201)) - 100) * spread;
	const float x = static_cast<float>(static_cast<int32_t>(localRand(201)) - 100) * spread;
	return {.velocity = {x, y, z}, .size = drawnSize * 4.0f};
}
