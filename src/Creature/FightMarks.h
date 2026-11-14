/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>

#include <glm/vec3.hpp>

#include "Creature/CreatureSkinRay.h"

/// The marks of the player's presses in a fight: each press the hand makes on a fighter's skin or on the arena's ground
/// leaves a mark where it landed, one for each move waiting in the player's creature's queue, at most twelve. A press is
/// taken only while fewer than twelve show. Each mark is drawn as a sparkle, coloured by its place: yellow next, then
/// pale yellow, orange, red and dark red towards the twelfth.
/// Wiki: docs/wiki/creature/fights.md#fights-the-marks-of-the-presses.
namespace openblack::creature_fight_marks
{

inline constexpr size_t k_Most = 12;
/// A blow's charge, and so its mark's flicker, counts up to this many milliseconds
inline constexpr uint32_t k_MostHeldMs = 1200;
/// The sparkles' alpha, over their colour
inline constexpr uint32_t k_Alpha = 0xA0000000u;

struct Mark
{
	enum class Kind : uint8_t
	{
		/// On the opponent's skin, at a spot
		Opponent = 0,
		/// On the player's own creature's skin
		Own = 1,
		/// On the ground, turning and moving with the creature: where it was pressed, and that point in the creature's own
		/// frame
		Ground = 2,
	};
	Kind kind {Kind::Ground};
	creature_skin_ray::Spot spot {};
	/// A ground mark: where it stands as the turn left it, and in the creature's frame
	glm::vec3 world {0.0f};
	glm::vec3 local {0.0f};
	/// How long its press was held, and its sparkle's frame of the sheet
	uint32_t heldMs {0};
	uint32_t frame {0};
};

/// The marks of one creature's presses, oldest first; the newest is pending while its press is held
struct Marks
{
	std::array<Mark, k_Most> marks {};
	size_t count {0};
	bool pending {false};

	/// A press on a skin: a mark there, its press now held. Nothing when twelve show
	void AddOnSkin(Mark::Kind kind, const creature_skin_ray::Spot& spot);
	/// A press on the ground: a mark at the point, and the point in the creature's frame. Nothing when twelve show
	void AddOnGround(const glm::vec3& world, const glm::vec3& local);
	/// A move in place of those queued: only the newest mark stays, now the first
	void KeepNewest();
	/// The move at the front of the queue taken: its mark goes
	void Shunt();
	/// The blow waiting to be let go taken away by a hit: its mark goes and its press is no longer held
	void DropWaiting();
	/// The press let go
	void LetGo() { pending = false; }
	/// A turn of the creature's own milliseconds: the held press's mark counts them, up to k_MostHeldMs
	void Hold(uint32_t ms);
	[[nodiscard]] bool Full() const { return count >= k_Most; }
};

/// A point on the ground in a creature's frame, from where it stands and its heading, worked as the game does
[[nodiscard]] glm::vec3 LocalOf(const glm::vec3& point, const glm::vec3& position, float heading);
/// A point in a creature's frame back on the ground, from where it stands and its heading
[[nodiscard]] glm::vec3 WorldOf(const glm::vec3& local, const glm::vec3& position, float heading);
/// Where a ground mark is drawn between the turn's place and where the creature's frame puts it now, by the share of
/// the turn the frame is at
[[nodiscard]] glm::vec3 DrawnOnGround(const glm::vec3& world, const glm::vec3& now, float turnFraction);
/// A sparkle's colour (0xAARRGGBB) by its mark's place among the queue's twelve
[[nodiscard]] uint32_t Colour(size_t index, size_t queueSize = k_Most);
/// A mark on the opponent flickers through the sheet's first sixteen frames, the faster the longer its press was held:
/// its next frame each time it is drawn
[[nodiscard]] constexpr uint32_t NextFrame(uint32_t frame, uint32_t heldMs)
{
	return (frame + ((heldMs * 8) / k_MostHeldMs)) & 15u;
}
/// A sparkle is drawn half way from where it sits to the camera
[[nodiscard]] glm::vec3 TowardsCamera(const glm::vec3& at, const glm::vec3& camera);

} // namespace openblack::creature_fight_marks
