/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <filesystem>
#include <map>
#include <string>

#include <bgfx/bgfx.h>

#include "Window.h"

namespace openblack::mods
{
class Mod;
}

namespace openblack::debug::gui
{

/// The Mods window (the Mods button of the menu bar): tabs Modpacks, Mods, Load order and Log over the mod library
/// (Locator::mods, docs/bw1-notes/mod-library.md). Each list shows the mod's image and name; the selected one shows
/// its image, information and settings on the right.
class ModsWindow final: public Window
{
public:
	ModsWindow() noexcept;
	~ModsWindow() noexcept override;

	static constexpr const char* k_Name = "Mods";

protected:
	void Draw() noexcept override;
	void Update() noexcept override {}
	void ProcessEventOpen(const SDL_Event& /*event*/) noexcept override {}
	void ProcessEventAlways(const SDL_Event& /*event*/) noexcept override {}

private:
	enum class Tab : uint8_t
	{
		Modpacks,
		Mods,
		LoadOrder,
		Log,
	};

	void DrawModpacks() noexcept;
	void DrawMods() noexcept;
	void DrawLoadOrder() noexcept;
	void DrawLog() noexcept;
	void DrawModEntry(mods::Mod& mod, int indent) noexcept;
	void DrawModDetails(mods::Mod& mod) noexcept;
	/// The image at that path as a texture (loaded once), or an invalid handle
	bgfx::TextureHandle Icon(const std::filesystem::path& path) noexcept;
	void IconOrBlank(const std::filesystem::path& path, float size) noexcept;

	std::map<std::filesystem::path, bgfx::TextureHandle> _icons;
	std::string _selectedMod;
	std::string _packFilter; ///< the Mods tab shows this modpack's mods; empty: the loose ones
	std::string _search;
	Tab _switchTo {Tab::Mods};
	bool _switchTab {false};
	bool _logInfo {true};
	bool _logWarnings {true};
	bool _logErrors {true};
	std::string _logMod;
};

} // namespace openblack::debug::gui
