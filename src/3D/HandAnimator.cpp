/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandAnimator.h"

#include <cmath>
#include <cstring>

#include <algorithm>
#include <limits>

#include <MorphFile.h>
#include <spdlog/spdlog.h>

using namespace openblack;

namespace
{
using R3 = std::array<float, 9>;
using Affine = HandAnimator::Affine;
using Pose = HandAnimator::Pose;

constexpr uint32_t k_NoParent = std::numeric_limits<uint32_t>::max();
/// hh.HBN is a Lionhead pack: "LiOnHeAd" + block name "Hand" (32 bytes) + block size.
/// The morph data (and all its offsets) start right after it.
constexpr size_t k_PackHeaderSize = 0x2C;
/// The trailing cursor's lag that leans a range to its last frame, either way
constexpr float k_LeanRange = 80.0f;

R3 NormalizeRows(R3 r)
{
	for (int row = 0; row < 3; ++row)
	{
		const float l = std::hypot(r[row * 3], r[row * 3 + 1], r[row * 3 + 2]);
		if (l > 0.0f)
		{
			r[row * 3] /= l;
			r[row * 3 + 1] /= l;
			r[row * 3 + 2] /= l;
		}
	}
	return r;
}

R3 LerpRotation(const R3& a, const R3& b, float t)
{
	R3 o {};
	for (size_t i = 0; i < 9; ++i)
	{
		o[i] = a[i] + (b[i] - a[i]) * t;
	}
	return NormalizeRows(o);
}

/// The cross-fade between two poses (the special hold's); the engine itself blends the bones in the world
Pose BlendPoses(const Pose& a, const Pose& b, float t)
{
	if (t <= 0.0f)
	{
		return a;
	}
	if (t >= 1.0f)
	{
		return b;
	}
	Pose out(a.size());
	for (size_t i = 0; i < a.size(); ++i)
	{
		out[i] = {LerpRotation(a[i].rows, b[i].rows, t), a[i].position + (b[i].position - a[i].position) * t};
	}
	return out;
}

float Ease(float t)
{
	t = std::clamp(t, 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

/// L3DMesh keeps bone matrices in column-vector form built as mat4(o0,o1,o2,0, o3,o4,o5,0, o6,o7,o8,0, p,1).
Affine FromColumnMatrix(const glm::mat4& m)
{
	return {{m[0][0], m[0][1], m[0][2], m[1][0], m[1][1], m[1][2], m[2][0], m[2][1], m[2][2]}, glm::vec3(m[3])};
}

} // namespace

bool HandAnimator::Load(const std::vector<uint8_t>& hbnFile, const SpecReader& readSpec,
                        const std::vector<glm::mat4>& bindLocals, const std::vector<uint32_t>& parents) noexcept
{
	auto logger = spdlog::get("game");
	_clips.clear();
	if (bindLocals.empty() || bindLocals.size() != parents.size())
	{
		SPDLOG_LOGGER_ERROR(logger, "HandAnimator: hand mesh has no bones");
		return false;
	}
	if (hbnFile.size() <= k_PackHeaderSize || std::memcmp(hbnFile.data(), "LiOnHeAd", 8) != 0)
	{
		SPDLOG_LOGGER_ERROR(logger, "HandAnimator: hh.HBN is not a LiOnHeAd pack");
		return false;
	}

	const std::vector<uint8_t> block(hbnFile.begin() + k_PackHeaderSize, hbnFile.end());
	morph::MorphFile morph;
	if (const auto result = morph.Open(block, readSpec); result != morph::MorphResult::Success)
	{
		SPDLOG_LOGGER_ERROR(logger, "HandAnimator: failed to read hh.HBN: {}", morph::ResultToStr(result));
		return false;
	}

	// The rest pose: the mesh's own bone matrices, and the rest the engine fills the clips against
	const auto boneCount = bindLocals.size();
	_parents = parents;
	_bindLocal.resize(boneCount);
	std::transform(bindLocals.begin(), bindLocals.end(), _bindLocal.begin(), FromColumnMatrix);
	_rest = anim_fill::RestFromLocals(bindLocals, parents);

	for (const auto& anim : morph.GetBaseAnimationSet())
	{
		if (anim.header.meshBoneCount != boneCount)
		{
			SPDLOG_LOGGER_WARN(logger, "HandAnimator: {} has {} bones, mesh has {}", anim.name, anim.header.meshBoneCount,
			                   boneCount);
			continue;
		}
		Clip clip;
		// The header gives the length in milliseconds and whether the clip loops (cycles, the C nodes).
		clip.info = {anim.name, anim.setName, anim.header.duration, anim.header.frameCount, anim.header.looping != 0};
		clip.frames = {
		    .durationMs = static_cast<int32_t>(anim.header.duration),
		    .cyclic = anim.header.looping != 0,
		    .boneCount = anim.header.meshBoneCount,
		    .angleBones = anim.rotatedJointIndices,
		    .positionBones = anim.translatedJointIndices,
		    .frames = {},
		};
		clip.frames.frames.reserve(anim.keyframes.size());
		for (const auto& key : anim.keyframes)
		{
			auto& frame = clip.frames.frames.emplace_back();
			frame.angles.reserve(key.eulerAngles.size());
			for (const auto& e : key.eulerAngles)
			{
				frame.angles.emplace_back(e[0], e[1], e[2]);
			}
			frame.positions.reserve(key.translations.size());
			for (const auto& t : key.translations)
			{
				frame.positions.emplace_back(t[0], t[1], t[2]);
			}
		}
		// Names such as Cspare repeat across sets: keep the first one.
		_clips.try_emplace(anim.name, std::move(clip));
	}

	// The stand frame is Cwiggle's first, which the engine reads by bone: Cwiggle turns and moves every bone in order
	_stand = {};
	if (const auto wiggle = _clips.find("Cwiggle"); wiggle != _clips.end() && !wiggle->second.frames.frames.empty())
	{
		_stand = wiggle->second.frames.frames.front();
	}

	SPDLOG_LOGGER_INFO(logger, "HandAnimator: loaded {} hand animations from hh.HBN", _clips.size());
	_pose = _bindLocal;
	_clipName.clear();
	Play("Cwiggle", std::chrono::milliseconds(0));
	ComputeBoneMatrices();
	return IsLoaded();
}

bool HandAnimator::Has(std::string_view name) const noexcept
{
	return _clips.contains(std::string(name));
}

std::vector<HandAnimator::ClipInfo> HandAnimator::ListClips() const noexcept
{
	std::vector<ClipInfo> out;
	out.reserve(_clips.size());
	for (const auto& [_, clip] : _clips)
	{
		out.push_back(clip.info);
	}
	std::sort(out.begin(), out.end(),
	          [](const auto& a, const auto& b) { return a.set != b.set ? a.set < b.set : a.name < b.name; });
	return out;
}

uint32_t HandAnimator::GetDurationMs(std::string_view name) const noexcept
{
	const auto it = _clips.find(std::string(name));
	return it == _clips.end() ? 0 : it->second.info.durationMs;
}

uint32_t HandAnimator::GetFrameCount(std::string_view name) const noexcept
{
	const auto it = _clips.find(std::string(name));
	return it == _clips.end() ? 0 : static_cast<uint32_t>(it->second.frames.frames.size());
}

bool HandAnimator::Play(std::string_view name, std::chrono::milliseconds blend) noexcept
{
	if (!Has(name))
	{
		return false;
	}
	if (name == _clipName)
	{
		return true;
	}
	_fromPose = _pose;
	_clipName = name;
	_timeMs = 0.0f;
	_blendMs = 0.0f;
	_blendDurationMs = static_cast<float>(blend.count());
	return true;
}

void HandAnimator::SetSpecialHold(std::optional<float> fill, std::chrono::milliseconds blend) noexcept
{
	if (fill.has_value() == _specialHold.has_value())
	{
		if (fill)
		{
			_specialHold = std::clamp(*fill, 0.0f, 1.0f);
		}
		return;
	}
	if (fill && (!Has("Cphile") || !Has("Chorn")))
	{
		return;
	}
	_specialHold = fill ? std::optional(std::clamp(*fill, 0.0f, 1.0f)) : std::nullopt;
	_fromPose = _pose;
	_blendMs = 0.0f;
	_blendDurationMs = static_cast<float>(blend.count());
	if (!_specialHold)
	{
		// Restart the C node from its first frame after letting go.
		_timeMs = 0.0f;
	}
}

void HandAnimator::SetLean(std::optional<float> sideways, std::optional<float> forwards) noexcept
{
	const auto clampLag = [](std::optional<float> lag) {
		return lag ? std::optional(std::clamp(*lag, -k_LeanRange, k_LeanRange)) : std::nullopt;
	};
	_leanSideways = clampLag(sideways);
	_leanForwards = clampLag(forwards);
}

Pose HandAnimator::Sample(const Clip& clip, int32_t timeMs) const noexcept
{
	auto pose = _bindLocal;
	anim_fill::Fill(pose, clip.frames, _rest, timeMs, _stand);
	return pose;
}

Pose HandAnimator::ApplyMotion(const Pose& base, std::string_view clipName) const noexcept
{
	if (clipName.empty() || clipName.front() != 'C')
	{
		return base;
	}
	const auto stem = std::string(clipName.substr(1));
	auto pose = base;
	// Each lean range adds its difference from its middle frame, at the time the lag gives
	for (const auto& [suffix, lag] : {std::pair {"_lr", _leanSideways}, std::pair {"_fb", _leanForwards}})
	{
		const auto name = "L" + stem + suffix;
		if (const auto timeMs = LeanTimeMs(name, lag))
		{
			const auto& range = _clips.at(name).frames;
			const auto& middle = range.frames[(range.frames.size() - 1) / 2];
			anim_fill::AddDifferenceFromFrame(pose, range, middle, _rest, *timeMs);
		}
	}
	return pose;
}

std::optional<int32_t> HandAnimator::LeanTimeMs(const std::string& rangeName, std::optional<float> lag) const noexcept
{
	const auto it = _clips.find(rangeName);
	if (!lag || it == _clips.end() || it->second.frames.frames.empty())
	{
		return std::nullopt;
	}
	return anim_fill::RangeTimeMs(*lag, k_LeanRange, it->second.frames.durationMs);
}

void HandAnimator::Update(std::chrono::microseconds dt) noexcept
{
	const auto it = _clips.find(_clipName);
	if (it == _clips.end())
	{
		return;
	}
	const float dtMs = static_cast<float>(dt.count()) / 1000.0f;
	_timeMs += dtMs;
	Pose target;
	_readout = {};
	if (_specialHold)
	{
		// 0% = Cphile, 100% = Chorn. special_hold has no L layers: reuse hold_fingers so the hand still reacts.
		const auto base = BlendPoses(Sample(_clips.at("Cphile"), 0), Sample(_clips.at("Chorn"), 0), *_specialHold);
		target = ApplyMotion(base, "Chold_fingers");
	}
	else if (_fixedTime)
	{
		_readout.timeMs = static_cast<int32_t>(*_fixedTime);
		target = ApplyMotion(Sample(it->second, _readout.timeMs), _clipName);
	}
	else
	{
		// The clip's own clock in whole milliseconds, a cycle's wrapped at its length
		const auto duration = std::max<int32_t>(1, it->second.frames.durationMs);
		const auto ms = static_cast<int32_t>(_timeMs);
		_readout.timeMs = it->second.frames.cyclic ? ms % duration : ms;
		target = ApplyMotion(Sample(it->second, _readout.timeMs), _clipName);
	}
	if (!_specialHold && !_clipName.empty() && _clipName.front() == 'C')
	{
		const auto stem = _clipName.substr(1);
		_readout.sidewaysMs = LeanTimeMs("L" + stem + "_lr", _leanSideways);
		_readout.forwardsMs = LeanTimeMs("L" + stem + "_fb", _leanForwards);
	}
	if (!_fromPose.empty() && _blendDurationMs > 0.0f && _blendMs < _blendDurationMs)
	{
		_blendMs += dtMs;
		_pose = BlendPoses(_fromPose, target, Ease(_blendMs / _blendDurationMs));
	}
	else
	{
		_pose = target;
		_fromPose.clear();
	}
	if (_rootLocked)
	{
		for (size_t i = 0; i < _pose.size(); ++i)
		{
			if (_parents[i] == k_NoParent)
			{
				_pose[i] = _bindLocal[i];
			}
		}
	}
	ComputeBoneMatrices();
}

std::vector<glm::mat4> HandAnimator::ToGlobals(const Pose& pose) const noexcept
{
	return anim_fill::ToMesh(pose, _parents);
}

void HandAnimator::ComputeBoneMatrices() noexcept
{
	_boneMatrices = ToGlobals(_pose);
}

std::vector<glm::mat4> HandAnimator::Evaluate(std::string_view name, int32_t timeMs, std::optional<float> sideways,
                                              std::optional<float> forwards) const noexcept
{
	const auto it = _clips.find(std::string(name));
	if (it == _clips.end())
	{
		return {};
	}
	auto copy = *this;
	copy.SetLean(sideways, forwards);
	return ToGlobals(copy.ApplyMotion(copy.Sample(it->second, timeMs), name));
}
