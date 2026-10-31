/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The marker over where the leash sent the local player's creature (src/Creature/LeashMarker.h and
// ECS/Archetypes/LeashMarkerArchetype.h): the species' footprint picture, the ring's and footprint's sizes at the
// bookmarks' pulse against the original's numbers from the exe oracle, and where it stands

#include <cstdint>

#include <array>
#include <bit>
#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <gtest/gtest.h>

#include "3D/FrameAnim.h"
#include "Camera/BookmarkPulse.h"
#include "Creature/LeashMarker.h"
#include "ECS/Archetypes/LeashMarkerArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Registry.h"

using namespace openblack;

namespace
{
/// The original's pulse after a drawn frame of 100 ms from 0: its width and height
const bookmark_pulse::Pulse k_AtHundred = bookmark_pulse::Step({.timeMs = 0}, 100);

void ExpectBits(float value, uint32_t expected)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(value), expected) << value;
}
} // namespace

// The footprint picture of each row of the creature tables, as the original's table holds it; a row past the eighteenth
// takes the first row's
TEST(LeashMarker, EachSpeciesShowsItsOwnFootprint)
{
	constexpr std::array<uint8_t, 18> k_Cells {0, 2, 4, 4, 4, 4, 3, 5, 6, 1, 1, 2, 0, 0, 0, 2, 0, 7};
	for (size_t row = 0; row < k_Cells.size(); ++row)
	{
		EXPECT_EQ(leash_marker::FootprintCell(row), k_Cells.at(row)) << row;
	}
	EXPECT_EQ(leash_marker::FootprintCell(18), 0);
	EXPECT_EQ(leash_marker::FootprintCell(1000), 0);
}

// The original's pulse at 100 ms sizes the ring as a bookmark's marker, and the footprint at 0.7 of it the other way
// round: its half width 0.7 of the height, its stretch 0.7 of the width over the height, each rounded to a float
TEST(LeashMarker, TheSizesAreTheOriginalsAtThePulse)
{
	ExpectBits(k_AtHundred.width, 0x3F9355D2u);
	ExpectBits(k_AtHundred.height, 0x3F49AC4Au);
	const auto ring = leash_marker::RingHalfSize(k_AtHundred);
	ExpectBits(ring.x, 0x3F9355D2u);
	ExpectBits(ring.y, 0x3F49AC4Au);
	// the oracle's size and stretch for a creature's marker
	const auto footprint = leash_marker::FootprintHalfSize(k_AtHundred);
	ExpectBits(footprint.x, 0x3F0D2BCDu);
	const float stretch = (k_AtHundred.width * leash_marker::k_FootprintShare) / k_AtHundred.height;
	ExpectBits(stretch, 0x3F82EAD1u);
	ExpectBits(footprint.y, std::bit_cast<uint32_t>(stretch * footprint.x));
	ExpectBits(footprint.y, 0x3F106377u);
}

// A pulse too small still draws the footprint at the smallest size
TEST(LeashMarker, TheFootprintIsNeverSmallerThanTheSmallestSize)
{
	const auto footprint = leash_marker::FootprintHalfSize({.timeMs = 0, .width = 0.2f, .height = 0.00001f});
	EXPECT_EQ(footprint.x, bookmark_pulse::k_SmallestSize);
}

// Over a thing, 2 m up from where it stands and its height on top; else at the point kept
TEST(LeashMarker, ItStandsOverTheThingAsItIsNowElseAtThePointKept)
{
	const glm::vec3 kept {1.0f, 2.0f, 3.0f};
	EXPECT_EQ(leash_marker::Position(std::nullopt, kept), kept);
	const auto over = leash_marker::Position(leash_marker::Thing {.place = {10.0f, 5.0f, -4.0f}, .height = 7.5f}, kept);
	EXPECT_EQ(over, glm::vec3(10.0f, 14.5f, -4.0f));
}

// The ring and the footprint come from the bookmarks' sheet, yellow and blended; the footprint turned a quarter turn
TEST(LeashMarker, TheSpritesAreTheRingAndTheSpeciesFootprint)
{
	using ecs::archetypes::LeashMarkerArchetype;
	const auto ring = LeashMarkerArchetype::RingSprite();
	EXPECT_EQ(ring.uvMin, graphics::frame_anim::SpriteCellUv(56)[0]);
	EXPECT_EQ(ring.uvMin, glm::vec2(0.0f, 0.875f));
	EXPECT_EQ(ring.uvExtent, glm::vec2(0.125f));
	EXPECT_EQ(ring.tint, glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));
	EXPECT_FALSE(ring.additive);
	EXPECT_TRUE(ring.alpha.has_value());
	EXPECT_EQ(ring.angle, 0.0f);

	const auto tiger = LeashMarkerArchetype::FootprintSprite(CreatureType::Tiger);
	EXPECT_EQ(tiger.texture, ring.texture);
	EXPECT_EQ(tiger.alpha, ring.alpha);
	EXPECT_EQ(tiger.uvMin, graphics::frame_anim::SpriteCellUv(4)[0]);
	ExpectBits(tiger.angle, 0xBFC90FDBu);
	// the ape's row is the first: the hand print
	EXPECT_EQ(LeashMarkerArchetype::FootprintSprite(CreatureType::GiantApe).uvMin, graphics::frame_anim::SpriteCellUv(0)[0]);
	EXPECT_EQ(LeashMarkerArchetype::FootprintSprite(CreatureType::Horse).uvMin, graphics::frame_anim::SpriteCellUv(3)[0]);
}

// While the creature has a target, the ring and then the footprint at the target's point, sized by the pulse; nothing
// without a target, or before the pulse's first step
TEST(LeashMarker, ItIsDrawnOnlyWhileTheCreatureHasATarget)
{
	using ecs::archetypes::LeashMarkerArchetype;
	using ecs::components::Creature;
	using ecs::components::CreatureLeash;
	ecs::Registry registry;
	const auto creature = registry.Create();
	registry.Assign<Creature>(creature, Creature {.owner = PlayerNames::PLAYER_ONE, .species = CreatureType::Tiger});
	auto& leashes = registry.Assign<CreatureLeash>(creature);
	EXPECT_FALSE(LeashMarkerArchetype::Drawn(registry, creature, k_AtHundred).has_value());

	leashes.targetPoint = {20.0f, 9.0f, 30.0f};
	leashes.hasTarget = true;
	EXPECT_FALSE(LeashMarkerArchetype::Drawn(registry, creature, {}).has_value());
	const auto marker = LeashMarkerArchetype::Drawn(registry, creature, k_AtHundred);
	ASSERT_TRUE(marker.has_value());
	const auto& [ring, footprint] = *marker;
	EXPECT_EQ(ring.sprite.uvMin, LeashMarkerArchetype::RingSprite().uvMin);
	EXPECT_EQ(footprint.sprite.uvMin, LeashMarkerArchetype::FootprintSprite(CreatureType::Tiger).uvMin);
	EXPECT_EQ(ring.transform.position, leashes.targetPoint);
	EXPECT_EQ(footprint.transform.position, leashes.targetPoint);
	EXPECT_EQ(glm::vec2(ring.transform.scale), leash_marker::RingHalfSize(k_AtHundred));
	EXPECT_EQ(glm::vec2(footprint.transform.scale), leash_marker::FootprintHalfSize(k_AtHundred));

	// a thing that is gone leaves it at the point kept
	leashes.targetThing = registry.Create();
	registry.Destroy(leashes.targetThing);
	const auto gone = LeashMarkerArchetype::Drawn(registry, creature, k_AtHundred);
	ASSERT_TRUE(gone.has_value());
	EXPECT_EQ(gone->front().transform.position, leashes.targetPoint);
}
