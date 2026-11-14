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

#include <array>
#include <optional>

#include <GameTailRecords.h>
#include <SaveParts.h>
#include <glm/vec3.hpp>

#include "Camera/CreatureFollow.h"
#include "Common/Zoomer.h"
#include "ECS/SavedWorld/WorldWriter.h"

namespace openblack::gamesave
{
struct WorldRead;
}

// The game camera in a saved game's world file, the object the game object's tail names
// (docs/wiki/engine/saved-games.md#the-saves-in-play). A load puts back the zoomers of the camera's focus, of its
// position and of its field of view, then every camera mode goes and the player's camera starts again from them, its
// mode's seconds at 0. The record's other values (where the camera is drawn, its frame's time, the weather at it and
// whether it has moved) are taken again by the camera on its next frame; a save writes them as the load kept them.
// The record ends with the camera's current mode: a plain one saves nothing of its own, a follow (the script's mode and
// Creature Mode's are follows) its view and the thing it follows. A load makes a saved follow again as a plain follow,
// never a script's mode, so a cut scene saved mid-way goes on with the player's camera where the save left it; a
// follow of a creature as Creature Mode makes it is Creature Mode again, on that creature with the saved view.
namespace openblack::ecs::saved_camera
{

/// The id by which the one camera is a source's object
inline constexpr uint64_t k_CameraId = 0;
/// The save number of the plain camera modes, the player's among them, which save nothing of their own
inline constexpr uint32_t k_PlainMode = 0;
/// The save numbers of the modes that save values of their own: a follow (the script's mode among them), a mode that
/// keeps a heading round a thing, and a free mode
inline constexpr uint32_t k_FollowMode = 0x46;
inline constexpr uint32_t k_HeadingMode = 0x48;
inline constexpr uint32_t k_FreeMode = 0x65;

/// What a follow mode saves of itself: its heading, pitch and distance round the thing it follows, its time factor,
/// whether the heading is behind the thing, whether it was made to keep its offsets from the thing, the thing (0 for
/// none) and whether it looks for a clear view of the thing. The offsets are kept only by a follow made with them, and
/// every follow made in play is made without (its maker turns them off as it ends): they are saved unset, written as 0
struct Follow
{
	float heading {0.0f};
	float pitch {0.0f};
	float distance {0.0f};
	float timeFactor {0.0f};
	bool behind {false};
	bool offsets {false};
	gamesave::ObjectNumber thing {0};
	bool clearView {false};
};
/// The mode part of a saved follow: its six words, its two offsets, the thing and whether the clear view is searched
[[nodiscard]] gamesave::CameraModePart FollowPartOf(const Follow& follow);
/// The follow a saved mode part keeps; none for another mode
[[nodiscard]] std::optional<Follow> FollowOf(const gamesave::CameraModePart& mode);
/// The player's follow of a thing (Creature Mode's, or a follow a load made again): its view and style, no offsets
[[nodiscard]] Follow PlayersFollow(const creature_follow::View& view, const creature_follow::Style& style,
                                   gamesave::ObjectNumber thing);
/// Creature Mode's follow of a creature (the creature key's and the double click's): its view, the time factor 1, the
/// heading not behind the creature, no offsets, and the clear view looked for
[[nodiscard]] Follow CreatureModeFollow(const creature_follow::View& view, gamesave::ObjectNumber creature);
/// How a saved follow eases and turns, which a load makes the player's follow of its thing with; none for a follow made
/// to keep offsets from its thing, which no follow made in play keeps
[[nodiscard]] std::optional<creature_follow::Style> StyleOf(const Follow& follow);

/// What a load makes of the saved mode
enum class LoadedMode : uint8_t
{
	/// The player's camera, from the loaded zoomers: a plain mode, or a follow of nothing, which goes at the first turn
	/// and leaves the player's camera to start again
	Player,
	/// A follow of a thing: made again once the loaded objects are back (LoadFollow)
	FollowOfAThing,
	/// A heading or a free mode, which the game makes only when it loads one, never in play: not made, the player's
	/// camera instead
	NotMade,
};
[[nodiscard]] LoadedMode LoadedModeOf(const gamesave::CameraModePart& mode);

/// The number of the camera's record the game object's tail names; 0 when the file names none
[[nodiscard]] gamesave::ObjectNumber CameraNumber(const gamesave::WorldRead& world);
/// The camera's record of a loaded world; none when the game object names none
[[nodiscard]] const gamesave::CameraRecord* CameraOf(const gamesave::WorldRead& world);

/// The camera's zoomers: its position's and its focus's, axis by axis, and its field of view's (radians)
struct Zoomers
{
	Zoomer3 origin;
	Zoomer3 focus;
	Zoomer fov;
};
/// The zoomers a camera's record keeps: the focus's three, then the position's three, then the field of view's
[[nodiscard]] Zoomers ZoomersOf(const gamesave::CameraRecord& record);
/// The camera's zoomers and the seconds since its mode last changed written over the record, which keeps the rest
void WriteLive(gamesave::CameraRecord& record, const Zoomers& zoomers, float modeSeconds);

/// What a save reads of the camera of the world for a camera with no record kept (a new game's), as the game holds it
/// once a frame: its zoomers, its mode's seconds and the last frame's camera milliseconds, whether a script holds the game's
/// speed, whether the camera drawn last moved, where it is drawn on the map (its height above its cell's own altitude),
/// the point of the land it looks at and the weather at it
struct Live
{
	Zoomers zoomers;
	float modeSeconds {0.0f};
	uint32_t frameMs {0};
	bool scriptHoldsSpeed {false};
	bool drawnMoved {false};
	gamesave::MapCoords drawnAt {};
	gamesave::Point lookedAt {};
	std::array<uint8_t, 8> weather {};
	/// The camera's current mode as a save writes it
	gamesave::CameraModePart mode {};
};
/// The record of a camera the game made since the load: the values a camera is made with where nothing writes them
/// again (no word, flags or byte), and the rest, its current mode among them, from what the game holds
[[nodiscard]] gamesave::CameraRecord NewRecord(const Live& live);

/// The drawn camera inside the citadel as the island camera's update takes it: held 1 m over the land under it, the
/// difference (ground + 1) - y added to its height when it is above 0, each step a float
[[nodiscard]] inline glm::vec3 HeldOverLand(glm::vec3 drawn, float ground)
{
	const float below = (ground + 1.0f) - drawn.y;
	if (below > 0.0f)
	{
		drawn.y += below;
	}
	return drawn;
}

/// Whether a camera made since the load can be written now: there is a camera of the world and a land, and inside the
/// citadel the island camera it keeps (WriteCamera.cpp)
[[nodiscard]] bool NewCameraWritable();

/// A load: the camera of the world (Locator::camera) and the script camera's field of view take the record's zoomers,
/// every camera mode goes and the player's camera starts again. A heading or a free mode (LoadedMode::NotMade) is put
/// back as the player's camera, with one log line
void Load(const gamesave::WorldRead& world);
/// Then, once the loaded objects are made: a saved follow of a thing made again as the player's follow of it, with the
/// saved view and style (Locator::creatureModeSystem): Creature Mode's follow of a creature, or a script's mode that
/// followed a thing in a cut scene, which the game makes again as a plain follow. A follow of a thing the load did not
/// make follows nothing, which the first turn takes off: the player's camera stays, with one log line, as it does for
/// a follow the camera can't take now
void LoadFollow(const gamesave::WorldRead& world, const saved_world_write::LoadedObjects& loaded);

} // namespace openblack::ecs::saved_camera
