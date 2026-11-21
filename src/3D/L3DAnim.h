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

#include <filesystem>
#include <vector>

#include <glm/fwd.hpp>

namespace openblack
{
namespace anm
{
class ANMFile;
} // namespace anm

namespace debug::gui
{
class MeshViewer;
}

class L3DAnim
{
public:
	struct Frame
	{
		uint32_t time;
		std::vector<glm::mat4> bones;
	};

	L3DAnim() noexcept = default;
	virtual ~L3DAnim() noexcept = default;

	void Load(const anm::ANMFile& anm) noexcept;
	bool LoadFromFilesystem(const std::filesystem::path& path) noexcept;
	bool LoadFromFile(const std::filesystem::path& path) noexcept;
	bool LoadFromBuffer(const std::vector<uint8_t>& data) noexcept;

	[[nodiscard]] const std::string& GetName() const noexcept { return _name; }
	/// The size in bytes of the clip's data in its file: not a time (the play time is GetPlayTime)
	[[nodiscard]] uint32_t GetDataSize() const noexcept { return _dataSize; }
	void SetLooping(bool loop) noexcept { _unknown_0x50 = loop ? (_unknown_0x50 | 0x100u) : (_unknown_0x50 & ~0x100u); }
	[[nodiscard]] const std::vector<Frame>& GetFrames() const noexcept { return _frames; }
	[[nodiscard]] std::vector<glm::mat4> GetBoneMatrices(uint32_t time) const noexcept;

	// The header as the original reads it (docs/wiki/beings/animation.md#playback):
	/// How long one play of the clip lasts in milliseconds of game time, over which its keyframes are evenly spread
	/// (`_dataSize` is the clip's size in bytes)
	[[nodiscard]] uint32_t GetPlayTime() const noexcept { return _unknown_0x20; }
	/// ground covered by one cycle: walking clips advance with the distance moved
	[[nodiscard]] float GetCycleDistance() const noexcept { return _unknown_0x28; }
	/// the loop flag, 0x100 of the flags
	[[nodiscard]] bool IsLooping() const noexcept { return (_unknown_0x50 & 0x100u) != 0; }
	/// the header's flags as the file has them: 0x100 the loop, 0x200 no play by the ground covered
	[[nodiscard]] uint32_t GetFlags() const noexcept { return _unknown_0x50; }
	/// Each bone's transform relative to its parent at `milliseconds`. The keys are evenly
	/// spaced (a looping clip wraps its last key back to the first, a one-shot one ends on it) and every element of the
	/// 3x4 matrices is lerped, without re-orthonormalising. `blendSampler`: the original's blend sampler instead (the
	/// SuperVillager's cross-fade), the same but the next key after the last is always the first, one-shot clips too.
	void SampleLocal(int32_t milliseconds, std::vector<glm::mat4>& bones, bool blendSampler = false) const noexcept;

private:
	std::string _name;
	uint32_t _unknown_0x20; // The play time in milliseconds
	float _unknown_0x24;    // TODO(#471)
	float _unknown_0x28;    // TODO(#471)
	float _unknown_0x2C;    // TODO(#471)
	float _unknown_0x30;    // TODO(#471)
	float _unknown_0x34;    // TODO(#471)
	uint32_t _unknown_0x3C; // TODO(#471): Always 1 in Body Block, a count
	uint32_t _dataSize;     // The size in bytes of the clip's data
	uint32_t _unknown_0x44; // TODO(#471): Always 1 in Body Block
	uint32_t _unknown_0x48; // TODO(#471): Always 0 in Body Block
	uint32_t _unknown_0x50; // TODO(#471): Seems to be a uint16_t padded

	std::vector<Frame> _frames;

	friend debug::gui::MeshViewer; // TODO(#471): Remove me once the unknowns are known and replace with getters
};

} // namespace openblack
