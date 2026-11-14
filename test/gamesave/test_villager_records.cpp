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

// The villagers' records, on synthetic values: the parts of a thing that walks round obstacles and of a living thing,
// the villager's own values, and the values its top and final states save by the exe's table of the states
// (docs/wiki/engine/saved-games.md#the-villagers).

using namespace openblack::gamesave;

namespace
{
constexpr StreamStart k_After100 {.objects = 100, .checksum = 0x27000};
/// Where the living part starts and where the states' values start in a villager with no circle and no reactions
constexpr size_t k_Living = 17 + 14 + 36 + 2 + 39;
constexpr size_t k_States = k_Living + 67 + 39;

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
	StreamWriter again(k_After100);
	Write(again, read, nullptr);
	EXPECT_EQ(again.Bytes(), bytes);
	return bytes;
}

VillagerRecord AVillager(uint8_t topState, uint8_t finalState, uint8_t disciple)
{
	VillagerRecord villager;
	villager.values.living.action = {topState, finalState, 0, 0, 0, 0};
	villager.values.villager.discipleType = disciple;
	villager.values.villager.home = {30};
	villager.values.villager.town = {14};
	for (auto [state, values] :
	     {std::pair {topState, &villager.values.topState}, std::pair {finalState, &villager.values.finalState}})
	{
		for (const auto size : StateValueSizes(VillagerStateSaveOf(state), disciple))
		{
			values->push_back(StateValue {.size = size, .bytes = {7}, .number = size == k_StateReference ? 40u : 0u});
		}
	}
	return villager;
}
} // namespace

TEST(SaveVillagerRecords, TheStatesSaveTheValuesTheExesTableNames)
{
	EXPECT_EQ(VillagerStateSaveOf(4), VillagerStateSave::InScript);
	EXPECT_EQ(VillagerStateSaveOf(13), VillagerStateSave::Dead);
	// Where the exe's table differs from the earlier version's: state 85 saves nothing, 90 a dance's values
	EXPECT_EQ(VillagerStateSaveOf(85), VillagerStateSave::None);
	EXPECT_EQ(VillagerStateSaveOf(90), VillagerStateSave::Dance);
	EXPECT_EQ(VillagerStateSaveOf(221), VillagerStateSave::DiscipleNothingToDo);
	EXPECT_EQ(VillagerStateSaveOf(254), VillagerStateSave::None);
	EXPECT_EQ(VillagerStateSaveOf(255), VillagerStateSave::None);
	EXPECT_EQ(StateValueSizes(VillagerStateSave::InScript, 0), (std::vector<uint8_t> {8, 4, 4, 4}));
	EXPECT_EQ(StateValueSizes(VillagerStateSave::Football, 0), (std::vector<uint8_t> {1, 1, 1, k_StateReference}));
	// Only a trader saves anything in its state of nothing to do
	EXPECT_TRUE(StateValueSizes(VillagerStateSave::DiscipleNothingToDo, 1).empty());
	EXPECT_EQ(StateValueSizes(VillagerStateSave::DiscipleNothingToDo, k_TraderDisciple),
	          (std::vector<uint8_t> {4, k_StateReference}));
}

TEST(SaveVillagerRecords, AVillagerWritesItsPartsItsValuesAndItsTopStatesValues)
{
	const auto bytes = RoundTrip(AVillager(4, 0, 1));
	// The object's parts, the moving thing's word, the walk's 39 bytes, the living part's 67, the villager's 39 and
	// the script state's four values
	ASSERT_EQ(bytes.size(), k_States + 20);
	EXPECT_EQ(bytes[k_Living + 8], 4);
	EXPECT_EQ(bytes[k_Living + 67 + 12], 1);
	EXPECT_EQ(At<uint32_t>(bytes, k_Living + 67 + 19), 30u);
	EXPECT_EQ(At<uint32_t>(bytes, k_Living + 67 + 23), 14u);
	EXPECT_EQ(bytes[k_States], 7);
	EXPECT_EQ(bytes[k_States + 8], 7);
}

TEST(SaveVillagerRecords, TheFinalStatesValuesFollowAndAReferenceIsANumber)
{
	// Top: building (an object and a word); final: dead (a byte)
	const auto bytes = RoundTrip(AVillager(39, 13, 0));
	ASSERT_EQ(bytes.size(), k_States + 4 + 4 + 1);
	EXPECT_EQ(At<uint32_t>(bytes, k_States), 40u);
	EXPECT_EQ(bytes[k_States + 4], 7);
	EXPECT_EQ(bytes[k_States + 8], 7);
}

TEST(SaveVillagerRecords, ATraderWithNothingToDoSavesWhatATraderSaves)
{
	EXPECT_EQ(RoundTrip(AVillager(221, 0, k_TraderDisciple)).size(), k_States + 8);
	EXPECT_EQ(RoundTrip(AVillager(221, 0, 1)).size(), k_States);
}

TEST(SaveVillagerRecords, TheWalkWritesItsCircleOnlyInTheCircleStatesAndItsTurnsWhenSteppingThrough)
{
	auto circling = AVillager(0, 0, 0);
	circling.values.move.moveState = k_FirstCircleState;
	circling.values.move.circle = CircleHug {.turnsToObject = 3, .entryDistance = 7000, .object = {50}};
	EXPECT_EQ(RoundTrip(circling).size(), k_States + 1 + 4 + 4 + 4 + 12);
	auto stepping = AVillager(0, 0, 0);
	stepping.values.move.moveState = k_StepThroughState;
	stepping.values.move.turnsToStepRebuild = int8_t {-1};
	EXPECT_EQ(RoundTrip(stepping).size(), k_States + 1);
	auto reacting = AVillager(0, 0, 0);
	reacting.values.living.reactionsDone = {{.reaction = 4, .turn = 100}};
	const auto bytes = RoundTrip(reacting);
	ASSERT_EQ(bytes.size(), k_States + 8);
	EXPECT_EQ(At<uint32_t>(bytes, k_Living + 63), 1u);
}

TEST(SaveVillagerRecords, ASpecialVillagerAddsTheRowOfItsKind)
{
	const auto villager = AVillager(0, 0, 0);
	SpecialVillagerRecord special {.thing = villager.thing,
	                               .position = villager.position,
	                               .object = villager.object,
	                               .mobile = villager.mobile,
	                               .values = villager.values,
	                               .infoIndex = 5};
	const auto bytes = RoundTrip(special);
	ASSERT_EQ(bytes.size(), k_States + 4);
	EXPECT_EQ(At<uint32_t>(bytes, k_States), 5u);
}
