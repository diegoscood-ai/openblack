/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureMindTables.h"

#include <cstring>

#include <algorithm>
#include <array>

#include "InfoConstants.h"

using namespace openblack;
using namespace openblack::creature_mind_tables;

namespace
{
/// The tables mark unused slots of the action lists with 0 (no action) or with a number past the last action
constexpr uint32_t k_NoAction = 0;

constexpr std::array<float, 17> k_MiracleMultipliers {1.0f, 1.7f,  1.5f, 1.2f, 1.1f, 1.3f, 1.4f, 1.2f, 1.4f,
                                                      1.3f, 1.35f, 1.9f, 0.9f, 4.0f, 1.2f, 1.5f, 1.1f};

template <size_t N>
std::string Text(const std::array<char, N>& chars)
{
	return {chars.data(), strnlen(chars.data(), chars.size())};
}

/// A table row of 32-bit values read as its numbers
template <typename T, typename Row>
std::vector<T> Words(const Row& row)
{
	static_assert(sizeof(Row) % sizeof(T) == 0);
	std::vector<T> values(sizeof(Row) / sizeof(T));
	std::memcpy(values.data(), &row, sizeof(Row));
	return values;
}
} // namespace

Tables creature_mind_tables::Build(const InfoConstants& info)
{
	Tables tables;
	for (const auto& row : info.creatureAction)
	{
		tables.actions.push_back({
		    .name = Text(row.name),
		    .desire = row.desire,
		    .desireMultiplier = row.desireMultiplier,
		    .alwaysApplies = row.field0xc8 != 0,
		    .learningWindowSeconds = row.field0xd0,
		    .learnable = row.field0xf0 != 0,
		    .eatWhenStroked = row.field0xe0 != 0,
		    .shownDesire = row.shownDesire,
		    .creatureNicenessChange = row.creatureNicenessChange,
		});
	}
	const auto actionCount = static_cast<uint32_t>(tables.actions.size());
	for (size_t d = 0; d < k_DesireCount; ++d)
	{
		// Compassion is always about a town it helps, which says what to do; its own row of the table is never planned
		if (d == static_cast<size_t>(creature_desires::Desire::Compassion))
		{
			continue;
		}
		for (const auto action : Words<uint32_t>(info.creatureDesireAction1.at(d)))
		{
			if (action != k_NoAction && action < actionCount &&
			    std::ranges::find(tables.desireActions.at(d), action) == tables.desireActions.at(d).end())
			{
				tables.desireActions.at(d).push_back(action);
			}
		}
		const auto& initial = info.creatureInitialDesire.at(d);
		// The row's cap on the desire (2 for hunger and tiredness, 3 for healing), then the range its per-turn decay is
		// drawn from as the creature starts (0.9 to 1 in the game's data, mostly 0.999 to 1)
		tables.rules.at(d) = {
		    .initialMax = initial.field0x3c,
		    .decayMin = initial.field0x40,
		    .decayMax = initial.field0x44,
		    .initialWeight = initial.field0x68,
		    .learnsWeight = initial.field0x6c != 0,
		    .lessonDivisor = initial.field0x54,
		    .learnable = initial.field0x28 != 0,
		    .distanceWeight = initial.field0x58,
		    .searchesForGoal = initial.field0x20 != 0,
		    .approvesOrders = initial.field0x2c != 0,
		};
		tables.texts.at(d) = {
		    .desire = Text(initial.field0xb0),
		    .trying = Text(initial.field0x130),
		    .doing = Text(initial.field0x170),
		};
		const auto dependencies = Words<float>(info.creatureDesireDependency.at(d));
		for (size_t e = 0; e < k_DesireCount && e < dependencies.size(); ++e)
		{
			tables.dependencies.at(d).at(e) = dependencies[e];
		}
		for (const auto attribute : Words<uint32_t>(info.creatureDesireAttributeEntry.at(d)))
		{
			if (attribute < creature_tree::k_NoAttribute)
			{
				tables.attributes.at(d).push_back(static_cast<creature_tree::Attribute>(attribute));
			}
		}
	}
	// Each list of the town table runs up to its first empty slot
	for (size_t d = 0; d < tables.townActions.size(); ++d)
	{
		for (const auto action : Words<uint32_t>(info.creatureDesireAction2.at(d)))
		{
			if (action == k_NoAction || action >= actionCount)
			{
				break;
			}
			tables.townActions.at(d).push_back(action);
		}
	}
	for (const auto& entry : info.creatireActionKnownAboutEntry)
	{
		tables.skills.push_back({
		    .name = Text(entry.field0x0),
		    .watchSeconds = entry.field0x48,
		    .minPhase = entry.field0x50,
		});
	}
	for (const auto& entry : info.creatureMagicActionKnownAboutEntry)
	{
		tables.miracles.push_back({
		    .name = Text(entry.field0x0),
		    .timesToSee = entry.field0x44,
		    .minPhase = entry.field0x50,
		    .knownAtStart = entry.field0x54 != 0,
		    .action = entry.field0x58,
		});
	}
	for (const auto& entry : info.creatureMimic)
	{
		creature_watching::MimicRule rule {
		    .name = Text(entry.name),
		    .chance = entry.field0x80,
		    .needsLearningLeash = entry.field0x84 != 0,
		    .noticeAction = entry.field0x88,
		    .desire = entry.field0xa4,
		    .stageSteps = entry.field0xa8,
		    .copiesDesire = entry.field0xac != 0,
		};
		for (const auto action :
		     {entry.field0x8c, entry.field0x90, entry.field0x94, entry.field0x98, entry.field0x9c, entry.field0xa0})
		{
			if (action != k_NoAction && action < actionCount)
			{
				rule.copyActions.push_back(action);
			}
		}
		tables.mimics.push_back(std::move(rule));
	}
	return tables;
}

std::optional<uint32_t> creature_mind_tables::FindAction(const Tables& tables, std::string_view name)
{
	const auto found = std::ranges::find(tables.actions, name, &ActionInfo::name);
	if (found == tables.actions.end())
	{
		return std::nullopt;
	}
	return static_cast<uint32_t>(std::distance(tables.actions.begin(), found));
}

float creature_mind_tables::MiracleMultiplier(size_t speciesRow)
{
	return k_MiracleMultipliers.at(std::min(speciesRow, k_MiracleMultipliers.size() - 1));
}

bool creature_mind_tables::NeedsInstrument(uint32_t action)
{
	// The game's own action rows that are done with something (StoneAndEat, Hurl, the storage pit gifts, BuildHome, the
	// throwing games, the one-off spells picked up, PlantTree, StartFire, the thefts, WaterTree and the rest)
	constexpr std::array<uint32_t, 37> k_WithInstrument {14,  15,  24,  28,  30,  31,  32,  33,  35,  36,  68,  69,  73,
	                                                     83,  135, 171, 232, 233, 234, 235, 241, 245, 247, 249, 252, 262,
	                                                     280, 281, 283, 285, 291, 303, 306, 307, 308, 326, 327};
	return std::ranges::binary_search(k_WithInstrument, action);
}

creature_mind_tables::TownAttitudeFamily creature_mind_tables::TownAttitudeFamilyOf(uint32_t action)
{
	// The game's own action rows a town fears (the hurls, stomps and kicks, the aggressive miracles, setting fire,
	// frightening villagers), respects (the gifts, building and repairing, putting out fires, the kind miracles, the
	// dances and games with villagers, watering) and judges by what was eaten; every other row is nothing to it
	constexpr std::array<uint32_t, 22> k_Fear {15,  18,  45,  46,  47,  95,  96,  97,  99,  100, 103,
	                                           105, 185, 186, 197, 228, 232, 236, 252, 263, 283, 296};
	constexpr std::array<uint32_t, 58> k_Respect {28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  48,
	                                              49,  53,  54,  98,  101, 102, 104, 136, 137, 138, 182, 183, 184, 229, 230,
	                                              231, 233, 234, 235, 238, 239, 240, 241, 245, 246, 247, 248, 262, 285, 286,
	                                              287, 288, 293, 294, 295, 300, 302, 304, 305, 309, 325, 326, 327};
	constexpr std::array<uint32_t, 8> k_Eating {11, 12, 13, 14, 275, 276, 277, 278};
	if (std::ranges::binary_search(k_Fear, action))
	{
		return TownAttitudeFamily::Fear;
	}
	if (std::ranges::binary_search(k_Respect, action))
	{
		return TownAttitudeFamily::Respect;
	}
	return std::ranges::binary_search(k_Eating, action) ? TownAttitudeFamily::Eating : TownAttitudeFamily::None;
}
