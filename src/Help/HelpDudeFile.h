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

#include "Help/SpiritAnimClip.h"

/// Data\HelpSprite\MarkGood.Hd / MarkEvil.Hd: a "LiOnHeAd" file with one segment "helpdude", read when an advisor
/// spirit loads (good = dude 0, evil = dude 1). The field names follow the spirit fields they land in; the order below
/// is the read order.
namespace openblack::help
{

/// One sound of an anim (played when the anim's phase crosses `phase`): 12 bytes
struct HelpDudeAnimEvent
{
	/// The sound's id in the in-game bank's anim effect rows: 155 the knock on the screen (the bank's knock on glass),
	/// 154 the hand gun (its gun shot)
	uint32_t sample = 0;
	uint32_t flag = 0; ///< Set: the sound plays; clear: it is stopped. 1 in every entry of both files
	float phase = 0.0f;
};

/// The events of one anim, and the loop window [start, end] of anim mode 4
struct HelpDudeAnimEvents
{
	std::vector<HelpDudeAnimEvent> events;
	float loopStart = 0.0f;
	float loopEnd = 0.0f;
};

/// The face record an advisor starts with, until its face is first updated: blinks every 5 to 10 seconds lasting half a
/// second, eyes and pupils at their natural size, the lids open and the base pose at its own pace
constexpr std::array<float, 16> k_DefaultFaceRecord {5.0f, 10.0f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f,
                                                     0.0f, 0.0f,  0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f};

struct HelpDudeFile
{
	static constexpr size_t k_AnimSlots = 80; ///< the slots of the names, flags and events

	uint32_t boneCount = 0;             ///< replaced by the mesh's bone count once it loads. 97 good, 73 evil
	uint32_t animCount = 0;             ///< 80: the names read
	float restHeight = 0.0f;            ///< 63.423 / 79.154, the max - min y of the rest skeleton, which setting the anim
	                                    ///< transform later writes again
	std::string meshName;               ///< char[0x80]: "DATA\Yogi_Mesh.l3d" / "C:\dev\TESTBED\DATA\Demon_Mesh.l3d"
	std::vector<std::string> animNames; ///< animCount x char[0x80]; an empty name has no clip
	uint32_t hasData = 0;               ///< 0 skips the mesh and the clips
	std::vector<uint8_t> mesh;          ///< u32 size then a plain "L3D0" mesh
	uint32_t clipCount = 0;             ///< 80
	/// One slot per index below clipCount; for a non-empty name a u32 record size (0 = no clip), then the clip binary
	/// (ReadSpiritAnimClip). The original only tests the size against 0.
	std::vector<std::optional<SpiritAnimClip>> clips;
	/// the record size read per slot (0 when the name is empty): SpiritAnimClipByteSize + 4, the size counting itself
	std::vector<uint32_t> clipRecordSizes;
	/// Seven u32: the first eye's three bones (0 for none), the second eye's three, then the head
	std::array<uint8_t, 0x1C> faceBoneIndices {};
	uint32_t unusedWord = 0;   ///< 1 in both; nothing reads it
	float pupilCentreU = 0.0f; ///< where the pupils' texture is centred
	float pupilCentreV = 0.0f;
	float pupilScale = 0.0f;   ///< how far the pupils' texture moves with the face
	uint32_t startEmotion = 0; ///< the u32 starting emotion; 0 in both
	/// u32 n then n bytes (n = 0x200 in both): the 8 face records of 0x40 bytes (16 floats), one per emotion, indexed by
	/// the emotion. For n != 0x200 the original reads each byte over the same place; here they are kept in order.
	std::vector<uint8_t> emotionFaces;
	float nearDepth = 0.0f; ///< 8.7333 both
	/// 0.02552 / 0.02683, the model scale S: the rows, the partner zone, the bank lean and the trail; the halo's size is
	/// S x haloScale x the halo size factor x 100
	float modelScale = 0.0f;
	float value35C0 = 0.0f;                        ///< (pending), 0 in both
	std::vector<HelpDudeAnimEvents> animEvents;    ///< u32 count then the entries (80 in both)
	float value35C4 = 0.0f;                        ///< (pending): 0.3467 / 0
	float value35C8 = 0.0f;                        ///< (pending): 0 / 0.3067
	float farDepth = 0.0f;                         ///< 6.0267 / 5.64 (x1.2 when the spirit is set up)
	std::array<uint8_t, k_AnimSlots> animFlags {}; ///< &0x20 = skip while clinging
	float haloScale = 0.0f;                        ///< 0.4733 good, 0 evil (no halo)
	glm::vec3 haloOffset {0.0f};                   ///< (0, -0.28, 0) good, 0 evil
	size_t bytesRead = 0;                          ///< the segment's bytes consumed (the whole segment for both files)
	size_t segmentSize = 0;                        ///< the "helpdude" segment's size

	[[nodiscard]] const SpiritAnimClip* Clip(size_t index) const
	{
		return index < clips.size() && clips[index].has_value() ? &*clips[index] : nullptr;
	}
};

/// Reads a whole .hd file (the bytes of the file, "LiOnHeAd" header included).
bool LoadHelpDudeFile(const std::vector<uint8_t>& bytes, HelpDudeFile& out, std::string* error);

/// The rest skeleton of the embedded L3D0 (l3d::L3DFile from memory, its bones as L3DMesh builds them) for the clip
/// samplers. False if the mesh does not load or has no bones.
bool LoadRestSkeleton(const HelpDudeFile& file, RestSkeleton& out, std::string* error);

} // namespace openblack::help
