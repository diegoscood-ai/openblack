/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// The mod loader's C API: the mod SDK's first SDK-facing header. Every header of src/ModSdk is the SDK's surface, and
// the SDK's version is a hash of them all, generated at build time into the game and the library alike
// (generated/ModSdk/ModSdkVersion.h): a game and a library built from different SDK headers do not agree, and the game
// starts unmodded.
//
// The mod loader is a shared library of its own (ModLoader.dll), which the game loads at start-up when it is there and
// never links against. It checks the mods folder's version, lists the mods its folders hold (each with a mod.txt) and
// keeps which are switched on (enabled.txt); a switched-on mod's files replace the game's (data mods). It depends on
// nothing but the standard library.

#if defined(_WIN32)
#if defined(MODLOADER_BUILD)
#define MODLOADER_API __declspec(dllexport)
#else
#define MODLOADER_API __declspec(dllimport)
#endif
#else
#define MODLOADER_API __attribute__((visibility("default")))
#endif

/// The mods folder's version: a mods folder is read only when its version.txt holds this number. The game and the
/// library agree by the SDK's version instead (ModLoader_SdkVersion)
constexpr int k_ModLoaderVersion = 2;

/// The values ModLoader_LoadMods returns, as an int:
/// 0 = version.txt matches (no mod is loaded yet);
/// 1 = there is no version.txt, or no folder was given: no mod is read;
/// 2 = version.txt cannot be read or does not hold an integer: no mod is read;
/// 3 = version.txt holds another version: no mod is read.
enum class ModLoaderResult : int
{
	Matched = 0,
	NoVersionFile = 1,
	UnreadableVersion = 2,
	VersionMismatch = 3,
};

/// Receives the one line ModLoader_LoadMods reports, with the context it was given
using ModLoaderLogFn = void (*)(const char* line, void* context);

/// The names the game looks the functions up by, and their types
constexpr const char* k_ModLoaderSdkVersionSymbol = "ModLoader_SdkVersion";
using ModLoaderSdkVersionFn = const char* (*)();
constexpr const char* k_ModLoaderVersionSymbol = "ModLoader_Version";
constexpr const char* k_ModLoaderLoadModsSymbol = "ModLoader_LoadMods";
using ModLoaderVersionFn = int (*)();
using ModLoaderLoadModsFn = int (*)(const char* modsFolder, ModLoaderLogFn log, void* context);

/// A mod as ModLoader_ListMods gives it; the strings live only for the call
struct ModLoaderModInfo
{
	const char* folder;  ///< its folder's name in the mods folder, which names it to ModLoader_SetModEnabled
	const char* name;    ///< from its mod.txt
	const char* version; ///< from its mod.txt, empty when it gives none
	const char* author;  ///< from its mod.txt, empty when it gives none
	int enabled;         ///< 1 when enabled.txt lists it
};
/// Receives each mod, with the context it was given
using ModLoaderModFn = void (*)(const ModLoaderModInfo* mod, void* context);

/// The values ModLoader_SetModEnabled returns, as an int
enum class ModLoaderSetResult : int
{
	Kept = 0,
	/// No such mod, or the mods folder is not read (its version.txt does not match)
	NoSuchMod = 1,
	/// enabled.txt cannot be written
	CannotWrite = 2,
};

constexpr const char* k_ModLoaderListModsSymbol = "ModLoader_ListMods";
constexpr const char* k_ModLoaderSetModEnabledSymbol = "ModLoader_SetModEnabled";
using ModLoaderListModsFn = int (*)(const char* modsFolder, ModLoaderModFn each, void* context);
using ModLoaderSetModEnabledFn = int (*)(const char* modsFolder, const char* modFolder, int enabled);

/// Receives each folder name, with the context it was given
using ModLoaderFolderFn = void (*)(const char* folder, void* context);
constexpr const char* k_ModLoaderEnabledModFoldersSymbol = "ModLoader_EnabledModFolders";
using ModLoaderEnabledModFoldersFn = int (*)(const char* modsFolder, ModLoaderFolderFn each, void* context);

extern "C" {
/// The SDK's version the library was built with: the hash of the SDK-facing headers, 64 hexadecimal digits. The game
/// uses the library only when it is the game's own
MODLOADER_API const char* ModLoader_SdkVersion() noexcept;

/// The mods folder's version, k_ModLoaderVersion
MODLOADER_API int ModLoader_Version() noexcept;

/// Reads `<modsFolder>/version.txt`, which holds the mods folder's version as an integer (whitespace around it is
/// allowed). Reports exactly one line through `log` (when it is not null) and returns one of the values above
MODLOADER_API int ModLoader_LoadMods(const char* modsFolder, ModLoaderLogFn log, void* context) noexcept;

/// Each mod of the mods folder, by its folder name's order: every folder whose mod.txt is a manifest of the format the
/// library reads. Returns how many were given, or -1 when the folder's version.txt does not match (nothing given)
MODLOADER_API int ModLoader_ListMods(const char* modsFolder, ModLoaderModFn each, void* context) noexcept;

/// Switches a mod on or off, keeping it in enabled.txt (written only when the list changes); one of the values above
MODLOADER_API int ModLoader_SetModEnabled(const char* modsFolder, const char* modFolder, int enabled) noexcept;
/// The folder names of the mods switched on, in enabled.txt's order (the library keeps it sorted by folder name), each
/// a folder of the mods folder with a manifest. A data mod's files replace the game's files of the same path below
/// the game's folder, the first such mod in this order winning. Returns how many were given, or -1 when the folder's
/// version.txt does not match (nothing given)
MODLOADER_API int ModLoader_EnabledModFolders(const char* modsFolder, ModLoaderFolderFn each, void* context) noexcept;
}
