/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AltarSacrifice.h"

namespace openblack::ecs::altar_sacrifice
{

namespace
{
/// A man cries one of eight samples, a woman one of six
constexpr Cry k_ManCry {.first = 221, .count = 8};
constexpr Cry k_WomanCry {.first = 536, .count = 6};
/// The script bank's child cries, one of which a sacrificed child picks and does not play
constexpr Cry k_ChildCry {.first = 13, .count = 10};
} // namespace

float SacrificeValue(float life, float tableValue)
{
	return (life * 0.5f + 0.5f) * tableValue;
}

std::optional<Sacrifice> Judge(const Offered& offered)
{
	const float value = SacrificeValue(offered.life, offered.tableValue);
	switch (offered.kind)
	{
	case Offering::Villager:
		if (!offered.siteBuilt || offered.indestructible)
		{
			return std::nullopt;
		}
		return Sacrifice {
		    .prayer = offered.child ? value * k_ChildBonus : value,
		    .deed = true,
		    .villagerDies = true,
		    .light = true,
		    .number = true,
		};
	case Offering::Animal:
		// An animal is taken whether or not the site is built, and even when indestructible
		return Sacrifice {
		    .prayer = value * offered.life,
		    .deed = true,
		    .animalCounts = true,
		    .number = true,
		};
	case Offering::Tree:
		if (offered.indestructible)
		{
			return std::nullopt;
		}
		return Sacrifice {.prayer = value * offered.life, .ghost = true, .number = true};
	case Offering::DeadTree:
		return Sacrifice {.prayer = value * offered.life, .ghost = true};
	}
	return std::nullopt;
}

float AnimalAlignmentChange(float sacrificeChange)
{
	return sacrificeChange * k_AnimalAlignmentShare;
}

std::optional<Cry> CryOf(bool child, bool female)
{
	if (child)
	{
		return std::nullopt;
	}
	return female ? k_WomanCry : k_ManCry;
}

void PlaySounds(Offering kind, bool child, bool female, const RandomSample& randomSample, const PlaySound& play)
{
	play({.bank = audio::SfxBank::InGame, .sample = k_SacrificeSample});
	if (kind != Offering::Villager)
	{
		return;
	}
	if (const auto cry = CryOf(child, female))
	{
		play({.bank = audio::SfxBank::Editor, .sample = randomSample(cry->first, cry->count)});
		return;
	}
	// a child: the same sound from the script bank, then one of its ten child cries picked and never played
	play({.bank = audio::SfxBank::ScriptSfx, .sample = k_SacrificeSample});
	static_cast<void>(randomSample(k_ChildCry.first, k_ChildCry.count));
}

} // namespace openblack::ecs::altar_sacrifice
