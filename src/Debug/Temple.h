/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Enums.h"
#include "Window.h"

namespace openblack::debug::gui
{

class TempleInterior final: public Window
{
public:
	TempleInterior() noexcept;

protected:
	void Draw() noexcept override;
	void Update() noexcept override;
	void ProcessEventOpen(const SDL_Event& event) noexcept override;
	void ProcessEventAlways(const SDL_Event& event) noexcept override;

private:
	/// A player's alignment, set from the window, and how their temple's outside follows it
	/// The challenges found, as the challenge room's slots show them
	void DrawChallenges() noexcept;
	void DrawStatistics() noexcept;
	void DrawAlignment() noexcept;
	/// The player's temple's three leash posts: where they stand, their spin, and the leash picked there, which the
	/// window can set
	void DrawLeashPosts() noexcept;
	/// The need signs over the altars of the player's worship sites: each sign's desire, how full it shows and whether
	/// it is drawn
	void DrawAltarNeeds() noexcept;
	/// The columns of light over the towns and temples: whether they are drawn, and the heartbeat the player's temple's
	/// column beats with
	void DrawLightColumns() noexcept;
	/// The creature room: whether it shows the player's creature, which a script can change and the window can too, and
	/// whether the cursor was last over it
	void DrawCreatureRoom() noexcept;
	/// The temple's depth fog: whether the rooms are drawn in it, and the fog of this frame
	void DrawTempleFog() noexcept;

	PlayerNames _player {PlayerNames::PLAYER_ONE};
};

} // namespace openblack::debug::gui
