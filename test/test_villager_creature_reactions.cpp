/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// How a villager reacts to a creature: the rules of ECS/Effects/VillagerCreatureRules, pinned to the numbers the
// original game's own functions give for the same inputs.

#include <array>
#include <bit>
#include <optional>

#include <gtest/gtest.h>

#include "ECS/Effects/VillagerCreatureRules.h"
#include "ECS/Town/TownCreatureAttitude.h"

using namespace openblack;
using namespace openblack::ecs::effects::creature_reaction;
namespace attitude = openblack::ecs::town_creature_attitude;

namespace
{
struct PriorityCase
{
	int dancing;
	int isCreature;
	int ignored;
	int invisible;
	uint32_t distance;
	uint32_t maxDistanceToRunAway;
	int hasTown;
	uint32_t attitude;
	uint32_t priority;
	uint32_t fleePriority;
	uint32_t followPriority;
	int sameOwner;
	int disciple;
	int32_t servedDesire;
	uint32_t amount;
	uint32_t raw;
	uint32_t boost;
	uint32_t boostA;
	uint32_t expected;
};

// The original's answers. Floats are their bits; the town's desire comes as its raw value and its two boosts
// clang-format off
constexpr std::array<PriorityCase, 52> k_PriorityCases {{
	{0, 1, 0, 0, 0x41F00000u, 0x42C80000u, 1, 0u, 60u, 150u, 50u, 1, 0, -1, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 90u},
	{1, 1, 0, 0, 0x41F00000u, 0x42C80000u, 1, 0u, 60u, 150u, 50u, 1, 0, -1, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0u},
	{0, 0, 0, 0, 0x41F00000u, 0x42C80000u, 1, 0u, 60u, 150u, 50u, 1, 0, -1, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0u},
	{0, 1, 1, 0, 0x41F00000u, 0x42C80000u, 1, 0u, 60u, 150u, 50u, 1, 0, -1, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0u},
	{0, 1, 0, 1, 0x41F00000u, 0x42C80000u, 1, 0u, 60u, 150u, 50u, 1, 0, -1, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0u},
	{0, 1, 0, 0, 0x42C80000u, 0x42C80000u, 1, 0u, 60u, 150u, 50u, 1, 0, -1, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 90u},
	{0, 1, 0, 0, 0x42C80001u, 0x42C80000u, 1, 0u, 60u, 150u, 50u, 1, 0, -1, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0u},
	{0, 1, 0, 0, 0x7FC00000u, 0x42C80000u, 1, 0u, 60u, 150u, 50u, 1, 0, -1, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 90u},
	{0, 1, 0, 0, 0x41F00000u, 0x42C80000u, 0, 0u, 60u, 150u, 50u, 1, 0, -1, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 60u},
	{0, 1, 0, 0, 0x41F00000u, 0x42C80000u, 1, 1u, 60u, 150u, 50u, 1, 0, -1, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0u},
	{0, 1, 0, 0, 0x41F00000u, 0x42C80000u, 1, 2u, 60u, 150u, 50u, 1, 0, -1, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 60u},
	{0, 1, 0, 0, 0x41F00000u, 0x42C80000u, 1, 3u, 60u, 150u, 50u, 1, 0, -1, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 44u},
	{0, 1, 0, 0, 0x41F00000u, 0x42C80000u, 1, 4u, 60u, 150u, 50u, 1, 0, -1, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 50u},
	{0, 1, 0, 0, 0x41F00000u, 0x42C80000u, 1, 5u, 60u, 150u, 50u, 1, 0, -1, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0u},
	{0, 1, 0, 0, 0x41F00000u, 0x42C80000u, 1, 4u, 60u, 150u, 50u, 0, 0, -1, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 50u},
	{0, 1, 0, 0, 0x41F00000u, 0x42C80000u, 1, 4u, 60u, 150u, 50u, 1, 1, -1, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0u},
	{0, 1, 0, 0, 0x41F00000u, 0x42C80000u, 1, 4u, 60u, 150u, 50u, 1, 0, 0, 0x3F800000u, 0x3F000000u, 0x00000000u, 0x00000000u, 0u},
	{0, 1, 0, 0, 0x41F00000u, 0x42C80000u, 1, 4u, 60u, 150u, 50u, 1, 0, 0, 0x3F800000u, 0x00000000u, 0x00000000u, 0x00000000u, 50u},
	{0, 1, 0, 0, 0x41F00000u, 0x42C80000u, 1, 4u, 60u, 150u, 50u, 1, 0, 16, 0xBF800000u, 0x3E800000u, 0x00000000u, 0x00000000u, 50u},
	{0, 1, 0, 0, 0x41F00000u, 0x42C80000u, 1, 4u, 60u, 150u, 50u, 1, 0, 17, 0x3F800000u, 0x3F800000u, 0x00000000u, 0x00000000u, 50u},
	{0, 1, 0, 0, 0x41F00000u, 0x42C80000u, 1, 4u, 60u, 150u, 50u, 1, 0, 3, 0x3F000000u, 0x3DCCCCCDu, 0xBE4CCCCDu, 0x3DCCCCCDu, 50u},
	{0, 1, 0, 0, 0x41F00000u, 0x42C80000u, 1, 0u, 200u, 150u, 50u, 1, 0, -1, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 44u},
	{0, 1, 0, 0, 0x41F00000u, 0x42C80000u, 1, 0u, 171u, 150u, 50u, 1, 0, -1, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0u},
	{0, 1, 0, 0, 0x41F00000u, 0x42C80000u, 1, 3u, 60u, 128u, 50u, 1, 0, -1, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0u},
	{0, 1, 0, 0, 0x41F00000u, 0x42C80000u, 1, 3u, 60u, 16777217u, 50u, 1, 0, -1, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0u},
	{0, 1, 0, 0, 0x41F00000u, 0x42C80000u, 0, 0u, 511u, 150u, 50u, 1, 0, -1, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 255u},
	{0, 1, 0, 0, 0x41F00000u, 0x42C80000u, 1, 4u, 60u, 150u, 291u, 0, 0, -1, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 35u},
	{0, 1, 0, 0, 0x41F00000u, 0x42C80000u, 1, 4u, 60u, 150u, 50u, 1, 0, 5, 0x7FC00000u, 0x3F800000u, 0x00000000u, 0x00000000u, 50u},
	{0, 1, 0, 0, 0x42C544DBu, 0x42C80000u, 1, 4u, 248u, 390u, 133u, 0, 0, 16, 0xBDF3AD58u, 0x3F04FB6Au, 0xBE01E23Fu, 0x3E8A4FC5u, 133u},
	{0, 1, 0, 0, 0x426AA4D1u, 0x42C80000u, 1, 3u, 101u, 373u, 211u, 0, 0, 15, 0x3F4C4FF2u, 0x3EEA619Au, 0x3E26CF47u, 0x3DE16057u, 234u},
	{0, 1, 0, 0, 0x42CAA0B5u, 0x42C80000u, 1, 0u, 171u, 47u, 185u, 1, 0, 1, 0x3F82D9ECu, 0x3ECD90ACu, 0x3E97C32Au, 0xBECE0DCEu, 0u},
	{0, 1, 0, 0, 0x428D41BAu, 0x42C80000u, 1, 0u, 102u, 335u, 184u, 1, 0, 16, 0x3F140AA2u, 0x3ECAA903u, 0x3E9C6B24u, 0x3EF3E76Du, 153u},
	{0, 1, 0, 0, 0x429808EAu, 0x42C80000u, 1, 0u, 125u, 308u, 220u, 1, 0, 1, 0xBFD2147Cu, 0x3F4299B2u, 0x3CCEF4BDu, 0xBEC42F98u, 187u},
	{0, 1, 0, 0, 0x420095AAu, 0x42C80000u, 1, 4u, 100u, 194u, 247u, 0, 0, 4, 0x3FAB028Cu, 0x3F408110u, 0x3CB56010u, 0xBE9F5C99u, 247u},
	{0, 1, 0, 0, 0x400302DEu, 0x42C80000u, 1, 1u, 170u, 285u, 156u, 1, 0, 10, 0xBF57E946u, 0x3EB704E1u, 0xBC383130u, 0xBEA119F2u, 0u},
	{0, 1, 0, 0, 0x42B31D79u, 0x42C80000u, 1, 4u, 203u, 49u, 225u, 0, 0, 17, 0x3F2E4D65u, 0x3F476E30u, 0xBD584AEEu, 0x3EA99AD9u, 225u},
	{0, 1, 0, 0, 0x42780275u, 0x42C80000u, 1, 0u, 241u, 155u, 209u, 0, 0, 6, 0xBE473CA7u, 0x3EE3F86Bu, 0x3EEE1E2Au, 0xBEF681CEu, 105u},
	{0, 1, 0, 0, 0x416D1C6Au, 0x42C80000u, 1, 4u, 184u, 96u, 131u, 1, 0, 14, 0xBF7B1F4Eu, 0xBE37263Cu, 0x3EC232CEu, 0x3DE5BD23u, 131u},
	{0, 1, 0, 0, 0x429098B2u, 0x42C80000u, 1, 3u, 245u, 251u, 72u, 1, 0, 18, 0x3F9C0703u, 0xBEB4D930u, 0x3E2310A3u, 0x3EF10840u, 246u},
	{0, 1, 0, 0, 0x42348024u, 0x42C80000u, 1, 4u, 136u, 163u, 203u, 1, 1, 3, 0x3FDF8BA8u, 0xBED63B35u, 0xBEFCD45Du, 0xBD4F6EBCu, 0u},
	{0, 1, 0, 0, 0x41AF466Cu, 0x42C80000u, 1, 0u, 57u, 349u, 189u, 1, 0, 16, 0xBFE01D83u, 0xBF1D5F63u, 0x3EE5107Cu, 0x3DE84F44u, 85u},
	{0, 1, 0, 0, 0x42C17DD4u, 0x42C80000u, 1, 4u, 251u, 38u, 240u, 0, 0, 9, 0x3FEC8659u, 0x3EB940CAu, 0xBE83664Fu, 0xBE3A08D7u, 240u},
	{0, 1, 0, 0, 0x4296B8C6u, 0x42C80000u, 1, 0u, 49u, 365u, 53u, 0, 1, 12, 0x3F657920u, 0xBF5CAC48u, 0xBEF034EDu, 0x3D52A534u, 73u},
	{0, 1, 0, 0, 0x42C7D6D6u, 0x42C80000u, 1, 4u, 42u, 82u, 135u, 1, 0, -2, 0x3F952988u, 0xBF39D1B3u, 0x3EE30590u, 0x3ED14ADEu, 135u},
	{0, 1, 0, 0, 0x4265FFA3u, 0x42C80000u, 1, 4u, 225u, 228u, 285u, 1, 0, 16, 0xBFDF66A0u, 0x3F19B3A7u, 0x3EF4C680u, 0xBE5B104Au, 29u},
	{0, 1, 0, 0, 0x4120AB42u, 0x42C80000u, 1, 0u, 141u, 143u, 140u, 1, 0, 9, 0xBFEFA915u, 0xBF54268Bu, 0x3EA1FB76u, 0x3E0DE5FDu, 211u},
	{0, 1, 0, 0, 0x42B2E99Cu, 0x42C80000u, 1, 4u, 88u, 172u, 189u, 0, 0, 4, 0x3F4D80A9u, 0xBEA04746u, 0xBED8457Eu, 0x3E6D138Cu, 189u},
	{0, 1, 0, 0, 0x423FD558u, 0x42C80000u, 1, 0u, 69u, 237u, 253u, 0, 0, 9, 0x3DA6069Cu, 0xBF70657Du, 0x3CD043FBu, 0xBEB30BD3u, 103u},
	{0, 1, 0, 0, 0x42B15BE7u, 0x42C80000u, 1, 3u, 198u, 114u, 113u, 0, 0, 3, 0x3F83BBA7u, 0x3F7CA23Cu, 0x3D79E9CDu, 0xBEFD8F84u, 228u},
	{0, 1, 0, 0, 0x42399BE9u, 0x42C80000u, 1, 4u, 166u, 195u, 269u, 1, 0, 14, 0x3E512314u, 0x3EA5F705u, 0x3E5D90BBu, 0x3E14A079u, 0u},
	{0, 1, 0, 0, 0x4232AAE4u, 0x42C80000u, 1, 4u, 94u, 76u, 84u, 0, 0, -1, 0xBFF9B9F4u, 0xBD63B145u, 0xBEE1ACFCu, 0x3E1A7D0Du, 84u},
	{0, 1, 0, 0, 0x414047E6u, 0x42C80000u, 1, 4u, 188u, 304u, 16u, 0, 1, 11, 0xBF8C8101u, 0x3E05BF75u, 0x3ED834EFu, 0xBE537538u, 16u},
}};
// clang-format on

struct TurnsCase
{
	int isCreature;
	uint32_t distance;
	uint32_t maxReactionDistance;
	uint32_t howImportantIsDistance;
	uint32_t impressiveType;
	int hasTown;
	uint32_t attitude;
	uint32_t turns;
	uint32_t action;
	uint32_t actionImpressiveType;
	uint32_t expected;
};

// The original's answers, a map cell being 10 metres. A NaN distance is left out: the original's conversion to a
// whole number gives 0 for it on the processor these answers were taken on and the smallest whole number on the
// others, which openblack follows
// clang-format off
constexpr std::array<TurnsCase, 46> k_TurnsToReactCases {{
	{1, 0x00000000u, 0x425C0000u, 0x3F000000u, 24u, 1, 0u, 300u, 0x00000000u, 24u, 0x0000012Cu},
	{1, 0x425C0000u, 0x425C0000u, 0x3F000000u, 24u, 1, 0u, 300u, 0x00000000u, 24u, 0x000000ADu},
	{1, 0x43020000u, 0x425C0000u, 0x3F000000u, 24u, 1, 0u, 300u, 0x00000000u, 24u, 0x00000000u},
	{1, 0x43480000u, 0x425C0000u, 0x3F000000u, 24u, 1, 0u, 300u, 0x00000000u, 24u, 0xFFFFFF5Fu},
	{1, 0x00000000u, 0x425C0000u, 0x3F000000u, 25u, 1, 0u, 300u, 0x00000000u, 24u, 0x00000258u},
	{1, 0x00000000u, 0x425C0000u, 0x3F000000u, 26u, 1, 0u, 300u, 0x00000000u, 24u, 0x00000384u},
	{1, 0x00000000u, 0x425C0000u, 0x3F000000u, 27u, 1, 0u, 300u, 0x00000000u, 24u, 0x0000012Cu},
	{1, 0x00000000u, 0x425C0000u, 0x3F000000u, 24u, 1, 3u, 300u, 0x00000000u, 24u, 0x000004B0u},
	{1, 0x00000000u, 0x425C0000u, 0x3F000000u, 24u, 1, 4u, 300u, 0x00000000u, 24u, 0x0000012Cu},
	{1, 0x00000000u, 0x425C0000u, 0x3F000000u, 24u, 1, 1u, 300u, 0x00000000u, 24u, 0x0000012Cu},
	{1, 0x00000000u, 0x425C0000u, 0x3F000000u, 24u, 1, 2u, 300u, 0x00000000u, 24u, 0x0000012Cu},
	{1, 0x00000000u, 0x425C0000u, 0x3F000000u, 24u, 1, 5u, 300u, 0x00000000u, 24u, 0x0000012Cu},
	{1, 0x00000000u, 0x425C0000u, 0x3F000000u, 24u, 0, 3u, 300u, 0x00000000u, 24u, 0x0000012Cu},
	{1, 0x41480000u, 0x425C0000u, 0x3F000000u, 26u, 1, 3u, 300u, 0x00000000u, 24u, 0x00000CB5u},
	{1, 0x00000000u, 0x425C0000u, 0x3F000000u, 24u, 1, 0u, 300u, 0x00000005u, 25u, 0x0000012Cu},
	{1, 0x00000000u, 0x425C0000u, 0x3F000000u, 24u, 1, 0u, 300u, 0x00000005u, 26u, 0x0000012Cu},
	{1, 0x00000000u, 0x425C0000u, 0x3F000000u, 24u, 1, 0u, 300u, 0x00000147u, 24u, 0x0000012Cu},
	{1, 0x00000000u, 0x425C0000u, 0x3F000000u, 24u, 1, 0u, 300u, 0x00000148u, 25u, 0x0000012Cu},
	{1, 0x00000000u, 0x425C0000u, 0x00000000u, 24u, 1, 0u, 300u, 0x00000000u, 24u, 0x0000012Cu},
	{1, 0x428C0000u, 0x425C0000u, 0x3F800000u, 24u, 1, 0u, 300u, 0x00000000u, 24u, 0xFFFFFFE9u},
	{1, 0x40400000u, 0x425C0000u, 0x3F000000u, 24u, 1, 0u, 16777217u, 0x00000000u, 24u, 0x00FA17A3u},
	{0, 0x00000000u, 0x425C0000u, 0x3F000000u, 24u, 1, 0u, 300u, 0x00000000u, 24u, 0x00000000u},
	{1, 0x42080605u, 0x42C088D0u, 0x3E015CE3u, 26u, 1, 0u, 280u, 0x0000007Eu, 24u, 0x00000326u},
	{1, 0x42706D18u, 0x407FF0EAu, 0x3EF98CC4u, 25u, 0, 4u, 393u, 0x000000CEu, 24u, 0xFFFFFCA6u},
	{1, 0x429187A4u, 0x4298496Cu, 0x3F337F57u, 26u, 1, 4u, 1847u, 0x000000C2u, 24u, 0x000008D3u},
	{1, 0x42C6E37Fu, 0x4129E9B3u, 0x3F50718Cu, 25u, 1, 4u, 1353u, 0x0000001Du, 25u, 0xFFFFE110u},
	{1, 0x420CA6CAu, 0x41184F9Cu, 0x3F6744B2u, 26u, 0, 1u, 1655u, 0x00000048u, 24u, 0xFFFFF3D5u},
	{1, 0x42A1EA2Du, 0x4096B047u, 0x3F33AA76u, 24u, 0, 4u, 1196u, 0x000000E3u, 25u, 0xFFFFF2A1u},
	{1, 0x42A7322Eu, 0x42AB4C3Fu, 0x3EDA2C65u, 24u, 0, 0u, 1413u, 0x000000A0u, 25u, 0x00000376u},
	{1, 0x430E7FFDu, 0x42A902F9u, 0x3F00ED55u, 24u, 1, 3u, 728u, 0x0000010Eu, 25u, 0x000002BCu},
	{1, 0x4298EFEFu, 0x4101216Fu, 0x3F673978u, 26u, 1, 1u, 499u, 0x00000006u, 25u, 0xFFFFEF80u},
	{1, 0x424DF053u, 0x41DB56CFu, 0x3ED8C821u, 25u, 0, 3u, 1825u, 0x000000D5u, 24u, 0x000005F3u},
	{1, 0x42C53B73u, 0x414828CCu, 0x3F6EBB31u, 24u, 1, 1u, 614u, 0x0000003Bu, 24u, 0xFFFFF89Au},
	{1, 0x42EEECE5u, 0x421AA8C7u, 0x3F561E18u, 26u, 1, 4u, 284u, 0x000000D1u, 26u, 0xFFFFFC7Fu},
	{1, 0x42744B00u, 0x410EF0E4u, 0x3D8F4F4Cu, 26u, 1, 0u, 174u, 0x00000036u, 25u, 0x00000194u},
	{1, 0x41B8633Eu, 0x42BB6B62u, 0x3EF9FA80u, 24u, 1, 0u, 34u, 0x00000047u, 25u, 0x0000001Eu},
	{1, 0x4267CCBEu, 0x42672C7Au, 0x3F3E6687u, 25u, 1, 3u, 6u, 0x000000FFu, 24u, 0x00000011u},
	{1, 0x4111992Cu, 0x42786F60u, 0x3F620610u, 26u, 1, 1u, 691u, 0x000000DCu, 26u, 0x00000732u},
	{1, 0x4191243Au, 0x41A6C38Fu, 0x3E655BDAu, 25u, 1, 1u, 249u, 0x00000037u, 26u, 0x000001B0u},
	{1, 0x42C25FDFu, 0x42906E3Au, 0x3F03B522u, 24u, 0, 4u, 806u, 0x000000B8u, 26u, 0x0000013Bu},
	{1, 0x42074FD1u, 0x41A46CEAu, 0x3EE47721u, 24u, 0, 1u, 9u, 0x00000133u, 24u, 0x00000004u},
	{1, 0x42C49050u, 0x42A631C8u, 0x3F072316u, 26u, 0, 1u, 469u, 0x000000A5u, 26u, 0x0000026Eu},
	{1, 0x42DF2F71u, 0x428228E9u, 0x3BDB0FC5u, 26u, 1, 3u, 1522u, 0x000000A4u, 26u, 0x000046A2u},
	{1, 0x4206336Eu, 0x41F2C39Eu, 0x3F0C1E62u, 26u, 0, 3u, 1741u, 0x000000A4u, 25u, 0x00000B1Du},
	{1, 0x42BB889Du, 0x42BF7F0Fu, 0x3F48849Bu, 25u, 1, 0u, 993u, 0x000000BDu, 26u, 0x0000025Eu},
	{1, 0x430FC99Cu, 0x42A9B78Cu, 0x3E9FBD4Du, 25u, 0, 4u, 785u, 0x0000010Cu, 25u, 0x0000033Bu},
}};

constexpr std::array<TurnsCase, 45> k_TurnsBeforeAgainCases {{
	{1, 0x00000000u, 0x425C0000u, 0x3F000000u, 24u, 1, 0u, 600u, 0x00000000u, 24u, 0x0000012Cu},
	{1, 0x425C0000u, 0x425C0000u, 0x3F000000u, 24u, 1, 0u, 600u, 0x00000000u, 24u, 0x00000229u},
	{1, 0x43020000u, 0x425C0000u, 0x3F000000u, 24u, 1, 0u, 600u, 0x00000000u, 24u, 0x00000384u},
	{1, 0x43480000u, 0x425C0000u, 0x3F000000u, 24u, 1, 0u, 600u, 0x00000000u, 24u, 0x000004C7u},
	{1, 0x00000000u, 0x425C0000u, 0x3F000000u, 25u, 1, 0u, 600u, 0x00000000u, 24u, 0x00000096u},
	{1, 0x00000000u, 0x425C0000u, 0x3F000000u, 26u, 1, 0u, 600u, 0x00000000u, 24u, 0x00000064u},
	{1, 0x00000000u, 0x425C0000u, 0x3F000000u, 27u, 1, 0u, 600u, 0x00000000u, 24u, 0x0000012Cu},
	{1, 0x00000000u, 0x425C0000u, 0x3F000000u, 24u, 1, 3u, 600u, 0x00000000u, 24u, 0x00000000u},
	{1, 0x00000000u, 0x425C0000u, 0x3F000000u, 24u, 1, 4u, 600u, 0x00000000u, 24u, 0x00000000u},
	{1, 0x00000000u, 0x425C0000u, 0x3F000000u, 24u, 1, 1u, 600u, 0x00000000u, 24u, 0x0000012Cu},
	{1, 0x00000000u, 0x425C0000u, 0x3F000000u, 24u, 1, 2u, 600u, 0x00000000u, 24u, 0x0000012Cu},
	{1, 0x00000000u, 0x425C0000u, 0x3F000000u, 24u, 1, 5u, 600u, 0x00000000u, 24u, 0x0000012Cu},
	{1, 0x00000000u, 0x425C0000u, 0x3F000000u, 24u, 0, 3u, 600u, 0x00000000u, 24u, 0x0000012Cu},
	{1, 0x41480000u, 0x425C0000u, 0x3F000000u, 26u, 1, 3u, 600u, 0x00000000u, 24u, 0x00000000u},
	{1, 0x00000000u, 0x425C0000u, 0x3F000000u, 24u, 1, 0u, 600u, 0x00000005u, 25u, 0x00000000u},
	{1, 0x00000000u, 0x425C0000u, 0x3F000000u, 24u, 1, 0u, 600u, 0x00000005u, 26u, 0x00000000u},
	{1, 0x00000000u, 0x425C0000u, 0x3F000000u, 24u, 1, 0u, 600u, 0x00000147u, 24u, 0x0000012Cu},
	{1, 0x00000000u, 0x425C0000u, 0x3F000000u, 24u, 1, 0u, 600u, 0x00000148u, 25u, 0x0000012Cu},
	{1, 0x00000000u, 0x425C0000u, 0x00000000u, 24u, 1, 0u, 600u, 0x00000000u, 24u, 0x00000258u},
	{1, 0x428C0000u, 0x425C0000u, 0x3F800000u, 24u, 1, 0u, 600u, 0x00000000u, 24u, 0x00000286u},
	{1, 0x40400000u, 0x425C0000u, 0x3F000000u, 24u, 1, 0u, 16777217u, 0x00000000u, 24u, 0x0085E860u},
	{1, 0x42424CB6u, 0x417EF1F2u, 0x3F26A3A4u, 24u, 1, 0u, 1193u, 0x0000001Du, 24u, 0x0000074Eu},
	{1, 0x40B3FAAAu, 0x422FB944u, 0x3D8F105Cu, 24u, 1, 4u, 1693u, 0x00000121u, 24u, 0x00000000u},
	{1, 0x430E1E12u, 0x427DBA55u, 0x3F153F49u, 24u, 1, 4u, 1999u, 0x00000071u, 24u, 0x00000000u},
	{1, 0x42A6FFDDu, 0x4162F2ECu, 0x3ED69965u, 26u, 0, 0u, 631u, 0x0000011Eu, 24u, 0x000001AAu},
	{1, 0x4177556Eu, 0x4266326Bu, 0x3E406143u, 24u, 0, 0u, 122u, 0x0000013Cu, 24u, 0x00000068u},
	{1, 0x4294ECA2u, 0x42568FACu, 0x3F46F877u, 25u, 1, 4u, 613u, 0x0000007Fu, 24u, 0x00000000u},
	{1, 0x42D1B2C6u, 0x41C9530Eu, 0x3F130D6Fu, 26u, 1, 4u, 1493u, 0x000000E5u, 25u, 0x00000000u},
	{1, 0x42B6B00Du, 0x4103F33Fu, 0x3F030E08u, 24u, 1, 3u, 1911u, 0x000000FAu, 26u, 0x00000000u},
	{1, 0x40BC31E1u, 0x42864E86u, 0x3F43BAEBu, 26u, 1, 3u, 1423u, 0x000000B3u, 26u, 0x00000000u},
	{1, 0x42ADF7F4u, 0x4238A846u, 0x3F570821u, 25u, 0, 4u, 1360u, 0x00000021u, 24u, 0x000003E1u},
	{1, 0x42DB5909u, 0x41FD3584u, 0x3F13F449u, 26u, 1, 4u, 1467u, 0x000000C5u, 25u, 0x00000000u},
	{1, 0x40589AA7u, 0x423AD4D2u, 0x3E2C14E0u, 24u, 1, 4u, 446u, 0x00000093u, 24u, 0x00000000u},
	{1, 0x42DD824Fu, 0x42219146u, 0x3F6AB478u, 25u, 1, 0u, 919u, 0x000000CDu, 25u, 0x00000000u},
	{1, 0x430481F0u, 0x42A437A8u, 0x3F5D2E16u, 25u, 1, 4u, 1398u, 0x000000C2u, 24u, 0x00000000u},
	{1, 0x41B51AE7u, 0x4193907Fu, 0x3E6D861Au, 24u, 1, 0u, 1702u, 0x0000012Du, 24u, 0x00000655u},
	{1, 0x421DA5E1u, 0x3FB3DFC8u, 0x3ED68028u, 25u, 1, 3u, 1414u, 0x00000107u, 24u, 0x00000000u},
	{1, 0x4288FE3Du, 0x42AE7435u, 0x3F73AED1u, 26u, 1, 4u, 817u, 0x000000C9u, 24u, 0x00000000u},
	{1, 0x429074F4u, 0x42229346u, 0x3E432F26u, 24u, 1, 4u, 225u, 0x000000AEu, 24u, 0x00000000u},
	{1, 0x4175B606u, 0x42647241u, 0x3F095FD8u, 25u, 1, 0u, 1790u, 0x0000006Au, 26u, 0x00000000u},
	{1, 0x41B242B6u, 0x41CFC9C4u, 0x3EB1DD0Bu, 25u, 1, 4u, 236u, 0x000000F9u, 26u, 0x00000000u},
	{1, 0x42901E58u, 0x41FEFCAEu, 0x3E139389u, 26u, 0, 3u, 542u, 0x000000F5u, 24u, 0x000000C7u},
	{1, 0x429AE67Eu, 0x41AA87C1u, 0x3F73B7A5u, 25u, 0, 1u, 1112u, 0x0000000Du, 25u, 0x00000000u},
	{1, 0x4312C673u, 0x42ACF038u, 0x3F3239F4u, 25u, 1, 3u, 728u, 0x00000072u, 25u, 0x00000000u},
	{1, 0x42BEEEBDu, 0x4276D6A0u, 0x3F49D489u, 24u, 1, 1u, 1515u, 0x00000074u, 24u, 0x00000776u},
}};

struct WholeCase
{
	uint32_t metres;
	int32_t whole;
};

constexpr std::array<WholeCase, 17> k_WholeCases {{
	{0x00000000u, 0},
	{0x3F000000u, 0},
	{0x3F7D70A4u, 0},
	{0x3F800000u, 6553},
	{0x3FC00000u, 6553},
	{0x40FCCCCDu, 45875},
	{0x41000000u, 52428},
	{0x41400000u, 78643},
	{0x419FEB85u, 124518},
	{0xBF000000u, 0},
	{0xBF800000u, -6553},
	{0xC06CCCCDu, -19660},
	{0x42C88000u, 655360},
	{0x449A5CCDu, 8087142},
	{0x454CCB33u, 21469594},
	{0x454CCCCDu, 21469594},
	{0x459C4000u, 32768000},
}};
// clang-format on

PriorityFacts FactsOf(const PriorityCase& c)
{
	const float raw = std::bit_cast<float>(c.raw) + std::bit_cast<float>(c.boost) + std::bit_cast<float>(c.boostA);
	return {
	    .dancing = c.dancing != 0,
	    .isCreature = c.isCreature != 0,
	    .ignoredByVillagers = c.ignored != 0,
	    .invisible = c.invisible != 0,
	    .distance = std::bit_cast<float>(c.distance),
	    .maxDistanceToRunAway = std::bit_cast<float>(c.maxDistanceToRunAway),
	    .hasTown = c.hasTown != 0,
	    .attitude = c.attitude,
	    .sameOwner = c.sameOwner != 0,
	    .disciple = c.disciple != 0,
	    .servedDesire = c.servedDesire,
	    .servedDesireAmount = std::bit_cast<float>(c.amount),
	    .townRawDesire = raw,
	    .priority = c.priority,
	    .fleePriority = c.fleePriority,
	    .followPriority = c.followPriority,
	};
}

TurnsFacts FactsOf(const TurnsCase& c)
{
	return {
	    .isCreature = c.isCreature != 0,
	    .distance = std::bit_cast<float>(c.distance),
	    .metresPerCell = 10.0f,
	    .maxReactionDistance = std::bit_cast<float>(c.maxReactionDistance),
	    .howImportantIsDistance = std::bit_cast<float>(c.howImportantIsDistance),
	    .impressiveType = c.impressiveType,
	    .currentActionImpressiveType = c.action < k_ActionRows ? std::optional(c.actionImpressiveType) : std::nullopt,
	    .townAttitude = c.hasTown != 0 ? std::optional(c.attitude) : std::nullopt,
	    .turns = c.turns,
	};
}
} // namespace

TEST(VillagerCreatureReactions, AVillagersPriorityForACreatureIsTheOriginals)
{
	for (size_t i = 0; i < k_PriorityCases.size(); ++i)
	{
		EXPECT_EQ(VillagerPriority(FactsOf(k_PriorityCases.at(i))), k_PriorityCases.at(i).expected) << "case " << i;
	}
}

TEST(VillagerCreatureReactions, AFearedCreaturesPriorityKeepsOnlyItsLowByte)
{
	// twice 150 is 300, of which the low byte is 44
	const PriorityFacts facts {
	    .distance = 10.0f, .maxDistanceToRunAway = 100.0f, .hasTown = true, .attitude = attitude::k_Fear, .fleePriority = 150};
	EXPECT_EQ(VillagerPriority(facts), 44);
}

TEST(VillagerCreatureReactions, TheTurnsToReactToACreatureAreTheOriginals)
{
	for (size_t i = 0; i < k_TurnsToReactCases.size(); ++i)
	{
		EXPECT_EQ(TurnsToReact(FactsOf(k_TurnsToReactCases.at(i))), k_TurnsToReactCases.at(i).expected) << "case " << i;
	}
}

TEST(VillagerCreatureReactions, TheTurnsBeforeReactingAgainToACreatureAreTheOriginals)
{
	for (size_t i = 0; i < k_TurnsBeforeAgainCases.size(); ++i)
	{
		EXPECT_EQ(TurnsBeforeReactingAgain(FactsOf(k_TurnsBeforeAgainCases.at(i))), k_TurnsBeforeAgainCases.at(i).expected)
		    << "case " << i;
	}
}

TEST(VillagerCreatureReactions, AWalkUpToACreatureCutsItsMetresToAWholeNumberFirst)
{
	for (const auto& c : k_WholeCases)
	{
		EXPECT_EQ(MetresToWholeDistance(std::bit_cast<float>(c.metres)), c.whole) << std::bit_cast<float>(c.metres);
	}
}

TEST(VillagerCreatureReactions, ATownsAttitudeChoosesHowAVillagerTakesTheReactionUp)
{
	EXPECT_EQ(SetupState(attitude::k_New), VillagerStates::InspectCreatureReaction);
	EXPECT_EQ(SetupState(attitude::k_None), std::nullopt);
	EXPECT_EQ(SetupState(2), VillagerStates::InspectCreatureReaction);
	EXPECT_EQ(SetupState(attitude::k_Fear), VillagerStates::FleeingFromCreatureReaction);
	EXPECT_EQ(SetupState(attitude::k_Respect), VillagerStates::InspectCreatureReaction);
	EXPECT_EQ(SetupState(5), std::nullopt);
}

TEST(VillagerCreatureReactions, AnInspectionLooksWalksCloserAndEndsNearEnough)
{
	EXPECT_EQ(InspectTurns(15, 29), 44u);
	EXPECT_EQ(PerformInspectStep(3, 20.0f), InspectStep::Look);
	EXPECT_EQ(PerformInspectStep(0, 20.0f), InspectStep::Approach);
	EXPECT_EQ(PerformInspectStep(0, 1.0f), InspectStep::Approach);
	EXPECT_EQ(PerformInspectStep(3, 0.99f), InspectStep::Wait);
	EXPECT_EQ(PerformInspectStep(0, -4.0f), InspectStep::Stop);
	// 20, 12, 4 and then below one metre
	EXPECT_EQ(NextInspectDistance(NextInspectDistance(k_InspectStartDistance)), 4.0f);
	EXPECT_TRUE(ApproachFurther(21.5f, 20.0f));
	EXPECT_FALSE(ApproachFurther(21.0f, 20.0f));
	EXPECT_FALSE(ApproachFurther(std::bit_cast<float>(0x7FC00000u), 20.0f));
	EXPECT_TRUE(Arrived(0.5f));
	EXPECT_FALSE(Arrived(0.51f));
	EXPECT_TRUE(Arrived(std::bit_cast<float>(0x7FC00000u)));
}

TEST(VillagerCreatureReactions, TheWalkUpToACreatureGoesAlongTheAngleToIt)
{
	// 31.9 metres off at 20 is 11 whole metres (1.1 cells) along angle 0, the x axis
	EXPECT_EQ(MetresToWholeDistance(11.9f), 72089);
	const auto step = ApproachStep(0, 31.9f, 20.0f);
	EXPECT_EQ(step.x, ((72089 >> 8) * 65536) >> 8);
	EXPECT_EQ(step.y, 0);
}

TEST(VillagerCreatureReactions, ARespectfulVillagerFacesTheCreatureAndRunsFromAFearedOne)
{
	EXPECT_FALSE(FacingAway(300, 44));
	EXPECT_TRUE(FacingAway(300, 43));
	// no wrap round the turn: facing angle 2047 for a creature at angle 1 reads as far away
	EXPECT_TRUE(FacingAway(1, 2047));
	EXPECT_EQ(FleeDistance(55.0f), 66.0f);
	EXPECT_FALSE(FleeGivesUp(66.0f, 99.0f));
	EXPECT_TRUE(FleeGivesUp(66.0f, 99.01f));
	EXPECT_TRUE(FleeGivesUp(66.0f, std::bit_cast<float>(0x7FC00000u)));
	EXPECT_EQ(HalfSpeed(7), 3);
	EXPECT_EQ(HalfSpeed(-7), -3);
	const map_coords::MapCoords edge {.x = 65536, .z = 8090858};
	const auto goal = RespectWalkGoal(edge);
	EXPECT_EQ(goal.x, 65536);
	EXPECT_EQ(goal.z, 8090857);
	EXPECT_EQ(goal.altitude, 0.0f);
}

TEST(VillagerCreatureReactions, ATownsChangeOfAttitudeMovesTheVillagerOn)
{
	using enum VillagerStates;
	EXPECT_EQ(StepFromAttitude(attitude::k_None, InspectCreatureReaction), AttitudeStep::Stop);
	EXPECT_EQ(StepFromAttitude(attitude::k_Fear, PerformInspectCreatureReaction), AttitudeStep::Flee);
	EXPECT_EQ(StepFromAttitude(attitude::k_Fear, FleeingFromCreatureReaction), AttitudeStep::Keep);
	EXPECT_EQ(StepFromAttitude(attitude::k_Respect, ApproachCreatureReaction), AttitudeStep::Respect);
	EXPECT_EQ(StepFromAttitude(attitude::k_Respect, InitialiseRespectCreatureReaction), AttitudeStep::Keep);
	EXPECT_EQ(StepFromAttitude(attitude::k_Respect, FinishRespectCreatureReaction), AttitudeStep::Keep);
	EXPECT_EQ(StepFromAttitude(attitude::k_Respect, ApproachHandReaction), AttitudeStep::Respect);
	EXPECT_EQ(StepFromAttitude(attitude::k_Respect, TurnToFaceCreatureReaction), AttitudeStep::Keep);
	EXPECT_EQ(StepFromAttitude(attitude::k_New, InspectCreatureReaction), AttitudeStep::Keep);
	EXPECT_EQ(StepFromAttitude(2, InspectCreatureReaction), AttitudeStep::Keep);
}
