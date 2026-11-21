/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <string>

#include <entt/core/fwd.hpp>

#include "Window.h"

namespace openblack::debug::gui
{
class Audio final: public Window
{
public:
	Audio() noexcept;

protected:
	void Draw() noexcept override;
	void Update() noexcept override;
	void ProcessEventOpen(const SDL_Event& event) noexcept override;
	void ProcessEventAlways(const SDL_Event& event) noexcept override;

private:
	void Sounds() noexcept;
	void Music() noexcept;
	/// a sample of the selected bank played 2D on the channels (audio::PlaySoundEffect, no owner)
	void PlaySelected() const noexcept;
	entt::id_type _selectedSound {0};
	/// the 2D pan the selected sample plays at: 0 left, 90 ahead (default, as every 2D sample but the spooky voices'),
	/// 180 right
	int _selectedPan {90};
	std::string _selectedSoundPack;
	/// MUSIC_TYPE of k_MusicBanks
	int _selectedMusicType {0};
};
} // namespace openblack::debug::gui
