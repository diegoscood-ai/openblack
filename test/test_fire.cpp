/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The fire's graphic (ECS/Fire/FireGraphic): the charring grey, checked against the original's integer code for every
// charring byte. The deed a player's miracle shows (Magic/MiracleDeeds), for every magic type and what it reached. A
// fire copied or moved from one object to another.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>

#include <array>
#include <bit>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/Components/MagicFireBall.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Fire/FireGraphic.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/FireSystem.h"
#include "Enums.h"
#include "Fire/ViaPoint.h"
#include "Locator.h"
#include "Magic/MiracleDeeds.h"
#include "support/WorldSystems.h"

using namespace openblack::ecs;
using openblack::MagicType;
namespace magic = openblack::magic;

namespace
{
/// The blue byte as the original computes it: -(k x 7 x 25), shifted right by 8, minus 1
uint8_t OriginalBlue(uint32_t k)
{
	const uint32_t ecx = 0u - k * 7u * 25u;
	return static_cast<uint8_t>((ecx >> 8) - 1u);
}
/// the green byte: the same value shifted left then right by 8, minus 0x100, its second byte
uint8_t OriginalGreen(uint32_t k)
{
	const uint32_t ecx = (((0u - k * 7u * 25u) << 8) >> 8) - 0x100u;
	return static_cast<uint8_t>(ecx >> 8);
}
/// the red byte: the same value shifted left by 16 then right by 8, minus 0x10000, bits 16..23
uint8_t OriginalRed(uint32_t k)
{
	const uint32_t eax = (((0u - k * 7u * 25u) << 16) >> 8) - 0x10000u;
	return static_cast<uint8_t>(eax >> 16);
}
} // namespace

TEST(FireGraphic, CharringGreyMatchesTheExe)
{
	fire::FireEffect effect;
	for (uint32_t k = 0; k < 256; ++k)
	{
		// a charring whose (x 255) truncated is exactly k
		effect.charring = static_cast<float>(k) / 255.0f;
		ASSERT_EQ(static_cast<uint32_t>(static_cast<int>(effect.charring * 255.0f) & 0xFF), k);
		const auto grey = fire::graphic::CharringGrey(effect);
		EXPECT_EQ(grey, OriginalBlue(k)) << "k " << k;
		EXPECT_EQ(grey, OriginalGreen(k)) << "k " << k;
		EXPECT_EQ(grey, OriginalRed(k)) << "k " << k;
	}
}

TEST(FireGraphic, BurningTreeAlphaRefMatchesTheExe)
{
	// {T, Tc, the ALPHAREF}: the original's burning tree draw run natively for these temperatures
	struct Case
	{
		float temperature;
		float combustion;
		uint8_t ref;
	};
	constexpr std::array<Case, 18> k_Cases = {{
	    {0.0f, 100.0f, 180},
	    {10.0f, 100.0f, 185},
	    {25.0f, 100.0f, 192},
	    {37.0f, 100.0f, 198},
	    {50.0f, 100.0f, 205},
	    {99.9f, 100.0f, 230},
	    {100.0f, 100.0f, 230},
	    {125.0f, 100.0f, 242},
	    {149.99f, 100.0f, 254},
	    {150.0f, 100.0f, 254},
	    {200.0f, 100.0f, 254},
	    {0.0f, 777.7f, 180},
	    {77.77f, 777.7f, 185},
	    {583.275f, 777.7f, 217},
	    {972.125f, 777.7f, 242},
	    {1166.4723f, 777.7f, 254},
	    {0.001f, 0.002f, 205},
	    {12345.0f, 10000.0f, 241},
	}};
	for (const auto& c : k_Cases)
	{
		EXPECT_EQ(fire::graphic::BurningTreeAlphaRef(c.temperature, c.combustion), c.ref)
		    << "T " << c.temperature << " Tc " << c.combustion;
	}
}

TEST(FireGraphic, FireballSteamsInsteadOfFlames)
{
	// the fire graphic's initial flags: all of bits 1-4 for an ordinary object; a MagicFireBall keeps only the steam
	// (bit 3)
	openblack::Locator::entitiesRegistry::emplace<openblack::ecs::Registry>();
	openblack::test::EmplaceWorldSystems();
	auto& registry = openblack::Locator::entitiesRegistry::value();
	const auto rock = registry.Create();
	const auto ball = registry.Create();
	registry.Assign<components::MagicFireBall>(ball);
	EXPECT_EQ(fire::graphic::InitialFlags(rock), 0x1E);
	EXPECT_EQ(fire::graphic::InitialFlags(ball), 0x08);
	openblack::Locator::entitiesRegistry::reset();
	openblack::test::ResetWorldSystems();
}

TEST(FireGraphic, CharringGreyEnds)
{
	fire::FireEffect effect;
	effect.charring = 0.0f;
	EXPECT_EQ(fire::graphic::CharringGrey(effect), 255);
	effect.charring = 1.0f;
	EXPECT_EQ(fire::graphic::CharringGrey(effect), 80); // 255 - ceil(175 x 255 / 256)
}

TEST(FireEffect, NoPlayerUntilOneIsGiven)
{
	// a fire starts with no player (as the old hasPlayer false); a player given keeps its value
	fire::FireEffect effect;
	EXPECT_FALSE(effect.player.has_value());
	EXPECT_EQ(effect.player.value_or(openblack::PlayerNames::NEUTRAL), openblack::PlayerNames::NEUTRAL);
	effect.player = openblack::PlayerNames::PLAYER_TWO;
	EXPECT_EQ(effect.player.value_or(openblack::PlayerNames::NEUTRAL), openblack::PlayerNames::PLAYER_TWO);
}

namespace
{
using openblack::map_coords::MapCoords;

/// A point in metres on the map
MapCoords At(float x, float z)
{
	return {openblack::map_coords::ToFixed(x), openblack::map_coords::ToFixed(z), 0.0f};
}

/// One way round a circle as the original game works it out: the inputs and the outputs as their bits
struct ViaPointCase
{
	uint32_t fromX, fromZ, toX, toZ, centreX, centreZ, radius, margin, side;
	uint32_t angle, pointX, pointZ;
	bool detour, inside;
};

// Run through the original game's own code. The point is only written when there is a detour
constexpr std::array k_ViaPointCases = std::to_array<ViaPointCase>({
    {0x00C80000, 0x00C80000, 0x00CC0000, 0x00C81999, 0x00CA0000, 0x00C80000, 0x40A00000, 0x3F800000, 0x00000000, 0xBCCCBFC3,
     0x00C9D1EC, 0x00C89286, true, false},
    {0x00C80000, 0x00C80000, 0x00CC0000, 0x00C81999, 0x00CA0000, 0x00C80000, 0x40A00000, 0x3F800000, 0xBF000000, 0xBCCCBFC3,
     0x00C9D1EC, 0x00C89286, true, false},
    {0x00C80000, 0x00C80000, 0x00CC0000, 0x00C81999, 0x00CA0000, 0x00C80000, 0x40A00000, 0x3F800000, 0x3F000000, 0x3CCCBFC3,
     0x00C9D1EC, 0x00C76D7A, true, false},
    {0x00C80000, 0x00C80000, 0x00CC0000, 0x00C7E667, 0x00CA0000, 0x00C80000, 0x40A00000, 0x3F800000, 0x00000000, 0x3CCCBFC3,
     0x00C9D1EC, 0x00C76D7A, true, false},
    {0x00C80000, 0x00CA0000, 0x00CC0000, 0x00CA0000, 0x00CA0000, 0x00C80000, 0x40A00000, 0x3F800000, 0x00000000, 0x00000000,
     0x11111111, 0x11111111, false, false},
    {0x00C9E666, 0x00C80000, 0x00CC0000, 0x00C80000, 0x00CA0000, 0x00C80000, 0x40A00000, 0x3F800000, 0x00000000, 0x00000000,
     0x11111111, 0x11111111, false, true},
    {0x00C80000, 0x00C80000, 0x00CA0000, 0x00C81999, 0x00CA0000, 0x00C80000, 0x40A00000, 0x3F800000, 0x00000000, 0xBD4C9BD1,
     0x00C9D1EC, 0x00C89286, true, true},
    {0x00C80000, 0x00C80000, 0x00C80000, 0x00C80000, 0x00CA0000, 0x00C80000, 0x40A00000, 0x3F800000, 0x00000000, 0x00000000,
     0x11111111, 0x11111111, false, false},
    {0x00C80000, 0x00C80000, 0x00CC0000, 0x00C80000, 0x00C88CCC, 0x00C80000, 0x40A00000, 0x3F800000, 0x00000000, 0x00000000,
     0x11111111, 0x11111111, false, false},
    {0x00C80000, 0x00C80000, 0x00C8CCCC, 0x00C80000, 0x00CA0000, 0x00C80000, 0x40A00000, 0x3F800000, 0x00000000, 0x00000000,
     0x00C9D1EC, 0x00C76D7A, true, false},
    {0x00C80000, 0x00C80000, 0x00CC0000, 0x00C80000, 0x00CA0000, 0x00C80000, 0x00000000, 0x3F800000, 0x00000000, 0x00000000,
     0x00C9FEB8, 0x00C7E66F, true, false},
    {0x00C80000, 0x00C80000, 0x00CC0000, 0x00C80000, 0x00CA0000, 0x00C80000, 0x40A00000, 0x00000000, 0x00000000, 0x00000000,
     0x00C9E000, 0x00C78411, true, false},
    {0x01894C13, 0x01B96155, 0x0185AF60, 0x01B8BDB9, 0x0186BEFC, 0x01B953E4, 0x410C71BB, 0x3FF9DB95, 0x00000000, 0xBE1E4032,
     0x01873798, 0x01B85D1A, true, false},
    {0x01110678, 0x00CD9BE3, 0x011220FC, 0x00CD994F, 0x01122C24, 0x00CD9ABB, 0x4115C62B, 0x3F0DE82A, 0x00000000, 0x3BAAD55D,
     0x01115040, 0x00CD1BEF, true, true},
    {0x0018FE25, 0x00368C9F, 0x001BA174, 0x0032A963, 0x001B85DF, 0x0032992B, 0x3FF47846, 0x3FE350A3, 0x00000000, 0xBCD9D04D,
     0x001BD116, 0x0032D223, true, true},
    {0x00198972, 0x001EBD37, 0x001C89D2, 0x0018C9A1, 0x001A6D5D, 0x001DE1F7, 0x3FF3F790, 0x40289C9F, 0x00000000, 0x3EACE4AA,
     0x001A03A1, 0x001DB1AD, true, false},
    {0x0111CA55, 0x006D6CBB, 0x01107055, 0x006DFBF6, 0x01102D62, 0x006E2C1C, 0x412DE369, 0x3FFE1E1E, 0x3E626FE3, 0x3D29F712,
     0x011164CE, 0x006E9658, true, true},
    {0x01C29549, 0x005469CE, 0x01BC8664, 0x005070FA, 0x01C12CB2, 0x0053E66F, 0x40C6EA79, 0x3F2EAFB2, 0x402F995A, 0x3E6C7ECD,
     0x01C14362, 0x00549592, true, false},
    {0x00D09F77, 0x00C799B7, 0x00D45BD6, 0x00CAF066, 0x00D419B5, 0x00CB2D65, 0x40C1E1B8, 0x3FDCB645, 0xC01E0900, 0xBD8FB776,
     0x00D376ED, 0x00CBA04C, true, true},
    {0x008C3098, 0x01EBB02E, 0x008F1A8D, 0x01ED67C1, 0x008E83D2, 0x01EA28B3, 0x40B52CC7, 0x40169F15, 0x80000000, 0x00000000,
     0x11111111, 0x11111111, false, false},
    {0x018BA2CA, 0x00F4B1A4, 0x018E18DC, 0x00F8CA81, 0x018A5D83, 0x00F0B8CC, 0x411B522C, 0x4036812C, 0x3F61FE4A, 0x00000000,
     0x11111111, 0x11111111, false, false},
    {0x0036BE9F, 0x0017026B, 0x0036B79A, 0x000F49BC, 0x00363AD8, 0x001386B0, 0x3ECD6F65, 0x3FA15E89, 0x00000000, 0x00000000,
     0x11111111, 0x11111111, false, false},
});
} // namespace

TEST(ViaPoint, MatchesTheOriginalGame)
{
	for (const auto& c : k_ViaPointCases)
	{
		const MapCoords from {static_cast<int32_t>(c.fromX), static_cast<int32_t>(c.fromZ), 0.0f};
		const MapCoords to {static_cast<int32_t>(c.toX), static_cast<int32_t>(c.toZ), 0.0f};
		const MapCoords centre {static_cast<int32_t>(c.centreX), static_cast<int32_t>(c.centreZ), 0.0f};
		const auto via = openblack::fire::GetViaPoint(from, to, centre, std::bit_cast<float>(c.radius),
		                                              std::bit_cast<float>(c.margin), std::bit_cast<float>(c.side));
		const float angle = std::bit_cast<float>(c.angle);
		EXPECT_EQ(via.detour, c.detour) << c.fromX << " " << c.fromZ;
		EXPECT_EQ(via.inside, c.inside) << c.fromX << " " << c.fromZ;
		// Only whether the angle is 0 and its sign are used
		EXPECT_EQ(via.angle == 0.0f, angle == 0.0f) << c.fromX << " " << c.fromZ;
		if (angle != 0.0f)
		{
			EXPECT_EQ(std::signbit(via.angle), std::signbit(angle)) << c.fromX << " " << c.fromZ;
		}
		if (c.detour)
		{
			EXPECT_EQ(static_cast<uint32_t>(via.point.x), c.pointX) << c.fromX << " " << c.fromZ;
			EXPECT_EQ(static_cast<uint32_t>(via.point.z), c.pointZ) << c.fromX << " " << c.fromZ;
		}
	}
}

TEST(ViaPoint, AVillagerGoesRoundAFireInItsWayOnTheSideItLeansTo)
{
	// A fire of 5 m in the middle of the way from 0 to 40 along x, passed by a margin of 1 m
	const auto via = openblack::fire::GetViaPoint(At(0.0f, 0.0f), At(40.0f, 1.0f), At(20.0f, 0.0f), 5.0f, 1.0f, 0.0f);
	EXPECT_TRUE(via.detour);
	EXPECT_FALSE(via.inside);
	EXPECT_NE(via.angle, 0.0f);
	// The point lies on the tangent from the start to the circle grown by the margin: 6 m from the centre
	const auto dx = static_cast<float>(via.point.x - openblack::map_coords::ToFixed(20.0f));
	const auto dz = static_cast<float>(via.point.z);
	EXPECT_NEAR(std::sqrt(dx * dx + dz * dz) / openblack::map_coords::k_FixedPerMetre, 6.0f, 0.01f);
	// Kept to the same side when asked, and to the other when the last detour turned the other way
	const auto again = openblack::fire::GetViaPoint(At(0.0f, 0.0f), At(40.0f, 1.0f), At(20.0f, 0.0f), 5.0f, 1.0f, via.angle);
	EXPECT_EQ(again.point.z > 0, via.point.z > 0);
	const auto other = openblack::fire::GetViaPoint(At(0.0f, 0.0f), At(40.0f, 1.0f), At(20.0f, 0.0f), 5.0f, 1.0f, -via.angle);
	EXPECT_NE(other.point.z > 0, via.point.z > 0);
	// A way that clears the circle needs no detour, and a start inside it none either
	EXPECT_FALSE(openblack::fire::GetViaPoint(At(0.0f, 20.0f), At(40.0f, 20.0f), At(20.0f, 0.0f), 5.0f, 1.0f, 0.0f).detour);
	EXPECT_TRUE(openblack::fire::GetViaPoint(At(19.0f, 0.0f), At(40.0f, 0.0f), At(20.0f, 0.0f), 5.0f, 1.0f, 0.0f).inside);
}

TEST(ViaPoint, NoDetourFromWithinTheMargin)
{
	// Outside the fire but closer than its radius and the margin: no detour, and not inside either
	const auto via = openblack::fire::GetViaPoint(At(0.0f, 0.0f), At(40.0f, 0.0f), At(5.5f, 0.0f), 5.0f, 1.0f, 0.0f);
	EXPECT_FALSE(via.detour);
	EXPECT_FALSE(via.inside);
	EXPECT_EQ(via.angle, 0.0f);
	// Nor without a way to go
	EXPECT_FALSE(openblack::fire::GetViaPoint(At(0.0f, 0.0f), At(0.0f, 0.0f), At(20.0f, 0.0f), 5.0f, 1.0f, 0.0f).detour);
}

TEST(WayRound, AVillagerGoesRoundEveryFireOfTheGroup)
{
	using openblack::fire::Circle;
	using openblack::fire::WayRoundOutcome;
	const auto clears = [](const MapCoords& from, const MapCoords& to, const Circle& circle, float margin) {
		const auto via = openblack::fire::GetViaPoint(from, to, circle.centre, circle.radius, margin, 0.0f);
		return via.angle == 0.0f && !(via.detour && via.inside);
	};
	const Circle fire {At(20.0f, 0.0f), 5.0f};
	// A clear way: on to where it was going
	auto way = openblack::fire::FindWayRound(At(0.0f, 20.0f), At(40.0f, 20.0f), fire, std::vector {fire}, 1.0f);
	EXPECT_EQ(way.outcome, WayRoundOutcome::Walk);
	EXPECT_EQ(way.target, At(40.0f, 20.0f));
	// A fire in the way: to the point round it
	way = openblack::fire::FindWayRound(At(0.0f, 0.0f), At(40.0f, 1.0f), fire, std::vector {fire}, 1.0f);
	EXPECT_EQ(way.outcome, WayRoundOutcome::Walk);
	EXPECT_EQ(way.target,
	          openblack::fire::GetViaPoint(At(0.0f, 0.0f), At(40.0f, 1.0f), fire.centre, fire.radius, 1.0f, 0.0f).point);
	// A second fire of the group on the way round the first: round that one too, so the way clears both
	const std::vector group {fire, Circle {At(10.0f, 4.0f), 2.0f}};
	way = openblack::fire::FindWayRound(At(0.0f, 0.0f), At(40.0f, 1.0f), fire, group, 1.0f);
	EXPECT_EQ(way.outcome, WayRoundOutcome::Walk);
	for (const auto& circle : group)
	{
		EXPECT_TRUE(clears(At(0.0f, 0.0f), way.target, circle, 1.0f));
	}
	// Going into the fire: it stops at the point round it
	way = openblack::fire::FindWayRound(At(0.0f, 0.0f), At(21.0f, 1.0f), fire, std::vector {fire}, 1.0f);
	EXPECT_EQ(way.outcome, WayRoundOutcome::Stop);
	EXPECT_EQ(way.target,
	          openblack::fire::GetViaPoint(At(0.0f, 0.0f), At(21.0f, 1.0f), fire.centre, fire.radius, 1.0f, 0.0f).point);
}

TEST(WayRound, AVillagerRingedByFiresGivesUp)
{
	using openblack::fire::Circle;
	using openblack::fire::WayRoundOutcome;
	// Eight fires round the villager, each overlapping the next: every way round one leads into another, all the way
	// round, until it gives up
	std::vector<Circle> ring;
	for (int k = 0; k < 8; ++k)
	{
		const float angle = static_cast<float>(k) * 0.785398163f;
		ring.push_back({At(100.0f + 10.0f * std::cos(angle), 100.0f + 10.0f * std::sin(angle)), 4.0f});
	}
	EXPECT_EQ(openblack::fire::FindWayRound(At(100.0f, 100.0f), At(130.0f, 100.5f), ring[0], ring, 1.0f).outcome,
	          WayRoundOutcome::GiveUp);
	// With a gap in the ring it finds its way out
	const std::vector<Circle> gap(ring.begin(), ring.begin() + 6);
	EXPECT_EQ(openblack::fire::FindWayRound(At(100.0f, 100.0f), At(130.0f, 100.5f), gap[0], gap, 1.0f).outcome,
	          WayRoundOutcome::Walk);
}

TEST(MiracleDeeds, TheLastThingAMiracleReachedSaysWhatThePlayerDid)
{
	using magic::MiracleDeed;
	const magic::DeedTarget nothing {};
	// Near another player's town a miracle not meant to anger is an attempt to impress it
	EXPECT_EQ(MiracleDeed(MagicType::Water, false, true, nothing), magic::k_DeedImpressWithMagic);
	EXPECT_EQ(MiracleDeed(MagicType::Fireball, true, true, nothing), magic::k_DeedDamageWithFire);
	// The water: a field is crops, something burning a fire put out, anything else crops too
	EXPECT_EQ(MiracleDeed(MagicType::WaterPowerUpOne, false, false, {.field = true, .onFire = true}),
	          magic::k_DeedCastWaterOnCrops);
	EXPECT_EQ(MiracleDeed(MagicType::Water, false, false, {.onFire = true}), magic::k_DeedCastWaterToPutOutFire);
	EXPECT_EQ(MiracleDeed(MagicType::Water, false, false, nothing), magic::k_DeedCastWaterOnCrops);
	// The blast's second power-up shows nothing; the others damage with magic
	EXPECT_EQ(MiracleDeed(MagicType::ExplosionOnePuOne, true, false, nothing), magic::k_DeedDamageWithMagic);
	EXPECT_EQ(MiracleDeed(MagicType::ExplosionOnePuTwo, true, false, nothing), magic::k_NoDeed);
	EXPECT_EQ(MiracleDeed(MagicType::Food, false, false, {.storagePit = true}), magic::k_DeedCastFoodInStoragePit);
	EXPECT_EQ(MiracleDeed(MagicType::Wood, false, false, nothing), magic::k_NoDeed);
	EXPECT_EQ(MiracleDeed(MagicType::FlockGround, true, false, nothing), magic::k_DeedImpressWithMagic);
}

TEST(MiracleDeeds, EveryMagicTypeShowsTheDeedOfTheOriginalsTable)
{
	using magic::MiracleDeed;
	// The deed of each magic type 0..30 with no town about and nothing special reached, as the original's table has it
	constexpr std::array<uint32_t, 31> k_Plain {46, 17, 17, 17, 18, 18, 18, 18, 18, 46, 42, 42, 46, 46, 46, 46,
	                                            46, 46, 46, 46, 46, 46, 33, 33, 20, 20, 46, 46, 46, 46, 46};
	for (uint32_t type = 0; type < k_Plain.size(); ++type)
	{
		const auto magicType = static_cast<MagicType>(type);
		EXPECT_EQ(MiracleDeed(magicType, false, false, {}), k_Plain.at(type)) << "magic type " << type;
		// Another player's town about makes every one an attempt to impress, unless it is meant to anger
		EXPECT_EQ(MiracleDeed(magicType, false, true, {}), magic::k_DeedImpressWithMagic) << "magic type " << type;
		EXPECT_EQ(MiracleDeed(magicType, true, true, {}), k_Plain.at(type)) << "magic type " << type;
	}
	// The food: a worship site before a storage pit, nothing else
	EXPECT_EQ(MiracleDeed(MagicType::FoodPowerUpOne, false, false, {.worshipSite = true}), 1u);
	EXPECT_EQ(MiracleDeed(MagicType::Food, false, false, {.storagePit = true, .worshipSite = true}), 1u);
	EXPECT_EQ(MiracleDeed(MagicType::FoodPowerUpOne, false, false, {.storagePit = true}), 3u);
	EXPECT_EQ(MiracleDeed(MagicType::Food, false, false, {.buildingBeingBuilt = true}), 46u);
	// The wood: a storage pit before a building going up
	EXPECT_EQ(MiracleDeed(MagicType::Wood, false, false, {.storagePit = true, .buildingBeingBuilt = true}), 5u);
	EXPECT_EQ(MiracleDeed(MagicType::Wood, false, false, {.buildingBeingBuilt = true}), 8u);
	EXPECT_EQ(MiracleDeed(MagicType::Wood, false, false, {.worshipSite = true}), 46u);
	// The water: a field before a fire
	EXPECT_EQ(MiracleDeed(MagicType::Water, false, false, {.field = true}), 33u);
	EXPECT_EQ(MiracleDeed(MagicType::WaterPowerUpOne, false, false, {.onFire = true}), 34u);
	EXPECT_EQ(MiracleDeed(MagicType::Water, false, false, {.storagePit = true}), 33u);
	// Any other magic type is the same whatever it reached
	const magic::DeedTarget everything {
	    .field = true, .onFire = true, .storagePit = true, .worshipSite = true, .buildingBeingBuilt = true};
	EXPECT_EQ(MiracleDeed(MagicType::Fireball, false, false, everything), 17u);
	EXPECT_EQ(MiracleDeed(MagicType::Heal, false, false, everything), 42u);
	EXPECT_EQ(MiracleDeed(MagicType::Forest, false, false, everything), 46u);
	EXPECT_EQ(MiracleDeed(MagicType::FlockFlying, false, false, everything), 20u);
}

namespace
{
/// The game's fires, real, over a registry of their own; fires are put on objects by hand, at a temperature
class FirePassing: public ::testing::Test
{
protected:
	void SetUp() override
	{
		openblack::Locator::entitiesRegistry::emplace<Registry>();
		fires = &openblack::Locator::fireSystem::emplace<systems::FireSystem>();
	}
	void TearDown() override
	{
		// the registry first, as the game's shutdown does
		openblack::Locator::entitiesRegistry::reset();
		openblack::Locator::fireSystem::reset();
	}

	[[nodiscard]] static entt::entity Object() { return openblack::Locator::entitiesRegistry::value().Create(); }

	/// The object burns at a temperature, with what it was before
	fire::FireEffect& Burn(entt::entity object, float temperature, float previous)
	{
		auto made = std::make_unique<fire::FireEffect>();
		made->id = fires->TakeId();
		made->object = object;
		made->temperature = temperature;
		made->previous = previous;
		return fires->Insert(std::move(made));
	}

	systems::FireSystemInterface* fires {nullptr};
};
} // namespace

// With no object info the half has no combustion temperature, so the copy makes no fire for it
TEST_F(FirePassing, AFireIsNotCopiedOntoAnObjectWithNoInfo)
{
	const auto rock = Object();
	const auto half = Object();
	Burn(rock, 900.0f, 850.0f);
	fires->CopyFire(rock, half);
	EXPECT_EQ(fires->FindByObject(half), nullptr);
}

TEST_F(FirePassing, ACopiedFireJoinsTheBlazeAndTakesTheHotterTemperature)
{
	const auto rock = Object();
	const auto hotter = Object();
	const auto cooler = Object();
	auto& source = Burn(rock, 900.0f, 850.0f);
	auto& hot = Burn(hotter, 1200.0f, 1100.0f);
	auto& cool = Burn(cooler, 500.0f, 450.0f);

	// a hotter one keeps its own temperature, now as its last one too
	fires->CopyFire(rock, hotter);
	EXPECT_EQ(hot.root, &source);
	EXPECT_EQ(hot.temperature, 1200.0f);
	EXPECT_EQ(hot.previous, 1200.0f);
	// a cooler one is raised to the source's
	fires->CopyFire(rock, cooler);
	EXPECT_EQ(cool.root, &source);
	EXPECT_EQ(cool.temperature, 900.0f);
	EXPECT_EQ(cool.previous, 900.0f);
	// the source itself is left as it was
	EXPECT_EQ(source.root, &source);
	EXPECT_EQ(source.temperature, 900.0f);
	EXPECT_EQ(source.previous, 850.0f);
	// a fire not on anything copies nothing
	const auto unlit = Object();
	const auto other = Object();
	fires->CopyFire(unlit, other);
	EXPECT_EQ(fires->FindByObject(other), nullptr);
}

TEST_F(FirePassing, AMovedFireGoesWholeToTheNewObject)
{
	const auto tree = Object();
	const auto neighbour = Object();
	const auto deadTree = Object();
	auto& fire = Burn(tree, 700.0f, 650.0f);
	auto& neighbourFire = Burn(neighbour, 600.0f, 600.0f);
	fires->CopyFire(tree, neighbour);

	fires->MoveFire(tree, deadTree);
	EXPECT_EQ(fires->FindByObject(deadTree), &fire);
	EXPECT_EQ(fires->FindByObject(tree), nullptr);
	EXPECT_EQ(fire.object, deadTree);
	// its heat and its blaze stay
	EXPECT_EQ(fire.temperature, 700.0f);
	EXPECT_EQ(fire.previous, 650.0f);
	EXPECT_EQ(fire.root, &fire);
	EXPECT_EQ(neighbourFire.root, &fire);
}

TEST_F(FirePassing, AMovedFireTakesThePlaceOfOneTheNewObjectHad)
{
	// the original does not ask whether the new object burns: its own fire is no longer found by it
	const auto tree = Object();
	const auto deadTree = Object();
	auto& fire = Burn(tree, 700.0f, 650.0f);
	Burn(deadTree, 300.0f, 300.0f);
	fires->MoveFire(tree, deadTree);
	EXPECT_EQ(fires->FindByObject(deadTree), &fire);
	EXPECT_EQ(fires->FindByObject(tree), nullptr);
	EXPECT_EQ(fire.object, deadTree);
}
