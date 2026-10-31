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
#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "3D/AnimationFill.h"

namespace openblack
{

/// Evaluates the player hand animations stored in Data/CTR/hh.HBN (spec: Data/hndspec5.txt).
///
/// Nodes named C* are the central animation of a hand state (Cwiggle, Ccan_pickup, Cgrip...).
/// Nodes named L*_lr / L*_fb are lean ranges: they are not played over time but sampled at a time given by the trailing
/// cursor's lag, and added as the difference from their middle frame on top of the matching C node.
///
/// The bones are filled as the 3D engine fills them (anim_fill): a clip's frame at whole milliseconds, the bones it
/// does not turn or move at Cwiggle's first frame, and the lean ranges' difference from their middle frame.
class HandAnimator
{
public:
	/// Row-vector 3x3 rotation (Lionhead convention) + translation.
	using Affine = anim_fill::BoneMatrix;
	using Pose = std::vector<Affine>;

	struct ClipInfo
	{
		std::string name;
		std::string set;
		uint32_t durationMs;
		uint32_t frameCount;
		bool loop;
	};

	/// Gives the bytes of the spec file the animations name (hndspec5.txt), or nothing when it cannot be read
	using SpecReader = std::function<std::optional<std::vector<uint8_t>>(const std::string& specName)>;

	/// @param readSpec reads the spec file, by its name, through the game's file caches.
	/// @param bindLocals each bone's rest matrix relative to its parent, column-vector form as stored by L3DMesh.
	/// @param parents parent index per bone, UINT32_MAX for the root.
	bool Load(const std::vector<uint8_t>& hbnFile, const SpecReader& readSpec, const std::vector<glm::mat4>& bindLocals,
	          const std::vector<uint32_t>& parents) noexcept;

	[[nodiscard]] bool IsLoaded() const noexcept { return !_clips.empty(); }
	[[nodiscard]] bool Has(std::string_view name) const noexcept;
	[[nodiscard]] std::vector<ClipInfo> ListClips() const noexcept;
	[[nodiscard]] const std::string& GetCurrentClip() const noexcept { return _clipName; }

	/// Cross-fade to a C node. Playing the current clip again does nothing.
	bool Play(std::string_view name, std::chrono::milliseconds blend) noexcept;
	/// special_hold (holding an object): blend of the static poses Cphile (0) -> Chorn (1) with the
	/// Lhold_fingers layers on top. nullopt returns to the current C node.
	void SetSpecialHold(std::optional<float> fill, std::chrono::milliseconds blend) noexcept;
	[[nodiscard]] bool IsSpecialHold() const noexcept { return _specialHold.has_value(); }
	/// Hold the current clip at a time in ms (the animation's "frame" argument in the holding state), sampled like Play.
	void SetTime(std::optional<float> timeMs) noexcept { _fixedTime = timeMs; }
	[[nodiscard]] uint32_t GetDurationMs(std::string_view name) const noexcept;
	[[nodiscard]] uint32_t GetFrameCount(std::string_view name) const noexcept;

	/// The trailing cursor's lag in pixels each way (sideways for the L*_lr range, back and forth for L*_fb), within
	/// -80..80, or nothing where the hand does not lean that way
	void SetLean(std::optional<float> sideways, std::optional<float> forwards) noexcept;
	/// Keep the root bone in its bind pose (the animated root turns/moves the whole hand).
	void SetRootLocked(bool locked) noexcept { _rootLocked = locked; }

	void Update(std::chrono::microseconds dt) noexcept;

	/// Stateless evaluation of a clip (plus its L layers at the given lags) at a given time, as global bone matrices.
	/// For tools/tests.
	[[nodiscard]] std::vector<glm::mat4> Evaluate(std::string_view name, int32_t timeMs,
	                                              std::optional<float> sideways = std::nullopt,
	                                              std::optional<float> forwards = std::nullopt) const noexcept;

	/// Global bone matrices (column-vector form), ready for bgfx::setTransform.
	[[nodiscard]] const std::vector<glm::mat4>& GetBoneMatrices() const noexcept { return _boneMatrices; }
	/// The last pose's bones in the world, under the hand's matrix, as the engine finishes them
	[[nodiscard]] std::vector<glm::mat4> ToWorld(const glm::mat4& hand) const noexcept
	{
		return anim_fill::ToWorld(_pose, _parents, hand);
	}

	/// What the last Update filled: the clip's time and its lean ranges' times, in ms (none where it did not lean)
	struct Readout
	{
		int32_t timeMs {0};
		std::optional<int32_t> sidewaysMs;
		std::optional<int32_t> forwardsMs;
	};
	[[nodiscard]] const Readout& GetReadout() const noexcept { return _readout; }

private:
	struct Clip
	{
		ClipInfo info;
		anim_fill::Clip frames;
	};

	[[nodiscard]] Pose Sample(const Clip& clip, int32_t timeMs) const noexcept;
	[[nodiscard]] Pose ApplyMotion(const Pose& base, std::string_view clipName) const noexcept;
	/// A lean range's time at a lag, none without the range or the lag
	[[nodiscard]] std::optional<int32_t> LeanTimeMs(const std::string& rangeName, std::optional<float> lag) const noexcept;
	void ComputeBoneMatrices() noexcept;
	[[nodiscard]] std::vector<glm::mat4> ToGlobals(const Pose& pose) const noexcept;

	std::unordered_map<std::string, Clip> _clips;
	std::vector<uint32_t> _parents;
	Pose _bindLocal;
	anim_fill::Rest _rest;
	/// Cwiggle's first frame: what every bone a clip neither turns nor moves takes
	anim_fill::Frame _stand;

	std::string _clipName;
	float _timeMs {0.0f};
	Pose _pose;
	Pose _fromPose;
	float _blendMs {0.0f};
	float _blendDurationMs {0.0f};
	std::optional<float> _leanSideways;
	std::optional<float> _leanForwards;
	bool _rootLocked {false};
	std::optional<float> _specialHold;
	std::optional<float> _fixedTime;
	std::vector<glm::mat4> _boneMatrices;
	Readout _readout;
};

} // namespace openblack
