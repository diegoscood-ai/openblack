/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureCreation.h"

#include <cstring>

#include <algorithm>

#include "3D/CreatureBody.h"
#include "InfoConstants.h"

using namespace openblack;
using namespace openblack::creature_creation;

namespace
{
/// How far the body's strength and fatness are moved, either way
constexpr float k_BodySpread = 0.1f;
/// The share of a desire's growing time it is moved by, either way
constexpr float k_IncreaseSpread = 0.2f;
/// How far each personality value is moved, either way: the first five, the sixth, then the last three
constexpr std::array<float, k_PersonalityCount> k_PersonalitySpreads {0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.3f, 0.4f, 0.4f, 0.4f};
/// Each previous action's value is a whole number below this
constexpr uint32_t k_PreviousActionRange = 120;

/// The 17 per species values of a creature table row
template <typename Row>
float PerSpecies(const Row& row, size_t species)
{
	static_assert(sizeof(Row) == 17 * sizeof(float));
	std::array<float, 17> values {};
	std::memcpy(values.data(), &row, sizeof(values));
	return values.at(std::min(species, values.size() - 1));
}

/// A float towards the range, with no draw for a range of 0, as the game's draw makes none
float Draw(const Random& random, float range)
{
	return range == 0.0f ? 0.0f : random.floatRand(range);
}

/// A value moved by up to the spread either way, the draw added to the value first
float AddThenSpread(const Random& random, float value, float spread)
{
	const float range = spread + spread;
	const float drawn = Draw(random, range);
	const float sum = drawn + value;
	return sum - spread;
}

/// A value moved by up to the spread either way, the spread taken from the draw first
float SpreadThenAdd(const Random& random, float value, float spread)
{
	const float range = spread + spread;
	const float drawn = Draw(random, range);
	const float moved = drawn - spread;
	return moved + value;
}

/// A personality value moved by its spread and kept within 0 and 1. Each test of the clamp moves the value again, with
/// a draw of its own
float Personality(const Random& random, float innate, float spread)
{
	if (AddThenSpread(random, innate, spread) > 0.0f)
	{
		if (AddThenSpread(random, innate, spread) >= 1.0f)
		{
			return 1.0f;
		}
	}
	if (AddThenSpread(random, innate, spread) <= 0.0f)
	{
		return 0.0f;
	}
	return AddThenSpread(random, innate, spread);
}
} // namespace

Setup creature_creation::SetupFor(const InfoConstants& info, CreatureType species)
{
	Setup setup;
	const auto row = creature::InfoRow(species);
	if (row < info.creature.size())
	{
		const auto& ofSpecies = info.creature.at(row);
		setup.strength = ofSpecies.strength;
		setup.startFatness = ofSpecies.startFatness;
		setup.personality = {
		    ofSpecies.innateKindness,
		    ofSpecies.innateAggression,
		    ofSpecies.innateLethargy,
		    ofSpecies.innateFriendliness,
		    ofSpecies.innateCommunicativeness,
		    ofSpecies.field0x354,
		    ofSpecies.field0x358,
		    ofSpecies.field0x35c,
		    ofSpecies.field0x360,
		};
	}
	setup.actionCount = info.creatureAction.size();
	for (size_t d = 0; d < setup.desires.size(); ++d)
	{
		const auto& initial = info.creatureInitialDesire.at(d);
		auto& desire = setup.desires.at(d);
		desire.max = initial.field0x3c;
		desire.decayMin = initial.field0x40;
		desire.decayMax = initial.field0x44;
		desire.increaseSeconds = PerSpecies(info.creatureDesireForType.at(d), row);
		desire.weight = initial.field0x68;
		const std::array<uint32_t, creature_desires::k_MaxSources> sourceTypes {
		    initial.field0x0,  initial.field0x4,  initial.field0x8,  initial.field0xc,
		    initial.field0x10, initial.field0x14, initial.field0x18, initial.field0x1c,
		};
		for (const auto type : sourceTypes)
		{
			// a slot with no source is passed over, and the next one read
			if (type >= creature_desires::k_NoSource)
			{
				continue;
			}
			const auto& table = info.desireSourceTable.at(type);
			desire.sources.push_back({
			    .type = type,
			    .value = PerSpecies(info.creatureInitialSource1.at(type), row),
			    .threshold = PerSpecies(info.creatureInitialSource2.at(type), row),
			    .multiplier = table.field0x8,
			    .spentWhenSatisfied = table.field0x4 != 0,
			});
			setup.spreads.at(d).push_back({.value = table.field0xc, .threshold = table.field0x10});
		}
	}
	return setup;
}

Made creature_creation::Make(const Setup& setup, const Random& random)
{
	Made made;
	made.strength = AddThenSpread(random, setup.strength, k_BodySpread);
	made.fatness = AddThenSpread(random, setup.startFatness, k_BodySpread);

	// Each desire's decay is drawn in its range, then its growing time is moved, desire by desire
	auto desires = setup.desires;
	for (auto& desire : desires)
	{
		const float range = desire.decayMax - desire.decayMin;
		const float drawn = Draw(random, range);
		desire.decayMin = drawn + desire.decayMin;
		desire.decayMax = desire.decayMin;
		const float spread = k_IncreaseSpread * desire.increaseSeconds;
		desire.increaseSeconds = SpreadThenAdd(random, desire.increaseSeconds, spread);
	}
	// Then each desire's sources, in their order: the value, then the threshold
	for (size_t d = 0; d < desires.size(); ++d)
	{
		auto& sources = desires.at(d).sources;
		const auto& spreads = setup.spreads.at(d);
		for (size_t s = 0; s < sources.size(); ++s)
		{
			const auto spread = s < spreads.size() ? spreads.at(s) : SourceSpread {};
			auto& source = sources.at(s);
			source.value = std::clamp(SpreadThenAdd(random, source.value, spread.value), 0.0f, 1.0f);
			source.threshold = std::clamp(SpreadThenAdd(random, source.threshold, spread.threshold), 0.0f, 1.0f);
		}
	}
	// the decays are drawn already: each desire takes its own
	made.desires = creature_desires::Create(desires, [](float low, float) { return low; });

	for (size_t i = 0; i < k_PersonalityCount; ++i)
	{
		made.personality.at(i) = Personality(random, setup.personality.at(i), k_PersonalitySpreads.at(i));
	}
	made.previousActions.reserve(setup.actionCount);
	for (size_t i = 0; i < setup.actionCount; ++i)
	{
		made.previousActions.push_back(random.rand(k_PreviousActionRange));
	}
	return made;
}
