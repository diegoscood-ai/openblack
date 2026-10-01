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
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

// A .sad bank as LHaudiodllR registers it for the music engine (LHBankRegister 0x10002240 with inMemory = 0, which is
// what the game always passes: 0x426EEE, 0x6427C1...). Only the headers are read; the LHAudioWaveData block stays in the
// file, which is kept open, and each segment is read when it is needed (the music thread 0x1000EB40 streams them).
// In a music bank every sample is a segment "c:\windows\temp\sectNNNN.mpg" of MPEG-2 Layer II, 22050 Hz, 21 frames =
// 24192 samples (the last one shorter), contiguous and frame-aligned (dev\tmp_dis\audio\music.md §3.1).
// Sources: dev\tmp_dis\audio\music.md §2.3, §2.3.1, §3.1 and the LHaudiodllR disassembly cited at each field.

namespace openblack::audio
{

/// Samples per segment: the 0x5E80 of the marker clock 0x1000DB90 (0x1000DBF7) = 21 MPEG Layer II frames of 1152
inline constexpr int k_MusicSamplesPerSegment = 0x5E80;

/// Flags of the first segment (+0x244) that LHMusicPlay 0x1000DF60 applies over the play options
enum class MusicBankFlag : uint32_t
{
	Volume = 0x20,       ///< 0x1000E1D6: channel +0x2C = u16 @+0x25C (else 127, 0x1000E0C5)
	Loops = 0x40,        ///< 0x1000E1F1: channel +0x10 = i32 @+0x248 (else opts+0x18, 0x1000E13E); -1 = forever
	MinDistance = 0x80,  ///< 0x1000E31F: distance mapping min = f32 @+0x268 (else opts+0x2C)
	MaxDistance = 0x100, ///< 0x1000E310: distance mapping max = f32 @+0x26C (else opts+0x30)
	Scale = 0x200,       ///< 0x1000E32D: distance mapping scale = f32 @+0x270 (else opts+0x34)
};

/// One sample of the bank's LHAudioBankSampleTable: its bytes inside the LHAudioWaveData block
struct MusicSegment
{
	uint32_t offset; ///< +0x110, from the start of LHAudioWaveData
	uint32_t size;   ///< +0x10C
};

/// A node of the list built by 0x1000D9E0 from the "!<sample>=<label>!..." texts of the segment descriptions (+0x140)
struct MusicMarker
{
	int chunk;         ///< node +0xC: segment index + 1 (1-based, like the channel's chunk counters)
	int sample;        ///< node +0x8: atoi of the digits, sample inside that chunk
	std::string label; ///< node +0x10
};

/// Distance mapping of a 3D music channel, as passed to QSWaveMixSetDistanceMapping (0x1000E355)
struct MusicDistanceMapping
{
	float minDistance; ///< opts+0x2C
	float maxDistance; ///< opts+0x30
	float scale;       ///< opts+0x34
};

class MusicBank
{
public:
	/// Size of one LHAudioBankSampleTable record (n x 0x280 are malloc'ed in bank+0x14)
	static constexpr size_t k_RecordSize = 0x280;
	using Record = std::array<uint8_t, k_RecordSize>;

	/// LHBankRegister(path, 0) 0x10002240: nullptr where the original returns 0 (file missing, no LiOnHeAd, no
	/// LHFileSegmentBankInfo 0x10002368, no LHAudioWaveData, no LHAudioBankSampleTable). The early returns of the
	/// system itself (sys+8 or sys+4 null 0x1000226A/0x10002275, path null 0x10002284) belong to the caller.
	[[nodiscard]] static std::unique_ptr<MusicBank> Register(const std::filesystem::path& path);

	MusicBank(const MusicBank&) = delete;
	MusicBank& operator=(const MusicBank&) = delete;
	~MusicBank();

	[[nodiscard]] const std::filesystem::path& GetPath() const { return _path; }

	/// LHIsMusicBank 0x10002EE0: bank+4 = 3rd u32 of LHFileSegmentBankInfo (0x100023BD); non-zero = music
	[[nodiscard]] bool IsMusic() const { return _musicFlag != 0; }
	[[nodiscard]] uint32_t GetMusicFlag() const { return _musicFlag; }

	/// LHBankGetNumberOfSamples 0x10002E90: bank+8 = low u16 of the table's first u32 (the high one, bank+0xC, counts
	/// the atmos samples and is 0 in music banks)
	[[nodiscard]] size_t GetSegmentCount() const { return _segments.size(); }
	[[nodiscard]] const std::vector<MusicSegment>& GetSegments() const { return _segments; }
	[[nodiscard]] const Record& GetRecord(size_t index) const { return _records[index]; }
	/// Bytes of the LHAudioWaveData block (the segments must fit in it)
	[[nodiscard]] uint32_t GetWaveDataSize() const { return _waveDataSize; }

	/// LHBankGetMusicGroupId 0x10002F30: u16 @+0x118 of the first segment, -1 if not a music bank (0x10002F4A)
	[[nodiscard]] int GetGroupId() const;
	/// LHBankGetMusicMinDistance 0x10002F10: f32 @+0x268 of the first segment, -1 if not a music bank
	[[nodiscard]] float GetMinDistance() const;
	/// LHBankGetMusicMaxDistance 0x10002EF0: f32 @+0x26C of the first segment, -1 if not a music bank
	[[nodiscard]] float GetMaxDistance() const;

	/// u32 @+0x244 of the first segment (LHGetFirstBankSample 0x10002EB0), see MusicBankFlag
	[[nodiscard]] uint32_t GetFlags() const;
	[[nodiscard]] bool HasFlag(MusicBankFlag flag) const { return (GetFlags() & static_cast<uint32_t>(flag)) != 0; }
	/// u32 @+0x128 of the first segment: LHMusicPlay copies it to channel +0x50 (0x1000E210), the Hz of every chunk
	[[nodiscard]] uint32_t GetSampleRate() const;
	/// Channel +0x2C of LHMusicPlay: 127 (0x1000E0C5), or u32 @+0x25C & 0xFFFF with flag 0x20 (0x1000E1DB..0x1000E1ED)
	[[nodiscard]] int GetVolume() const;
	/// Channel +0x10 of LHMusicPlay: the requested loops (opts+0x18, 0x1000E13E), or i32 @+0x248 with flag 0x40
	/// (0x1000E200)
	[[nodiscard]] int GetLoops(int requestedLoops) const;
	/// Distance mapping of LHMusicPlay for a 3D channel: the requested one with +0x26C if 0x100, +0x268 if 0x80 and
	/// +0x270 if 0x200 (0x1000E30A..0x1000E338)
	[[nodiscard]] MusicDistanceMapping GetDistanceMapping(const MusicDistanceMapping& requested) const;

	/// 0x1000D9E0 (LHMusicPlay calls it when opts+0x48, the marker callback, is set): the markers in the order of its
	/// linked list. Segments are walked from the last to the first and each node is pushed at the head, so the list
	/// goes by segment and, inside a segment, in the reverse order of the text.
	[[nodiscard]] std::vector<MusicMarker> ParseMarkers() const;

	/// Read the bytes of one segment from the open file: LHAudioWaveData + offset, size bytes (music thread
	/// 0x1000EB40). Not thread-safe: the caller serialises the reads, like the original's single music thread.
	[[nodiscard]] bool ReadSegment(size_t index, std::vector<uint8_t>& out);

private:
	MusicBank() = default;

	[[nodiscard]] uint32_t FirstU32(size_t offset) const;
	[[nodiscard]] float FirstF32(size_t offset) const;

	std::filesystem::path _path;
	std::ifstream _file;          ///< bank+0x18, kept open because inMemory = 0 (0x10002792)
	uint32_t _musicFlag {0};      ///< bank+4
	uint64_t _waveDataOffset {0}; ///< file offset of the LHAudioWaveData data
	uint32_t _waveDataSize {0};
	std::vector<Record> _records; ///< bank+0x14
	std::vector<MusicSegment> _segments;
};

} // namespace openblack::audio
