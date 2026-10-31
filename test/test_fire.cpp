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
// fire copied or moved from one object to another. The burning values info.dat gives the rocks, stones, toys, flowers,
// the bonfire and the features (an integration test that skips itself without the game's data, with a twin over a
// hand-made table), and what each class does when it burns: a field's food, a burnt field, the bonfire, a burnt
// animal. A weak fire's crackle giving its slot back. What a script's question about a fire near a place finds on the
// map cells, and the rows the features and rocks burn by.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <list>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Audio/Audio.h"
#include "Camera/Camera.h"
#include "Common/GUtilsDistance.h"
#include "ECS/AnimalAI.h"
#include "ECS/Archetypes/BonfireArchetype.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Transform.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Fire/FireGraphic.h"
#include "ECS/Fire/FireGraphicData.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/Fire/FireSound.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/DayNightClockSystem.h"
#include "ECS/Systems/Implementations/FireSystem.h"
#include "ECS/ToBeDeleted.h"
#include "Enums.h"
#include "Fire/FireModel.h"
#include "Fire/ViaPoint.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MiracleDeeds.h"
#include "support/ParticleFakes.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"
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

TEST(FireGraphic, ABurningTreesTintIsCappedByTheTreeBrightness)
{
	// {life, the frame's tree brightness, the grey}: the original's burning tree draw run natively for these
	// (the tree's colour is multiplied by the grey / 256): 50, above 0.9 life 255 - (1 - life) x 2550 truncated but at
	// least 50, then no more than the brightness
	struct Case
	{
		float life;
		uint8_t brightness;
		uint8_t grey;
	};
	constexpr std::array<Case, 13> k_Cases = {{
	    {1.0f, 200, 200},
	    {1.0f, 255, 255},
	    {1.0f, 40, 40},
	    {0.999f, 255, 252},
	    {0.98f, 255, 204},
	    {0.97f, 100, 100},
	    {0.95f, 200, 127},
	    {0.93f, 255, 76},
	    {0.92f, 255, 51},
	    {0.9000001f, 255, 50},
	    {0.9f, 255, 50},
	    {0.5f, 200, 50},
	    {0.15f, 30, 30},
	}};
	openblack::Locator::entitiesRegistry::emplace<openblack::ecs::Registry>();
	openblack::test::EmplaceWorldSystems();
	auto& registry = openblack::Locator::entitiesRegistry::value();
	auto& fires = openblack::Locator::fireSystem::value();
	const auto tree = registry.Create();
	EXPECT_FALSE(fire::graphic::TreeDrawColour(tree).has_value()); // no fire, no tint
	auto burning = std::make_unique<fire::FireEffect>();
	burning->id = fires.TakeId();
	burning->object = tree;
	fires.Insert(std::move(burning));
	for (const auto& c : k_Cases)
	{
		registry.AssignOrReplace<components::Life>(tree, c.life);
		openblack::Locator::treeSystem::value().SetBrightness(c.brightness);
		const auto colour = fire::graphic::TreeDrawColour(tree);
		ASSERT_TRUE(colour.has_value());
		EXPECT_EQ(colour->r, c.grey) << "life " << c.life << " brightness " << static_cast<int>(c.brightness);
		EXPECT_EQ(colour->g, c.grey);
		EXPECT_EQ(colour->b, c.grey);
	}
	openblack::Locator::entitiesRegistry::reset();
	openblack::test::ResetWorldSystems();
}

TEST(FireGraphic, ABurntTreeNarrowsBelowAFifthOfItsLife)
{
	// The width the original's burning tree draw gives the matrix's first and third rows, run natively: 1 - (0.2 -
	// life) x 5 in float below 0.2 life, so just under 0.2 is the float just under 1; from 0.2 up it is left as it is
	EXPECT_FALSE(fire::graphic::BurntTreeWidth(0.3f).has_value());
	EXPECT_FALSE(fire::graphic::BurntTreeWidth(0.2f).has_value());
	struct Case
	{
		float life;
		uint32_t width; // the float's bits
	};
	constexpr std::array<Case, 5> k_Cases = {{
	    {0.19999999f, 0x3F7FFFFF},
	    {0.15f, 0x3F400000},
	    {0.1f, 0x3F000000},
	    {0.05f, 0x3E800000},
	    {0.0f, 0x00000000},
	}};
	for (const auto& c : k_Cases)
	{
		const auto width = fire::graphic::BurntTreeWidth(c.life);
		ASSERT_TRUE(width.has_value()) << "life " << c.life;
		EXPECT_EQ(std::bit_cast<uint32_t>(*width), c.width) << "life " << c.life;
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

TEST(FireGraphic, TreesHaveTwoFlamesAndBigThingsSeven)
{
	// Any kind of tree 2, whatever its size; otherwise 7 once the larger of the 2D radius and the height reaches 3
	EXPECT_EQ(fire::graphic::MaxFlames(true, 9.0f, 9.0f), 2);
	EXPECT_EQ(fire::graphic::MaxFlames(false, 2.9f, 2.9f), 2);
	EXPECT_EQ(fire::graphic::MaxFlames(false, 2.99f, 1.0f), 2);
	EXPECT_EQ(fire::graphic::MaxFlames(false, 3.0f, 0.0f), 7);
	EXPECT_EQ(fire::graphic::MaxFlames(false, 0.0f, 3.0f), 7);
}

TEST(FireGraphic, FlamesGrowInWithTheFiresStrengthAndAtOnceWhenItCatchesOrIsFirstVeryHot)
{
	// seven flames at most: over a flame's 4.3 s, half strength for a second adds 7 / 4.3 x 0.5; nothing while it does
	// not burn
	float accumulator = 0.0f;
	bool veryHotDone = false;
	fire::graphic::AccumulateFlames(accumulator, veryHotDone, 7, 0.0f, 1.0f, true, true);
	EXPECT_EQ(accumulator, 0.0f);
	EXPECT_FALSE(veryHotDone);
	fire::graphic::AccumulateFlames(accumulator, veryHotDone, 7, 0.5f, 1.0f, false, false);
	EXPECT_FLOAT_EQ(accumulator, 7.0f / 4.3f * 0.5f);
	// the turn it catches: half its most flames, rounded up (4 of 7)
	accumulator = 0.0f;
	fire::graphic::AccumulateFlames(accumulator, veryHotDone, 7, 1.0f, 0.0f, false, true);
	EXPECT_EQ(accumulator, 4.0f);
	// the first very hot turn: its most flames once more, and only once
	accumulator = 0.0f;
	fire::graphic::AccumulateFlames(accumulator, veryHotDone, 7, 1.0f, 0.0f, true, false);
	EXPECT_EQ(accumulator, 7.0f);
	EXPECT_TRUE(veryHotDone);
	fire::graphic::AccumulateFlames(accumulator, veryHotDone, 7, 1.0f, 0.0f, true, false);
	EXPECT_EQ(accumulator, 7.0f);
}

TEST(FireGraphic, ASizeOrAnAgeThatComparesWithNothingGivesTwoFlamesAndNoAlpha)
{
	// the original's unordered compares: a size that is not a number counts as under 3, and a flame of such an age
	// stays, drawn with alpha 0
	const float nan = std::numeric_limits<float>::quiet_NaN();
	EXPECT_EQ(fire::graphic::MaxFlames(false, nan, nan), 2);
	EXPECT_EQ(fire::graphic::MaxFlames(false, 5.0f, nan), 2);
	const auto alpha = fire::graphic::FlameAlpha(nan);
	ASSERT_TRUE(alpha.has_value());
	EXPECT_EQ(*alpha, 0u);
}

TEST(FireGraphic, AFlameFadesInOverASecondAndLivesFourPointThree)
{
	// In over the first second, out over the other 3.3 s by 1 / 3.3 a second, x 250 and truncated, each step in float;
	// gone once older than 4.3 s
	EXPECT_EQ(fire::graphic::FlameAlpha(0.0f), 0);
	EXPECT_EQ(fire::graphic::FlameAlpha(0.5f), 125);
	EXPECT_EQ(fire::graphic::FlameAlpha(0.999f), 249);
	EXPECT_EQ(fire::graphic::FlameAlpha(1.0f), 250);
	EXPECT_EQ(fire::graphic::FlameAlpha(2.0f), 174);
	EXPECT_EQ(fire::graphic::FlameAlpha(2.65f), 125);
	EXPECT_EQ(fire::graphic::FlameAlpha(4.0f), 22);
	EXPECT_EQ(fire::graphic::FlameAlpha(4.3f), 0); // still there at its 4.3 s, faded out exactly
	EXPECT_FALSE(fire::graphic::FlameAlpha(std::nextafter(4.3f, 5.0f)).has_value());
}

TEST(FireGraphic, OnlyAMultiCellFixedObjectWiderThanTwoMetresLightsTheLand)
{
	// The light map goes only to a multi-cell fixed object (a field is one, a tree is not: MapCells.ClassesAsTheVtables)
	// whose 2D radius is strictly over 2 m
	EXPECT_TRUE(fire::graphic::GetsLightMap(true, 2.01f));
	EXPECT_TRUE(fire::graphic::GetsLightMap(true, 10.0f));
	EXPECT_FALSE(fire::graphic::GetsLightMap(true, 2.0f));
	EXPECT_FALSE(fire::graphic::GetsLightMap(false, 10.0f));
}

TEST(FireGraphic, ACooledFireAboveSeventyFiveDegreesSteams)
{
	// A burst of steam starts when the fire was cooled this turn (rain cools it), is above 75 degrees and is hotter
	// than when its last burst started
	constexpr auto k_Cooled = fire::FireEffect::k_Cooling;
	EXPECT_TRUE(fire::graphic::StartsSteam(k_Cooled, 80.0f, 0.0f));
	EXPECT_FALSE(fire::graphic::StartsSteam(k_Cooled, 75.0f, 0.0f));
	EXPECT_FALSE(fire::graphic::StartsSteam(0, 80.0f, 0.0f));
	EXPECT_FALSE(fire::graphic::StartsSteam(k_Cooled, 80.0f, 80.0f));
	EXPECT_TRUE(fire::graphic::StartsSteam(k_Cooled, 80.5f, 80.0f));
}

TEST(FireGraphic, SteamAndSmokeDriftWithHalfTheWindAndRise)
{
	// A puff ages, grows from 0.2 to 2.6 times its base scale over its 3 s and fades from its first alpha; its velocity
	// eases towards half the wind by a tenth of the difference a second and rises, and it moves by that velocity times
	// the frame and its base scale: the original's steam and smoke step, its constants and order
	using fire::graphic::SpritePos;
	std::list<SpritePos> steam {SpritePos {.position = glm::vec3(0.0f), .baseScale = 2.0f}};
	const glm::vec3 wind(8.0f, 0.0f, -4.0f);
	fire::graphic::StepPuffs(steam, 0.1f, 100.0f, 1.0f, wind);
	ASSERT_EQ(steam.size(), 1u);
	const auto& puff = steam.front();
	const float t = (1.0f / 3.0f) * 0.1f;
	EXPECT_FLOAT_EQ(puff.age, 0.1f);
	EXPECT_FLOAT_EQ(puff.scale, (2.4f * t + 0.2f) * 2.0f);
	EXPECT_EQ(puff.alpha, 97); // 96.67, rounded to the nearest
	EXPECT_FLOAT_EQ(puff.velocity.x, 4.0f * 0.1f * 0.1f);
	EXPECT_FLOAT_EQ(puff.velocity.y, 1.0f * 0.1f);
	EXPECT_FLOAT_EQ(puff.velocity.z, -2.0f * 0.1f * 0.1f);
	EXPECT_FLOAT_EQ(puff.position.x, puff.velocity.x * 0.2f);
	EXPECT_FLOAT_EQ(puff.position.y, puff.velocity.y * 0.2f);
	EXPECT_FLOAT_EQ(puff.position.z, puff.velocity.z * 0.2f);

	// The smoke: no wind slows it by a tenth a second, and it rises twice as fast; it goes after 3 s, not at them
	std::list<SpritePos> smoke {SpritePos {.age = 1.5f, .velocity = glm::vec3(1.0f, 0.0f, 0.0f), .baseScale = 1.0f}};
	fire::graphic::StepPuffs(smoke, 1.5f, 180.0f, 2.0f, glm::vec3(0.0f));
	ASSERT_EQ(smoke.size(), 1u);
	EXPECT_FLOAT_EQ(smoke.front().velocity.x, 1.0f - 0.1f * 1.5f);
	EXPECT_FLOAT_EQ(smoke.front().velocity.y, 3.0f);
	EXPECT_EQ(smoke.front().alpha, 0);
	fire::graphic::StepPuffs(smoke, 0.01f, 180.0f, 2.0f, glm::vec3(0.0f));
	EXPECT_TRUE(smoke.empty());
}

TEST(FireGraphic, SmokeAndSteamComeInBurstsOfThirtyTurnsAtFourASecond)
{
	// A burst goes on for the 30 turns after the one it started on, and is over on the 31st
	EXPECT_FALSE(fire::graphic::BurstOver(100, 100));
	EXPECT_FALSE(fire::graphic::BurstOver(100, 130));
	EXPECT_TRUE(fire::graphic::BurstOver(100, 131));

	// 4 puffs a second: the first at once, then one each quarter of a second
	float accumulator = 0.0f;
	int count = 0;
	for (int frame = 0; frame < 4; ++frame)
	{
		EXPECT_EQ(fire::graphic::PuffsDue(accumulator, count, 0.25f), 1) << "frame " << frame;
	}
	EXPECT_EQ(count, 4);
	EXPECT_FLOAT_EQ(accumulator, 4.0f);
	// a long frame lets out what it owes at once; a short one may let out none
	accumulator = 0.0f;
	count = 0;
	EXPECT_EQ(fire::graphic::PuffsDue(accumulator, count, 1.0f), 4);
	EXPECT_EQ(fire::graphic::PuffsDue(accumulator, count, 0.1f), 1);
	EXPECT_EQ(fire::graphic::PuffsDue(accumulator, count, 0.1f), 0);
	EXPECT_EQ(count, 5);
}

TEST(FireGraphic, TheSizzleIsTheInGameSteamSampleIn3DUntracked)
{
	// The hiss a burst of steam starts with: the in-game bank's steam sample, 0x35, owned by the fire graphic, in 3D at
	// the point given, and left where it starts
	namespace audio = openblack::audio;
	const glm::vec3 at(12.5f, 3.0f, -7.25f);
	const auto options = fire::graphic::SizzleOptions(42, at);
	EXPECT_EQ(options.sample.bank, audio::Bank(audio::SfxBank::InGame));
	EXPECT_EQ(options.sample.number, 0x35);
	EXPECT_TRUE(options.owner == audio::Owner::Object(42));
	EXPECT_TRUE(options.is3D);
	EXPECT_FALSE(options.track);
	EXPECT_EQ(options.position, at);
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

namespace
{
using openblack::GObjectInfo;
using openblack::InfoConstants;
using openblack::Locator;
using openblack::MobileStaticInfo;

/// What a fire reads of an object's info row
struct BurningValues
{
	std::string_view name;
	float combustion;
	float capacity;
	float defence;
};

/// The row of a table whose debug name is the one given, or nullptr
template <typename Row, size_t N>
const Row* RowNamed(const std::array<Row, N>& table, std::string_view name)
{
	const auto found = std::ranges::find_if(table, [name](const Row& row) {
		const auto end = std::ranges::find(row.debugString, '\0');
		return std::string_view(row.debugString.data(), static_cast<size_t>(end - row.debugString.begin())) == name;
	});
	return found != table.end() ? &*found : nullptr;
}

/// Each named row of the table burns as given
template <typename Row, size_t N>
void ExpectBurning(const std::array<Row, N>& table, std::span<const BurningValues> expected)
{
	for (const auto& values : expected)
	{
		const auto* row = RowNamed(table, values.name);
		ASSERT_NE(row, nullptr) << values.name;
		const GObjectInfo& info = *row;
		EXPECT_EQ(info.combustionTemperature, values.combustion) << values.name;
		EXPECT_EQ(info.heatCapacity, values.capacity) << values.name;
		EXPECT_EQ(info.defenceMultiplierBurn, values.defence) << values.name;
	}
}

/// The game's own info.dat, or nullptr without the game's data
std::unique_ptr<InfoConstants> GameInfoDat()
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		return nullptr;
	}
	std::ifstream file(std::filesystem::path(game) / "Scripts" / "info.dat", std::ios::binary);
	if (!file.is_open())
	{
		return nullptr;
	}
	const std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	if (data.size() != 0x2C + sizeof(InfoConstants))
	{
		return nullptr;
	}
	auto info = std::make_unique<InfoConstants>();
	std::memcpy(info.get(), data.data() + 0x2C, sizeof(InfoConstants));
	return info;
}

// The burning values of the rocks, stones, toys, flowers and the bonfire: none of them has any defence against a burn
constexpr float k_RockCombustion = 500.0f;
constexpr float k_RockCapacity = 4000.0f;
constexpr float k_StoneCombustion = 1000.0f;
constexpr float k_StoneCapacity = 5000.0f;
constexpr std::array k_NoBurnDefence = std::to_array<BurningValues>({
    {"Rock", k_RockCombustion, k_RockCapacity, 0.0f},
    {"Boulder1 Chalk", k_RockCombustion, k_RockCapacity, 0.0f},
    {"Boulder1 Lime", k_RockCombustion, k_RockCapacity, 0.0f},
    {"Boulder1 Sand", k_RockCombustion, k_RockCapacity, 0.0f},
    {"Boulder1 Volcanic", k_RockCombustion, k_RockCapacity, 0.0f},
    {"Boulder2 Chalk", k_RockCombustion, k_RockCapacity, 0.0f},
    {"Boulder2 Lime", k_RockCombustion, k_RockCapacity, 0.0f},
    {"Boulder2 Sand", k_RockCombustion, k_RockCapacity, 0.0f},
    {"Boulder2 Volcanic", k_RockCombustion, k_RockCapacity, 0.0f},
    {"Boulder3 Chalk", k_RockCombustion, k_RockCapacity, 0.0f},
    {"Boulder3 Lime", k_RockCombustion, k_RockCapacity, 0.0f},
    {"Boulder3 Sand", k_RockCombustion, k_RockCapacity, 0.0f},
    {"Boulder3 Volcanic", k_RockCombustion, k_RockCapacity, 0.0f},
    {"Flatrock Chalk", k_RockCombustion, k_RockCapacity, 0.0f},
    {"Flatrock Limestone", k_RockCombustion, k_RockCapacity, 0.0f},
    {"Flatrock Sandstone", k_RockCombustion, k_RockCapacity, 0.0f},
    {"Flatrock Volcanic", k_RockCombustion, k_RockCapacity, 0.0f},
    {"Longrock Chalk", k_RockCombustion, k_RockCapacity, 0.0f},
    {"Longrock Limestone", k_RockCombustion, k_RockCapacity, 0.0f},
    {"Longrock Sandstone", k_RockCombustion, k_RockCapacity, 0.0f},
    {"Longrock Volcanic", k_RockCombustion, k_RockCapacity, 0.0f},
    {"Rock Chalk", k_RockCombustion, k_RockCapacity, 0.0f},
    {"Rock Limestone", k_RockCombustion, k_RockCapacity, 0.0f},
    {"Rock Sandstone", k_RockCombustion, k_RockCapacity, 0.0f},
    {"Rock Volcanic", k_RockCombustion, k_RockCapacity, 0.0f},
    {"Sharprock Chalk", k_RockCombustion, k_RockCapacity, 0.0f},
    {"Sharprock Limestone", k_RockCombustion, k_RockCapacity, 0.0f},
    {"Sharprock Sandstone", k_RockCombustion, k_RockCapacity, 0.0f},
    {"Sharprock Volcanic", k_RockCombustion, k_RockCapacity, 0.0f},
    {"Squarerock Chalk", k_RockCombustion, k_RockCapacity, 0.0f},
    {"Squarerock Limestone", k_RockCombustion, k_RockCapacity, 0.0f},
    {"Squarerock Sandstone", k_RockCombustion, k_RockCapacity, 0.0f},
    {"Squarerock Volcanic", k_RockCombustion, k_RockCapacity, 0.0f},
    {"Singing Stone 1", k_StoneCombustion, k_StoneCapacity, 0.0f},
    {"Weeping Stone", k_StoneCombustion, k_StoneCapacity, 0.0f},
    {"Gate Totem Ape", k_StoneCombustion, k_StoneCapacity, 0.0f},
    {"Gate Totem Blank", k_StoneCombustion, k_StoneCapacity, 0.0f},
    {"Gate Totem Cow", k_StoneCombustion, k_StoneCapacity, 0.0f},
    {"Gate Totem Tiger", k_StoneCombustion, k_StoneCapacity, 0.0f},
    {"Toy Ball", k_StoneCombustion, k_StoneCapacity, 0.0f},
    {"Toy Cuddly", k_StoneCombustion, k_StoneCapacity, 0.0f},
    {"Toy Die", k_StoneCombustion, k_StoneCapacity, 0.0f},
    {"Toy Skittle", k_StoneCombustion, k_StoneCapacity, 0.0f},
    {"Toy Bowling Ball", k_StoneCombustion, k_StoneCapacity, 0.0f},
    {"Bonfire", 95.0f, 10.0f, 0.0f},
});
constexpr std::array k_FlowersNoBurnDefence = std::to_array<BurningValues>({
    {"WhiteFlowers", 100.0f, 50.0f, 0.0f},
    {"PinkFlowers", 100.0f, 50.0f, 0.0f},
});

// Every feature (the pyramids, temples, statues, pillars, the whale, the phone box...) burns only at 2000 and holds
// 50000 units of heat, with a hundredth of a defence; the ark and its dry dock and wreck alone burn at 250
constexpr float k_FeatureCombustion = 2000.0f;
constexpr float k_FeatureCapacity = 50000.0f;
constexpr float k_FeatureDefence = 0.01f;
constexpr std::array k_ArkFeatures {openblack::FeatureInfo::Ark, openblack::FeatureInfo::ArkDryDock,
                                    openblack::FeatureInfo::ArkWreck};
} // namespace

TEST(FireValues, RocksStonesToysFlowersAndTheBonfireHaveNoBurnDefence)
{
	const auto info = GameInfoDat();
	if (info == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set, or no info.dat there";
	}
	ExpectBurning(info->mobileStatic, k_NoBurnDefence);
	ExpectBurning(info->flowers, k_FlowersNoBurnDefence);
}

TEST(FireValues, FeaturesBurnOnlyAtTwoThousandWithAHugeCapacity)
{
	const auto info = GameInfoDat();
	if (info == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set, or no info.dat there";
	}
	for (size_t row = 0; row < info->feature.size(); ++row)
	{
		const GObjectInfo& feature = info->feature.at(row);
		const bool ark = std::ranges::find(k_ArkFeatures, static_cast<openblack::FeatureInfo>(row)) != k_ArkFeatures.end();
		EXPECT_EQ(feature.combustionTemperature, ark ? 250.0f : k_FeatureCombustion) << "feature " << row;
		EXPECT_EQ(feature.heatCapacity, ark ? 5000.0f : k_FeatureCapacity) << "feature " << row;
		EXPECT_EQ(feature.defenceMultiplierBurn, k_FeatureDefence) << "feature " << row;
	}
	// the pyramid and the pillars among them, found by their names
	constexpr std::array k_Named = std::to_array<BurningValues>({
	    {"Egyptian Pyramid Feature", k_FeatureCombustion, k_FeatureCapacity, k_FeatureDefence},
	    {"Tibetan Large Pillar Feature", k_FeatureCombustion, k_FeatureCapacity, k_FeatureDefence},
	    {"Prison Pillar 1", k_FeatureCombustion, k_FeatureCapacity, k_FeatureDefence},
	    {"Fat Pilar Chalk", k_FeatureCombustion, k_FeatureCapacity, k_FeatureDefence},
	    {"Spikey Pilar Volcanic", k_FeatureCombustion, k_FeatureCapacity, k_FeatureDefence},
	    {"Piper Cave", k_FeatureCombustion, k_FeatureCapacity, k_FeatureDefence},
	    {"TombStone", k_FeatureCombustion, k_FeatureCapacity, k_FeatureDefence},
	});
	ExpectBurning(info->feature, k_Named);
}

TEST(FireValues, NoBurnDefenceMeansNoHarmAndAFeatureTakesAThousandTurnsAtItsHottest)
{
	// A turn of burning takes (T - Tc) / (2 Tc - Tc) x the burn defence x 0.1 of the life: nothing at all for a rock
	// at its hottest, and a thousandth for a feature at its hottest
	const openblack::fire::Material rock {.combustion = k_RockCombustion, .capacity = k_RockCapacity, .defence = 0.0f};
	EXPECT_EQ(openblack::fire::BurnDamage(2.0f * k_RockCombustion, rock), 0.0f);
	EXPECT_EQ(openblack::fire::BurnDamage(1.5f * k_RockCombustion, rock), 0.0f);
	const openblack::fire::Material feature {
	    .combustion = k_FeatureCombustion, .capacity = k_FeatureCapacity, .defence = k_FeatureDefence};
	EXPECT_FLOAT_EQ(openblack::fire::BurnDamage(2.0f * k_FeatureCombustion, feature), 0.001f);
}

namespace
{
/// The spot visuals started, as they were asked for
class SpotVisualParticles final: public openblack::test::InertParticleSystem
{
public:
	struct Started
	{
		openblack::SpotVisualType type;
		glm::vec3 position;
		std::optional<int> turns;
		entt::entity owner;
		float magnitude;
	};

	entt::entity StartSpotVisual(openblack::SpotVisualType type, glm::vec3 position, std::optional<int> turns,
	                             entt::entity owner, float magnitude) override
	{
		started.push_back({type, position, turns, owner, magnitude});
		return entt::null;
	}

	std::vector<Started> started;
};

/// The answers of an object's own class to the fire, over the game's world lists and a hand-made info table
class FireTraits: public ::testing::Test
{
protected:
	void SetUp() override
	{
		openblack::test::EmplaceMapAndVillagerDefaults();
		Locator::infoConstants::reset(MakeInfo().release());
		Locator::entitiesRegistry::emplace<Registry>();
		openblack::test::EmplaceWorldSystems();
		map_cells::Clear();
		object_index::OnLoadMap();
	}

	void TearDown() override
	{
		map_cells::Clear();
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		openblack::test::ResetMapAndVillagerDefaults();
	}

	static Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// The rows the tests read, each with numbers of its own; the bonfire's are info.dat's
	static std::unique_ptr<InfoConstants> MakeInfo()
	{
		auto info = std::make_unique<InfoConstants>();
		const auto set = [](GObjectInfo& row, float combustion, float capacity, float defence) {
			row.combustionTemperature = combustion;
			row.heatCapacity = capacity;
			row.defenceMultiplierBurn = defence;
		};
		set(info->mobileStatic.at(static_cast<size_t>(MobileStaticInfo::Rock)), 501.0f, 4001.0f, 0.0f);
		set(info->mobileStatic.at(static_cast<size_t>(MobileStaticInfo::ToyBall)), 1003.0f, 5003.0f, 0.0f);
		set(info->mobileStatic.at(static_cast<size_t>(MobileStaticInfo::Bonfire)), 95.0f, 10.0f, 0.0f);
		set(info->flowers.at(static_cast<size_t>(openblack::FlowersInfo::Pink)), 104.0f, 54.0f, 0.0f);
		set(info->feature.at(static_cast<size_t>(openblack::FeatureInfo::Ark)), 250.0f, 5000.0f, 0.01f);
		set(info->feature.at(static_cast<size_t>(openblack::FeatureInfo::Tombstone)), 2005.0f, 50005.0f, 0.015f);
		// the six crop rows are the same in info.dat: ripe at 1200, sown 30 times, 350 food when full
		for (auto& crop : info->fieldType)
		{
			crop.ageRecolt = 1200.0f;
			crop.timesToSow = 30.0f;
			crop.totalFoodInField = 350.0f;
		}
		return info;
	}

	[[nodiscard]] static entt::entity Placed()
	{
		const auto entity = Reg().Create();
		Reg().Assign<components::Transform>(entity, glm::vec3(100.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		return entity;
	}

	/// The object burns at a temperature
	static fire::FireEffect& Burn(entt::entity object, float temperature)
	{
		auto& fires = Locator::fireSystem::value();
		auto made = std::make_unique<fire::FireEffect>();
		made->id = fires.TakeId();
		made->object = object;
		made->temperature = temperature;
		made->previous = temperature;
		return fires.Insert(std::move(made));
	}

	const openblack::test::RestoreService<Locator::infoConstants> _restoreInfo;
};
} // namespace

TEST_F(FireTraits, EachClassReadsItsOwnInfoRow)
{
	// a rock, a toy and the bonfire read their mobile static row, flowers their flowers row and a feature its feature row
	struct Case
	{
		entt::entity object;
		float combustion;
		float capacity;
		float defence;
	};
	const auto withStatic = [](MobileStaticInfo type) {
		const auto entity = Placed();
		Reg().Assign<components::MobileStatic>(entity, type);
		return entity;
	};
	const auto flowers = Placed();
	Reg().Assign<components::Flowers>(flowers, openblack::FlowersInfo::Pink);
	const auto ark = Placed();
	Reg().Assign<components::Feature>(ark, openblack::FeatureInfo::Ark);
	const auto tombstone = Placed();
	Reg().Assign<components::Feature>(tombstone, openblack::FeatureInfo::Tombstone);
	const std::array k_Cases {
	    Case {withStatic(MobileStaticInfo::Rock), 501.0f, 4001.0f, 0.0f},
	    Case {withStatic(MobileStaticInfo::ToyBall), 1003.0f, 5003.0f, 0.0f},
	    Case {withStatic(MobileStaticInfo::Bonfire), 95.0f, 10.0f, 0.0f},
	    Case {flowers, 104.0f, 54.0f, 0.0f},
	    Case {ark, 250.0f, 5000.0f, 0.01f},
	    Case {tombstone, 2005.0f, 50005.0f, 0.015f},
	};
	for (const auto& c : k_Cases)
	{
		EXPECT_EQ(fire::traits::CombustionTemperature(c.object), c.combustion) << static_cast<uint32_t>(c.object);
		EXPECT_EQ(fire::traits::HeatCapacity(c.object), c.capacity) << static_cast<uint32_t>(c.object);
		EXPECT_EQ(fire::traits::DefenceMultiplierBurn(c.object), c.defence) << static_cast<uint32_t>(c.object);
	}
}

TEST_F(FireTraits, AFieldBurnsItsFoodEvenWhenNotHurtByFire)
{
	// A burning field loses damage x its full food of its food and answers a whole life, even when scripts said it is
	// not hurt by fire: a ripe field of 500 food burning by 0.2 loses 70 of it
	const auto field = Placed();
	Reg().Assign<components::Field>(field, components::Field {.town = -1, .crops = 30, .growth = 1200.0f, .food = 500.0f});
	fire::traits::SetNotHurtByFire(field, true);
	EXPECT_EQ(fire::traits::ReduceLifeDueToBurning(field, 0.2f, std::nullopt), 1.0f);
	EXPECT_EQ(Reg().Get<components::Field>(field).food, 430.0f);

	// Any other thing scripts said is not hurt by fire keeps its life, and answers it
	const auto rock = Placed();
	Reg().Assign<components::MobileStatic>(rock, MobileStaticInfo::Rock);
	Reg().Assign<components::Life>(rock, 0.8f);
	fire::traits::SetNotHurtByFire(rock, true);
	EXPECT_EQ(fire::traits::ReduceLifeDueToBurning(rock, 0.1f, std::nullopt), 0.8f);
	EXPECT_EQ(Reg().Get<components::Life>(rock).value, 0.8f);
	// and once hurt by fire again it loses life
	fire::traits::SetNotHurtByFire(rock, false);
	EXPECT_LT(fire::traits::ReduceLifeDueToBurning(rock, 0.1f, std::nullopt), 0.8f);
}

TEST_F(FireTraits, ABurntFieldEmptiesAndItsFireGoes)
{
	// Destroyed by its fire, a field loses its crops, growth and food, its fire is cooled to nothing and deleted, and
	// the field stays
	const auto field = Placed();
	Reg().Assign<components::Field>(field, components::Field {.town = -1, .crops = 40, .growth = 0.5f, .food = 300.0f});
	auto& burning = Burn(field, 200.0f);
	fire::traits::DestroyedByEffect(field, std::nullopt, 0.0f);
	ASSERT_TRUE(Reg().Valid(field));
	const auto& emptied = Reg().Get<components::Field>(field);
	EXPECT_EQ(emptied.crops, 0);
	EXPECT_EQ(emptied.growth, 0.0f);
	EXPECT_EQ(emptied.food, 0.0f);
	EXPECT_EQ(burning.temperature, 0.0f);
	EXPECT_NE(burning.flags & fire::FireEffect::k_Deleted, 0);
	EXPECT_EQ(fire::Find(field), nullptr);

	// A field with no fire gets none
	const auto cold = Placed();
	Reg().Assign<components::Field>(cold, components::Field {.town = -1, .crops = 40, .growth = 0.5f, .food = 300.0f});
	fire::traits::DestroyedByEffect(cold, std::nullopt, 0.0f);
	EXPECT_EQ(Reg().Get<components::Field>(cold).food, 0.0f);
	EXPECT_EQ(fire::Find(cold), nullptr);
}

TEST_F(FireTraits, ABonfireHasAnEndlessSpotVisualAndNoFire)
{
	// A bonfire is a rock of the bonfire's mobile static row with the bonfire spot visual on it, at full strength for
	// ever; no fire is made for it, and its row gives it no defence against a burn
	const openblack::test::RestoreService<Locator::particleSystem> restoreParticles;
	auto& particles = static_cast<SpotVisualParticles&>(Locator::particleSystem::emplace<SpotVisualParticles>());
	const glm::vec3 at(120.0f, 4.0f, 130.0f);
	const auto bonfire = archetypes::BonfireArchetype::Create(at, 0.0f, 1.0f);
	ASSERT_TRUE(Reg().Valid(bonfire));
	ASSERT_TRUE(Reg().AllOf<components::MobileStatic>(bonfire));
	EXPECT_EQ(Reg().Get<components::MobileStatic>(bonfire).type, MobileStaticInfo::Bonfire);
	ASSERT_EQ(particles.started.size(), 1u);
	const auto& visual = particles.started.front();
	EXPECT_EQ(visual.type, openblack::SpotVisualType::Bonfire);
	EXPECT_EQ(static_cast<int>(visual.type), 25);
	EXPECT_EQ(visual.position, at);
	EXPECT_EQ(visual.turns, std::optional<int>(-1));
	EXPECT_EQ(visual.owner, bonfire);
	EXPECT_EQ(visual.magnitude, 1.0f);
	EXPECT_EQ(fire::Find(bonfire), nullptr);
	EXPECT_EQ(fire::traits::DefenceMultiplierBurn(bonfire), 0.0f);
}

TEST_F(FireTraits, ABurntAnimalStartsDyingAndStays)
{
	// Destroyed by its fire, an animal is not deleted: it starts dying, its life at 0, in the dying state (14) with
	// the three status flags the dying sets (0x31)
	const auto sheep = Placed();
	Reg().Assign<components::Animal>(sheep, openblack::AnimalInfo::Sheep, 0u);
	Reg().Assign<components::Life>(sheep, 0.0f);
	fire::traits::DestroyedByEffect(sheep, openblack::PlayerNames::PLAYER_ONE, 0.0f);
	ASSERT_TRUE(Reg().Valid(sheep));
	ASSERT_TRUE(Reg().AllOf<components::AnimalBrain>(sheep));
	const auto& brain = Reg().Get<components::AnimalBrain>(sheep);
	EXPECT_EQ(brain.topState, static_cast<uint8_t>(animal_ai::AnimalState::Dying));
	EXPECT_EQ(brain.topState, 14);
	EXPECT_EQ(brain.status & 0x31, 0x31);
	EXPECT_EQ(Reg().Get<components::Life>(sheep).value, 0.0f);
}

namespace
{
/// The fires' crackle slots, over the game's fires
class FireSound: public FirePassing
{
protected:
	void TearDown() override
	{
		// the slots and the sound owners let go of the fires before they go
		fire::sound::Clear();
		FirePassing::TearDown();
	}
};
} // namespace

// A fire taking the slot it already holds stops its own crackle first, as the slot's fire always is; it plays again with
// the slots' next turn
TEST_F(FireSound, AFireRetakingItsOwnSlotStopsItsCrackleFirst)
{
	auto& fire = Burn(Object(), 300.0f, 300.0f);
	fire.flags |= fire::FireEffect::k_SoundPlaying;
	auto& slots = fires->GetSlots();
	slots = {fire::sound::Slot {&fire, 15.0f}, fire::sound::Slot {}};
	fires->SetMaxDistance(15.0f);
	// no camera: the fire is at distance 0, nearer than the farthest slot
	fire::sound::Consider(fire, true);
	EXPECT_EQ(slots.at(0).fire, &fire);
	EXPECT_EQ(slots.at(0).distance, 0.0f);
	EXPECT_EQ(fire.flags & fire::FireEffect::k_SoundPlaying, 0);
}

// A nearer fire always takes the first slot, the one it held stopping its crackle, and the second slot stays empty:
// the first is taken when it is empty or not nearer than the farthest slot, which it is while the second is empty
TEST_F(FireSound, ANearerFireTakesTheFirstSlotAndTheSecondStaysEmpty)
{
	auto& farther = Burn(Object(), 300.0f, 300.0f);
	auto& nearer = Burn(Object(), 300.0f, 300.0f);
	farther.flags |= fire::FireEffect::k_SoundPlaying;
	auto& slots = fires->GetSlots();
	slots = {fire::sound::Slot {&farther, 20.0f}, fire::sound::Slot {}};
	fires->SetMaxDistance(20.0f);
	// no camera: the nearer fire is at distance 0
	fire::sound::Consider(nearer, true);
	EXPECT_EQ(slots.at(0).fire, &nearer);
	EXPECT_EQ(slots.at(0).distance, 0.0f);
	EXPECT_EQ(farther.flags & fire::FireEffect::k_SoundPlaying, 0);
	EXPECT_EQ(slots.at(1).fire, nullptr);
}

// A fire's distance to the camera is the game's map distance: x and z as map coordinates and the 16.16 hypotenuse in
// metres, not the float distance of the positions
TEST_F(FireSound, AFiresCameraDistanceIsTheGamesMapDistance)
{
	const openblack::test::RestoreService<openblack::Locator::camera> restoreCamera;
	const auto& camera = openblack::Locator::camera::emplace(glm::vec3(100.0f, 30.0f, 100.0f));
	const auto object = Object();
	const glm::vec3 at {110.0f, 0.0f, 100.0f};
	openblack::Locator::entitiesRegistry::value().Assign<components::Transform>(object, at, glm::mat3(1.0f), glm::vec3(1.0f));
	auto& fire = Burn(object, 300.0f, 300.0f);
	auto& slots = fires->GetSlots();
	slots = {fire::sound::Slot {}, fire::sound::Slot {}};
	fires->SetMaxDistance(0.0f);
	fire::sound::Consider(fire, true);
	ASSERT_EQ(slots.at(0).fire, &fire);
	const auto origin = camera.GetOrigin();
	const float mapDistance =
	    openblack::gutils::GetDistanceInMetres(openblack::map_coords::FromWorld(at), openblack::map_coords::FromWorld(origin));
	EXPECT_EQ(slots.at(0).distance, mapDistance);
	EXPECT_NE(slots.at(0).distance, glm::length(glm::vec2(at.x - origin.x, at.z - origin.z)));
}

TEST_F(FireSound, AWeakFireGivesItsSlotBackAndItsCrackleStops)
{
	// A fire too weak to crackle whose sound plays: the first slot holding it is emptied, its sound stops, and the
	// farthest distance is worked out again from the slots left
	auto& closer = Burn(Object(), 300.0f, 300.0f);
	auto& farther = Burn(Object(), 300.0f, 300.0f);
	closer.flags |= fire::FireEffect::k_SoundPlaying;
	farther.flags |= fire::FireEffect::k_SoundPlaying;
	auto& slots = fires->GetSlots();
	slots = {fire::sound::Slot {&closer, 15.0f}, fire::sound::Slot {&farther, 25.0f}};
	fires->SetMaxDistance(25.0f);
	fire::sound::Consider(closer, false);
	EXPECT_EQ(slots.at(0).fire, nullptr);
	EXPECT_EQ(slots.at(0).distance, 0.0f);
	EXPECT_EQ(closer.flags & fire::FireEffect::k_SoundPlaying, 0);
	EXPECT_EQ(slots.at(1).fire, &farther);
	EXPECT_EQ(slots.at(1).distance, 25.0f);
	EXPECT_NE(farther.flags & fire::FireEffect::k_SoundPlaying, 0);
	EXPECT_EQ(fires->MaxDistance(), 25.0f);

	// Without its sound playing nothing changes, even in a slot
	slots.at(0) = fire::sound::Slot {&closer, 15.0f};
	fire::sound::Consider(closer, false);
	EXPECT_EQ(slots.at(0).fire, &closer);
	EXPECT_EQ(slots.at(0).distance, 15.0f);
	EXPECT_EQ(fires->MaxDistance(), 25.0f);

	// A fire in both slots gives back only the first; the farthest distance is then the other's
	farther.flags &= static_cast<uint8_t>(~fire::FireEffect::k_SoundPlaying);
	closer.flags |= fire::FireEffect::k_SoundPlaying;
	slots = {fire::sound::Slot {&closer, 30.0f}, fire::sound::Slot {&closer, 12.0f}};
	fires->SetMaxDistance(30.0f);
	fire::sound::Consider(closer, false);
	EXPECT_EQ(slots.at(0).fire, nullptr);
	EXPECT_EQ(slots.at(1).fire, &closer);
	EXPECT_EQ(slots.at(1).distance, 12.0f);
	EXPECT_EQ(closer.flags & fire::FireEffect::k_SoundPlaying, 0);
	EXPECT_EQ(fires->MaxDistance(), 12.0f);

	// A fire in no slot keeps its flag: there is nothing to stop
	auto& other = Burn(Object(), 300.0f, 300.0f);
	other.flags |= fire::FireEffect::k_SoundPlaying;
	fire::sound::Consider(other, false);
	EXPECT_NE(other.flags & fire::FireEffect::k_SoundPlaying, 0);
	EXPECT_EQ(fires->MaxDistance(), 12.0f);
	other.flags &= static_cast<uint8_t>(~fire::FireEffect::k_SoundPlaying);
}

// A patch of flowers takes its fire values from its own flowers row, which info.dat fills with a combustion
// temperature of 100 and a heat capacity of 50 for both patches; a fire that destroys it deletes it
TEST(FireTraitRows, APatchOfFlowersBurnsByItsFlowersRow)
{
	const openblack::test::RestoreService<openblack::Locator::infoConstants> restoreInfo;
	const openblack::test::RestoreService<openblack::Locator::entitiesRegistry> restoreRegistry;
	auto info = std::make_unique<openblack::InfoConstants>();
	auto& pink = info->flowers.at(static_cast<size_t>(openblack::FlowersInfo::Pink));
	pink.heatCapacity = 50.0f;
	pink.combustionTemperature = 100.0f;
	const openblack::GObjectInfo* row = &pink;
	openblack::Locator::infoConstants::reset(info.release());
	auto& registry = openblack::Locator::entitiesRegistry::emplace<Registry>();
	openblack::test::EmplaceWorldSystems();
	const auto patch = registry.Create();
	registry.Assign<components::Transform>(patch, glm::vec3(1000.0f, 0.0f, 1000.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<components::Flowers>(patch, openblack::FlowersInfo::Pink);

	EXPECT_EQ(fire::traits::InfoOf(patch), row);
	EXPECT_EQ(fire::traits::CombustionTemperature(patch), 100.0f);
	EXPECT_EQ(fire::traits::HeatCapacity(patch), 50.0f);
	fire::traits::DestroyedByEffect(patch);
	EXPECT_FALSE(registry.Valid(patch));

	openblack::Locator::entitiesRegistry::reset();
	openblack::test::ResetWorldSystems();
}

namespace
{
/// The game's fires, real, over the map cells of a registry of their own. The info table burns the rocks and two
/// features by the game's info.dat: a rock at 500 with a heat capacity of 4000, a pillar at 2000 with 50000, the ark at
/// 250 with 5000
class FiresOnTheMap: public ::testing::Test
{
protected:
	void SetUp() override
	{
		auto info = std::make_unique<openblack::InfoConstants>();
		const auto burns = [](openblack::GObjectInfo& row, float combustion, float capacity) {
			row.combustionTemperature = combustion;
			row.heatCapacity = capacity;
		};
		burns(info->mobileStatic.at(static_cast<size_t>(openblack::MobileStaticInfo::Rock)), 500.0f, 4000.0f);
		burns(info->feature.at(static_cast<size_t>(openblack::FeatureInfo::Pilar3Lime)), 2000.0f, 50000.0f);
		burns(info->feature.at(static_cast<size_t>(openblack::FeatureInfo::Ark)), 250.0f, 5000.0f);
		openblack::Locator::infoConstants::reset(info.release());
		openblack::test::EmplaceMapAndVillagerDefaults();
		openblack::Locator::entitiesRegistry::emplace<Registry>();
		openblack::test::EmplaceWorldSystems();
		map_cells::Clear();
		object_index::OnLoadMap();
		fires = &openblack::Locator::fireSystem::emplace<systems::FireSystem>();
		// the weather a fire asks at its place reads the clock
		openblack::Locator::dayNightClock::emplace<systems::DayNightClockSystem>();
	}
	void TearDown() override
	{
		openblack::Locator::dayNightClock::reset();
		map_cells::Clear();
		openblack::Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		openblack::test::ResetMapAndVillagerDefaults();
		openblack::Locator::infoConstants::reset();
	}

	static Registry& Reg() { return openblack::Locator::entitiesRegistry::value(); }

	/// A thing at the position, with nothing yet that says what it is
	static entt::entity Thing(const glm::vec3& position)
	{
		const auto thing = Reg().Create();
		object_index::Assign(thing);
		Reg().Assign<components::Transform>(thing, position, glm::mat3(1.0f), glm::vec3(1.0f));
		return thing;
	}

	/// A rock of the rock row at the position, on the map
	static entt::entity RockAt(const glm::vec3& position)
	{
		const auto rock = Thing(position);
		Reg().Assign<components::MobileStatic>(rock, openblack::MobileStaticInfo::Rock);
		map_cells::InsertMapObject(rock);
		return rock;
	}

	/// The object burns at a temperature
	fire::FireEffect& Burn(entt::entity object, float temperature)
	{
		auto made = std::make_unique<fire::FireEffect>();
		made->id = fires->TakeId();
		made->object = object;
		made->temperature = temperature;
		made->previous = temperature;
		return fires->Insert(std::move(made));
	}

	systems::FireSystemInterface* fires {nullptr};
};
} // namespace

TEST_F(FiresOnTheMap, AFireNearAPlaceIsAThingOnFireWithinTheRadius)
{
	// The rock is 15 m across and 20 m along from the place: 25 m away
	const glm::vec3 place(100.0f, 0.0f, 100.0f);
	const auto rock = RockAt(glm::vec3(115.0f, 0.0f, 120.0f));
	auto& fire = Burn(rock, 499.0f);
	// hot, but below its combustion temperature: not on fire
	EXPECT_FALSE(fires->IsFireNear(place, 30.0f));
	// at 500 a rock is on fire, and counts when it is no farther than the radius
	fire.temperature = 500.0f;
	EXPECT_TRUE(fires->IsFireNear(place, 30.0f));
	// the distance is worked out on the map's fixed-point coordinates, a hair over 25
	EXPECT_TRUE(fires->IsFireNear(place, 25.01f));
	EXPECT_FALSE(fires->IsFireNear(place, 24.9f));
}

TEST_F(FiresOnTheMap, AFeatureBurnsByItsOwnRow)
{
	const auto pillar = Thing(glm::vec3(50.0f, 0.0f, 50.0f));
	Reg().Assign<components::Feature>(pillar, openblack::FeatureInfo::Pilar3Lime);
	const auto ark = Thing(glm::vec3(80.0f, 0.0f, 50.0f));
	Reg().Assign<components::Feature>(ark, openblack::FeatureInfo::Ark);
	EXPECT_EQ(fire::traits::CombustionTemperature(pillar), 2000.0f);
	EXPECT_EQ(fire::traits::HeatCapacity(pillar), 50000.0f);
	EXPECT_EQ(fire::traits::CombustionTemperature(ark), 250.0f);
	EXPECT_EQ(fire::traits::HeatCapacity(ark), 5000.0f);
	// burnt away, a feature goes as anything with no destruction of its own does
	fire::traits::DestroyedByEffect(pillar);
	EXPECT_FALSE(openblack::ecs::IsAvailable(pillar));
	EXPECT_TRUE(openblack::ecs::IsAvailable(ark));
}

/// With OPENBLACK_GAME_PATH set to the install: what the features and the rocks burn by
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(FireTraitRows, realInfoDatFeatures)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	std::ifstream file(std::filesystem::path(game) / "Scripts" / "info.dat", std::ios::binary);
	ASSERT_TRUE(file.is_open());
	const std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	ASSERT_EQ(data.size(), 0x2C + sizeof(openblack::InfoConstants));
	auto info = std::make_unique<openblack::InfoConstants>();
	std::memcpy(info.get(), data.data() + 0x2C, sizeof(openblack::InfoConstants));
	// every feature catches at 2000 and holds 50000, but the ark, its dry dock and its wreck: 250 and 5000
	for (size_t row = 0; row < info->feature.size(); ++row)
	{
		const bool ark = row >= static_cast<size_t>(openblack::FeatureInfo::Ark) &&
		                 row <= static_cast<size_t>(openblack::FeatureInfo::ArkWreck);
		EXPECT_EQ(info->feature.at(row).combustionTemperature, ark ? 250.0f : 2000.0f) << "row " << row;
		EXPECT_EQ(info->feature.at(row).heatCapacity, ark ? 5000.0f : 50000.0f) << "row " << row;
	}
	// every row of the Rock kind catches at 500 and holds 4000
	for (const auto& row : info->mobileStatic)
	{
		if (row.mobileType == openblack::MobileStaticInfo::Rock)
		{
			EXPECT_EQ(row.combustionTemperature, 500.0f);
			EXPECT_EQ(row.heatCapacity, 4000.0f);
		}
	}
}
