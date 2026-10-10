/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The scenarios in audio/scenarios were produced by running the original BW1 v1.20 code (runblack.exe and
// LHaudiodllR.dll) under an x86 emulator on synthetic inputs (audio/scripts). These tests check our sound map, the
// atmos banks' fade, the audio's alignment value, the sky type and the audio library's random numbers against them,
// bit for bit. The scenarios and the scripts are taken from raffclar's tree as he wrote them; the tests are his,
// pointed at our code.

#include <cstdint>
#include <cstring>

#include <algorithm>
#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
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
#include "Audio/Engine/SamplePlay.h"
#include "Audio/GameQueries.h"
#include "Audio/Services/AtmosBanks.h"
#include "Audio/Services/SoundMap.h"
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
	[[nodiscard]] const graphics::FrameBuffer& GetFootprintFramebuffer() const final { throw std::logic_error("unused"); }
	[[nodiscard]] const graphics::FrameBuffer& GetStaticShadowFramebuffer() const final { throw std::logic_error("unused"); }
	[[nodiscard]] const graphics::FrameBuffer& GetLandAlphaFramebuffer() const final { throw std::logic_error("unused"); }
	[[nodiscard]] U16Extent2 GetIndexExtent() const final { throw std::logic_error("unused"); }
	[[nodiscard]] glm::mat4 GetOrthoView() const final { throw std::logic_error("unused"); }
	[[nodiscard]] glm::mat4 GetOrthoProj() const final { throw std::logic_error("unused"); }
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

/// The scheduler scenarios replay the library's atmos mixer: each turn they set every bank's volume and group, then
/// run one pass. Our mixer (atmos_banks) cannot be driven that way, so these stay disabled:
///  - its banks' volumes are not set from outside: each turn they fade towards the sound map's volumes (by 0.02 or
///    0.04 x 127 at most), and the scenarios jump them anywhere in 0..127;
///  - the library's generator is seeded with time(0) at each bank registration, where the scenarios give a fixed
///    clock.
/// Running them needs our mixer split from the banks' fade, behind its own voice output.
void RunSchedulerScenario(const std::string& name)
{
	const auto scenario = LoadScenario(name);
	ASSERT_FALSE(scenario.at("banks").empty());
	ASSERT_FALSE(scenario.at("script").empty());
	ADD_FAILURE() << name << ": our atmos mixer cannot replay a scheduler scenario yet (see RunSchedulerScenario)";
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

TEST(AtmosPlayer, DISABLED_MatchesOriginalScheduler1)
{
	RunSchedulerScenario("scheduler_1.json");
}

TEST(AtmosPlayer, DISABLED_MatchesOriginalScheduler2)
{
	RunSchedulerScenario("scheduler_2.json");
}

TEST(AtmosPlayer, DISABLED_MatchesOriginalScheduler3)
{
	RunSchedulerScenario("scheduler_3.json");
}

// Disabled: the precision. The scenarios' sound map volumes, alignment values and sky types are those of a model
// that keeps double intermediates (raffclar's port matches them bit for bit), and the scripts do not give the emulated
// code the game's control word. Ours rounds every step to a float, by the 24-bit FPU rule of docs/bw1-notes/audio.md,
// and differs in the last bits: the sound map in 244 of the 600 queries of volumes_1 and 268 of volumes_2 (the
// distances, the altitude and the fades), 33 and 28 of the 206 alignment values ((alignment + 1) / 2 in float) and 22
// and 17 of the 300 sky types. The bank steps are float in both and match.

TEST(AtmosAudio, DISABLED_MatchesOriginalSoundMap1)
{
	RunSoundMapQueries(LoadScenario("volumes_1.json"));
}

TEST(AtmosAudio, DISABLED_MatchesOriginalSoundMap2)
{
	RunSoundMapQueries(LoadScenario("volumes_2.json"));
}

TEST(AtmosAudio, DISABLED_MatchesOriginalAlignments1)
{
	RunAlignments(LoadScenario("volumes_1.json"));
}

TEST(AtmosAudio, DISABLED_MatchesOriginalAlignments2)
{
	RunAlignments(LoadScenario("volumes_2.json"));
}

TEST(AtmosAudio, DISABLED_MatchesOriginalSkyTypes1)
{
	RunSkyTypes(LoadScenario("volumes_1.json"));
}

TEST(AtmosAudio, DISABLED_MatchesOriginalSkyTypes2)
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
