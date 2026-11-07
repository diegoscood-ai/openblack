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

namespace openblack::morph
{
struct Animation;
} // namespace openblack::morph

/// The keyframe animations of the boned meshes the hand and the creatures are posed with, and the maths the game poses
/// them by: keyframes of YXZ Euler angles and translations per bone, blended between keyframes on the rotation
/// matrices' elements.
namespace openblack::skeletal_animation
{
/// The game's 3 by 3 matrix, row-major and used with row vectors
using Matrix = std::array<std::array<float, 3>, 3>;

constexpr Matrix k_Identity {{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}};
constexpr uint32_t k_NoParent = std::numeric_limits<uint32_t>::max();

/// A keyframe animation: per bone YXZ Euler angles and translations of the bones it moves
struct Animation
{
	uint32_t duration;
	bool looping;
	std::vector<uint32_t> rotatedJoints;
	std::vector<uint32_t> translatedJoints;
	struct Frame
	{
		std::vector<glm::vec3> eulerAngles;
		std::vector<glm::vec3> translations;
	};
	std::vector<Frame> frames;
	/// Where the root ends up after one cycle, in the mesh's units, ahead being -z: a walk's or run's stride, a step's
	/// move
	glm::vec3 displacement {0.0f};
	/// How far the root goes over one cycle, as the file has it: a blend of two steps takes its move to this length
	float stride {0.0f};
};

/// A bone's pose relative to its parent: a row-major rotation and a translation, used with row vectors
struct Pose
{
	Matrix rotation;
	glm::vec3 translation;
};

/// The rest pose the keyframes are relative to: each bone's parent, and its rest rotation in the mesh's space and the
/// inverse of it
struct Skeleton
{
	std::vector<uint32_t> parents;
	std::vector<Matrix> restRotations;
	std::vector<Matrix> inverseRestRotations;

	/// From the rest pose's matrices in the mesh's space, in the form L3DMesh::GetBoneMatrices gives them
	[[nodiscard]] static Skeleton FromRestMatrices(std::span<const uint32_t> parents, std::span<const glm::mat4> rest);
	[[nodiscard]] bool Empty() const { return parents.empty(); }
};

/// A rotation matrix from y, x and z angles, combined in that order as the game does (affine::RotationYXZ by rows).
/// Keyframe angles are stored x, y, z.
[[nodiscard]] Matrix RotationYXZ(const glm::vec3& euler);
/// The x, y and z angles of a rotation matrix made by RotationYXZ, read with asin and atan2. Only a rotation reads back
/// right this way: blends of rotations are read with BlendedAnglesYXZ, as the game reads them.
[[nodiscard]] glm::vec3 EulerYXZ(const Matrix& m);
/// The x, y and z angles of a matrix as the game reads them back once it has blended rotations on their elements
/// (affine::DecomposeYXZ): the turn about y from the third row, then x and z from the rows turned back by it. A blend of
/// two rotations is not a rotation, and this reads it as the game does, where EulerYXZ would read it otherwise.
[[nodiscard]] glm::vec3 BlendedAnglesYXZ(const Matrix& m);
/// Row vector product a * b
[[nodiscard]] Matrix Multiply(const Matrix& a, const Matrix& b);
[[nodiscard]] Matrix Transpose(const Matrix& m);
[[nodiscard]] Matrix Inverse(const Matrix& m);
/// a moved towards b by t, element by element
[[nodiscard]] Matrix Lerp(const Matrix& a, const Matrix& b, float t);
/// Each row made unit length again, as keyframe rotations are after being blended
[[nodiscard]] Matrix NormaliseRows(Matrix m);

/// The two keyframes around a time and how far between them it is. Cycles wrap from their last frame to the first,
/// pose ranges end on their last frame.
struct FrameSpan
{
	size_t from;
	size_t to;
	float t;
};
[[nodiscard]] FrameSpan FindFrames(const Animation& animation, uint32_t timeMs);

/// The bones' poses at a time of a cycle: the bones it moves follow its keyframes, all others take the first frame of
/// the stand animation. Keyframe rotations are in the rest pose's space; each bone's own is relative to its parent's
/// rest rotation.
///
/// Given a mirror (see MirrorJoints), the animation plays left to right: the pose is reflected across the body, as
/// MirrorPoses does.
[[nodiscard]] std::vector<Pose> SampleCycle(const Animation& animation, const Animation& stand, uint32_t timeMs,
                                            const Skeleton& skeleton, std::span<const uint32_t> mirror = {});

/// Poses reflected across the body's x = 0 plane: each bone bends away from its rest pose as its mirror bone did,
/// reflected, and stands where its mirror bone stood, reflected. Whatever frames the bones' own rest poses are in, the
/// body as drawn is the exact mirror image of the unmirrored one, for a skeleton whose rest pose is symmetric.
[[nodiscard]] std::vector<Pose> MirrorPoses(std::span<const Pose> poses, const Skeleton& skeleton,
                                            std::span<const uint32_t> mirror);

/// Two animations made one, as a turn on the spot or a step off is drawn: the spins or steps either side of the angle,
/// `weight` of the way from `from` to `to`. It has the first's frames and looping, and lasts `durationMs`. It moves every
/// bone either moves, of the first `boneCount`; a bone one of them leaves alone takes the first frame of `stand` on
/// that side. Frame by frame (the second's last frame standing in for any it lacks), each rotation is the two
/// keyframes' matrices blended on their elements and read back as angles (BlendedAnglesYXZ), and each translation is
/// the two blended. Played, it is a rotation again at every bone, however far apart the two were.
[[nodiscard]] Animation BlendPair(const Animation& from, const Animation& to, const Animation& stand, float weight,
                                  uint32_t durationMs, size_t boneCount);

/// Poses of animations playing together (walking into running, say) blended as the game blends them: each set of poses
/// is carried down the skeleton into the mesh's space, the bones' turns and places there are summed by weight element
/// by element, and the sum is set back relative to each bone's parent. A set of weight 0 is left out; with none left
/// the first set is kept. Bones only some sets have keep the first set's pose.
[[nodiscard]] std::vector<Pose> BlendInMeshSpace(std::span<const std::vector<Pose>> poses, std::span<const float> weights,
                                                 std::span<const uint32_t> parents);

/// Poses summed by weight as they stand, relative to their parents, rotation matrices and translations element by
/// element. The weights are expected to add up to 1. The game blends animations in the mesh's space instead
/// (BlendInMeshSpace).
[[nodiscard]] std::vector<Pose> WeightedSum(std::span<const std::vector<Pose>> poses, std::span<const float> weights);

/// A bone drawn bigger by a scale: its turn from its parent multiplied by it element by element, its place kept, so the
/// bone and all that hangs from it grow about it. Nothing for a bone the poses lack
void ScaleJoint(std::span<Pose> poses, uint32_t joint, float scale);

/// Plays an animation on top of poses: each bone it moves is turned and moved by how far its keyframes at the time are
/// from a reference keyframe of the same animation. Turning the head to look about, the face's expressions and
/// gestures such as nodding are layered this way. A mirror flips the layer as SampleCycle does.
void AddLayer(std::vector<Pose>& poses, const Animation& layer, uint32_t timeMs, size_t referenceFrame,
              const Skeleton& skeleton, std::span<const uint32_t> mirror = {});

/// Each bone's mirror bone across the body's x = 0 plane in the rest pose: a left leg's right leg, a bone along the
/// middle itself. Bones are paired when each is where the other would be reflected; any without a clear partner keep
/// their own place.
[[nodiscard]] std::vector<uint32_t> MirrorJoints(std::span<const glm::mat4> rest);

/// The matrices in the mesh's space of the bones posed, in the form L3DMesh::GetBoneMatrices gives the rest pose
[[nodiscard]] std::vector<glm::mat4> ComposeBoneMatrices(std::span<const Pose> poses, std::span<const uint32_t> parents);

/// An animation of a .cbn or .hbn file
[[nodiscard]] Animation FromMorph(const morph::Animation& animation);
} // namespace openblack::skeletal_animation
