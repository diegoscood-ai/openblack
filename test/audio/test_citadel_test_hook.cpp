/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The temple's audio test hook (ecs::audio_queries::RunCitadelTestHook): its turns are the game's own, read from
// OPENBLACK_AUDIO_TEST_CITADEL as the game's debug hooks' store is made, or given by a list's land run for that game
// only. A fake temple; a debug hooks' store of the test's own, made again where a new game makes it again.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstdlib>

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/OrientedText.h"
#include "3D/TempleDoors.h"
#include "3D/TempleInteriorInterface.h"
#include "ECS/AudioQueries.h"
#include "ECS/Systems/Implementations/DebugHooks.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs::audio_queries;

namespace
{
constexpr const char* k_Variable = "OPENBLACK_AUDIO_TEST_CITADEL";

/// Sets the hook's variable, or takes it away when the value is empty
void SetCitadelVariable(const char* value)
{
#if defined(_WIN32)
	_putenv_s(k_Variable, value);
#else
	if (*value == '\0')
	{
		unsetenv(k_Variable);
	}
	else
	{
		setenv(k_Variable, value, 1);
	}
#endif
}

/// Takes the caller's value of the hook's variable away for a test, and gives it back when the test ends
class SavedCitadelVariable
{
public:
	SavedCitadelVariable()
	{
		if (const char* value = std::getenv(k_Variable); value != nullptr)
		{
			_saved = value;
		}
		SetCitadelVariable("");
	}
	~SavedCitadelVariable() { SetCitadelVariable(_saved ? _saved->c_str() : ""); }
	SavedCitadelVariable(const SavedCitadelVariable&) = delete;
	SavedCitadelVariable& operator=(const SavedCitadelVariable&) = delete;
	SavedCitadelVariable(SavedCitadelVariable&&) = delete;
	SavedCitadelVariable& operator=(SavedCitadelVariable&&) = delete;

private:
	std::optional<std::string> _saved;
};

/// Sets the hook's variable for the test, and takes it away again when it goes
class ScopedCitadelVariable
{
public:
	explicit ScopedCitadelVariable(const char* value) { SetCitadelVariable(value); }
	~ScopedCitadelVariable() { SetCitadelVariable(""); }
	ScopedCitadelVariable(const ScopedCitadelVariable&) = delete;
	ScopedCitadelVariable& operator=(const ScopedCitadelVariable&) = delete;
	ScopedCitadelVariable(ScopedCitadelVariable&&) = delete;
	ScopedCitadelVariable& operator=(ScopedCitadelVariable&&) = delete;
};

/// The temple as far as the hook asks of it: whether the player is in it, entering and leaving it
class FakeTemple final: public TempleInteriorInterface
{
public:
	bool active {false};
	uint32_t entries {0};
	uint32_t exits {0};

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
	void Deactivate() override
	{
		active = false;
		++exits;
	}

private:
	TempleDoors _doors {[](const temple_sounds::Options&) {}};
	std::vector<OrientedTextVertex> _text;
	std::vector<TempleMapMarker> _markers;
	std::vector<TempleCaveTrophy> _trophies;
	TempleLight _light;
};

/// A temple and a game's debug hooks' store of the test's own, both put back afterwards, and the hook's variable unset
/// for the test whatever the caller gave it
class CitadelTestHookTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (spdlog::get("audio") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("audio");
		}
		_temple = &static_cast<FakeTemple&>(Locator::temple::emplace<FakeTemple>());
	}

	/// A new game's store, which reads the variable again on its first use
	static void NewGameStore() { Locator::debugHooks::emplace<ecs::systems::DebugHooks>(); }

	/// The hook's turns up to `turn`, from the last one run
	void RunTo(uint32_t turn)
	{
		for (; _turn < turn; ++_turn)
		{
			RunCitadelTestHook();
		}
	}

	FakeTemple* _temple {nullptr};
	uint32_t _turn {0};

private:
	SavedCitadelVariable _savedVariable;
	test::RestoreService<Locator::temple> _restoreTemple;
	test::RestoreService<Locator::debugHooks> _restoreDebugHooks;
};
} // namespace

TEST(CitadelTestTurns, TheVariableIsReadAsTheHookReadsIt)
{
	EXPECT_FALSE(ParseCitadelTestTurns(nullptr).has_value());
	EXPECT_FALSE(ParseCitadelTestTurns("").has_value());
	EXPECT_FALSE(ParseCitadelTestTurns("x").has_value());
	const auto in = ParseCitadelTestTurns("120");
	ASSERT_TRUE(in.has_value());
	EXPECT_EQ(in->in, 120u);
	EXPECT_EQ(in->out, 0u);
	const auto inOut = ParseCitadelTestTurns("120,200");
	ASSERT_TRUE(inOut.has_value());
	EXPECT_EQ(inOut->in, 120u);
	EXPECT_EQ(inOut->out, 200u);
	// what follows the numbers is not looked at
	const auto extra = ParseCitadelTestTurns("120,200,9");
	ASSERT_TRUE(extra.has_value());
	EXPECT_EQ(extra->out, 200u);
	const auto trailing = ParseCitadelTestTurns("120x");
	ASSERT_TRUE(trailing.has_value());
	EXPECT_EQ(trailing->in, 120u);
	EXPECT_EQ(trailing->out, 0u);
}

TEST_F(CitadelTestHookTest, WithoutTheVariableTheTempleIsNeverEntered)
{
	NewGameStore();
	RunTo(300);
	EXPECT_EQ(_temple->entries, 0u);
	EXPECT_EQ(_temple->exits, 0u);
}

TEST_F(CitadelTestHookTest, TheVariableEntersAndLeavesAtItsHookTurns)
{
	const ScopedCitadelVariable variable("3,5");
	NewGameStore();
	RunTo(2);
	EXPECT_FALSE(_temple->active);
	RunTo(3);
	EXPECT_TRUE(_temple->active);
	RunTo(4);
	EXPECT_TRUE(_temple->active);
	RunTo(5);
	EXPECT_FALSE(_temple->active);
	RunTo(20);
	EXPECT_EQ(_temple->entries, 1u);
	EXPECT_EQ(_temple->exits, 1u);
}

TEST_F(CitadelTestHookTest, TheTurnsCountWithoutATemple)
{
	const ScopedCitadelVariable variable("3");
	NewGameStore();
	Locator::temple::reset();
	RunTo(2);
	_temple = &static_cast<FakeTemple&>(Locator::temple::emplace<FakeTemple>());
	RunTo(3);
	EXPECT_TRUE(_temple->active);
	// no way out without one
	RunTo(50);
	EXPECT_EQ(_temple->exits, 0u);
}

TEST_F(CitadelTestHookTest, AListsTurnsReplaceTheVariablesForTheirGameOnly)
{
	const ScopedCitadelVariable variable("3");
	NewGameStore();
	SetCitadelTestTurns({.in = 2, .out = 4});
	RunTo(2);
	EXPECT_TRUE(_temple->active);
	// the variable's turn 3 is not this game's
	RunTo(3);
	EXPECT_EQ(_temple->entries, 1u);
	RunTo(4);
	EXPECT_FALSE(_temple->active);
	RunTo(10);
	EXPECT_EQ(_temple->entries, 1u);
	// the next game's store reads the variable again, and its turns count from 1
	NewGameStore();
	_turn = 0;
	RunTo(2);
	EXPECT_EQ(_temple->entries, 1u);
	RunTo(3);
	EXPECT_EQ(_temple->entries, 2u);
	EXPECT_TRUE(_temple->active);
}
