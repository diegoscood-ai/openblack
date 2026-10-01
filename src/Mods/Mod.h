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

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "Semver.h"

namespace openblack::mods
{

/// A choice setting of a mod: shown in the Mods window, saved in Mods/<mod>/settings.cfg, set with
/// --mod <mod>.<option>=<choice>
struct ModOption
{
	std::string id;
	std::string label;
	std::vector<std::string> choices;
	size_t value {0}; ///< index into choices
	bool slider {false}; ///< shown as a slider over the choices instead of a drop-down (choices that are a scale)
	std::string description; ///< tooltip (optional)
};

/// How a mod depends on another
struct Dependency
{
	enum class Kind : uint8_t
	{
		Required,     ///< must be there, on and in range, or this mod is blocked
		Optional,     ///< loaded first if it is there; nothing happens if not
		Incompatible, ///< this mod is blocked while that one is on
	};
	std::string id;
	VersionRange range;
	Kind kind {Kind::Required};
};

/// Something that changes the original game, off by default. A mod is a folder of Mods/ with a mod.json (Manifest.h);
/// older folders with a mod.cfg (data mods, modules) are still read. The ModRegistry owns them, draws their window,
/// saves their state, resolves their dependencies and applies them.
class Mod
{
public:
	/// What kind of folder it came from
	enum class Kind : uint8_t
	{
		Package, ///< a mod.json (or a manifest built into openblack)
		Data,    ///< an old folder of replacement files with a mod.cfg ("data.<folder>")
		Module,  ///< an old module folder, mod.cfg "module_of = <mod id>"
	};

	struct Info
	{
		std::string id;          ///< stable key: its folder Mods/<id>/ and --mod, e.g. "graphics.msaa"
		std::string name;        ///< menu label
		std::string description; ///< shown in the Mods window
		std::string category;    ///< menu section, e.g. "Graphics"
		bool restartRequired {false};
		/// Rare: the mod is on the first time the game runs. Mods are off by default (mod-library.md); only when the
		/// user asks for it, as with game.skip-intro. A settings.cfg that already exists still wins, as for any mod
		bool enabledByDefault {false};
		/// A module of another mod (its id): listed under it, and only in effect while that mod is on
		std::string parent;
		Version version {1, 0, 0};
		std::vector<std::string> authors;
		std::filesystem::path icon; ///< absolute path of its image, empty if none
		std::string api;            ///< the mod API range it was written for, e.g. ">=1.0 <2.0" (empty: any)
		std::vector<Dependency> dependencies;
		std::vector<std::string> loadAfter;
		std::vector<std::string> loadBefore;
		std::vector<std::string> provides; ///< interfaces it publishes for other mods, e.g. "foliage.v1"
		std::string pack;                  ///< the modpack it belongs to (its id), empty for a loose mod
		std::string url;
		Kind kind {Kind::Package};
		/// The "replace" object of its mod.json as JSON text (Replacements.h reads it), empty if none
		std::string replaceJson;
		/// mod.json "entry": its Lua script (Lua/LuaHost.h) and its native library (Native/NativeHost.h), absolute,
		/// empty if none
		std::filesystem::path luaEntry;
		std::filesystem::path nativeEntry;
	};

	explicit Mod(Info info, std::filesystem::path root = {});
	virtual ~Mod();

	Mod(const Mod&) = delete;
	Mod& operator=(const Mod&) = delete;

	[[nodiscard]] const Info& GetInfo() const noexcept { return _info; }
	[[nodiscard]] bool IsEnabled() const noexcept { return _enabled; }
	[[nodiscard]] const std::vector<ModOption>& GetOptions() const noexcept { return _options; }
	/// Its folder (Mods/<id>/), where its files and its settings.cfg are
	[[nodiscard]] const std::filesystem::path& GetRoot() const noexcept { return _root; }
	/// Why the registry keeps it off although it is enabled (a missing dependency, an incompatible mod...), or empty
	[[nodiscard]] const std::string& GetBlockedReason() const noexcept { return _blockedReason; }

	/// The engine switches it sets while it is active (Switches.h): name -> value. The registry merges those of every
	/// active mod in load order (a later mod wins) and puts every other switch back to its default. The base adds the
	/// ones its script or library set (ModRegistry::SetRuntimeSwitch); a mod that overrides this calls it last
	virtual void CollectSwitches(std::map<std::string, double, std::less<>>& switches) const;
	/// Puts the rest of the mod's state into effect (anything that is not a switch). Called at start-up, once the
	/// settings and the command line are read, and again whenever the mod or one of its options changes
	virtual void Apply();

	/// The selected choice of an option, or an empty string if there is no such option
	[[nodiscard]] const std::string& GetChoice(std::string_view optionId) const;

protected:
	void AddOption(ModOption option);
	Info& MutableInfo() noexcept { return _info; }

private:
	friend class ModRegistry;

	Info _info;
	std::filesystem::path _root;
	bool _enabled; ///< Info::enabledByDefault, then whatever the settings.cfg and the --mod arguments say
	std::string _blockedReason;
	std::vector<ModOption> _options;
	/// switches its Lua script or native library set while running
	std::map<std::string, double, std::less<>> _runtimeSwitches;
};

/// A modpack: a folder of Mods/ with a modpack.json and its mods inside, each in its own folder. Turning the pack on or
/// off turns all its mods on or off; each one can still be set apart
struct Modpack
{
	std::string id;
	std::string name;
	std::string description;
	std::string category;
	Version version {1, 0, 0};
	std::vector<std::string> authors;
	std::filesystem::path icon;
	std::filesystem::path root;
	std::vector<std::string> mods; ///< ids of its mods, in folder order
};

} // namespace openblack::mods
