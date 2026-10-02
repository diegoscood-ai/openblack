/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace openblack::mods::switches
{

/// The engine switches a mod can set, by name ("graphics.msaa.samples"). Every one is a field of EngineConfig that the
/// engine reads; the mods never touch EngineConfig directly any more, they set switches (from mod.json, Lua or a DLL),
/// so one table (EngineSwitches.cpp) says what can be changed, its type and when it takes effect.
///
/// Values travel as double: a bool is 0 or 1, an int is rounded.
enum class Type : uint8_t
{
	Bool,
	Int,
	Float,
};

/// When a change takes effect
enum class When : uint8_t
{
	Live,    ///< at once
	MapLoad, ///< the next time a land loads
	Restart, ///< the next time openblack starts
};

struct Switch
{
	std::string name;
	Type type {Type::Bool};
	When when {When::Live};
	std::string description;
	double min {0.0};
	double max {1.0};
	double defaultValue {0.0};
	std::function<double()> get;
	std::function<void(double)> set;
	/// Optional: what the engine must do when the value changes (e.g. rebuild the back buffer for MSAA)
	std::function<void()> onChange;
};

/// Adds a switch; a second one with the same name replaces the first
void Register(Switch value);
/// nullptr if there is no such switch
[[nodiscard]] const Switch* Find(std::string_view name);
[[nodiscard]] const std::vector<Switch>& All();

/// Sets a switch, clamped to its range and rounded for Bool / Int; calls its onChange when the value changed.
/// @return false if there is no such switch, or the value is NaN or infinite
bool Set(std::string_view name, double value);
/// The current value, or nullopt-like NaN if there is no such switch
[[nodiscard]] double Get(std::string_view name);
/// Every switch back to its default value (all mods off)
void ResetAll();
/// Removes every switch (tests)
void Clear();

[[nodiscard]] std::string_view TypeName(Type type);
[[nodiscard]] std::string_view WhenName(When when);

/// The table of EngineConfig fields (EngineSwitches.cpp). Call once, after Locator::config exists
void RegisterEngineSwitches();

} // namespace openblack::mods::switches
