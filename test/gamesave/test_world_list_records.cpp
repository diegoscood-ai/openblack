/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstring>

#include <vector>

#include <SavedRecords.h>
#include <gtest/gtest.h>

// The records of the classes the game object's global lists meet, on default values: each record's length is the
// game's, and each reads back as it was written (docs/wiki/engine/saved-games.md#the-game-objects-global-lists).

using namespace openblack::gamesave;

namespace
{
constexpr StreamStart k_After100 {.objects = 100, .checksum = 0x27000};

template <typename T>
T At(const std::vector<uint8_t>& bytes, size_t offset)
{
	T value {};
	std::memcpy(&value, bytes.data() + offset, sizeof(T));
	return value;
}

std::vector<uint8_t> RoundTrip(const SavedRecord& record)
{
	StreamWriter writer(k_After100);
	Write(writer, record, nullptr);
	const auto bytes = writer.Take();
	StreamReader reader(bytes, k_After100);
	uint32_t type = 0;
	uint32_t extra = 0;
	reader.Get(type);
	reader.Get(extra);
	SavedRecord read;
	EXPECT_TRUE(ReadRecord(reader, type, extra, read, nullptr));
	EXPECT_EQ(reader.Position(), bytes.size());
	EXPECT_EQ(TypeOf(read), TypeOf(record));
	StreamWriter again(k_After100);
	Write(again, read, nullptr);
	EXPECT_EQ(again.Bytes(), bytes);
	return bytes;
}
} // namespace

TEST(SaveWorldListRecords, EachRecordHasTheGamesLength)
{
	EXPECT_EQ(RoundTrip(PotRecord {}).size(), 98u);
	EXPECT_EQ(RoundTrip(StreetLanternRecord {}).size(), 72u);
	EXPECT_EQ(RoundTrip(StreetLightRecord {}).size(), 31u);
	EXPECT_EQ(RoundTrip(PooRecord {}).size(), 85u);
	EXPECT_EQ(RoundTrip(AtomCoreRecord {}).size(), 276u);
	EXPECT_EQ(RoundTrip(ParticleSpriteRecord {}).size(), 37u);
	EXPECT_EQ(RoundTrip(BonfireRecord {}).size(), 112u);
	EXPECT_EQ(RoundTrip(FireFlyRecord {}).size(), 177u);
	EXPECT_EQ(RoundTrip(ParticlePointRecord {}).size(), 18u);
	EXPECT_EQ(RoundTrip(ScriptMarkerRecord {}).size(), 31u);
	EXPECT_EQ(RoundTrip(ArenaRecord {}).size(), 51u);
	EXPECT_EQ(RoundTrip(BeamSubCollectionRecord {}).size(), 41u);
	EXPECT_EQ(RoundTrip(EmitterCollectionRecord {}).size(), 34u);
	EXPECT_EQ(RoundTrip(MobileObjectRecord {}).size(), 85u);
	EXPECT_EQ(RoundTrip(HanoiBlockRecord {}).size(), 109u);
	EXPECT_EQ(RoundTrip(RewardRecord {}).size(), 137u);
	EXPECT_EQ(RoundTrip(ParticleChainJointRecord {}).size(), 34u);
	EXPECT_EQ(RoundTrip(MistRecord {}).size(), 43u);
	EXPECT_EQ(RoundTrip(StreamRecord {}).size(), 29u);
	EXPECT_EQ(RoundTrip(InfluenceRingRecord {}).size(), 47u);
	EXPECT_EQ(RoundTrip(WaypointRecord {}).size(), 31u);
	EXPECT_EQ(RoundTrip(FeatureRecord {}).size(), 84u);
	// a thing whose storm is over writes the word 0 and no storm; one with a storm its 112 bytes more
	EXPECT_EQ(RoundTrip(WeatherThingRecord {}).size(), 119u);
	EXPECT_EQ(RoundTrip(WeatherThingRecord {.storm = {.present = 1, .storm = WeatherStorm {}}}).size(), 231u);
	EXPECT_EQ(RoundTrip(AnimatedStaticRecord {}).size(), 92u);
	EXPECT_EQ(RoundTrip(ClimateRecord {}).size(), 135u);
	EXPECT_EQ(RoundTrip(SpellWaterRecord {}).size(), 229u);
	EXPECT_EQ(RoundTrip(SpellShieldRecord {}).size(), 241u);
	// every spell's 208 bytes after the part every object writes, then each class's own
	EXPECT_EQ(RoundTrip(SpellRecord {}).size(), 225u);
	EXPECT_EQ(RoundTrip(SpellStormAndTornadoRecord {}).size(), 225u);
	EXPECT_EQ(RoundTrip(SpellHealRecord {}).size(), 225u);
	EXPECT_EQ(RoundTrip(SpellResourceRecord {}).size(), 226u);
	EXPECT_EQ(RoundTrip(SpellWithObjectsRecord {}).size(), 229u);
	EXPECT_EQ(RoundTrip(SpellTeleportRecord {.objects = {.numbers = {7, 8}}}).size(), 237u);
	EXPECT_EQ(RoundTrip(SpellFlockGroundRecord {}).size(), 257u);
	EXPECT_EQ(RoundTrip(SpellFlockFlyingRecord {}).size(), 266u);
	EXPECT_EQ(RoundTrip(SpellForestRecord {}).size(), 234u);
	EXPECT_EQ(RoundTrip(SpellCreatureRecord {}).size(), 230u);
	EXPECT_EQ(RoundTrip(HandSprinkleCollectionRecord {}).size(), 30u);
	EXPECT_EQ(RoundTrip(PhysicalShieldRecord {}).size(), 225u);
	EXPECT_EQ(RoundTrip(LandscapeVortexInRecord {}).size(), 140u);
	EXPECT_EQ(RoundTrip(SoundAtomDataRecord {}).size(), 26u);
	EXPECT_EQ(RoundTrip(ParticleTexturedObjectRecord {}).size(), 42u);
	EXPECT_EQ(RoundTrip(ChainRecord {}).size(), 71u);
	EXPECT_EQ(RoundTrip(ParticleMistRecord {}).size(), 49u);
	EXPECT_EQ(RoundTrip(BeamAtomDataRecord {}).size(), 57u);
	EXPECT_EQ(RoundTrip(ParticleSoundRecord {}).size(), 54u);
	EXPECT_EQ(RoundTrip(TargetAtomsCollectionRecord {}).size(), 33u);
	EXPECT_EQ(RoundTrip(TrailCollectionRecord {}).size(), 34u);
	EXPECT_EQ(RoundTrip(PuzzleGameRecord {}).size(), 1118u);
	EXPECT_EQ(RoundTrip(VortexSaveRecord {}).size(), 30u);
	EXPECT_EQ(RoundTrip(VortexAttractAtomDataRecord {}).size(), 52u);
	EXPECT_EQ(RoundTrip(ParticleGameObjectRecord {}).size(), 79u);
}

TEST(SaveWorldListRecords, ASpellsValuesComeBeforeItsOwn)
{
	SpellShieldRecord shield;
	shield.spell.objects[3] = {40};
	shield.objects.numbers = {41};
	shield.others[1] = {42};
	const auto bytes = RoundTrip(shield);
	// The head, the spell's position and values, then the two lists and the two objects
	ASSERT_EQ(bytes.size(), 17u + 208 + 4 + 4 + 4 + 8);
	EXPECT_EQ(At<uint32_t>(bytes, 17 + 14 + 4 + 4 + 4 + 4 + 36 + 24 + 60 + 4 + 2 + 12), 40u);
	EXPECT_EQ(At<uint32_t>(bytes, 17 + 208), 1u);
	EXPECT_EQ(At<uint32_t>(bytes, bytes.size() - 4), 42u);
}

TEST(SaveWorldListRecords, AClimateWritesItsAreasWithTheirCount)
{
	ClimateRecord climate;
	climate.areas.elements.resize(2);
	climate.areas.elements[1].words[1] = 7;
	const auto bytes = RoundTrip(climate);
	ASSERT_EQ(bytes.size(), 17u + 12 + 32 + 16 + 8 + 12 + 4 + 1 + 4 + 2 * 112 + 28 + 1);
	EXPECT_EQ(At<uint32_t>(bytes, 17 + 85), 2u);
	EXPECT_EQ(At<uint32_t>(bytes, 17 + 89 + 112 + 12 + 80 + 4), 7u);
}
