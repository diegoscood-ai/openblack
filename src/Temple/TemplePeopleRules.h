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
#include <functional>
#include <optional>
#include <span>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "3D/AllMeshes.h"
#include "3D/TempleLight.h"

/// The player's worshippers walking about inside the temple.
///
/// As the player goes into the temple, the villagers worshipping at the player's worship sites come in with them as
/// figures that wander the temple's floors until the player leaves. They walk a mesh of triangles laid over the floors:
/// to a point on an edge of the triangle they are in, preferring to carry straight on, then into the triangle beyond,
/// now and then stopping to stand for a few seconds. They have no say in the game: nothing else sees them, and the hand
/// passes through them.
namespace openblack::temple_people
{

/// At most this many come in
constexpr size_t k_MostVisitors = 32;
/// Each visit they arrive standing, and stand this long, in seconds, before they set off
constexpr float k_FirstPause = 0.1f;
/// The speed they have before they are first seen, in temple units a second
constexpr float k_UnseenSpeed = 1.0f;
/// On reaching a point, the chance of stopping to stand
constexpr float k_StopChance = 0.2f;
/// A stand lasts this long and up to this much longer, in seconds
constexpr float k_PauseLength = 2.0f;
constexpr float k_PauseSpread = 2.0f;
/// A way out of a triangle with nothing beyond it is all but never taken
constexpr float k_DeadEndWeight = 0.0001f;
/// Each way on is the best of so many random points
constexpr int k_SamplesPerWay = 4;
/// They are drawn only while their feet's depth leaves more than k_DrawnFade of a fade that starts at k_FadeStart and
/// loses k_FadePerUnit a unit of depth: nearer than about 249.5 temple units
constexpr float k_FadeStart = 200.0f;
constexpr float k_FadePerUnit = 0.02f;
constexpr float k_DrawnFade = 0.01f;
/// The share of a step that counts as reaching the point; a point nearer than this is reached
constexpr float k_ReachedShare = 0.999f;
constexpr float k_ReachedDistance = 0.001f;

/// The floors they walk: the corners of the mesh's triangles, merged where they meet, and for each triangle its corners
/// and the triangle beyond each of its edges. Edge k is the one facing corner k; -1 has nothing beyond it.
struct WalkMesh
{
	struct Triangle
	{
		std::array<uint32_t, 3> corners {};
		std::array<int32_t, 3> beyond {-1, -1, -1};
	};
	std::vector<glm::vec3> corners;
	std::vector<Triangle> triangles;
};

/// The walk mesh from a mesh's vertices and triangles. Vertices at the same place are one corner. The triangle beyond an
/// edge is the first that has the same edge the other way round.
[[nodiscard]] WalkMesh BuildWalkMesh(std::span<const glm::vec3> vertices, std::span<const std::array<uint16_t, 3>> triangles);

/// Where their random numbers come from, drawn in the order the game draws them
class Dice
{
public:
	virtual ~Dice() = default;
	/// 0 to n - 1
	virtual uint32_t Below(int32_t n) = 0;
	/// 0 up to x
	virtual float UpTo(float x) = 0;
};

/// A random point in a triangle: along its first edge by one draw and its second by a draw up to what is left
[[nodiscard]] glm::vec3 PointIn(const WalkMesh& mesh, int32_t triangle, Dice& dice);
/// A random point on a triangle's edge facing a corner
[[nodiscard]] glm::vec3 PointOnEdge(const WalkMesh& mesh, int32_t triangle, int edge, Dice& dice);

/// The way across the floor from one point to another, in radians: 0 along x, a quarter turn along z
[[nodiscard]] float HeadingBetween(const glm::vec3& from, const glm::vec3& to);
/// The turn the model is drawn with to face along a direction, a quarter turn on from its heading; 0 for none
[[nodiscard]] float FacingOf(const glm::vec3& direction);

enum class Gait : uint8_t
{
	Walking,
	Standing,
};

/// One of them
struct Walker
{
	glm::vec3 position {0.0f};
	glm::vec3 target {0.0f};
	int32_t triangle {0};
	/// The triangle the target is in, which they are in once they reach it
	int32_t nextTriangle {0};
	/// The next point is inside their triangle, rather than on one of its edges
	bool inside {false};
	/// Temple units a second, as their clip last had it when they were drawn
	float speed {k_UnseenSpeed};
	/// Seconds since they last started or stopped walking
	float timer {0.0f};
	/// How long they stand before setting off again
	float pause {k_FirstPause};
	/// The way they last set off, and the turn their model faces
	float heading {0.0f};
	float facing {0.0f};
	Gait gait {Gait::Standing};
	/// Where they are in their clip, in milliseconds
	uint32_t clipPlace {0};
};

/// One arriving, standing at a random point of a random triangle with another point of it to go to
[[nodiscard]] Walker Arrive(const WalkMesh& mesh, Dice& dice);

/// Moves one on by so many seconds: walking towards its point, choosing the next on reaching it, or standing until its
/// pause is over
void Step(Walker& walker, const WalkMesh& mesh, float seconds, Dice& dice);

/// Who comes in: the worshippers of each of the player's worship sites in turn, each site's newest first, skipping any
/// not available and any already in, until there are k_MostVisitors
[[nodiscard]] std::vector<entt::entity> ChooseVisitors(std::span<const std::vector<entt::entity>> worshippersBySite,
                                                       const std::function<bool(entt::entity)>& available);

/// Where its feet are in clip space: each row of the world-to-clip matrix times the point summed x, z, y and then the
/// offset, each step a float, as the game projects a point
[[nodiscard]] glm::vec4 FeetInClip(const glm::mat4& worldToClip, const glm::vec3& feet);
/// Whether one is drawn, from where its feet are in clip space on a screen of `resolution` pixels: not nearer the eye
/// than the near plane, on a pixel of the screen (each of its places in pixels cut towards zero, so up to a pixel past
/// the screen's left and top edges still counts), and near enough (k_DrawnFade), each step a float
[[nodiscard]] bool Drawn(const glm::vec4& feet, float nearPlane, glm::vec2 resolution);

/// One of the times a frame draws them: in the reflection under a room's floor, or in the room itself
enum class Pass : uint8_t
{
	Reflection,
	Room,
};
/// A room the temple draws in a frame
struct RoomDrawn
{
	/// It is mirrored under its floor (the main room and the picture rooms)
	bool mirrored {false};
	/// It draws the hand itself, after drawing them (the creature's room)
	bool drawsHand {false};
};
/// The times a frame draws them, in order. Each room drawn, the one the player is in first, draws them in its
/// reflection when it is mirrored and the reflections are shown, then in itself; once the hand is drawn, nothing more
/// draws them that frame. Their clip plays on at each of these draws.
[[nodiscard]] std::vector<Pass> DrawsInFrame(std::span<const RoomDrawn> rooms, bool reflections);

/// The grey they are drawn in, 0xAARRGGBB: light in the rooms, dark in the reflections under their floors
constexpr uint32_t k_RoomColour = 0xFFE0E0E0u;
constexpr uint32_t k_ReflectionColour = 0xFF606060u;
/// The temple's fade at their feet: their depth, the frame's fade and the colour it adds at its end
struct FadeAtFeet
{
	float depth {0.0f};
	TempleFade fade {};
	glm::vec3 endColour {0.0f};
};
/// The colour and specular one is drawn with, 0xAARRGGBB
struct Look
{
	uint32_t colour {k_RoomColour};
	uint32_t specular {0};
};
/// Its look in a draw: the draw's grey faded once at its feet (each of red, green and blue times the fade's share
/// there over 256, cut down), and the fade's colour there as its specular; with no fade, the grey and no specular
[[nodiscard]] Look LookOf(Pass pass, const std::optional<FadeAtFeet>& fade);

/// The clip one plays: walking as a man or a woman, or standing
[[nodiscard]] AnimId ClipOf(Gait gait, bool man);

/// The speed a clip carries one along at: its stride each play, by its scale
[[nodiscard]] float SpeedOf(float stride, uint32_t playTime, float scale);

/// Where one is in its clip after it plays on by so many milliseconds: round again for a looping clip, else held at its
/// last millisecond. (openblack) A clip with no play time stays at 0: the game would divide by zero
[[nodiscard]] uint32_t AdvanceClipPlace(uint32_t place, uint32_t played, uint32_t playTime, bool looping);

} // namespace openblack::temple_people
