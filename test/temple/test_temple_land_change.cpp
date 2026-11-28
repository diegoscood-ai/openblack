/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What a land's load does with the temple the player may be inside (land_load::TempleBeforeLoad / TempleAfterLoad): a
// script's change of land leaves the player inside it, and every other load takes them out first.
// docs/wiki/scripts/land-change-and-saving.md#load-map-inside-the-temple

#include <cstdint>

#include <chrono>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "3D/OrientedText.h"
#include "3D/TempleDoors.h"
#include "3D/TempleInteriorInterface.h"
#include "LandLoad.h"
#include "LandLoadTemple.h"

using namespace openblack;

namespace
{
/// The temple as far as a land's load asks of it: whether the player is in it, leaving it and keeping them in it
class FakeTemple final: public TempleInteriorInterface
{
public:
	bool active {false};
	uint32_t keptRoomsForgotten {0};
	uint32_t landChangesBegun {0};
	uint32_t landChangesEnded {0};

	[[nodiscard]] bool Active() const override { return active; }
	[[nodiscard]] glm::vec3 GetPosition() const override { return {}; }
	[[nodiscard]] TempleRoom GetCurrentRoom() const override { return TempleRoom::Main; }
	void SetCurrentRoom(TempleRoom) override {}
	[[nodiscard]] std::optional<TempleRoom> GetTransitionRoom() const override { return std::nullopt; }
	void SetTransitionRoom(std::optional<TempleRoom>) override {}
	void GoToRoom(TempleRoom) override {}
	void EnterRoom(TempleRoom) override {}
	void LookAtScroll(size_t) override {}
	[[nodiscard]] bool IsRoomDrawn(TempleRoom) const override { return false; }
	[[nodiscard]] TempleDoors& GetDoors() override { return _doors; }
	[[nodiscard]] const TempleDoors& GetDoors() const override { return _doors; }
	[[nodiscard]] std::optional<TempleCursorHit> GetCursorHit() const override { return std::nullopt; }
	void SetInterface(gui::GameInterface*) override {}
	bool HoldControl(bool, float) override { return false; }
	[[nodiscard]] std::vector<TempleSubMeshTexture> GetScrollTextures(TempleRoom) const override { return {}; }
	[[nodiscard]] std::vector<uint32_t> GetHiddenSubMeshes(TempleRoom) const override { return {}; }
	[[nodiscard]] std::vector<TempleSubMeshGlow> GetControlGlows(TempleRoom) const override { return {}; }
	[[nodiscard]] const std::vector<OrientedTextVertex>& GetText() const override { return _text; }
	[[nodiscard]] const graphics::Texture2D* GetTextTexture() const override { return nullptr; }
	[[nodiscard]] const std::vector<OrientedTextVertex>& GetMap() const override { return _text; }
	[[nodiscard]] uint32_t GetVisits() const override { return 0; }
	[[nodiscard]] const std::vector<TempleMapMarker>& GetMapMarkers() const override { return _markers; }
	[[nodiscard]] const TempleLight& GetLight() const override { return _light; }
	[[nodiscard]] const std::vector<TempleCaveTrophy>& GetCaveTrophies() const override { return _trophies; }
	[[nodiscard]] float GetMapMarkerTurn() const override { return 0.0f; }
	[[nodiscard]] float GetPoolTime() const override { return 0.0f; }
	void Escape() override {}
	void RequestLeave() override {}
	void FadeToWhite() override {}
	void Update(std::chrono::microseconds) override {}
	void ProcessTurn() override {}
	[[nodiscard]] glm::vec2 GetWaterfallSlide() const override { return {}; }
	void Activate() override { Activate(TempleRoom::Main); }
	void Activate(TempleRoom) override { active = true; }
	void ForgetKeptRoom() override { ++keptRoomsForgotten; }
	void BeginLandChange() override { ++landChangesBegun; }
	void EndLandChange() override { ++landChangesEnded; }
	void Deactivate() override { active = false; }

private:
	TempleDoors _doors {[](const temple_sounds::Options&) {}};
	std::vector<OrientedTextVertex> _text;
	std::vector<TempleMapMarker> _markers;
	std::vector<TempleCaveTrophy> _trophies;
	TempleLight _light;
};

/// A script's change of land (LOAD_MAP) of the campaign: the next land, no new game
land_load::Resets ScriptLandChange()
{
	auto resets = land_load::ResetsFor(false, false);
	resets.keepsTemple = true;
	return resets;
}
} // namespace

TEST(TempleLandChange, AScriptsChangeOfLandLeavesThePlayerInsideTheTemple)
{
	FakeTemple temple;
	temple.active = true;
	land_load::TempleBeforeLoad(temple, ScriptLandChange());
	// the player is still inside, and the temple lets go of the land before it goes; the room they may be kept in stays
	EXPECT_TRUE(temple.active);
	EXPECT_EQ(temple.landChangesBegun, 1u);
	EXPECT_EQ(temple.keptRoomsForgotten, 0u);
	land_load::TempleAfterLoad(temple, ScriptLandChange());
	// and once the next land is loaded, the rooms are set up again on it, still inside
	EXPECT_TRUE(temple.active);
	EXPECT_EQ(temple.landChangesEnded, 1u);
}

TEST(TempleLandChange, EveryOtherLoadTakesThePlayerOutOfTheTempleFirst)
{
	// a new game, a playground's start, and a land loaded other than by a script (Load Island without the creature)
	for (const auto resets :
	     {land_load::ResetsFor(true, false), land_load::ResetsFor(false, true), land_load::ResetsFor(false, false)})
	{
		FakeTemple temple;
		temple.active = true;
		land_load::TempleBeforeLoad(temple, resets);
		EXPECT_FALSE(temple.active);
		EXPECT_EQ(temple.keptRoomsForgotten, 1u);
		land_load::TempleAfterLoad(temple, resets);
		EXPECT_EQ(temple.landChangesBegun, 0u);
		EXPECT_EQ(temple.landChangesEnded, 0u);
	}
}

TEST(TempleLandChange, AScriptsChangeOfLandWithThePlayerOutsideLeavesTheTempleAsItWas)
{
	FakeTemple temple;
	land_load::TempleBeforeLoad(temple, ScriptLandChange());
	land_load::TempleAfterLoad(temple, ScriptLandChange());
	EXPECT_FALSE(temple.active);
	EXPECT_EQ(temple.landChangesBegun, 0u);
	EXPECT_EQ(temple.landChangesEnded, 0u);
	// a room the game over kept the player in holds them no longer, as for every load
	EXPECT_EQ(temple.keptRoomsForgotten, 1u);
}
