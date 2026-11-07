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
#include <limits>
#include <span>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

/// How the 3D engine fills a boned mesh's bone matrices from a keyframe clip, and how it adds a lean range on top, to
/// the bit: every product, sum and rounding in the engine's own order (the game's FPU rounds each operation to a float,
/// docs/wiki/engine/floating-point.md#the-games-fpu-runs-at-24-bits). Pure maths, no Locator.
namespace openblack::anim_fill
{

constexpr uint32_t k_NoParent = std::numeric_limits<uint32_t>::max();

/// A 3 by 3 matrix by rows, used with row vectors
using Rows = std::array<float, 9>;

/// A bone's matrix relative to its parent: its rows and its place
struct BoneMatrix
{
	Rows rows;
	glm::vec3 position;
};

/// One keyframe: the x, y and z angles of the bones the clip turns, and the places of the bones it moves, in the
/// order of the clip's bone lists
struct Frame
{
	std::vector<glm::vec3> angles;
	std::vector<glm::vec3> positions;
};

/// A keyframe clip. A cycle spends its length on every frame including the wrap back to the first; a range spans its
/// length from the first frame to the last
struct Clip
{
	int32_t durationMs;
	bool cyclic;
	uint32_t boneCount;
	/// The bones the frames turn and move, each list in rising order
	std::vector<uint32_t> angleBones;
	std::vector<uint32_t> positionBones;
	std::vector<Frame> frames;
};

/// The rest pose the frames are relative to: each bone's parent, its rest matrix in the mesh and that matrix's inverse
struct Rest
{
	std::vector<uint32_t> parents;
	std::vector<Rows> base;
	std::vector<Rows> inverseBase;
};

/// The two keyframes around a time and how far between them it is
struct FrameSpan
{
	int32_t frame;
	int32_t next;
	float blend;
};

/// The frame a time falls on, in whole milliseconds: a cycle's length is its duration, a range's the duration times
/// its frame count over one less; frame = count x time / length, kept to the clip's frames, and the next frame wraps
/// to the first after the last (a range's last frame blends towards its first, as the engine's does); blend = count /
/// length x time - frame
[[nodiscard]] FrameSpan FindFrame(int32_t frameCount, int32_t durationMs, bool cyclic, int32_t timeMs);

/// The rotation of three angles turned about z, then x, then y, by rows, in the engine's own rounding
[[nodiscard]] Rows RotationZXY(float z, float x, float y);

/// The rest pose of a mesh from its bones' matrices relative to their parents: carried down the skeleton into the
/// mesh, set back relative to the parents and carried down again (the round trip the hand's good and evil look makes
/// at load), and each result's inverse
[[nodiscard]] Rest RestFromLocals(std::span<const glm::mat4> locals, std::span<const uint32_t> parents);

/// Every bone's matrix at a time of a clip. A bone the clip turns takes its two keyframes' rotations blended on their
/// elements, the rows made unit length again; any other takes the stand frame's angles, read by bone. Each is set in
/// the rest pose's space and then relative to the parent's rest rotation. A bone the clip moves takes its two
/// keyframes' places blended; any other the stand frame's place, read by bone
void Fill(std::span<BoneMatrix> out, const Clip& clip, const Rest& rest, int32_t timeMs, const Frame& stand);

/// A lean range added on top of a filled pose: each bone the range turns is turned by its blended rotation less the
/// base frame's, in its parent's rest space, and each bone it moves is moved by its blended place less the base
/// frame's
void AddDifferenceFromFrame(std::span<BoneMatrix> out, const Clip& clip, const Frame& baseFrame, const Rest& rest,
                            int32_t timeMs);

/// The time of a range sampled by a value within -range..range: (range + value) / (range + range) of its length,
/// truncated to whole milliseconds
[[nodiscard]] int32_t RangeTimeMs(float value, float range, int32_t durationMs);

/// The bones' matrices in the mesh: each one's own times its parent's, the root's as it is
[[nodiscard]] std::vector<glm::mat4> ToMesh(std::span<const BoneMatrix> bones, std::span<const uint32_t> parents);

/// The bones' matrices in the world, as the engine finishes a pose: the root's own times the object's matrix (L3DMesh's
/// column form), each other bone's own times its parent's world matrix
[[nodiscard]] std::vector<glm::mat4> ToWorld(std::span<const BoneMatrix> bones, std::span<const uint32_t> parents,
                                             const glm::mat4& object);

} // namespace openblack::anim_fill
