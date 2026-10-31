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

#include <limits>
#include <numbers>
#include <optional>
#include <span>

#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>

#include "3D/MapCoords.h"
#include "Enums.h"

namespace openblack
{
struct GCreatureInfo;
}

/// The rules of a village's totem: the statue on the town centre that rises with the share of the town's people who
/// worship, and that the hand takes hold of and slides up and down to set that share. The statue's ease itself is the
/// worship percentage's Zoomer (worship::percentage)
namespace openblack::ecs::village_totem
{

/// How long the statue takes to ease to a new share: this many milliseconds for the whole way from none to all
constexpr float k_EaseMsPerShare = 5200.0f;
/// An ease shorter than this, in milliseconds, is no ease: the statue is put at its new share at once. It is the
/// Zoomer's own shortest move, which the statue's ease goes through
constexpr float k_SnapMs = 0.001f;
/// The statue has arrived once it is this close to its share
constexpr float k_ArrivedWithin = 0.005f;
/// How far the statue stands risen with all its town at worship
constexpr float k_RiseAtFullShare = 8.0f;
/// How far above the plinth's place its icon stands
constexpr float k_IconAbovePlinth = 2.729f;
/// The hand holds the icon this much of the icon's height above the icon's place
constexpr float k_GripHeight = 0.7f;
/// Moving the mouse the screen's height slides the hand this many times the icon's height
constexpr float k_SlidePerScreen = 3.0f;
/// The share the held statue moves for each unit the hand slides
constexpr float k_SharePerSlide = 0.1f;

/// How far the statue stands risen for a share
[[nodiscard]] float RiseOf(float share);

/// How far up the hand slides for the mouse's move up the screen since the last drawn frame, in pixels; going down it
/// stops at the land under the hand, which is how far above the land the hand is; going up it is never held back
[[nodiscard]] float HandSlide(float upPixels, float screenHeight, float iconHeight, float handAboveLand);
/// The share the hand holds the statue at after it slides; between none and all
[[nodiscard]] float SlideShare(float held, float slide);
/// Where the hand holds the statue: above the icon's place by part of its height
[[nodiscard]] float GripY(float iconY, float iconHeight);

/// The hand's standard size, against which it tips near the land and closes on the icon
constexpr float k_HandSize = 3.2f;
/// Below this height in the world the hand holding the statue tips over further
constexpr float k_LowHand = 2.5f;
/// How far the hand holding the statue is tipped higher up: seven sixteenths of a half turn
constexpr float k_HandTilt = 7.0f * std::numbers::pi_v<float> / 16.0f;
/// How far the hand holding the statue at a height in the world is tipped over: more the lower it is below k_LowHand
[[nodiscard]] float HandTiltAt(float handY);
/// How far the hand closes on the icon, from its hold radius: up to all of the way
[[nodiscard]] float GripClosure(float holdRadius);
/// The time in the hand's side hold, in whole milliseconds, for the closure and the hold's clip length: half the
/// clip at most
[[nodiscard]] int32_t GripTimeMs(float closure, uint32_t clipLength);
/// Where the cursor's image is pinned while the hand holds the statue, from the hand's point on the screen (pixels
/// from the top left): kept on the screen, from the first pixel to the last, then to the nearest pixel, a half going to
/// the even one. The mouse itself goes on moving; only its image stays
[[nodiscard]] glm::ivec2 CursorPin(glm::vec2 handOnScreen, glm::ivec2 screenSize);
/// The turn of the hand holding the statue: tipped over by the tilt about its side, then turned half a turn on from its
/// heading (the yaw of the line of sight through the cursor) about the up
[[nodiscard]] glm::mat3 HoldRotation(float tilt, float heading);

/// What the hand's grip of a totem needs, in the order it is asked
struct GripFacts
{
	/// The gripping player is the totem's: its town's player
	bool samePlayer {false};
	/// The totem is functional: available, built, and its life above the life it stops working at
	bool functional {false};
	/// The player's temple has a heart, built
	bool heartBuilt {false};
	/// The totem's town has a worship site, built
	bool siteBuilt {false};
};
/// Whether the hand may take hold of the totem: all of them
[[nodiscard]] bool ValidForLockedSelect(const GripFacts& facts);

/// Where the statue is drawn, and the second statue at the other share. Held by the hand it stands where the hand
/// holds it, the second where it has eased to; otherwise it stands where it has eased to, the second at the share the
/// hand holds, which is then the share it was set to. The second shows only while the two would stand apart
struct Shown
{
	float solid {0.0f};
	std::optional<float> ghost;
};
[[nodiscard]] Shown ShownShares(float current, float held, bool gripped);

/// The second totem's alpha, of 255
constexpr uint8_t k_GhostAlpha = 0x80;
/// Whether the totem is placed again this frame: its rise changed since it was last placed, or the second totem shows
[[nodiscard]] bool Replaces(float solidRise, float lastRise, bool ghostShown);
/// Whether a re-place forces the share tooltip: only for the local player's totem, and not on the first re-place after
/// its icon changed
[[nodiscard]] bool ShowsShareToolTip(bool replaces, bool quietOnce, bool localPlayers);
/// The rise a totem whose icon has just changed was last placed at: none it could stand at, so the next frame
/// places it again
constexpr float k_NeverPlaced = std::numeric_limits<float>::max();
/// The built percentage a totem keeps when a script sets it: the value as given, none for a negative value or for no
/// number at all, and the whole of it (built) from the whole up
[[nodiscard]] float PercentBuiltSetTo(float value);
/// Whether a totem is built: all of it; not for no number at all
[[nodiscard]] bool IsBuilt(float percentBuilt);
/// Whether a totem is drawn: built and alive (its life above none; not for no number at all). Undrawn, nothing of it
/// shows, not its plinth, its icon or its second totem, and the hand cannot find it
[[nodiscard]] bool IsDrawn(bool built, float life);

/// The share as the tooltip and the floating number show it: as a percentage
[[nodiscard]] float AsPercentage(float share);

/// The icon a player without a creature has on their towns' totems, by its place in the mesh pack: the hand
/// (BuildingSpellHand)
constexpr int32_t k_HandIcon = 250;
/// The icon on a totem, by its place in the mesh pack: the icon of the species of its town's player's creature, or the
/// hand without one. A place outside the pack is the pack's first mesh
[[nodiscard]] uint32_t IconMesh(std::optional<int32_t> creatureIcon, uint32_t meshesInPack);
/// The totem icon of a creature's species, by its place in the mesh pack: its info's; none without a creature, or for
/// a species with no info row
[[nodiscard]] std::optional<int32_t> CreatureIcon(std::optional<CreatureType> species, std::span<const GCreatureInfo> infos);
/// The way a totem faces, given to its town with a worship site: side on to the site, a quarter turn on from the way
/// from the totem to the site
[[nodiscard]] float FacingSite(const map_coords::MapCoords& totem, const map_coords::MapCoords& site);

} // namespace openblack::ecs::village_totem
