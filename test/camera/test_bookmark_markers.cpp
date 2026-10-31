/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Where the camera bookmarks' markers stand (src/Camera/BookmarkMarkers.h), the bookmarks' update placing them and
// hiding them, and the markers' picture

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <bit>

#include <entt/core/hashed_string.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <gtest/gtest.h>

#include "3D/FrameAnim.h"
#include "3D/MapCoords.h"
#include "Camera/BookmarkMarkers.h"
#include "Camera/BookmarkPulse.h"
#include "ECS/Archetypes/CameraBookmarkArchetype.h"
#include "ECS/Components/CameraBookmark.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/CameraBookmarkSystem.h"
#include "Graphics/RenderModes.h"
#include "Locator.h"
#include "support/LandFakes.h"
#include "support/RestoreService.h"

using namespace openblack;

namespace
{
/// 5 cells east and two and a half north of the map's corner, a quarter above the ground
constexpr map_coords::MapCoords k_Dropped {.x = 0x00050000, .z = 0x00028000, .altitude = 0.25f};
/// The land stands 7.5 high everywhere
constexpr float k_Ground = 7.5f;

void ExpectBits(const glm::vec3& value, const glm::vec3& expected)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(value.x), std::bit_cast<uint32_t>(expected.x));
	EXPECT_EQ(std::bit_cast<uint32_t>(value.y), std::bit_cast<uint32_t>(expected.y));
	EXPECT_EQ(std::bit_cast<uint32_t>(value.z), std::bit_cast<uint32_t>(expected.z));
}
} // namespace

// A marker stands 2 above the land point of its map position: the ground there plus the position's altitude
TEST(BookmarkMarkers, AMarkerStandsTwoAboveItsLandPoint)
{
	const test::HeightFieldIsland island([](glm::vec2 /*xz*/) { return k_Ground; });
	ExpectBits(bookmark_markers::MarkerPosition(&island, k_Dropped), glm::vec3(50.0f, 9.75f, 25.0f));
	ASSERT_EQ(island.asked.size(), 1u);
	EXPECT_EQ(island.asked.front(), glm::vec2(50.0f, 25.0f));
	// without a land the ground is 0
	ExpectBits(bookmark_markers::MarkerPosition(nullptr, k_Dropped), glm::vec3(50.0f, 2.25f, 25.0f));
}

// Each drawn frame a set bookmark's marker goes above its land point as the land is then, at the pulse's half size
TEST(BookmarkMarkers, TheUpdatePutsASetMarkerAboveTheLandAsItIsNow)
{
	test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	test::RestoreService<Locator::terrainSystem> restoreTerrain;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	Locator::terrainSystem::emplace<test::HeightFieldIsland>([](glm::vec2 /*xz*/) { return k_Ground; });
	const auto set = registry.Create();
	registry.Assign<ecs::components::CameraBookmark>(set, ecs::components::CameraBookmark {.number = 1, .position = k_Dropped});
	registry.Assign<ecs::components::Transform>(set, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));

	ecs::systems::CameraBookmarkSystem bookmarks {};
	bookmarks.Update(100, false);
	const auto& transform = registry.Get<ecs::components::Transform>(set);
	ExpectBits(transform.position, glm::vec3(50.0f, 9.75f, 25.0f));
	const auto halfSize = bookmark_pulse::MarkerHalfSize(bookmark_pulse::Step({}, 100));
	EXPECT_EQ(std::bit_cast<uint32_t>(transform.scale.x), std::bit_cast<uint32_t>(halfSize.x));
	EXPECT_EQ(std::bit_cast<uint32_t>(transform.scale.y), std::bit_cast<uint32_t>(halfSize.y));

	// the land rises under it: the next frame's marker rises with it
	Locator::terrainSystem::emplace<test::HeightFieldIsland>([](glm::vec2 /*xz*/) { return k_Ground + 1.0f; });
	bookmarks.Update(16, false);
	ExpectBits(registry.Get<ecs::components::Transform>(set).position, glm::vec3(50.0f, 10.75f, 25.0f));
}

// Each marker is its bookmark's own cell of misc0.raw with its alpha, blended as the misc material's mode 6 is, in
// yellow and unturned
TEST(BookmarkMarkers, AMarkerIsItsCellOfTheMiscSheetBlended)
{
	for (uint8_t i = 0; i < 8; ++i)
	{
		const auto sprite = ecs::archetypes::CameraBookmarkArchetype::MarkerSprite(i);
		EXPECT_EQ(sprite.texture, entt::hashed_string("raw/misc0").value()) << int {i};
		ASSERT_TRUE(sprite.alpha.has_value());
		EXPECT_EQ(*sprite.alpha, entt::hashed_string("raw/misc0a").value()) << int {i};
		EXPECT_FALSE(sprite.additive) << int {i};
		EXPECT_EQ(sprite.uvMin, graphics::frame_anim::SpriteCellUv(24 + i)[0]) << int {i};
		EXPECT_EQ(sprite.uvExtent, glm::vec2(1.0f / 8.0f));
		EXPECT_EQ(sprite.tint, glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));
		EXPECT_EQ(std::bit_cast<uint32_t>(sprite.angle), 0u);
	}
	// a blended sprite is drawn in the misc material's mode
	EXPECT_EQ(graphics::render_modes::SpriteMode(false), graphics::render_modes::materials::k_Misc0.mode);
}

// The markers show only while the bookmarks are on and no script holds the wide screen
TEST(BookmarkMarkers, TheMarkersShowWhileOnAndWithoutAScriptsWideScreen)
{
	EXPECT_TRUE(bookmark_markers::MarkersShown(true, false));
	EXPECT_FALSE(bookmark_markers::MarkersShown(true, true));
	EXPECT_FALSE(bookmark_markers::MarkersShown(false, false));
	EXPECT_FALSE(bookmark_markers::MarkersShown(false, true));
}

// Turned off, or behind a script's wide screen, a set bookmark's marker is not drawn (it has no Transform), an unset
// one never is, and once shown again the set one is drawn above its land point; the pulse goes on all the while
TEST(BookmarkMarkers, AHiddenMarkerIsNotDrawnAndComesBackWhereItWas)
{
	test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	test::RestoreService<Locator::terrainSystem> restoreTerrain;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	Locator::terrainSystem::emplace<test::HeightFieldIsland>([](glm::vec2 /*xz*/) { return k_Ground; });
	const auto set = registry.Create();
	registry.Assign<ecs::components::CameraBookmark>(set, ecs::components::CameraBookmark {.number = 1, .position = k_Dropped});
	const auto unset = registry.Create();
	registry.Assign<ecs::components::CameraBookmark>(unset, ecs::components::CameraBookmark {.number = 2});

	ecs::systems::CameraBookmarkSystem bookmarks {};
	bookmarks.Update(10, false);
	EXPECT_TRUE(registry.AllOf<ecs::components::Transform>(set));
	EXPECT_FALSE(registry.AllOf<ecs::components::Transform>(unset));

	bookmarks.Update(10, true);
	EXPECT_FALSE(registry.AllOf<ecs::components::Transform>(set));
	EXPECT_EQ(bookmarks.GetPulse().timeMs, 20);

	bookmarks.Update(10, false);
	ASSERT_TRUE(registry.AllOf<ecs::components::Transform>(set));
	ExpectBits(registry.Get<ecs::components::Transform>(set).position, glm::vec3(50.0f, 9.75f, 25.0f));

	bookmarks.SetEnabled(false);
	bookmarks.Update(10, false);
	EXPECT_FALSE(registry.AllOf<ecs::components::Transform>(set));
	EXPECT_FALSE(registry.AllOf<ecs::components::Transform>(unset));
	EXPECT_EQ(bookmarks.GetPulse().timeMs, 40);

	bookmarks.SetEnabled(true);
	bookmarks.Update(10, false);
	EXPECT_TRUE(registry.AllOf<ecs::components::Transform>(set));
}

// A bookmark dropped on a thing takes the thing's map position; once a turn it follows the thing, its marker stands 2
// above where the thing is drawn, and once the thing is no longer available the bookmark forgets it and stays where
// the thing last was
TEST(BookmarkMarkers, ABookmarkOnAThingFollowsItAndStaysWhenItGoes)
{
	test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	test::RestoreService<Locator::terrainSystem> restoreTerrain;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	Locator::terrainSystem::emplace<test::HeightFieldIsland>([](glm::vec2 /*xz*/) { return k_Ground; });
	const auto thing = registry.Create();
	registry.Assign<ecs::components::Transform>(thing, glm::vec3(50.0f, k_Ground, 25.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	const auto mark = registry.Create();
	registry.Assign<ecs::components::CameraBookmark>(mark, ecs::components::CameraBookmark {.number = 1});

	ecs::systems::CameraBookmarkSystem bookmarks {};
	// the system's bookmark 1 is the entity made above
	const_cast<std::array<entt::entity, 8>&>(bookmarks.GetBookmarks())[0] = mark;
	bookmarks.SetBookmark(0, map_coords::MapCoords {}, thing, glm::vec3(0.0f));
	const auto& bookmark = registry.Get<ecs::components::CameraBookmark>(mark);
	ASSERT_TRUE(bookmark.position.has_value());
	EXPECT_EQ(*bookmark.position, map_coords::FromWorld(glm::vec3(50.0f, k_Ground, 25.0f)));
	ASSERT_TRUE(bookmark.target.has_value());

	// it moves; the turn follows it, and the marker is drawn over it
	registry.Get<ecs::components::Transform>(thing).position = glm::vec3(60.0f, k_Ground, 30.0f);
	static_cast<void>(bookmarks.ProcessTurn(true, false, {}));
	EXPECT_EQ(*bookmark.position, map_coords::FromWorld(glm::vec3(60.0f, k_Ground, 30.0f)));
	bookmarks.Update(16, false);
	ExpectBits(registry.Get<ecs::components::Transform>(mark).position, glm::vec3(60.0f, k_Ground + 2.0f, 30.0f));

	// it goes: the bookmark forgets it at the next turn and stays where it last was
	registry.Destroy(thing);
	static_cast<void>(bookmarks.ProcessTurn(true, false, {}));
	EXPECT_FALSE(bookmark.target.has_value());
	ASSERT_TRUE(bookmark.position.has_value());
	EXPECT_EQ(*bookmark.position, map_coords::FromWorld(glm::vec3(60.0f, k_Ground, 30.0f)));
	bookmarks.Update(16, false);
	ExpectBits(registry.Get<ecs::components::Transform>(mark).position,
	           bookmark_markers::MarkerPosition(&Locator::terrainSystem::value(), *bookmark.position));
}

// A flight to a bookmark looks at the land point of its map position, from the offset the camera had from that point
// when it was dropped; the land moving under it moves the flight with it, and an unset bookmark has none
TEST(BookmarkMarkers, AFlightLooksAtTheLandPointFromTheDroppedOffset)
{
	test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	test::RestoreService<Locator::terrainSystem> restoreTerrain;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	Locator::terrainSystem::emplace<test::HeightFieldIsland>([](glm::vec2 /*xz*/) { return k_Ground; });
	const auto mark = registry.Create();
	registry.Assign<ecs::components::CameraBookmark>(mark, ecs::components::CameraBookmark {.number = 1});
	ecs::systems::CameraBookmarkSystem bookmarks {};
	const_cast<std::array<entt::entity, 8>&>(bookmarks.GetBookmarks())[0] = mark;
	EXPECT_FALSE(bookmarks.FlightTo(0).has_value());

	bookmarks.SetBookmark(0, k_Dropped, std::nullopt, glm::vec3(40.0f, 30.0f, 0.0f));
	ExpectBits(registry.Get<ecs::components::CameraBookmark>(mark).cameraOffset, glm::vec3(-10.0f, 22.25f, -25.0f));
	auto flight = bookmarks.FlightTo(0);
	ASSERT_TRUE(flight.has_value());
	ExpectBits(flight->focus, glm::vec3(50.0f, 7.75f, 25.0f));
	ExpectBits(flight->origin, glm::vec3(40.0f, 30.0f, 0.0f));

	Locator::terrainSystem::emplace<test::HeightFieldIsland>([](glm::vec2 /*xz*/) { return k_Ground + 1.0f; });
	flight = bookmarks.FlightTo(0);
	ASSERT_TRUE(flight.has_value());
	ExpectBits(flight->focus, glm::vec3(50.0f, 8.75f, 25.0f));
	ExpectBits(flight->origin, glm::vec3(40.0f, 31.0f, 0.0f));
}

namespace
{
/// A bookmark system whose bookmarks 1 to 3 are entities of the registry, with the land 7.5 high
struct ThreeBookmarks
{
	test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	test::RestoreService<Locator::terrainSystem> restoreTerrain;
	ecs::Registry& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	ecs::systems::CameraBookmarkSystem bookmarks {};

	ThreeBookmarks()
	{
		Locator::terrainSystem::emplace<test::HeightFieldIsland>([](glm::vec2 /*xz*/) { return k_Ground; });
		auto& slots = const_cast<std::array<entt::entity, 8>&>(bookmarks.GetBookmarks());
		for (uint8_t i = 0; i < 3; ++i)
		{
			slots.at(i) = registry.Create();
			registry.Assign<ecs::components::CameraBookmark>(
			    slots.at(i), ecs::components::CameraBookmark {.number = static_cast<uint8_t>(i + 1)});
		}
	}
};
} // namespace

// Shift with the key of a bookmark on a thing follows it: each turn of the player's camera flies to it; again stops;
// another camera, or a turn whose camera input counted for the camera help, stops it too
TEST(BookmarkMarkers, ShiftFollowsABookmarkOnAThingEachTurn)
{
	ThreeBookmarks three;
	auto& registry = three.registry;
	auto& bookmarks = three.bookmarks;
	const auto thing = registry.Create();
	registry.Assign<ecs::components::Transform>(thing, glm::vec3(50.0f, k_Ground, 25.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	bookmarks.SetBookmark(2, map_coords::MapCoords {}, thing, glm::vec3(40.0f, 30.0f, 0.0f));
	EXPECT_EQ(bookmarks.GetCurrent(), 2u);

	EXPECT_FALSE(bookmarks.ToggleFollow(2, {}).has_value());
	ASSERT_EQ(bookmarks.GetFollowed(), std::optional<uint8_t>(2));
	registry.Get<ecs::components::Transform>(thing).position = glm::vec3(60.0f, k_Ground, 25.0f);
	auto flight = bookmarks.ProcessTurn(true, false, {});
	ASSERT_TRUE(flight.has_value());
	ExpectBits(flight->focus,
	           map_coords::ToWorld(&Locator::terrainSystem::value(), map_coords::FromWorld(glm::vec3(60.0f, k_Ground, 25.0f))));
	EXPECT_EQ(bookmarks.GetFollowed(), std::optional<uint8_t>(2));

	// not inside the citadel: no flight, the follow kept
	EXPECT_FALSE(bookmarks.ProcessTurn(true, false, {.insideCitadel = true}).has_value());
	EXPECT_EQ(bookmarks.GetFollowed(), std::optional<uint8_t>(2));
	// a camera input that counted: this turn's flight, then no more
	EXPECT_TRUE(bookmarks.ProcessTurn(true, true, {}).has_value());
	EXPECT_FALSE(bookmarks.GetFollowed().has_value());
	EXPECT_FALSE(bookmarks.ProcessTurn(true, false, {}).has_value());

	// another camera ends it with no flight; Shift again stops it
	EXPECT_FALSE(bookmarks.ToggleFollow(2, {}).has_value());
	EXPECT_FALSE(bookmarks.ProcessTurn(false, false, {}).has_value());
	EXPECT_FALSE(bookmarks.GetFollowed().has_value());
	EXPECT_FALSE(bookmarks.ToggleFollow(2, {}).has_value());
	EXPECT_TRUE(bookmarks.GetFollowed().has_value());
	EXPECT_FALSE(bookmarks.ToggleFollow(2, {}).has_value());
	EXPECT_FALSE(bookmarks.GetFollowed().has_value());
}

// Shift with the key of a bookmark on no thing jumps to it and follows nothing; the key alone makes it the current one
// and stops a follow of another; Shift and Ctrl clear one, which no key flies to any more
TEST(BookmarkMarkers, ShiftJumpsToABookmarkOnNoThingAndShiftCtrlClearsOne)
{
	ThreeBookmarks three;
	auto& registry = three.registry;
	auto& bookmarks = three.bookmarks;
	const auto thing = registry.Create();
	registry.Assign<ecs::components::Transform>(thing, glm::vec3(50.0f, k_Ground, 25.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	bookmarks.SetBookmark(0, k_Dropped, std::nullopt, glm::vec3(40.0f, 30.0f, 0.0f));
	bookmarks.SetBookmark(2, map_coords::MapCoords {}, thing, glm::vec3(40.0f, 30.0f, 0.0f));
	EXPECT_FALSE(bookmarks.ToggleFollow(2, {}).has_value());
	ASSERT_TRUE(bookmarks.GetFollowed().has_value());

	const auto flight = bookmarks.ToggleFollow(0, {});
	ASSERT_TRUE(flight.has_value());
	ExpectBits(flight->origin, glm::vec3(40.0f, 30.0f, 0.0f));
	EXPECT_FALSE(bookmarks.GetFollowed().has_value());
	EXPECT_EQ(bookmarks.GetCurrent(), 0u);

	EXPECT_FALSE(bookmarks.ToggleFollow(2, {}).has_value());
	EXPECT_TRUE(bookmarks.JumpTo(0, {}).has_value());
	EXPECT_FALSE(bookmarks.GetFollowed().has_value());
	// a jump to the followed one keeps the follow
	EXPECT_FALSE(bookmarks.ToggleFollow(2, {}).has_value());
	EXPECT_TRUE(bookmarks.JumpTo(2, {}).has_value());
	EXPECT_EQ(bookmarks.GetFollowed(), std::optional<uint8_t>(2));

	bookmarks.ClearBookmark(0);
	EXPECT_FALSE(bookmarks.IsSet(0));
	EXPECT_FALSE(bookmarks.JumpTo(0, {}).has_value());
	EXPECT_FALSE(bookmarks.ToggleFollow(0, {}).has_value());
	bookmarks.Update(16, false);
	EXPECT_FALSE(registry.AllOf<ecs::components::Transform>(bookmarks.GetBookmarks()[0]));
}

// A jump keeps the view it leaves as the bookmark before the jump: the interface status's place, and the camera's
// offset from its land point; Backspace jumps back to it, and that jump keeps the view it leaves in its turn. A new
// land forgets it
TEST(BookmarkMarkers, AJumpKeepsTheViewItLeavesForBackspace)
{
	ThreeBookmarks three;
	auto& bookmarks = three.bookmarks;
	constexpr auto k_BeforeJump = ecs::systems::k_BeforeJumpBookmark;
	EXPECT_FALSE(bookmarks.IsSet(k_BeforeJump));
	EXPECT_FALSE(bookmarks.JumpTo(k_BeforeJump, {}).has_value());

	bookmarks.SetBookmark(0, k_Dropped, std::nullopt, glm::vec3(40.0f, 30.0f, 0.0f));
	// the hand synced 3 cells east and 1 north, the camera elsewhere
	constexpr map_coords::MapCoords k_Status {.x = 0x00030000, .z = 0x00010000, .altitude = 0.0f};
	const auto first = bookmarks.JumpTo(0, {.statusPlace = k_Status, .cameraOrigin = glm::vec3(20.0f, 40.0f, -5.0f)});
	ASSERT_TRUE(first.has_value());
	ExpectBits(first->origin, glm::vec3(40.0f, 30.0f, 0.0f));
	ASSERT_TRUE(bookmarks.IsSet(k_BeforeJump));

	// Backspace: back to the land point of the status's place (30, 7.5, 10), from the camera's offset then
	const auto back = bookmarks.JumpTo(k_BeforeJump, {.statusPlace = k_Dropped, .cameraOrigin = first->origin});
	ASSERT_TRUE(back.has_value());
	ExpectBits(back->focus, glm::vec3(30.0f, 7.5f, 10.0f));
	ExpectBits(back->origin, glm::vec3(20.0f, 40.0f, -5.0f));
	EXPECT_EQ(bookmarks.GetCurrent(), k_BeforeJump);
	// and that jump kept the view it left: Backspace again goes to bookmark 1's view
	const auto again = bookmarks.JumpTo(k_BeforeJump, {.statusPlace = k_Status, .cameraOrigin = back->origin});
	ASSERT_TRUE(again.has_value());
	ExpectBits(again->focus, glm::vec3(50.0f, 7.75f, 25.0f));
	ExpectBits(again->origin, glm::vec3(40.0f, 30.0f, 0.0f));

	// not inside the citadel
	EXPECT_FALSE(bookmarks.JumpTo(k_BeforeJump, {.insideCitadel = true}).has_value());
}
