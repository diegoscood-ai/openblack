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

#include <string>
#include <string_view>
#include <vector>

namespace openblack::mods
{

/// A choice setting of a mod: shown in the Mods menu, saved in Mods/<mod>/settings.cfg, set with --mod <mod>.<option>=<choice>
struct ModOption
{
	std::string id;
	std::string label;
	std::vector<std::string> choices;
	size_t value {0}; ///< index into choices
	bool slider {false}; ///< shown as a slider over the choices instead of a drop-down (choices that are a scale)
};

/// Something that changes the original game, off by default. Built-in mods are C++ classes (src/Mods/*Mods.cpp);
/// data mods are folders of replacement files (DataMod). The ModRegistry owns them, draws their menu, saves their
/// state and applies them.
class Mod
{
public:
	struct Info
	{
		std::string id;          ///< stable key: its folder Mods/<id>/ and --mod, e.g. "graphics.msaa"
		std::string name;        ///< menu label
		std::string description; ///< tooltip
		std::string category;    ///< menu section, e.g. "Graphics"
		bool restartRequired {false};
		/// A module of another mod (its id): listed under it in the menu, and only in effect while that mod is on
		std::string parent;
	};

	explicit Mod(Info info);
	virtual ~Mod();

	Mod(const Mod&) = delete;
	Mod& operator=(const Mod&) = delete;

	[[nodiscard]] const Info& GetInfo() const noexcept { return _info; }
	[[nodiscard]] bool IsEnabled() const noexcept { return _enabled; }
	[[nodiscard]] const std::vector<ModOption>& GetOptions() const noexcept { return _options; }

	/// Puts the mod's current state into effect. Called once at start-up, once the settings and the command line are
	/// read, and again whenever the mod or one of its options changes.
	virtual void Apply() = 0;

protected:
	void AddOption(ModOption option);
	/// The selected choice of an option, or an empty string if there is no such option
	[[nodiscard]] const std::string& GetChoice(std::string_view optionId) const;

private:
	friend class ModRegistry;

	Info _info;
	bool _enabled {false};
	std::vector<ModOption> _options;
};

} // namespace openblack::mods
