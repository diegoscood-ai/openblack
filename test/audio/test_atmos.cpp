/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The scenarios in audio/scenarios were produced by running the original BW1 code (runblack.exe and LHaudiodllR.dll)
// under an x86 emulator on synthetic inputs (audio/scripts). These tests check our sound map, the atmos banks' fade,
// the audio's alignment value, the sky type, the audio library's random numbers and its ambience mixer against them,
// bit for bit. The scenarios and the scripts are taken from raffclar's tree as he wrote them; the tests are his,
// pointed at our code. The scheduler scenarios' LHaudiodllR.dll is the game's own (the same file in 1.20 and in the
// frozen v1.42 game), so they are the original's mixer as it is. The volume scenarios' sound map, alignment and sky
// type results were recorded again on the game's own v1.42 executable, run natively at its FPU control word
// (scripts/oracle_volumes.py): the emulator kept every intermediate at full precision, where the game rounds each step
// to a float.

#include <cstdint>
#include <cstring>

#include <algorithm>
#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <LNDFile.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <json.hpp>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/SkyType.h"
#include "Audio/Audio.h"
#include "Audio/Device/Sound.h"
#include "Audio/Engine/SamplePlay.h"
#include "Audio/GameQueries.h"
#include "Audio/Services/AtmosBanks.h"
#include "Audio/Services/AtmosVoicesInterface.h"
#include "Audio/SoundMap.h"
#include "Camera/Camera.h"
#include "ECS/AudioQueries.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::audio;
using json = nlohmann::json;

namespace
{
const auto k_ScenarioPath = std::filesystem::path(TEST_BINARY_DIR) / "audio" / "scenarios";

json LoadScenario(const std::string& name)
{
	std::ifstream stream(k_ScenarioPath / name);
	EXPECT_TRUE(stream.is_open()) << name;
	return json::parse(stream);
}

std::vector<uint8_t> FromHex(const std::string& hex)
{
	std::vector<uint8_t> bytes(hex.size() / 2);
	for (size_t i = 0; i < bytes.size(); ++i)
	{
		bytes[i] = static_cast<uint8_t>(std::stoul(hex.substr(i * 2, 2), nullptr, 16));
	}
	return bytes;
}

float FloatFromBits(const json& value)
{
	return std::bit_cast<float>(value.get<uint32_t>());
}

/// Only what the sound map reads from a landscape: the block lookup (32 x 32 blocks of 16 x 16 cells, 0 = no block),
/// the blocks' 17 x 17 cells (with the shared border row) and the land height between them
class SyntheticIsland final: public LandIslandInterface
{
public:
	SyntheticIsland(std::vector<uint8_t> lookup, std::vector<std::vector<lnd::LNDCell>> blocks)
	    : _lookup(std::move(lookup))
	    , _blocks(std::move(blocks))
	{
	}

	[[nodiscard]] bool HasBlockAt(const glm::u16vec2& cell) const final { return BlockOf(cell) != 0; }

	[[nodiscard]] const lnd::LNDCell& GetCell(const glm::u16vec2& cell) const final
	{
		static const lnd::LNDCell k_Empty {};
		const auto* found = FindCell(cell);
		return found != nullptr ? *found : k_Empty;
	}

	/// The same integer arithmetic as LandIsland::HeightAt with the sea flattening on (what GetHeightAt gives): the
	/// point's MapCoords, the cell's four corners from its own block (+1 = z + 1, +17 = x + 1), the heights of 3 or
	/// less at 0 next to the sea (base corner 4 or less), the fourth corner extrapolated along the cell's split, and
	/// the bilinear blend on 8-bit fractions
	[[nodiscard]] float GetHeightAt(glm::vec2 point) const final
	{
		const int32_t fixedX = map_coords::ToFixed(point.x);
		const int32_t fixedZ = map_coords::ToFixed(point.y);
		const int32_t cellX = map_coords::SignedCellOf(fixedX);
		const int32_t cellZ = map_coords::SignedCellOf(fixedZ);
		if (cellX < 0 || cellZ < 0 || cellX >= GetCellsPerSide() || cellZ >= GetCellsPerSide())
		{
			return 0.0f;
		}
		const auto* base = FindCell(glm::u16vec2(cellX, cellZ));
		if (base == nullptr)
		{
			return 0.0f;
		}
		const auto fracX = static_cast<uint32_t>(fixedX) % 65536u;
		const auto fracZ = static_cast<uint32_t>(fixedZ) % 65536u;
		int64_t v00 = GetCellAltitude(base[0]);
		int64_t v01 = GetCellAltitude(base[1]);
		int64_t v10 = GetCellAltitude(base[17]);
		int64_t v11 = GetCellAltitude(base[18]);
		if (v00 <= 4)
		{
			const auto sea = [](int64_t v) { return v > 3 ? v : int64_t {0}; };
			v00 = sea(v00);
			v01 = sea(v01);
			v10 = sea(v10);
			v11 = sea(v11);
		}
		int64_t c00 = v00;
		int64_t c01 = v01;
		int64_t c10 = v10;
		int64_t c11 = v11;
		if (base[0].properties.split)
		{
			if (fracZ > 65535u - fracX)
			{
				c00 = v10 + v01 - v11;
			}
			else
			{
				c11 = v10 + v01 - v00;
			}
		}
		else if (fracX > fracZ)
		{
			c01 = v00 + v11 - v10;
		}
		else
		{
			c10 = v00 + v11 - v01;
		}
		const int64_t fx = fracX >> 8;
		const int64_t fz = fracZ >> 8;
		const int64_t atX1 = (c11 - c10) * fz + (c10 << 8);
		const int64_t atX0 = (c01 - c00) * fz + (c00 << 8);
		const int64_t height = (((atX1 - atX0) * fx) >> 8) + atX0;
		return static_cast<float>(height) * LandIslandInterface::k_HeightUnit * (1.0f / 256.0f);
	}

	[[nodiscard]] float GetUnflattenedHeightAt(glm::vec2) const final { throw std::logic_error("unused"); }
	[[nodiscard]] glm::vec3 GetNormalAt(glm::vec2) const final { throw std::logic_error("unused"); }
	void DumpTextures() const final {}
	void DumpMaps() const final {}
	[[nodiscard]] std::vector<LandBlock>& GetBlocks() final { throw std::logic_error("unused"); }
	[[nodiscard]] const std::vector<LandBlock>& GetBlocks() const final { throw std::logic_error("unused"); }
	[[nodiscard]] const std::vector<lnd::LNDCountry>& GetCountries() const final { throw std::logic_error("unused"); }
	[[nodiscard]] const graphics::Texture2D& GetAlbedoArray() const final { throw std::logic_error("unused"); }
	[[nodiscard]] const graphics::Texture2D& GetBump() const final { throw std::logic_error("unused"); }
	[[nodiscard]] const graphics::Texture2D& GetSmallBump() const final { throw std::logic_error("unused"); }
	[[nodiscard]] const graphics::Texture2D& GetHeightMap() const final { throw std::logic_error("unused"); }
	[[nodiscard]] const graphics::Texture2D& GetCellMap() const final { throw std::logic_error("unused"); }
	[[nodiscard]] U16Extent2 GetIndexExtent() const final { throw std::logic_error("unused"); }
	[[nodiscard]] Extent2 GetExtent() const final { throw std::logic_error("unused"); }
	uint8_t GetNoise(glm::u8vec2) final { throw std::logic_error("unused"); }

private:
	[[nodiscard]] uint8_t BlockOf(const glm::u16vec2& cell) const
	{
		if (cell.x > 511 || cell.y > 511)
		{
			return 0;
		}
		return _lookup.at(((cell.x >> 4) << 5) | (cell.y >> 4));
	}

	[[nodiscard]] const lnd::LNDCell* FindCell(const glm::u16vec2& cell) const
	{
		const auto block = BlockOf(cell);
		if (block == 0)
		{
			return nullptr;
		}
		return &_blocks.at(block - 1).at(((cell.x % 16) * 17) + (cell.y % 16));
	}

	std::vector<uint8_t> _lookup;
	std::vector<std::vector<lnd::LNDCell>> _blocks;
};

/// The sound map's part of a volume scenario: the synthetic island, the camera and the camera's weather go in
/// through the Locator and the audio's queries, sound_map::Update runs once per query
void RunSoundMapQueries(const json& scenario)
{
	const auto& expected = scenario.at("expected");

	std::vector<std::vector<lnd::LNDCell>> blocks;
	for (const auto& hex : scenario.at("blocks"))
	{
		const auto bytes = FromHex(hex.get<std::string>());
		auto& cells = blocks.emplace_back(bytes.size() / sizeof(lnd::LNDCell));
		std::memcpy(cells.data(), bytes.data(), bytes.size());
	}

	std::array<float, 15> infoValues {};
	for (size_t i = 0; i < infoValues.size(); ++i)
	{
		infoValues.at(i) = FloatFromBits(scenario.at("soundInfo").at(i));
	}
	auto info = std::make_unique<InfoConstants>();
	static_assert(sizeof(info->sound) == sizeof(infoValues));
	std::memcpy(&info->sound, infoValues.data(), sizeof(info->sound));

	const test::RestoreService<Locator::terrainSystem> restoreTerrain;
	const test::RestoreService<Locator::camera> restoreCamera;
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	Locator::terrainSystem::emplace<SyntheticIsland>(FromHex(scenario.at("lookup").get<std::string>()), std::move(blocks));
	auto& camera = Locator::camera::emplace(glm::vec3(0.0f));
	Locator::infoConstants::reset(info.release());

	// The weather of the query at the camera
	CameraWeatherInfo weather {};
	GameQueries queries;
	queries.weatherSmooth = [&weather](glm::vec3) { return weather; };
	audio::Init(std::move(queries));

	const auto& queriesJson = scenario.at("queries");
	for (size_t q = 0; q < queriesJson.size(); ++q)
	{
		const auto& query = queriesJson[q];
		const auto& rainSnowWind = query.at("weather");
		weather = {
		    .rain = rainSnowWind.at(0).get<int8_t>(),
		    .snow = rainSnowWind.at(1).get<int8_t>(),
		    .windX = rainSnowWind.at(2).get<int8_t>(),
		    .windZ = rainSnowWind.at(3).get<int8_t>(),
		};
		camera.SetOrigin({FloatFromBits(query.at("x")), FloatFromBits(query.at("y")), FloatFromBits(query.at("z"))});
		sound_map::Update(FloatFromBits(query.at("sky")));

		// The height above the land is not kept outside the sound map: the stratosphere volume shows it
		const auto& result = expected.at("soundMap").at(q);
		for (size_t type = 0; type < k_AtmosTypeCount; ++type)
		{
			EXPECT_EQ(std::bit_cast<uint32_t>(sound_map::GetVolumes().at(type)), result.at("volumes").at(type).get<uint32_t>())
			    << "query " << q << " type " << k_AtmosTypes.at(type).name;
		}
		if (::testing::Test::HasFailure())
		{
			break;
		}
	}

	audio::Shutdown();
	sample_play::SetBackend({});
}

/// The players' update hands the audio clamp((alignment + 1) / 2, 0, 1), in float steps
/// (ecs::effects::alignment::InterfaceAlignmentAt); the audio's alignment value is computed from that
float AlignmentValueOf(float alignment)
{
	const float x = std::clamp((alignment + 1.0f) * 0.5f, 0.0f, 1.0f);
	return ecs::audio_queries::AudioAlignmentValue(x);
}

void RunAlignments(const json& scenario)
{
	const auto& expected = scenario.at("expected");
	for (size_t i = 0; i < scenario.at("alignments").size(); ++i)
	{
		const auto alignment = FloatFromBits(scenario.at("alignments").at(i));
		ASSERT_EQ(std::bit_cast<uint32_t>(AlignmentValueOf(alignment)), expected.at("alignments").at(i).get<uint32_t>())
		    << "alignment " << alignment;
	}
}

void RunSkyTypes(const json& scenario)
{
	const auto& expected = scenario.at("expected");
	for (size_t i = 0; i < scenario.at("sky").size(); ++i)
	{
		const auto& sky = scenario.at("sky").at(i);
		// The scenario lists the times from full day down to full night; the thresholds run the other way
		const auto& times = sky.at("times");
		const sky_type::Thresholds thresholds {FloatFromBits(times.at(3)), FloatFromBits(times.at(2)),
		                                       FloatFromBits(times.at(1)), FloatFromBits(times.at(0))};
		ASSERT_EQ(std::bit_cast<uint32_t>(sky_type::At(FloatFromBits(sky.at("time")), thresholds)),
		          expected.at("sky").at(i).get<uint32_t>())
		    << "sky case " << i;
	}
}

void RunBankSteps(const json& scenario)
{
	const auto& expected = scenario.at("expected");
	for (size_t i = 0; i < scenario.at("bankSteps").size(); ++i)
	{
		const auto& step = scenario.at("bankSteps").at(i);
		const auto& result = expected.at("bankSteps").at(i);
		for (size_t bank = 0; bank < k_AtmosTypeCount; ++bank)
		{
			const auto [current, sent] = atmos_banks::StepBankVolume(FloatFromBits(step.at("current").at(bank)),
			                                                         FloatFromBits(step.at("targets").at(bank)));
			ASSERT_EQ(std::bit_cast<uint32_t>(current), result.at("current").at(bank).get<uint32_t>())
			    << "bank step " << i << " bank " << bank;
			ASSERT_EQ(sent, result.at("sent").at(bank).get<int32_t>()) << "bank step " << i << " bank " << bank;
		}
	}
}

/// The fields of a .sad sample record (0x280 bytes) that the library's mixer and its sample start read
struct SadRecord
{
	int32_t sample {0};                 ///< +0x104, the sample number
	uint16_t cloneGroup {0};            ///< +0x118
	uint16_t atmosGroup {0};            ///< +0x11A
	uint32_t flags {0x20};              ///< +0x244, the fields the record sets over the start's
	int32_t loops {0};                  ///< +0x248
	uint16_t volume {127};              ///< +0x25C
	uint32_t pitch {0};                 ///< +0x260
	uint32_t deviation {0};             ///< +0x264
	uint32_t minDistance {0x3F800000u}; ///< +0x268, the float's bits (1)
	uint32_t maxDistance {0x461C3C00u}; ///< +0x26C (9999)
	uint32_t scale {0x3E99999Au};       ///< +0x270 (0.3)
	uint16_t playMode {0};              ///< +0x274
	int32_t frequency {-1};             ///< +0x27C, the atmos frequency
};

template <typename T>
T ReadAt(const std::vector<uint8_t>& bytes, size_t offset)
{
	T value {};
	std::memcpy(&value, bytes.data() + offset, sizeof(T));
	return value;
}

SadRecord ParseSadRecord(const std::vector<uint8_t>& header)
{
	return {
	    .sample = ReadAt<int32_t>(header, 0x104),
	    .cloneGroup = ReadAt<uint16_t>(header, 0x118),
	    .atmosGroup = ReadAt<uint16_t>(header, 0x11A),
	    .flags = ReadAt<uint32_t>(header, 0x244),
	    .loops = ReadAt<int32_t>(header, 0x248),
	    .volume = ReadAt<uint16_t>(header, 0x25C),
	    .pitch = ReadAt<uint32_t>(header, 0x260),
	    .deviation = ReadAt<uint32_t>(header, 0x264),
	    .minDistance = ReadAt<uint32_t>(header, 0x268),
	    .maxDistance = ReadAt<uint32_t>(header, 0x26C),
	    .scale = ReadAt<uint32_t>(header, 0x270),
	    .playMode = ReadAt<uint16_t>(header, 0x274),
	    .frequency = ReadAt<int32_t>(header, 0x27C),
	};
}

/// The scenarios' model of the library's sample functions, as scripts/emu_scheduler.py stubs them around the real
/// mixer: every start takes a fresh voice; a loop (loops < 0) plays until it is stopped, any other start for
/// 1 + (7 x sample + 3 x bank) mod 13 turns. Play mode 2 gives back the first voice of the bank playing the sample or
/// its clone group; mode 3 stops the first playing the sample, else the first of its clone group, then starts. The
/// record's volume, loops, distances, scale, pitch and play mode replace the start's where its flags set them and the
/// caller's mask leaves them; the pitch deviation draws the library's generator, which the first start seeds with the
/// clock. It records every start, every change of a playing voice's volume and every stop of a playing voice
class ModelVoices final: public AtmosVoicesInterface
{
public:
	explicit ModelVoices(uint32_t clock)
	    : _clock(clock)
	{
	}

	void Add(entt::id_type sound, int32_t bank, const SadRecord& record) { _records.emplace(sound, Entry {bank, record}); }
	void SetTurn(int32_t turn) { _turn = turn; }
	[[nodiscard]] const std::vector<json>& Trace() const { return _trace; }

	Channel Play(const sample_play::Options& options) override
	{
		if (!_seeded)
		{
			sample_play::SeedRand(_clock);
			_seeded = true;
		}
		const auto& entry = _records.at(options.sound);
		const int32_t bank = entry.bank;
		const SadRecord& record = entry.record;
		const auto fromRecord = [&record, &options](uint32_t bit) {
			return (record.flags & bit) != 0 && (options.callerMask & bit) == 0;
		};
		const int mode = fromRecord(0x400) ? record.playMode : options.mode;
		const auto first = [this, bank, &record](bool sameSample, bool sameGroup) -> Voice* {
			for (auto& voice : _voices)
			{
				if (voice.bank != bank || !Active(voice))
				{
					continue;
				}
				if ((sameSample && voice.sample == record.sample) ||
				    (sameGroup && record.cloneGroup != 0 && voice.group == record.cloneGroup))
				{
					return &voice;
				}
			}
			return nullptr;
		};
		if (mode == 2)
		{
			if (auto* playing = first(true, true); playing != nullptr)
			{
				return HandleOf(*playing);
			}
		}
		else if (mode == 3)
		{
			auto* playing = first(true, false);
			if (playing == nullptr)
			{
				playing = first(false, true);
			}
			if (playing != nullptr)
			{
				_trace.push_back(json {{"turn", _turn}, {"event", "stop"}, {"voice", HandleOf(*playing)}});
				playing->on = false;
			}
		}

		const int volume = std::min(fromRecord(0x20) ? static_cast<int>(record.volume) : options.volume, 127);
		const int loops = fromRecord(0x40) ? record.loops : options.loops;
		const uint32_t minDistance = fromRecord(0x80) ? record.minDistance : std::bit_cast<uint32_t>(options.minDistance);
		const uint32_t maxDistance = fromRecord(0x100) ? record.maxDistance : std::bit_cast<uint32_t>(options.maxDistance);
		const uint32_t scale = fromRecord(0x200) ? record.scale : std::bit_cast<uint32_t>(options.scale);
		uint32_t pitch = fromRecord(0x1) ? record.pitch : static_cast<uint32_t>(options.pitch);
		pitch = pitch == 0 ? 100 : pitch;
		const uint32_t deviation = pitch * record.deviation / 100;
		pitch = pitch - deviation + static_cast<uint32_t>(sample_play::Rand()) * deviation * 2 / 0x7FFF;
		pitch = pitch == 0 ? 100 : pitch;

		_voices.push_back({
		    .bank = bank,
		    .sample = record.sample,
		    .group = record.cloneGroup,
		    .volume = volume,
		    .on = true,
		    .loop = loops < 0,
		    .end = _turn + 1 + (record.sample * 7 + bank * 3) % 13,
		});
		const Channel handle = HandleOf(_voices.back());
		_trace.push_back(json {
		    {"turn", _turn},
		    {"event", "play"},
		    {"voice", handle},
		    {"bank", bank},
		    {"sample", record.sample},
		    {"volume", volume},
		    {"pitch", pitch},
		    {"loop", loops},
		    {"positional", options.is3D},
		    {"x", std::bit_cast<uint32_t>(options.position.x)},
		    {"y", std::bit_cast<uint32_t>(options.position.y)},
		    {"z", std::bit_cast<uint32_t>(options.position.z)},
		    {"min", minDistance},
		    {"max", maxDistance},
		    {"scale", scale},
		});
		return handle;
	}

	[[nodiscard]] bool IsPlaying(Channel voice) const override
	{
		const auto* found = Find(voice);
		return found != nullptr && Active(*found);
	}

	[[nodiscard]] int Volume(Channel voice) const override
	{
		const auto* found = Find(voice);
		return found != nullptr ? found->volume : 0;
	}

	void SetVolume(Channel voice, int volume) override
	{
		auto* found = Find(voice);
		const int clamped = std::clamp(volume, 0, 127);
		if (found == nullptr || !Active(*found) || found->volume == clamped)
		{
			return;
		}
		found->volume = clamped;
		_trace.push_back(json {{"turn", _turn}, {"event", "volume"}, {"voice", voice}, {"volume", clamped}});
	}

	void Stop(Channel voice) override
	{
		auto* found = Find(voice);
		if (found == nullptr)
		{
			return;
		}
		if (Active(*found))
		{
			_trace.push_back(json {{"turn", _turn}, {"event", "stop"}, {"voice", voice}});
		}
		found->on = false;
	}

private:
	struct Entry
	{
		int32_t bank;
		SadRecord record;
	};
	struct Voice
	{
		int32_t bank;
		int32_t sample;
		uint16_t group;
		int volume;
		bool on;
		bool loop;
		int32_t end;
	};

	[[nodiscard]] bool Active(const Voice& voice) const { return voice.on && (voice.loop || _turn < voice.end); }
	[[nodiscard]] Channel HandleOf(const Voice& voice) const { return static_cast<Channel>(&voice - _voices.data()) + 1; }
	[[nodiscard]] const Voice* Find(Channel voice) const
	{
		return voice >= 1 && voice <= _voices.size() ? &_voices[voice - 1] : nullptr;
	}
	[[nodiscard]] Voice* Find(Channel voice) { return voice >= 1 && voice <= _voices.size() ? &_voices[voice - 1] : nullptr; }

	uint32_t _clock;
	bool _seeded {false};
	int32_t _turn {0};
	std::unordered_map<entt::id_type, Entry> _records;
	std::vector<Voice> _voices;
	std::vector<json> _trace;
};

/// A synthetic bank registered with the mixer as the library reads it (the record's volume whole when its flag 0x20 is
/// set, else 127) and given to the model; its samples are (bank + 1) << 16 | (index + 1)
void RegisterSynthetic(ModelVoices& voices, size_t bank, std::span<const SadRecord> records, uint32_t clock)
{
	std::vector<atmos_banks::Record> list;
	for (size_t i = 0; i < records.size(); ++i)
	{
		const auto& record = records[i];
		const auto sound = static_cast<entt::id_type>(((bank + 1) << 16) | (i + 1));
		voices.Add(sound, static_cast<int32_t>(bank), record);
		list.push_back({
		    .sound = sound,
		    .sample = record.sample,
		    .frequency = record.frequency,
		    .group = record.atmosGroup,
		    .volume = (record.flags & 0x20u) != 0 ? static_cast<int32_t>(record.volume) : 127,
		});
	}
	atmos_banks::RegisterBank(bank, list, clock);
}

/// One turn of a script: every bank's group and volume, then one pass, active or not
struct Step
{
	uint32_t group {1};
	std::vector<int32_t> volumes;
	bool active {true};
};

/// The scenarios' turns: the turn given to the model, each bank's group then volume, then the pass
void RunScript(ModelVoices& voices, const std::vector<Step>& script)
{
	for (size_t turn = 0; turn < script.size(); ++turn)
	{
		voices.SetTurn(static_cast<int32_t>(turn));
		const auto& step = script[turn];
		for (size_t bank = 0; bank < step.volumes.size(); ++bank)
		{
			atmos_banks::SetGroup(bank, step.group);
			atmos_banks::SetBankVolume(voices, bank, step.volumes[bank]);
		}
		atmos_banks::Process(voices, step.active);
	}
}

/// Seven turns of one bank at the bank volume 127 in group 1, then the volumes and groups given
std::vector<Step> AfterSevenTurns(std::initializer_list<std::pair<uint32_t, int32_t>> turns)
{
	std::vector<Step> script(7, Step {.group = 1, .volumes = {127}});
	for (const auto& [group, volume] : turns)
	{
		script.push_back({.group = group, .volumes = {volume}});
	}
	return script;
}

/// The model's trace against the original's, event by event, up to the first difference
void ExpectTrace(const std::vector<json>& trace, const json& expected)
{
	const size_t common = std::min(trace.size(), expected.size());
	for (size_t i = 0; i < common; ++i)
	{
		ASSERT_EQ(trace[i], expected[i]) << "event " << i;
	}
	EXPECT_EQ(trace.size(), expected.size());
}

/// A scheduler scenario, recorded on the game's own LHaudiodllR.dll (scripts/emu_scheduler.py): its synthetic banks,
/// those with atmos records registered in their order with the scenario's clock, then each turn every bank's group and
/// volume and one pass, on the scenarios' model of the sample start. The voices' trace is the original's, event by
/// event
void RunSchedulerScenario(const std::string& name)
{
	const auto scenario = LoadScenario(name);
	const auto clock = scenario.at("time").get<uint32_t>();
	sample_play::SetBackend({});
	ModelVoices voices(clock);
	const auto& banks = scenario.at("banks");
	ASSERT_LE(banks.size(), k_AtmosTypeCount);
	for (size_t bank = 0; bank < banks.size(); ++bank)
	{
		if (banks[bank].at("atmosCount").get<int>() == 0)
		{
			continue;
		}
		std::vector<SadRecord> records;
		for (const auto& hex : banks[bank].at("headers"))
		{
			records.push_back(ParseSadRecord(FromHex(hex.get<std::string>())));
		}
		RegisterSynthetic(voices, bank, records, clock);
	}
	std::vector<Step> script;
	for (const auto& step : scenario.at("script"))
	{
		script.push_back({
		    .group = step.at("group").get<uint32_t>(),
		    .volumes = step.at("volumes").get<std::vector<int32_t>>(),
		    .active = step.at("active").get<bool>(),
		});
	}
	RunScript(voices, script);
	ExpectTrace(voices.Trace(), scenario.at("expected"));
	sample_play::SetBackend({});
}
} // namespace

TEST(AtmosPlayer, MsvcRandomSequence)
{
	// srand(1) in the MSVC C runtime: the library's generator as at load
	sample_play::SetBackend({});
	EXPECT_EQ(sample_play::Rand(), 41);
	EXPECT_EQ(sample_play::Rand(), 18467);
	EXPECT_EQ(sample_play::Rand(), 6334);
	EXPECT_EQ(sample_play::Rand(), 26500);
	sample_play::SetBackend({});
}

TEST(AtmosPlayer, RegisteringABankDrawsTheEarlierOnesAgain)
{
	// The library's bank registration on the game's own DLL (run under the emulator with the clock 1000): a bank with
	// a loose sample of frequency 1, then a bank with one of frequency 50 and a loop. The first registration puts the
	// counter 20 turns before its sample, below 0, so that sample is drawn again from the wrapped counter; the second
	// puts the counter 20 turns before its own sample, and the first bank's, due at a wrapped time, stays behind it
	sample_play::SetBackend({});
	const std::array first {atmos_banks::Record {.sound = 11, .sample = 1, .frequency = 1}};
	const std::array second {atmos_banks::Record {.sound = 21, .sample = 1, .frequency = 50},
	                         atmos_banks::Record {.sound = 22, .sample = 2, .frequency = 0}};
	atmos_banks::RegisterBank(0, first, 1000);
	atmos_banks::RegisterBank(1, second, 1000);

	const auto view = atmos_banks::GetView();
	EXPECT_EQ(view.counter, 225u);
	ASSERT_EQ(view.queue.size(), 2u);
	EXPECT_EQ(view.queue[0].sample, 21u);
	EXPECT_EQ(view.queue[0].next, 245u);
	EXPECT_EQ(view.queue[1].sample, 11u);
	EXPECT_EQ(view.queue[1].next, 4294967288u);
	ASSERT_EQ(view.loops.size(), 1u);
	EXPECT_EQ(view.loops[0].sample, 22u);
	sample_play::SetBackend({});
}

TEST(AtmosPlayer, ABanksVolumeReachesItsPlayingLooseSamples)
{
	// The library's mixer on the game's own DLL (run under the emulator with the scenarios' model of a sample start,
	// the clock 1000): one loose sample of the group 1 with the volume 100 starts at turn 6. A new bank volume goes to it
	// at once (100 x 64 / 127), out of its group it fades by 5 from the volume it has, the bank's volume does not reach
	// it there, and back in its group the new volume does (100 x 100 / 127)
	sample_play::SetBackend({});
	ModelVoices voices(1000);
	const std::array records {SadRecord {.sample = 1, .atmosGroup = 1, .volume = 100, .frequency = 1}};
	RegisterSynthetic(voices, 0, records, 1000);
	RunScript(voices, AfterSevenTurns({{1, 64}, {2, 64}, {2, 127}, {1, 100}, {1, 100}}));
	ExpectTrace(voices.Trace(), json::parse(R"([
	    {"turn":6,"event":"play","voice":1,"bank":0,"sample":1,"volume":100,"pitch":100,"loop":0,"positional":true,
	     "x":3212836864,"y":1065353216,"z":0,"min":1065353216,"max":1176255488,"scale":1050253722},
	    {"turn":7,"event":"volume","voice":1,"volume":50},
	    {"turn":8,"event":"volume","voice":1,"volume":45},
	    {"turn":9,"event":"volume","voice":1,"volume":40},
	    {"turn":10,"event":"volume","voice":1,"volume":78}])"));
	sample_play::SetBackend({});
}

TEST(AtmosPlayer, ALooseStartGivenTheLoopsVoiceLeavesItTheLoops)
{
	// The library's mixer on the game's own DLL, as above: a loop of the clone group 1 (volume 20) and a loose sample of
	// the same clone group with the .sad's play mode 2. At turn 6 the loose start finds the loop playing and gives its
	// voice back; that voice stays the loop's, so the bank's new volume at turn 7 reaches it only through the loop (64 x
	// 20 / 127), not as a loose sample's (100 x 64 / 127)
	sample_play::SetBackend({});
	ModelVoices voices(1000);
	const std::array records {
	    SadRecord {.sample = 1, .cloneGroup = 1, .volume = 20, .frequency = 0},
	    SadRecord {.sample = 2, .cloneGroup = 1, .flags = 0x420, .volume = 100, .playMode = 2, .frequency = 1},
	};
	RegisterSynthetic(voices, 0, records, 1000);
	RunScript(voices, AfterSevenTurns({{1, 64}, {1, 64}}));
	ExpectTrace(voices.Trace(), json::parse(R"([
	    {"turn":0,"event":"play","voice":1,"bank":0,"sample":1,"volume":0,"pitch":100,"loop":-1,"positional":false,
	     "x":0,"y":0,"z":0,"min":1065353216,"max":1176255488,"scale":1050253722},
	    {"turn":0,"event":"volume","voice":1,"volume":5},
	    {"turn":1,"event":"volume","voice":1,"volume":10},
	    {"turn":2,"event":"volume","voice":1,"volume":15},
	    {"turn":3,"event":"volume","voice":1,"volume":20},
	    {"turn":7,"event":"volume","voice":1,"volume":10}])"));
	sample_play::SetBackend({});
}

TEST(AtmosPlayer, ASampleRecordGivesTheMixerItsWholeVolume)
{
	// The library's registration reads the record's volume as a whole u16 when its flag 0x20 is set (no clamp to 127)
	Sound sound;
	sound.id = 7;
	sound.atmosFrequency = 0;
	sound.atmosGroup = 2;
	sound.volume127 = 127;
	sound.atmosVolume = 140;
	const auto record = atmos_banks::RecordOf(99, sound);
	EXPECT_EQ(record.sound, 99u);
	EXPECT_EQ(record.sample, 7);
	EXPECT_EQ(record.frequency, 0);
	EXPECT_EQ(record.group, 2u);
	EXPECT_EQ(record.volume, 140);
}

TEST(AtmosPlayer, OnlyTheSampleNumberZeroIsLeftOut)
{
	// The library's registration on the game's own DLL (under the emulator): of two loops numbered -3 and 0, the
	// number is compared unsigned with 0 and the -3 registers. With no loose sample the mixer's turn counter stays 0
	// through five passes
	sample_play::SetBackend({});
	ModelVoices voices(1000);
	const std::array records {SadRecord {.sample = -3, .frequency = 0}, SadRecord {.sample = 0, .frequency = 0}};
	RegisterSynthetic(voices, 0, records, 1000);
	RunScript(voices, std::vector<Step>(5, Step {.group = 1, .volumes = {0}}));
	const auto view = atmos_banks::GetView();
	ASSERT_EQ(view.loops.size(), 1u);
	EXPECT_EQ(view.loops[0].sample, (1u << 16) | 1u);
	EXPECT_TRUE(view.queue.empty());
	EXPECT_EQ(view.counter, 0u);
	EXPECT_TRUE(voices.Trace().empty());
	sample_play::SetBackend({});
}

TEST(AtmosPlayer, MatchesOriginalScheduler1)
{
	RunSchedulerScenario("scheduler_1.json");
}

TEST(AtmosPlayer, MatchesOriginalScheduler2)
{
	RunSchedulerScenario("scheduler_2.json");
}

TEST(AtmosPlayer, MatchesOriginalScheduler3)
{
	RunSchedulerScenario("scheduler_3.json");
}

// The sound map, the alignment values and the sky types as the game's executable gives them at its 24-bit FPU
// precision (docs/wiki/engine/floating-point.md#the-games-fpu-runs-at-24-bits); the emulator's first recording, at full
// precision, differed in the last bits in 244 and 268 of the 600 queries, 33 and 28 of the 206 alignments and 22 and 17
// of the 300 sky types. The bank steps are float in both and kept as first recorded.

TEST(AtmosAudio, MatchesOriginalSoundMap1)
{
	RunSoundMapQueries(LoadScenario("volumes_1.json"));
}

TEST(AtmosAudio, MatchesOriginalSoundMap2)
{
	RunSoundMapQueries(LoadScenario("volumes_2.json"));
}

TEST(AtmosAudio, MatchesOriginalAlignments1)
{
	RunAlignments(LoadScenario("volumes_1.json"));
}

TEST(AtmosAudio, MatchesOriginalAlignments2)
{
	RunAlignments(LoadScenario("volumes_2.json"));
}

TEST(AtmosAudio, MatchesOriginalSkyTypes1)
{
	RunSkyTypes(LoadScenario("volumes_1.json"));
}

TEST(AtmosAudio, MatchesOriginalSkyTypes2)
{
	RunSkyTypes(LoadScenario("volumes_2.json"));
}

TEST(AtmosAudio, MatchesOriginalBankSteps1)
{
	RunBankSteps(LoadScenario("volumes_1.json"));
}

TEST(AtmosAudio, MatchesOriginalBankSteps2)
{
	RunBankSteps(LoadScenario("volumes_2.json"));
}
