/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// KEY_DOWN, as a script calls it: whether the key it names by the game's own key number is held, read from a fake of
// the game's input that holds the keys it is told to; and the table from the game's key numbers to SDL's

#include <cstdint>

#include <algorithm>
#include <array>
#include <optional>
#include <set>
#include <string_view>

#include <LHVM.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "Input/GameActionMapInterface.h"
#include "Input/GameKeys.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using input::BindableActionMap;

namespace
{
/// The game's input with no actions, holding the keys in `held`
class HeldKeys final: public input::GameActionInterface
{
public:
	std::set<SDL_Scancode> held;

	[[nodiscard]] bool GetBindable(BindableActionMap) const override { return false; }
	[[nodiscard]] bool GetUnbindable(input::UnbindableActionMap) const override { return false; }
	[[nodiscard]] bool GetBindableChanged(BindableActionMap) const override { return false; }
	[[nodiscard]] bool GetUnbindableChanged(input::UnbindableActionMap) const override { return false; }
	[[nodiscard]] bool GetBindableRepeat(BindableActionMap) const override { return false; }
	[[nodiscard]] bool GetUnbindableRepeat(input::UnbindableActionMap) const override { return false; }
	[[nodiscard]] glm::uvec2 GetMousePosition() const override { return {}; }
	[[nodiscard]] glm::ivec2 GetMouseDelta() const override { return {}; }
	[[nodiscard]] float GetMouseWheelDelta() const override { return 0.0f; }
	[[nodiscard]] std::array<std::optional<glm::vec3>, 2> GetHandPositions() const override { return {}; }
	[[nodiscard]] bool IsKeyHeld(SDL_Scancode key) const override { return held.contains(key); }
	void Frame() override {}
	void ProcessEvent(const SDL_Event&) override {}
};

class KeyDownNative: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// the natives log to the scripts' logger: a null sink, as the other script tests
		if (!spdlog::get("scripting"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		Locator::vm::emplace<lhvm::LHVM>();
		_keys = &static_cast<HeldKeys&>(Locator::gameActionSystem::emplace<HeldKeys>());
	}

	/// The native as a script calls it, with a value under its one that it must leave there
	bool KeyDown(int32_t key)
	{
		const auto& table = _api.GetFunctionsTable();
		const auto native =
		    std::ranges::find_if(table, [](const lhvm::NativeFunction& f) { return std::string_view(f.name) == "KEY_DOWN"; });
		EXPECT_NE(native, table.end());
		if (native == table.end())
		{
			return false;
		}
		EXPECT_EQ(native->stackIn, 1);
		EXPECT_EQ(native->stackOut, 1u);
		auto& vm = Locator::vm::value();
		vm.Pushi(77);
		vm.Pushi(key);
		native->impl();
		const bool down = vm.Pop().intVal != 0;
		EXPECT_EQ(vm.Pop().intVal, 77);
		return down;
	}

	test::RestoreService<Locator::vm> _restoreVm;
	test::RestoreService<Locator::gameActionSystem> _restoreActions;
	chlapi::CHLApi _api;
	HeldKeys* _keys {nullptr};
};
} // namespace

TEST(GameKeys, TheScriptsSpaceAndTheExtendedKeysAreSdlsKeys)
{
	// 57, the only key the shipped scripts ask about, is the space bar
	EXPECT_EQ(input::game_keys::ScancodeOf(57), SDL_SCANCODE_SPACE);
	EXPECT_EQ(input::game_keys::ScancodeOf(0x01), SDL_SCANCODE_ESCAPE);
	EXPECT_EQ(input::game_keys::ScancodeOf(0x1C), SDL_SCANCODE_RETURN);
	// an extended key is its plain key's number with 0x80 added
	EXPECT_EQ(input::game_keys::ScancodeOf(0x1C + 0x80), SDL_SCANCODE_KP_ENTER);
	EXPECT_EQ(input::game_keys::ScancodeOf(0x48), SDL_SCANCODE_KP_8);
	EXPECT_EQ(input::game_keys::ScancodeOf(0x48 + 0x80), SDL_SCANCODE_UP);
	EXPECT_EQ(input::game_keys::ScancodeOf(0x56), SDL_SCANCODE_NONUSBACKSLASH);
	EXPECT_EQ(input::game_keys::ScancodeOf(0x58), SDL_SCANCODE_F12);
}

TEST(GameKeys, ANumberThatIsNoKeyHasNone)
{
	EXPECT_FALSE(input::game_keys::ScancodeOf(0).has_value());
	EXPECT_FALSE(input::game_keys::ScancodeOf(0x54).has_value());
	EXPECT_FALSE(input::game_keys::ScancodeOf(256 + 57).has_value());
	EXPECT_FALSE(input::game_keys::ScancodeOf(-1).has_value());
}

TEST(GameKeys, EachKeyIsNumberedOnceAndGoesToItsOwnScancode)
{
	const auto& keys = input::game_keys::k_GameKeys;
	EXPECT_TRUE(std::ranges::is_sorted(keys, {}, [](const auto& entry) { return entry.first; }));
	EXPECT_EQ(std::ranges::adjacent_find(keys, {}, [](const auto& entry) { return entry.first; }), keys.end());
	std::set<SDL_Scancode> scancodes;
	for (const auto& [number, scancode] : keys)
	{
		EXPECT_TRUE(scancodes.insert(scancode).second) << "key " << int {number};
	}
}

TEST_F(KeyDownNative, IsWhetherTheKeyIsHeld)
{
	EXPECT_FALSE(KeyDown(57));
	_keys->held.insert(SDL_SCANCODE_SPACE);
	EXPECT_TRUE(KeyDown(57));
	// another key is not the space bar, and a number that is no key is never held
	EXPECT_FALSE(KeyDown(0x1C));
	EXPECT_FALSE(KeyDown(0x54));
}
