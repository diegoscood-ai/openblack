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

#include <optional>
#include <string>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

/// CAnim: the clip format of the advisor spirits (Data\HelpSprite\*.hd, HelpDude::LoadAnims 0x5C14E0), read by
/// CAnim::ReadBinary 0x860860 / CFrame::ReadBinary 0x860C30. It is not the .anm format of components/anm.
///
/// Semantics (runblack.exe; verified by running fn_00860E00 and fn_00861EE0 under an x86 emulator on both .hd files
/// against this code's formulas, max error 4.5e-5):
/// - A clip has `frameCount` evenly spaced keys over `durationMs`. A looping clip spreads them over the whole duration
///   and wraps the last key to the first; a one-shot clip puts its last key at the duration
///   (period = duration * frames / (frames - 1), integer maths, 0x860E1A / 0x861EF9). The next key after the last one is
///   always key 0, even for a one-shot clip (0x860E5B / 0x861F33).
/// - A rotation channel is three Euler angles in radians (x, y, z) turned into a matrix by LHMatrix::SetYXZMatrixOnly
///   0x7FAC10 (lh_matrix::YXZ) called as (y, x, z): with column vectors M = Rz(z) Rx(x) Ry(y). The two keys' matrices
///   are lerped element by element and each row is normalised (fn_007FB5C0, lh_matrix::NormaliseRows: InverseSquareRoot
///   0x841170 with its 128-byte table).
/// - The angles are not relative to the parent bone: they rotate the bone in model axes from its rest pose. The local
///   matrix is rest world (bone) x key x inverse rest world (parent) in LH row-vector order (0x8610B7..0x861456).
/// - A position channel is the bone's translation relative to its parent, absolute (lerped, 0x861964).
/// - Layers are additive (fn_00861EE0): the change from a reference key of the same clip to the sampled key is applied
///   to the current local matrices (rotation in the parent's rest axes, translation added). There are no blend
///   weights: HelpDude passes the "weight" as the time into the clip (vowels 7..9 fn_005BF810, look 18/19 fn_005BF8C0).
/// - The mirrored path of both functions (a bone remap table, last argument) is not ported: HelpDude always passes 0.
namespace openblack::help
{

/// One key (CFrame::ReadBinary 0x860C30): +0x00 the rotation channels, +0x04 the position channels.
struct CFrame
{
	std::vector<glm::vec3> rotations; ///< per rotation channel: Euler angles (x, y, z) in radians
	std::vector<glm::vec3> positions; ///< per position channel: translation relative to the parent bone
};

/// CAnim (0x38 bytes, `new` at 0x5C1108). The fields in the order CAnim::ReadBinary 0x860860 reads them.
struct CAnim
{
	int32_t durationMs = 0;            ///< +0x00 length of the clip in milliseconds
	bool looping = false;              ///< +0x04 the read word & 1 (0x86089F)
	float speed = 0.0f;                ///< +0x08 (inferred) distance / durationMs in both files; never read by HelpDude
	float distance = 0.0f;             ///< +0x0C (inferred) |displacement| in both files; never read by HelpDude
	glm::vec3 displacement {0.0f};     ///< +0x10..+0x18 root move of the whole clip (HelpDude::ApplyAnim 0x5BBA19)
	uint32_t frameCount = 0;           ///< +0x1C
	uint32_t boneCount = 0;            ///< +0x20 bones walked by the samplers (97 good, 73 evil)
	std::vector<uint32_t> rotationBones; ///< +0x2C, count at +0x24: channel -> bone, ascending
	std::vector<uint32_t> positionBones; ///< +0x30, count at +0x28: channel -> bone, ascending
	std::vector<CFrame> frames;        ///< +0x34
};

/// CAnim::ReadBinary 0x860860 at `offset`, which it moves past the clip. False (and `error`) if the data ends first.
bool ReadCAnim(const uint8_t* data, size_t size, size_t& offset, CAnim& out, std::string* error);

/// fn_00860BE0: the bytes ReadBinary consumes (0x2C + 4 per channel + 12 per channel and key). The .hd record size
/// that precedes a clip is this + 4 (HelpDude save 0x5C105D).
[[nodiscard]] size_t CAnimBinarySize(const CAnim& clip);

/// The rest skeleton HelpDude keeps: `world` = LH3DAnim::SetTransform 0x83A1D0 into +0x28AC (each L3D bone matrix put
/// under its parent, root under the identity, 0x5C11E3..0x5C124B), `worldInverse` = LHMatrix::SetInverse 0x7FB290 of
/// each into +0x28B0 (0x5C1261..0x5C127F). glm matrices built like L3DMesh (columns = the LH rows).
struct RestSkeleton
{
	std::vector<uint32_t> parents;          ///< 0xFFFFFFFF for a root
	std::vector<glm::mat4> local;           ///< the L3D bone matrices, relative to the parent
	std::vector<glm::mat4> world;           ///< +0x28AC
	std::vector<glm::mat4> worldInverse;    ///< +0x28B0, LHMatrix::SetInverse 0x7FB290 (lh_matrix::Inverse)
	float height = 0.0f;                    ///< SetTransform's return, max - min of the world y with 0 (0x83A26B..0x83A2E9)
};

/// `local` holds the bone matrices of the L3D (parents before children, like L3DMesh)
[[nodiscard]] RestSkeleton MakeRestSkeleton(const std::vector<uint32_t>& parents, const std::vector<glm::mat4>& local);

/// The keys around `milliseconds` and the fraction between them (prologue of fn_00860E00 / fn_00861EE0).
struct KeySample
{
	uint32_t key0 = 0;
	uint32_t key1 = 0;
	float fraction = 0.0f;
};
/// `clampBelow`: fn_00860E00 clamps a negative key to 0 (0x860E42); fn_00861EE0 does not (negative times are taken to
/// 0 here, the original would read out of the clip).
[[nodiscard]] KeySample SampleKeys(const CAnim& clip, int32_t milliseconds, bool clampBelow);

/// fn_00860E00 (HelpDude 0x5BB8E1 / 0x5BBBBB): writes the clip at `milliseconds` into the local matrices. Bones
/// without a channel keep their matrix, or take `fill`'s key indexed by bone (the stand clip's key 0, HelpDude +0x28)
/// when `fill` is given (the last argument 1 of 0x5BB8BE).
void SetPose(const CAnim& clip, int32_t milliseconds, const RestSkeleton& rest, const CFrame* fill,
             std::vector<glm::mat4>& local);

/// fn_00861EE0 (HelpDude::ApplyAnim 0x5BBB17, fn_005BF810, fn_005BF8C0): adds the change from `reference` (a key of
/// the same clip) to the clip at `milliseconds` onto the local matrices.
void ApplyAdditive(const CAnim& clip, int32_t milliseconds, const CFrame& reference, const RestSkeleton& rest,
                   std::vector<glm::mat4>& local);

/// The clip alone at `milliseconds` (truncated like __ftol): the rest locals, then SetPose without fill. The result is
/// what graphics::ComputePose chains: each bone's matrix relative to its parent, in glm's column-vector convention.
void SampleLocal(const CAnim& clip, float milliseconds, const RestSkeleton& rest, std::vector<glm::mat4>& local);

/// FinishAnimStack 0x5C0610 -> 0x839F10: each local put under its parent's result, roots under `root` (world =
/// parent * local here; L * W(parent) in the original's row vectors).
void ComposeWorld(const RestSkeleton& rest, const std::vector<glm::mat4>& local, const glm::mat4& root,
                  std::vector<glm::mat4>& world);

/// What HelpDude::ApplyAnim 0x5BB980 (anim, phase, referencePhase, wrap) passes to fn_00861EE0.
struct ApplyAnimArgs
{
	float phase = 0.0f;        ///< after the wrap (fraction, 0x5BB9A8) or the clamp to 1 (0x5BBB26), then to >= 0
	int32_t milliseconds = 0;  ///< ftol(duration * phase), at most duration - 1 (0x5BB9F5 / 0x5BBADC)
	uint32_t referenceKey = 0; ///< ftol((frames - 1) * referencePhase), at most duration - 1 (sic, 0x5BBA0C / 0x5BBAE1)
	glm::vec3 rootMove {0.0f}; ///< phase * displacement: HelpDude turns it by its 3x3 at +0x3350 and adds it to +0x3374
};
[[nodiscard]] ApplyAnimArgs ApplyAnimArguments(const CAnim& clip, float phase, float referencePhase, bool wrap);

} // namespace openblack::help
