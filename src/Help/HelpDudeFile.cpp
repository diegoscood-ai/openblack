/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HelpDudeFile.h"

#include <cstring>

#include <algorithm>
#include <array>

#include <L3DFile.h>
#include <PackFile.h>
#include <glm/mat4x4.hpp>

namespace openblack::help
{

namespace
{
/// LHFile::GetSegmentData 0x7BE040 over the segment's bytes
struct SegmentReader
{
	const std::vector<uint8_t>& data;
	size_t offset = 0;

	bool Bytes(void* out, size_t count)
	{
		if (count > data.size() - offset)
		{
			return false;
		}
		std::memcpy(out, data.data() + offset, count);
		offset += count;
		return true;
	}

	bool U32(uint32_t& out) { return Bytes(&out, 4); }
	bool F32(float& out) { return Bytes(&out, 4); }

	/// char[0x80] up to its first zero
	bool Name(std::string& out)
	{
		std::array<char, 0x80> name {};
		if (!Bytes(name.data(), name.size()))
		{
			return false;
		}
		out.assign(name.data(), static_cast<size_t>(std::find(name.begin(), name.end(), '\0') - name.begin()));
		return true;
	}
};

bool Fail(std::string* error, const std::string& what)
{
	if (error != nullptr)
	{
		*error = "helpdude: " + what;
	}
	return false;
}
} // namespace

bool LoadHelpDudeFile(const std::vector<uint8_t>& bytes, HelpDudeFile& out, std::string* error)
{
	out = {};
	pack::PackFile container;
	if (const auto result = container.Open(bytes); result != pack::PackResult::Success)
	{
		return Fail(error, "not a LiOnHeAd segment file (" + std::string(pack::ResultToStr(result)) + ")");
	}
	// LHFile::OpenSegment 0x7BDEC0 "helpdude" (0x5C21A8)
	if (!container.HasBlock("helpdude"))
	{
		return Fail(error, "no \"helpdude\" segment");
	}
	const auto& segment = container.GetBlock("helpdude");
	out.segmentSize = segment.size();
	SegmentReader in {segment};

	// 0x5C21BE..0x5C21F9
	if (!in.U32(out.boneCount) || !in.U32(out.animCount) || !in.F32(out.restHeight) || !in.Name(out.meshName))
	{
		return Fail(error, "header past the end");
	}
	if (out.animCount > HelpDudeFile::k_AnimSlots)
	{
		return Fail(error, "more than 80 anim names"); // (inferred) the original would write past +0xAC's 80 names
	}
	// 0x5C2219..0x5C2235: the names
	out.animNames.resize(HelpDudeFile::k_AnimSlots);
	for (uint32_t i = 0; i < out.animCount; ++i)
	{
		if (!in.Name(out.animNames[i]))
		{
			return Fail(error, "anim names past the end");
		}
	}
	if (!in.U32(out.hasData))
	{
		return Fail(error, "no data flag");
	}
	out.clips.resize(HelpDudeFile::k_AnimSlots);
	out.clipRecordSizes.assign(HelpDudeFile::k_AnimSlots, 0);
	if (out.hasData != 0)
	{
		// fn_005C0F30: the mesh size into +0x18, then the L3D0 bytes
		uint32_t meshSize = 0;
		if (!in.U32(meshSize) || meshSize > segment.size() - in.offset)
		{
			return Fail(error, "mesh past the end");
		}
		out.mesh.resize(meshSize);
		in.Bytes(out.mesh.data(), meshSize);
		// LoadAnims 0x5C14E0: the count, then fn_005C1090 per slot
		if (!in.U32(out.clipCount) || out.clipCount > HelpDudeFile::k_AnimSlots)
		{
			return Fail(error, "bad clip count"); // (inferred) +0x24 holds 0x50 pointers (new 0x140 at 0x5C1505)
		}
		for (uint32_t i = 0; i < out.clipCount; ++i)
		{
			if (out.animNames[i].empty())
			{
				continue; // 0x5C10CD: no record at all for an empty name
			}
			if (!in.U32(out.clipRecordSizes[i]))
			{
				return Fail(error, "clip record past the end");
			}
			if (out.clipRecordSizes[i] == 0)
			{
				continue; // 0x5C10F6
			}
			CAnim clip;
			std::string clipError;
			if (!ReadCAnim(segment.data(), segment.size(), in.offset, clip, &clipError))
			{
				return Fail(error, "clip " + std::to_string(i) + ": " + clipError);
			}
			out.clips[i] = std::move(clip);
		}
	}
	// 0x5C2287..0x5C22EE
	bool ok = in.Bytes(out.block2ECC.data(), out.block2ECC.size());
	for (auto& word : out.words2EE8)
	{
		ok = ok && in.U32(word);
	}
	ok = ok && in.U32(out.startEmotion);
	uint32_t count = 0;
	ok = ok && in.U32(count) && count <= segment.size() - in.offset;
	if (!ok)
	{
		return Fail(error, "block +0x2ECC..+0x2C38 past the end");
	}
	out.block2C38.resize(count);
	in.Bytes(out.block2C38.data(), count);
	// 0x5C234C..0x5C237D
	if (!in.F32(out.nearDepth) || !in.F32(out.scale35B4) || !in.F32(out.value35C0) || !in.U32(count))
	{
		return Fail(error, "depths past the end");
	}
	// 0x5C23BC..0x5C24BB: the per-anim events (the table is new 0x504 = 4 + 80 x 16 at 0x5C2391)
	if (count > HelpDudeFile::k_AnimSlots)
	{
		return Fail(error, "more than 80 event entries"); // (inferred)
	}
	out.animEvents.resize(count);
	for (auto& entry : out.animEvents)
	{
		uint32_t events = 0;
		if (!in.U32(events) || events > (segment.size() - in.offset) / 12)
		{
			return Fail(error, "events past the end");
		}
		entry.events.resize(events);
		for (auto& event : entry.events)
		{
			in.U32(event.sample);
			in.U32(event.flag);
			in.F32(event.phase);
		}
		if (!in.F32(entry.loopStart) || !in.F32(entry.loopEnd))
		{
			return Fail(error, "event loop window past the end");
		}
	}
	// 0x5C24C1..0x5C2528
	ok = in.F32(out.value35C4) && in.F32(out.value35C8) && in.F32(out.farDepth) &&
	     in.Bytes(out.animFlags.data(), out.animFlags.size()) && in.F32(out.haloScale) && in.F32(out.haloOffset.x) &&
	     in.F32(out.haloOffset.y) && in.F32(out.haloOffset.z);
	if (!ok)
	{
		return Fail(error, "tail past the end");
	}
	out.bytesRead = in.offset; // CloseSegment 0x5C2539
	return true;
}

bool LoadRestSkeleton(const HelpDudeFile& file, RestSkeleton& out, std::string* error)
{
	if (file.mesh.empty())
	{
		return Fail(error, "no mesh");
	}
	l3d::L3DFile l3d;
	if (const auto result = l3d.Open(file.mesh); result != l3d::L3DResult::Success)
	{
		return Fail(error, "the L3D0 mesh does not load");
	}
	const auto& bones = l3d.GetBones();
	if (bones.empty())
	{
		return Fail(error, "the mesh has no bones");
	}
	std::vector<uint32_t> parents(bones.size());
	std::vector<glm::mat4> local(bones.size());
	for (size_t i = 0; i < bones.size(); ++i)
	{
		const auto& bone = bones[i];
		const auto& o = bone.orientation;
		// as L3DMesh (src/3D/L3DMesh.cpp): the columns are the LH rows, the position last
		local[i] = glm::mat4(o[0], o[1], o[2], 0.0f, o[3], o[4], o[5], 0.0f, o[6], o[7], o[8], 0.0f, bone.position.x,
		                     bone.position.y, bone.position.z, 1.0f);
		parents[i] = bone.parent;
	}
	out = MakeRestSkeleton(parents, local);
	return true;
}

} // namespace openblack::help
