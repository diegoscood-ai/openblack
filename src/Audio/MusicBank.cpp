/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MusicBank.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <map>
#include <string_view>

#include <spdlog/spdlog.h>

namespace openblack::audio
{

namespace
{
// LiOnHeAd file: 8 byte magic, then blocks { char name[32]; u32 size; u8 data[size]; } (engine.md §1.5; the same layout
// that pack::PackFile reads)
constexpr std::array<char, 8> k_Magic = {'L', 'i', 'O', 'n', 'H', 'e', 'A', 'd'};
constexpr size_t k_BlockNameSize = 32;

struct BlockSpan
{
	uint64_t offset;
	uint32_t size;
};

// Record offsets used here (dev\tmp_dis\audio\music.md §3.1; engine.md §1.5)
constexpr size_t k_RecSize = 0x10C;        // segment bytes
constexpr size_t k_RecOffset = 0x110;      // segment offset in LHAudioWaveData
constexpr size_t k_RecGroup = 0x118;       // music group (u16, first segment only)
constexpr size_t k_RecRate = 0x128;        // Hz
constexpr size_t k_RecDesc = 0x140;        // description: the markers
constexpr size_t k_RecDescSize = 0x100;    // up to the priority at +0x240
constexpr size_t k_RecFlags = 0x244;       // flags (first segment only)
constexpr size_t k_RecLoops = 0x248;       // loops
constexpr size_t k_RecVolume = 0x25C;      // volume
constexpr size_t k_RecMinDistance = 0x268; // 3D min distance
constexpr size_t k_RecMaxDistance = 0x26C; // 3D max distance
constexpr size_t k_RecScale = 0x270;       // 3D scale

// 0x10002F05 / 0x10002F25: fld [0x10030304] = -1.0f
constexpr float k_NoDistance = -1.0f;

uint32_t ReadU32(const uint8_t* p)
{
	uint32_t v;
	std::memcpy(&v, p, sizeof(v));
	return v;
}

// The "audio" logger exists in the game, not in the tests
void LogRegisterFailure(std::string_view format, const std::string& path)
{
	if (auto logger = spdlog::get("audio"))
	{
		SPDLOG_LOGGER_WARN(logger, fmt::runtime(format), path);
	}
}
} // namespace

MusicBank::~MusicBank() = default;

std::unique_ptr<MusicBank> MusicBank::Register(const std::filesystem::path& path)
{
	// new LH_AudioBank and LHReleasedFile::Open(path, 2) (0x1000228A..0x100022FE): a failure logs and returns 0
	auto bank = std::unique_ptr<MusicBank>(new MusicBank());
	bank->_path = path;
	bank->_file.open(path, std::ios::binary);
	if (!bank->_file.is_open())
	{
		LogRegisterFailure("LHBankRegister: cannot open {}", path.string());
		return nullptr;
	}

	auto& file = bank->_file;
	std::array<char, 8> magic {};
	if (!file.read(magic.data(), magic.size()) || magic != k_Magic)
	{
		LogRegisterFailure("LHBankRegister: {} is not a LiOnHeAd file", path.string());
		return nullptr;
	}

	// The DLL looks each block up by name (fn_100183A0); walking them once gives the same spans. Stopping at a truncated
	// block is defensive (not read in fn_100183A0).
	std::map<std::string, BlockSpan, std::less<>> blocks;
	file.seekg(0, std::ios::end);
	const auto fileSize = static_cast<uint64_t>(file.tellg());
	uint64_t pos = magic.size();
	while (pos + k_BlockNameSize + sizeof(uint32_t) <= fileSize)
	{
		std::array<char, k_BlockNameSize> name {};
		uint32_t size = 0;
		file.seekg(static_cast<std::streamoff>(pos));
		file.read(name.data(), name.size());
		file.read(reinterpret_cast<char*>(&size), sizeof(size));
		if (!file)
		{
			break;
		}
		const auto dataOffset = pos + name.size() + sizeof(size);
		if (dataOffset + size > fileSize)
		{
			break;
		}
		const auto length = std::find(name.begin(), name.end(), '\0') - name.begin();
		blocks.emplace(std::string(name.data(), static_cast<size_t>(length)), BlockSpan {dataOffset, size});
		pos = dataOffset + size;
	}
	file.clear();

	const auto findBlock = [&blocks](std::string_view name) -> const BlockSpan* {
		const auto it = blocks.find(name);
		return it != blocks.end() ? &it->second : nullptr;
	};

	// LHFileSegmentBankInfo: 3 u32, bank+4 = the 3rd (0x10002357..0x100023BD); missing = error (0x10002368)
	const auto* info = findBlock("LHFileSegmentBankInfo");
	if (info == nullptr || info->size < 3 * sizeof(uint32_t))
	{
		LogRegisterFailure("LHBankRegister: {} has no LHFileSegmentBankInfo", path.string());
		return nullptr;
	}
	std::array<uint32_t, 3> infoWords {};
	file.seekg(static_cast<std::streamoff>(info->offset));
	file.read(reinterpret_cast<char*>(infoWords.data()), sizeof(infoWords));
	bank->_musicFlag = infoWords[2];

	// LHAudioWaveData (0x100023C0..): inMemory = 0, so bank+0 = 1 and the data is read from the file when used
	const auto* wave = findBlock("LHAudioWaveData");
	if (wave == nullptr)
	{
		LogRegisterFailure("LHBankRegister: {} has no LHAudioWaveData", path.string());
		return nullptr;
	}
	bank->_waveDataOffset = wave->offset;
	bank->_waveDataSize = wave->size;

	// LHAudioBankSampleTable: u32 w, n = w & 0xFFFF (bank+8), w >> 16 = atmos samples (bank+0xC), n x 0x280 (bank+0x14)
	const auto* table = findBlock("LHAudioBankSampleTable");
	if (table == nullptr || table->size < sizeof(uint32_t))
	{
		LogRegisterFailure("LHBankRegister: {} has no LHAudioBankSampleTable", path.string());
		return nullptr;
	}
	uint32_t countWord = 0;
	file.seekg(static_cast<std::streamoff>(table->offset));
	file.read(reinterpret_cast<char*>(&countWord), sizeof(countWord));
	const size_t count = countWord & 0xFFFF;
	// (defensive, not in the original: 0x100026AA..0x10002733 reads n x 0x280 without checking the block size)
	if (table->size < sizeof(uint32_t) + count * k_RecordSize)
	{
		LogRegisterFailure("LHBankRegister: {} has a short sample table", path.string());
		return nullptr;
	}
	bank->_records.resize(count);
	for (auto& record : bank->_records)
	{
		file.read(reinterpret_cast<char*>(record.data()), record.size());
	}
	if (!file)
	{
		LogRegisterFailure("LHBankRegister: cannot read the sample table of {}", path.string());
		return nullptr;
	}

	bank->_segments.reserve(count);
	for (const auto& record : bank->_records)
	{
		bank->_segments.push_back({ReadU32(&record[k_RecOffset]), ReadU32(&record[k_RecSize])});
	}

	// The music engine's group count (sys+0x40 = max(sys+0x40, u16 @+0x118), 0x100027AB..0x100027C7) is left to the
	// caller: it is the maximum GetGroupId() of the registered music banks.
	return bank;
}

uint32_t MusicBank::FirstU32(size_t offset) const
{
	return ReadU32(&_records.front()[offset]);
}

float MusicBank::FirstF32(size_t offset) const
{
	float v;
	std::memcpy(&v, &_records.front()[offset], sizeof(v));
	return v;
}

int MusicBank::GetGroupId() const
{
	// 0x10002F30: bank+0x14 (first sample) and bank+4 (music) must be set; u32 @+0x118 & 0xFFFF
	if (_records.empty() || !IsMusic())
	{
		return -1;
	}
	return static_cast<int>(FirstU32(k_RecGroup) & 0xFFFF);
}

float MusicBank::GetMinDistance() const
{
	// 0x10002F10
	if (_records.empty() || !IsMusic())
	{
		return k_NoDistance;
	}
	return FirstF32(k_RecMinDistance);
}

float MusicBank::GetMaxDistance() const
{
	// 0x10002EF0
	if (_records.empty() || !IsMusic())
	{
		return k_NoDistance;
	}
	return FirstF32(k_RecMaxDistance);
}

uint32_t MusicBank::GetFlags() const
{
	// 0x1000E1CC (byte), 0x1000E30A (dword): flags of LHGetFirstBankSample 0x10002EB0 (bank+0x14)
	return _records.empty() ? 0 : FirstU32(k_RecFlags);
}

uint32_t MusicBank::GetSampleRate() const
{
	// 0x1000E210
	return _records.empty() ? 0 : FirstU32(k_RecRate);
}

int MusicBank::GetVolume() const
{
	// 0x1000E0C5: channel +0x2C = 127; 0x1000E1D6..0x1000E1ED: with 0x20, u32 @+0x25C & 0xFFFF
	constexpr int k_DefaultVolume = 0x7F;
	if (!HasFlag(MusicBankFlag::Volume))
	{
		return k_DefaultVolume;
	}
	return static_cast<int>(FirstU32(k_RecVolume) & 0xFFFF);
}

int MusicBank::GetLoops(int requestedLoops) const
{
	// 0x1000E13E: channel +0x10 = opts+0x18; 0x1000E1F1..0x1000E206: with 0x40, i32 @+0x248
	if (!HasFlag(MusicBankFlag::Loops))
	{
		return requestedLoops;
	}
	return static_cast<int32_t>(FirstU32(k_RecLoops));
}

MusicDistanceMapping MusicBank::GetDistanceMapping(const MusicDistanceMapping& requested) const
{
	// 0x1000E2D9..0x1000E338, in this order: 0x100 max, 0x80 min, 0x200 scale
	auto mapping = requested;
	if (HasFlag(MusicBankFlag::MaxDistance))
	{
		mapping.maxDistance = FirstF32(k_RecMaxDistance);
	}
	if (HasFlag(MusicBankFlag::MinDistance))
	{
		mapping.minDistance = FirstF32(k_RecMinDistance);
	}
	if (HasFlag(MusicBankFlag::Scale))
	{
		mapping.scale = FirstF32(k_RecScale);
	}
	return mapping;
}

std::vector<MusicMarker> MusicBank::ParseMarkers() const
{
	// 0x1000D9E0. For i = n-1 .. 0 (0x1000DA04, 0x1000DB04): desc = record +0x140; only if desc[0] == '!' (0x1000DA20):
	//   repeat: skip the '!', take the digits (isdigit 0x1001F64B, _DIGIT mask 4) -> sample = atoi (0x1001F47D);
	//           skip ONE character (the '=', 0x1000DA87), label = up to '\0' or '!' (0x1000DA92..0x1000DAA2);
	//           node {next = head, sample, chunk = i + 1, label} becomes the head (0x1000DAE5..0x1000DAED);
	//   while the label ended at a '!' (0x1000DAF0).
	// The node holds the label in 0x20 bytes (malloc 0x30, label at +0x10); the data has no longer label.
	std::vector<MusicMarker> reversed; // built in push order; the list is its reverse
	for (size_t i = _records.size(); i-- > 0;)
	{
		const auto* desc = reinterpret_cast<const char*>(&_records[i][k_RecDesc]);
		const auto descLength = strnlen(desc, k_RecDescSize);
		const std::string_view text(desc, descLength);
		if (text.empty() || text[0] != '!')
		{
			continue;
		}
		size_t cursor = 0; // at a '!'
		while (true)
		{
			const auto digitsBegin = cursor + 1;
			auto digitsEnd = digitsBegin;
			while (digitsEnd < text.size() && std::isdigit(static_cast<unsigned char>(text[digitsEnd])) != 0)
			{
				++digitsEnd;
			}
			const std::string digits(text.substr(digitsBegin, digitsEnd - digitsBegin));
			const int sample = std::atoi(digits.c_str());
			// The original skips the character after the digits even if it is the terminator, and would read past it
			// (not in the data); here the label is then empty (approximated).
			const auto labelBegin = std::min(digitsEnd + 1, text.size());
			auto labelEnd = labelBegin;
			while (labelEnd < text.size() && text[labelEnd] != '!')
			{
				++labelEnd;
			}
			reversed.push_back({static_cast<int>(i) + 1, sample, std::string(text.substr(labelBegin, labelEnd - labelBegin))});
			if (labelEnd >= text.size())
			{
				break;
			}
			cursor = labelEnd;
		}
	}
	return {reversed.rbegin(), reversed.rend()};
}

bool MusicBank::ReadSegment(size_t index, std::vector<uint8_t>& out)
{
	if (index >= _segments.size())
	{
		return false;
	}
	const auto& segment = _segments[index];
	// (defensive: these bounds checks are not taken from the music thread 0x1000EB40)
	if (static_cast<uint64_t>(segment.offset) + segment.size > _waveDataSize)
	{
		return false;
	}
	out.resize(segment.size);
	_file.clear();
	_file.seekg(static_cast<std::streamoff>(_waveDataOffset + segment.offset));
	_file.read(reinterpret_cast<char*>(out.data()), segment.size);
	return static_cast<bool>(_file);
}

} // namespace openblack::audio
