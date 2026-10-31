/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <memory>
#include <optional>

#include <entt/fwd.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "Camera/BookmarkPulse.h"

namespace openblack::ecs::systems
{

/// Where a flight to a bookmark takes the camera
struct BookmarkFlight
{
	glm::vec3 origin;
	glm::vec3 focus;
};

/// What a jump to a bookmark is made from: the place the interface status was synced to at the last turn and the
/// camera's origin, which the view before the jump keeps, and whether the game is inside the citadel
struct BookmarkJumpFrom
{
	map_coords::MapCoords statusPlace;
	glm::vec3 cameraOrigin {0.0f};
	bool insideCitadel {false};
};

/// The view before the last jump, a bookmark of its own (Backspace goes back to it); it is never drawn
inline constexpr uint8_t k_BeforeJumpBookmark = 10;

class CameraBookmarkSystemInterface
{
public:
	virtual ~CameraBookmarkSystemInterface() = default;
	virtual bool Initialize() = 0;
	/// Once a frame that draws the land: the shared pulse moves on by the frame's game ms; then, while the bookmarks are
	/// on and no script holds the wide screen, every set bookmark's marker is drawn above its land point at the pulse's
	/// half width and half height, and otherwise none is (bookmark_pulse::Step, bookmark_pulse::MarkerHalfSize,
	/// bookmark_markers::MarkersShown)
	virtual void Update(uint32_t frameGameMs, bool scriptWideScreen) = 0;
	/// The shared pulse as the last update left it
	[[nodiscard]] virtual const bookmark_pulse::Pulse& GetPulse() const = 0;
	[[nodiscard]] virtual const std::array<entt::entity, 8>& GetBookmarks() const = 0;
	/// Drops a bookmark (0 to 7): on the thing when one is given (its map position then the thing's, followed once a
	/// turn), else at the map position, keeping the camera's origin as an offset from that position's land point. No
	/// bookmark is followed any more, and this one becomes the current one
	virtual void SetBookmark(uint8_t index, const map_coords::MapCoords& position, std::optional<entt::entity> target,
	                         const glm::vec3& cameraOrigin) = 0;
	/// Whether a bookmark (0 to 7, or k_BeforeJumpBookmark) is set
	[[nodiscard]] virtual bool IsSet(uint8_t index) const = 0;
	/// A flight to a set bookmark: the focus the land point of its map position (as it is now, for a bookmark on a thing),
	/// the origin that point plus the offset the camera had when it was dropped; none for a bookmark not set
	[[nodiscard]] virtual std::optional<BookmarkFlight> FlightTo(uint8_t index) const = 0;
	/// The jump to a bookmark (its key alone, a followed one each turn, and Backspace to k_BeforeJumpBookmark): for a set
	/// bookmark, and not inside the citadel, it becomes the current bookmark, a follow of another bookmark stops, the
	/// flight to it is returned, and then the view before the jump is set at the interface status's place, with the
	/// camera's offset from its land point; otherwise nothing changes and there is no flight
	[[nodiscard]] virtual std::optional<BookmarkFlight> JumpTo(uint8_t index, const BookmarkJumpFrom& from) = 0;
	/// Shift with a set bookmark's key: a bookmark on no thing is jumped to and nothing is followed; one on a thing is
	/// followed, or no longer followed when it already is. The jump's flight, if any
	[[nodiscard]] virtual std::optional<BookmarkFlight> ToggleFollow(uint8_t index, const BookmarkJumpFrom& from) = 0;
	/// The bookmark followed, if any, and the current bookmark (the last dropped or jumped to; 0 at first)
	[[nodiscard]] virtual std::optional<uint8_t> GetFollowed() const = 0;
	[[nodiscard]] virtual uint8_t GetCurrent() const = 0;
	/// Once a turn: each set bookmark on a thing forgets it once it is no longer available, and otherwise takes the
	/// thing's map position; then, while the player's camera has the view, the followed bookmark is jumped to (its
	/// flight returned), else nothing is followed any more; and a turn whose camera input counted for the camera help
	/// stops the follow too
	[[nodiscard]] virtual std::optional<BookmarkFlight> ProcessTurn(bool playersCamera, bool cameraInputCounted,
	                                                                const BookmarkJumpFrom& from) = 0;
	/// Shift and Ctrl with a set bookmark's key: the bookmark is no longer set, and forgets its thing
	virtual void ClearBookmark(uint8_t index) = 0;
	/// On at start-up (PLAY_JC_SPECIAL 14 / 15 turn it on / off; a script reboot turns it on; saved with the bookmarks,
	/// pending with the saved games): the bookmark keys and the bookmarks' markers only while it is set
	virtual void SetEnabled(bool enabled) = 0;
	[[nodiscard]] virtual bool IsEnabled() const = 0;
};

} // namespace openblack::ecs::systems
