/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// How an animal reacts to a creature: its priority, the way a creature goes as the animals that run from it see it,
// and the two turn counts with no town, pinned to the numbers the original game's own functions give for the same
// inputs (an animal's own class answering, as the game's animals do).

#include <array>
#include <bit>
#include <optional>

#include <gtest/gtest.h>

#include "ECS/Effects/FleeGeometry.h"
#include "ECS/Effects/VillagerCreatureRules.h"

using namespace openblack::ecs::effects;
using namespace openblack::ecs::effects::creature_reaction;

namespace
{
struct PriorityCase
{
	uint32_t dancing;
	uint32_t priority;
	uint32_t expected;
};

struct MovementCase
{
	uint32_t yaw;
	uint32_t x;
	uint32_t y;
	uint32_t z;
};

struct TurnsCase
{
	uint32_t distance;
	uint32_t impressiveType;
	uint32_t action;
	uint32_t actionImpressiveType;
	uint32_t expected;
};

// The original's answers. Floats are their bits; the dancing value is the word the original tests for a dance
// clang-format off
constexpr std::array<PriorityCase, 18> k_PriorityCases {{
	{0x00000000u, 0x0000003Cu, 60u},
	{0x00000000u, 0x00000000u, 0u},
	{0x00000000u, 0x000000FFu, 255u},
	{0x00000000u, 0x00000100u, 0u},
	{0x00000000u, 0x000001FFu, 255u},
	{0x00000000u, 0x12345678u, 120u},
	{0x00000001u, 0x0000003Cu, 0u},
	{0x00000001u, 0x00000000u, 0u},
	{0x00000001u, 0x000000FFu, 0u},
	{0x00000001u, 0x00000100u, 0u},
	{0x00000001u, 0x000001FFu, 0u},
	{0x00000001u, 0x12345678u, 0u},
	{0x80000000u, 0x0000003Cu, 0u},
	{0x80000000u, 0x00000000u, 0u},
	{0x80000000u, 0x000000FFu, 0u},
	{0x80000000u, 0x00000100u, 0u},
	{0x80000000u, 0x000001FFu, 0u},
	{0x80000000u, 0x12345678u, 0u},
}};

constexpr std::array<MovementCase, 40> k_MovementCases {{
	{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
	{0x3FC90FDBu, 0x47800000u, 0x00000000u, 0x47800000u},
	{0x40490FDBu, 0x00000000u, 0x00000000u, 0x00000000u},
	{0xBFC90FDBu, 0xC7800000u, 0x00000000u, 0xC7800000u},
	{0x3F800000u, 0x47577000u, 0x00000000u, 0x47577000u},
	{0x40200000u, 0x4719C200u, 0x00000000u, 0x4719C200u},
	{0xC0400000u, 0xC6132400u, 0x00000000u, 0xC6132400u},
	{0x40C90FDBu, 0x00000000u, 0x00000000u, 0x00000000u},
	{0x3A83126Fu, 0x43490000u, 0x00000000u, 0x43490000u},
	{0x42C80000u, 0xC7024200u, 0x00000000u, 0xC7024200u},
	{0xC2C80000u, 0x47024200u, 0x00000000u, 0x47024200u},
	{0x3F490FDBu, 0x47350400u, 0x00000000u, 0x47350400u},
	{0xBF490FDBu, 0xC7350400u, 0x00000000u, 0xC7350400u},
	{0x4096CBE4u, 0xC77FFF00u, 0x00000000u, 0xC77FFF00u},
	{0x3AC90FDBu, 0x43490000u, 0x00000000u, 0x43490000u},
	{0x3B490FDBu, 0x43490000u, 0x00000000u, 0x43490000u},
	{0x33D6BF95u, 0x43490000u, 0x00000000u, 0x43490000u},
	{0xBD9E0E65u, 0xC59CE800u, 0x00000000u, 0xC59CE800u},
	{0x3F4B2EE4u, 0x4736AD00u, 0x00000000u, 0x4736AD00u},
	{0x408AC3C7u, 0xC76DFC00u, 0x00000000u, 0xC76DFC00u},
	{0xC020BA7Cu, 0xC7173C00u, 0x00000000u, 0xC7173C00u},
	{0xC02069C3u, 0xC7187F00u, 0x00000000u, 0xC7187F00u},
	{0x402D13D6u, 0x46D97C00u, 0x00000000u, 0x46D97C00u},
	{0xBE464EB2u, 0xC644B000u, 0x00000000u, 0xC644B000u},
	{0xBFFC0DD2u, 0xC76C3600u, 0x00000000u, 0xC76C3600u},
	{0xBFA9580Bu, 0xC7782200u, 0x00000000u, 0xC7782200u},
	{0xC0187FE6u, 0xC7308500u, 0x00000000u, 0xC7308500u},
	{0xC094EBA3u, 0x477F8400u, 0x00000000u, 0x477F8400u},
	{0x3FEC2E36u, 0x47768400u, 0x00000000u, 0x47768400u},
	{0x40B069C5u, 0xC732C800u, 0x00000000u, 0xC732C800u},
	{0x40292D4Eu, 0x46F58000u, 0x00000000u, 0x46F58000u},
	{0x3FF18673u, 0x47735300u, 0x00000000u, 0x47735300u},
	{0xC0118EBCu, 0xC7436000u, 0x00000000u, 0xC7436000u},
	{0xBF6E44C2u, 0xC74D2600u, 0x00000000u, 0xC74D2600u},
	{0xC09F69A8u, 0x4776EF00u, 0x00000000u, 0x4776EF00u},
	{0x404CF01Fu, 0xC56EA000u, 0x00000000u, 0xC56EA000u},
	{0xC0365CD3u, 0xC694A000u, 0x00000000u, 0xC694A000u},
	{0xC03A935Cu, 0xC6667C00u, 0x00000000u, 0xC6667C00u},
	{0x40D760A0u, 0x46DC5200u, 0x00000000u, 0x46DC5200u},
	{0xC00F0E78u, 0xC749D100u, 0x00000000u, 0xC749D100u},
}};

// The reaction info's row for a creature (frozen info.dat): 300 and 600 turns, a reach of 55 metres, distance half
// important
// A distance that is not a number is left out: none is one in play, as distances come from whole map units
constexpr std::array<TurnsCase, 25> k_TurnsToReactCases {{
	{0x00000000u, 24u, 0u, 24u, 0x0000012Cu},
	{0x425C0000u, 24u, 0u, 24u, 0x000000ADu},
	{0x42C80000u, 24u, 0u, 24u, 0x00000045u},
	{0x43020000u, 24u, 0u, 24u, 0x00000000u},
	{0x43030000u, 24u, 0u, 24u, 0xFFFFFFFEu},
	{0x43480000u, 24u, 0u, 24u, 0xFFFFFF5Fu},
	{0x00000000u, 25u, 0u, 24u, 0x00000258u},
	{0x00000000u, 26u, 0u, 24u, 0x00000384u},
	{0x00000000u, 24u, 5u, 25u, 0x0000012Cu},
	{0x00000000u, 24u, 5u, 26u, 0x0000012Cu},
	{0x00000000u, 24u, 328u, 25u, 0x0000012Cu},
	{0x41E804DFu, 26u, 229u, 25u, 0x000002BBu},
	{0x42194711u, 25u, 321u, 26u, 0x000001A7u},
	{0x41EA34F7u, 25u, 234u, 25u, 0x000001D0u},
	{0x41FB07EEu, 26u, 60u, 25u, 0x000002AAu},
	{0x42A712EAu, 24u, 120u, 24u, 0x0000006Bu},
	{0x4217EF15u, 26u, 187u, 24u, 0x0000027Du},
	{0x4301AEA4u, 25u, 110u, 25u, 0x00000001u},
	{0x422998EBu, 25u, 207u, 24u, 0x00000194u},
	{0x42F6F96Bu, 25u, 275u, 25u, 0x0000001Eu},
	{0x42BDE874u, 26u, 139u, 25u, 0x000000F2u},
	{0x42BFFA47u, 25u, 282u, 24u, 0x0000009Cu},
	{0x42340C95u, 26u, 256u, 24u, 0x0000024Cu},
	{0x429EBB08u, 26u, 82u, 24u, 0x0000015Eu},
	{0x4204604Du, 26u, 223u, 24u, 0x0000029Eu},
}};

constexpr std::array<TurnsCase, 25> k_TurnsBeforeAgainCases {{
	{0x00000000u, 24u, 0u, 24u, 0x0000012Cu},
	{0x425C0000u, 24u, 0u, 24u, 0x00000229u},
	{0x42C80000u, 24u, 0u, 24u, 0x000002F9u},
	{0x43020000u, 24u, 0u, 24u, 0x00000384u},
	{0x43030000u, 24u, 0u, 24u, 0x00000388u},
	{0x43480000u, 24u, 0u, 24u, 0x000004C7u},
	{0x00000000u, 25u, 0u, 24u, 0x00000096u},
	{0x00000000u, 26u, 0u, 24u, 0x00000064u},
	{0x00000000u, 24u, 5u, 25u, 0x00000000u},
	{0x00000000u, 24u, 5u, 26u, 0x00000000u},
	{0x00000000u, 24u, 328u, 25u, 0x0000012Cu},
	{0x42A708ECu, 25u, 187u, 25u, 0x00000000u},
	{0x41DF99DCu, 26u, 276u, 25u, 0x00000000u},
	{0x418D01E6u, 24u, 127u, 26u, 0x00000000u},
	{0x43026E06u, 25u, 129u, 25u, 0x00000000u},
	{0x42CCF7CBu, 26u, 205u, 24u, 0x00000101u},
	{0x431FC064u, 24u, 71u, 24u, 0x0000040Du},
	{0x41C12BFCu, 26u, 272u, 24u, 0x00000089u},
	{0x4253DCF4u, 24u, 327u, 24u, 0x00000220u},
	{0x4245E340u, 24u, 259u, 26u, 0x00000000u},
	{0x42CCDB2Bu, 26u, 72u, 26u, 0x00000000u},
	{0x430223F8u, 25u, 9u, 26u, 0x00000000u},
	{0x426E3413u, 24u, 181u, 24u, 0x0000023Eu},
	{0x43198555u, 25u, 296u, 24u, 0x000001F8u},
	{0x430AD8A0u, 24u, 59u, 24u, 0x000003ACu},
}};
// clang-format on

/// The facts an animal's turn counts read: the creature's actions and no town
TurnsFacts AnimalFactsOf(const TurnsCase& c, uint32_t turns)
{
	return {
	    .isCreature = true,
	    .distance = std::bit_cast<float>(c.distance),
	    .metresPerCell = 10.0f,
	    .maxReactionDistance = 55.0f,
	    .howImportantIsDistance = 0.5f,
	    .impressiveType = c.impressiveType,
	    .currentActionImpressiveType = c.action < k_ActionRows ? std::optional(c.actionImpressiveType) : std::nullopt,
	    .townAttitude = std::nullopt,
	    .turns = turns,
	};
}
} // namespace

TEST(AnimalCreatureReactions, AnAnimalsPriorityForACreatureIsTheOriginals)
{
	for (const auto& c : k_PriorityCases)
	{
		EXPECT_EQ(AnimalPriority(c.dancing != 0, c.priority), c.expected) << c.dancing << " " << c.priority;
	}
}

TEST(AnimalCreatureReactions, ACreatureGoesAlongADiagonalAheadOfItsBody)
{
	for (const auto& c : k_MovementCases)
	{
		const auto way = flee::CreatureMovement(std::bit_cast<float>(c.yaw));
		EXPECT_EQ(std::bit_cast<uint32_t>(way.x), c.x) << std::hex << c.yaw;
		EXPECT_EQ(std::bit_cast<uint32_t>(way.y), c.y) << std::hex << c.yaw;
		EXPECT_EQ(std::bit_cast<uint32_t>(way.z), c.z) << std::hex << c.yaw;
	}
}

TEST(AnimalCreatureReactions, AnAnimalsTurnsToReactToACreatureAreTheOriginals)
{
	for (const auto& c : k_TurnsToReactCases)
	{
		EXPECT_EQ(TurnsToReact(AnimalFactsOf(c, 300)), c.expected) << std::hex << c.distance << " " << c.impressiveType;
	}
}

TEST(AnimalCreatureReactions, AnAnimalsTurnsBeforeReactingAgainToACreatureAreTheOriginals)
{
	for (const auto& c : k_TurnsBeforeAgainCases)
	{
		EXPECT_EQ(TurnsBeforeReactingAgain(AnimalFactsOf(c, 600)), c.expected)
		    << std::hex << c.distance << " " << c.impressiveType << " " << c.action;
	}
}

TEST(AnimalCreatureReactions, ACreatureFarEnoughEndsAnAnimalsReactionAtOnce)
{
	// beyond 130 metres the count is negative: kept as an unsigned number, it must be compared as a signed one
	const TurnsCase far {0x43030000u, k_NotImpressive, 0u, k_NotImpressive, 0u};
	EXPECT_LT(static_cast<int32_t>(TurnsToReact(AnimalFactsOf(far, 300))), 0);
}
