/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LivingFirstClip.h"

using namespace openblack;

namespace
{
/// A coin: the first clip on 0, the second on anything else
int32_t Coin(const ecs::living::IntRandom& random, int32_t heads, int32_t tails)
{
	return random(2) == 0 ? heads : tails;
}

std::optional<ecs::living::FirstClip> Put(int32_t clip, bool fromStart) noexcept
{
	if (clip < 0)
	{
		return std::nullopt;
	}
	return ecs::living::FirstClip {.clip = clip, .fromStart = fromStart};
}
} // namespace

bool ecs::living::IsBirdForClips(AnimalInfo type) noexcept
{
	switch (type)
	{
	case AnimalInfo::Dove:
	case AnimalInfo::Crow:
	case AnimalInfo::Pigeon:
	case AnimalInfo::Seagull:
	case AnimalInfo::Swallow:
	case AnimalInfo::SpellDove:
	case AnimalInfo::Bat:
	case AnimalInfo::SpellBat:
	case AnimalInfo::Vulture:
		return true;
	default:
		return false;
	}
}

int32_t ecs::living::BirdMoveClip(AnimalInfo type, const IntRandom& random)
{
	switch (type)
	{
	case AnimalInfo::Dove:
		return Coin(random, 8, 9);
	case AnimalInfo::Crow:
		return Coin(random, 3, 4);
	case AnimalInfo::Pigeon:
		return Coin(random, 13, 14);
	case AnimalInfo::Seagull:
		return Coin(random, 18, 20);
	case AnimalInfo::Swallow:
	{
		// 0 -> 27, 1 -> 26, 2 -> 25
		const auto third = random(3);
		return third == 2 ? 25 : (third == 1 ? 26 : 27);
	}
	case AnimalInfo::SpellDove:
		return 23;
	case AnimalInfo::Bat:
	case AnimalInfo::SpellBat:
		return 1;
	default:
		// the vulture's is none, and anything that is not a bird has no bird clip
		return -1;
	}
}

std::optional<ecs::living::FirstClip> ecs::living::VillagerFirstClip(int32_t bornStateTableClip) noexcept
{
	return Put(bornStateTableClip, true);
}

std::optional<ecs::living::FirstClip> ecs::living::AnimalFirstClip(AnimalInfo type, int32_t standClip, const IntRandom& random)
{
	if (IsBirdForClips(type))
	{
		return Put(BirdMoveClip(type, random), false);
	}
	return Put(standClip, true);
}
