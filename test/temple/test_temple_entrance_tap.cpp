/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Tapping a temple's entrance (worship::citadel::EntranceTap): the player's own temple, tapped from this computer's
// interface, takes the player inside; another player's temple, or a tap from another interface, does not.
// docs/wiki/project/progress/hand/clicking_and_activating.md

#include <cstdint>

#include <chrono>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "3D/OrientedText.h"
#include "3D/TempleDoors.h"
#include "3D/TempleInteriorInterface.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerMagic.h"
#include "ECS/Components/Temple.h"
#include "ECS/Registry.h"
#include "Enums.h"
#include "Locator.h"
#include "Worship/Citadel.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// The temple as far as the tap asks of it: whether the player is in it, and entering it
class FakeTemple final: public TempleInteriorInterface
{
public:
	bool active {false};
	uint32_t entries {0};

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
	void Activate(TempleRoom) override
	{
		active = true;
		++entries;
	}
	void Deactivate() override { active = false; }

private:
	TempleDoors _doors {[](const temple_sounds::Options&) {}};
	std::vector<OrientedTextVertex> _text;
	std::vector<TempleMapMarker> _markers;
	std::vector<TempleCaveTrophy> _trophies;
	TempleLight _light;
};

/// A registry with the human player and a temple of the test's own, put back when the test ends
class TempleEntranceTap: public ::testing::Test
{
protected:
	void SetUp() override
	{
		_registry = &Locator::entitiesRegistry::emplace<ecs::Registry>();
		_temple = &static_cast<FakeTemple&>(Locator::temple::emplace<FakeTemple>());
		// the human player
		const auto player = _registry->Create();
		_registry->Assign<Player>(player, PlayerNames::PLAYER_ONE);
		_registry->Assign<PlayerMagic>(player).playerType = 1;
	}

	/// The entrance of a temple of the player
	[[nodiscard]] entt::entity Entrance(PlayerNames owner) const
	{
		const auto heart = _registry->Create();
		_registry->Assign<Temple>(heart, Temple {.owner = owner});
		const auto entrance = _registry->Create();
		_registry->Assign<CitadelEntrance>(entrance, CitadelEntrance {.heart = heart});
		return entrance;
	}

	ecs::Registry* _registry {nullptr};
	FakeTemple* _temple {nullptr};

private:
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::temple> _restoreTemple;
};
} // namespace

TEST_F(TempleEntranceTap, TappingThePlayersOwnTempleEntranceTakesThemInside)
{
	const auto entrance = Entrance(PlayerNames::PLAYER_ONE);
	EXPECT_EQ(worship::citadel::EntranceTap(entrance, true), 1u);
	EXPECT_TRUE(_temple->active);
	EXPECT_EQ(_temple->entries, 1u);
	// already inside: not entered again
	EXPECT_EQ(worship::citadel::EntranceTap(entrance, true), 1u);
	EXPECT_EQ(_temple->entries, 1u);
}

TEST_F(TempleEntranceTap, AnotherInterfacesTapOrAnotherPlayersTempleStaysOutside)
{
	EXPECT_EQ(worship::citadel::EntranceTap(Entrance(PlayerNames::PLAYER_ONE), false), 1u);
	EXPECT_EQ(worship::citadel::EntranceTap(Entrance(PlayerNames::PLAYER_TWO), true), 1u);
	// an entrance with no temple
	const auto loose = _registry->Create();
	_registry->Assign<CitadelEntrance>(loose);
	EXPECT_EQ(worship::citadel::EntranceTap(loose, true), 1u);
	EXPECT_FALSE(_temple->active);
	EXPECT_EQ(_temple->entries, 0u);
}
