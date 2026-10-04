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
#include <string>
#include <vector>

#include <glm/vec3.hpp>

#include "Help/CAnim.h"

/// Data\HelpSprite\MarkGood.Hd / MarkEvil.Hd: a "LiOnHeAd" file with one segment "helpdude", read by HelpDude::Load
/// 0x5C2194 (good = dude 0 = file pointer 0x915D20, evil = dude 1 = 0x915D24, Init 0x5C2A4E). The field names are the
/// HelpDude members they land in; the order below is the read order.
namespace openblack::help
{

/// One sound of an anim (PlaySoundFX 0x5C2800 when the anim's phase crosses `phase`): 12 bytes read at 0x5C245D
struct HelpDudeAnimEvent
{
	uint32_t sample = 0; ///< InGame bank sample (155 KnockScreen, 154 HandGun)
	uint32_t flag = 0;   ///< (pending) 1 in every entry of both files
	float phase = 0.0f;
};

/// HelpDude +0x333C[i], 16 bytes: +0 count, +4 the events (new at 0x5C2423), +8 / +0xC the loop window [start, end]
/// of anim mode 4 (0x5BCE49..0x5BCE7A)
struct HelpDudeAnimEvents
{
	std::vector<HelpDudeAnimEvent> events;
	float loopStart = 0.0f; ///< +8 (0x5C2487)
	float loopEnd = 0.0f;   ///< +0xC (0x5C249B)
};

struct HelpDudeFile
{
	static constexpr size_t k_AnimSlots = 80; ///< 0x50: the name block (0xA00 dwords cleared at 0x5C2206), flags, events

	uint32_t boneCount = 0;     ///< +0x0C (0x5C21C2); replaced by the mesh's bone count at 0x5C101F. 97 good, 73 evil
	uint32_t animCount = 0;     ///< local at 0x5C21CD, 80: the names read
	float restHeight = 0.0f;    ///< +0x10 (0x5C21E1): 63.423 / 79.154, the max - min y of the rest skeleton that
	                            ///< LH3DAnim::SetTransform returns into +0x10 again at 0x5C1250
	std::string meshName;       ///< +0x2C, char[0x80]: "DATA\Yogi_Mesh.l3d" / "C:\dev\TESTBED\DATA\Demon_Mesh.l3d"
	std::vector<std::string> animNames; ///< +0xAC, animCount x char[0x80]; an empty name has no clip
	uint32_t hasData = 0;       ///< local at 0x5C224A: 0 skips the mesh and the clips (0x5C2261)
	std::vector<uint8_t> mesh;  ///< fn_005C0F30: u32 size (+0x18) then a plain "L3D0" mesh (LH3DMesh::Create 0x5C0FB7)
	uint32_t clipCount = 0;     ///< LoadAnims 0x5C152E: 80
	/// HelpDude +0x24: one slot per index below clipCount; for a non-empty name a u32 record size (0 = no clip), then
	/// CAnim::ReadBinary (fn_005C1090 0x5C10CD..0x5C112E). The original only tests the size against 0.
	std::vector<std::optional<CAnim>> clips;
	/// the record size read per slot (0 when the name is empty): CAnimBinarySize + 4, the size counting itself
	std::vector<uint32_t> clipRecordSizes;
	std::array<uint8_t, 0x1C> block2ECC {}; ///< +0x2ECC (pending)
	std::array<uint32_t, 4> words2EE8 {};   ///< +0x2EE8 / +0x2EEC / +0x2EF0 / +0x2EF4 (pending; 1 then three floats)
	uint32_t startEmotion = 0;              ///< +0x2C28 (0x5C22EE), the u32 starting emotion (spec_spirits_motion 0); 0 in both
	/// +0x2C38: u32 n then n bytes (0x5C2307; n = 0x200 in both): the 8 face records of 0x40 bytes (16 floats), one per
	/// emotion, that fn_005BD0B0 indexes by +0x2C28 << 6 (shl edx, 6 at 0x5BD101). For n != 0x200 the original reads
	/// each byte over +0x2C38 itself (0x5C231E); here they are kept in order.
	std::vector<uint8_t> block2C38;
	float nearDepth = 0.0f;  ///< +0x35B8 (0x5C2350): 8.7333 both
	/// +0x35B4 (0x5C2362): 0.02552 / 0.02683, the model scale S: the rows (0x5BE323), the partner zone, the bank lean
	/// and the trail; the halo's size is S x +0x34DC x +0x37E4 x 100 (0x5C0AB8)
	float scale35B4 = 0.0f;
	float value35C0 = 0.0f;  ///< +0x35C0 (0x5C2374) (pending), 0 in both
	std::vector<HelpDudeAnimEvents> animEvents; ///< +0x333C: u32 count (0x5C23DA) then the entries (80 in both)
	float value35C4 = 0.0f;  ///< +0x35C4 (0x5C24C5) (pending): 0.3467 / 0
	float value35C8 = 0.0f;  ///< +0x35C8 (0x5C24D7) (pending): 0 / 0.3067
	float farDepth = 0.0f;   ///< +0x35BC (0x5C24E9): 6.0267 / 5.64 (x1.2 in Init 0x5C2C9B)
	std::array<uint8_t, k_AnimSlots> animFlags {}; ///< +0x2E7C (0x5C24FB); &0x20 = skip while clinging (0x5BCEDC)
	float haloScale = 0.0f;  ///< +0x34DC (0x5C250D): 0.4733 good, 0 evil (no halo)
	glm::vec3 haloOffset {0.0f}; ///< +0x34E0 (0x5C251F): (0, -0.28, 0) good, 0 evil
	size_t bytesRead = 0;        ///< the segment's bytes consumed (the whole segment for both files)
	size_t segmentSize = 0;      ///< the "helpdude" segment's size

	[[nodiscard]] const CAnim* Clip(size_t index) const
	{
		return index < clips.size() && clips[index].has_value() ? &*clips[index] : nullptr;
	}
};

/// Reads a whole .hd file (the bytes of the file, "LiOnHeAd" header included).
bool LoadHelpDudeFile(const std::vector<uint8_t>& bytes, HelpDudeFile& out, std::string* error);

/// The rest skeleton of the embedded L3D0 (l3d::L3DFile from memory, its bones as L3DMesh builds them) for the CAnim
/// samplers. False if the mesh does not load or has no bones.
bool LoadRestSkeleton(const HelpDudeFile& file, RestSkeleton& out, std::string* error);

} // namespace openblack::help
