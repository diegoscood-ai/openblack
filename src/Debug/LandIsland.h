/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Window.h"

namespace openblack::debug::gui
{
class GameHost;

class LandIsland final: public Window
{
public:
	LandIsland() noexcept;
	/// The game whose last frame the object list's readout shows, or none once it has gone
	void SetHost(const GameHost* host) noexcept { _host = host; }

protected:
	void Draw() noexcept override;
	void Update() noexcept override;
	void ProcessEventOpen(const SDL_Event& event) noexcept override;
	void ProcessEventAlways(const SDL_Event& event) noexcept override;

private:
	/// The block textures' queue: how many wait, a trigger that puts every block in it and a switch that draws every
	/// block's prints again every frame (off), closed by default
	void DrawBlockTextures() noexcept;
	/// The object draw list's readout and its draw switch, closed by default
	void DrawObjectList() noexcept;
	/// The town desire flags' readout: the desire each one flies for and how far up it is, closed by default
	void DrawDesireFlags() noexcept;
	/// What the scripts set that a land change keeps (only a new game or a playground's start puts it back), closed by
	/// default
	void DrawKeptByALandChange() noexcept;

	const GameHost* _host {nullptr};
};

} // namespace openblack::debug::gui
