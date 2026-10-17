/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The challenge records against the numbers the original game gives: one sequence of five snapshots run through the
// original's own record keeping, and the alignment change of a repeated snapshot. Wiki:
// docs/wiki/interface/temple-interior.md#the-challenge-list.

#include <array>
#include <bit>
#include <limits>
#include <utility>
#include <vector>

#include <ECS/ChallengeRecords.h>
#include <ECS/Components/ChallengeRecord.h>
#include <ECS/Registry.h>
#include <gtest/gtest.h>

namespace
{
namespace records = openblack::ecs::challenge_records;
namespace snapshots = openblack::script::challenge_snapshots;
using openblack::ecs::components::ChallengeRecord;
using openblack::lhvm::DataType;

snapshots::Value Thing(uint32_t id)
{
	snapshots::Value value;
	value.value.uintVal = id;
	value.type = DataType::Object;
	return value;
}

snapshots::Value Number(uint32_t n)
{
	snapshots::Value value;
	value.value.uintVal = n;
	value.type = DataType::Int;
	return value;
}

snapshots::Snapshot Start(uint32_t challenge, const char* script, uint32_t title, float alignment, float success,
                          glm::vec3 position, glm::vec3 focus, bool quest, std::vector<snapshots::Value> arguments)
{
	snapshots::Snapshot snapshot;
	snapshot.challenge = challenge;
	snapshot.reminderScript = script;
	snapshot.title = title;
	snapshot.alignment = alignment;
	snapshot.success = success;
	snapshot.position = position;
	snapshot.focus = focus;
	snapshot.quest = quest;
	snapshot.arguments = std::move(arguments);
	return snapshot;
}

struct Held
{
	std::vector<entt::entity> added;
	std::vector<entt::entity> removed;
	records::References references {.add = [this](entt::entity e) { added.push_back(e); },
	                                .remove = [this](entt::entity e) { removed.push_back(e); }};
};

entt::entity E(uint32_t id)
{
	return static_cast<entt::entity>(id);
}
} // namespace

TEST(ChallengeRecordsOriginal, FiveSnapshotsKeepTheRecordsAsTheOriginalDoes)
{
	openblack::ecs::Registry registry;
	Held held;
	constexpr uint32_t k_Turn = 1234;

	// A first start: no alignment change, its picture taken, its thing held
	auto outcome =
	    records::Record(registry, Start(12, "Introduction", 2456, 0.5f, 0.25f, {1, 2, 3}, {4, 5, 6}, false, {Thing(77)}),
	                    snapshots::Call::Start, k_Turn, held.references);
	EXPECT_FALSE(outcome.alignment.has_value());
	EXPECT_TRUE(outcome.takesPicture);
	EXPECT_EQ(held.added, (std::vector {E(77)}));

	// A second challenge goes after the first
	outcome =
	    records::Record(registry, Start(7, "Other", 100, -0.25f, 0.0f, {7, 8, 9}, {1, 1, 1}, true, {Number(5), Thing(88)}),
	                    snapshots::Call::Start, k_Turn, held.references);
	EXPECT_FALSE(outcome.alignment.has_value());
	EXPECT_EQ(held.added, (std::vector {E(77), E(88)}));

	// An update of the first, done, with no arguments: the alignment moves by the update's, the thing it held is let go,
	// the camera kept, the turn it was done on noted
	auto update = Start(12, "Introduction2", 2457, -0.75f, 1.0f, {9, 9, 9}, {8, 8, 8}, false, {});
	update.position.reset();
	update.focus.reset();
	outcome = records::Record(registry, update, snapshots::Call::Update, k_Turn, held.references);
	ASSERT_TRUE(outcome.alignment.has_value());
	EXPECT_EQ(*outcome.alignment, -0.75f);
	EXPECT_FALSE(outcome.takesPicture);
	EXPECT_EQ(held.removed, (std::vector {E(77)}));
	{
		const auto& record = registry.Get<ChallengeRecord>(outcome.record);
		EXPECT_EQ(record.reminderScript, "Introduction2");
		EXPECT_EQ(record.title, 2457u);
		EXPECT_EQ(record.success, 1.0f);
		EXPECT_EQ(record.alignment, -0.75f);
		EXPECT_TRUE(record.arguments.empty());
		EXPECT_EQ(record.position, glm::vec3(1, 2, 3));
		EXPECT_EQ(record.focus, glm::vec3(4, 5, 6));
		EXPECT_EQ(record.startedTurn, k_Turn);
		EXPECT_EQ(record.doneTurn, k_Turn);
	}

	// A start of the first again at 0.3: it stays done, takes the new camera and its new thing
	outcome = records::Record(registry, Start(12, "Intro3", 2458, 0.1f, 0.3f, {9, 9, 9}, {8, 8, 8}, false, {Thing(66)}),
	                          snapshots::Call::Start, k_Turn + 1, held.references);
	EXPECT_EQ(outcome.alignment, 0.1f);
	EXPECT_TRUE(outcome.takesPicture);
	EXPECT_EQ(held.added, (std::vector {E(77), E(88), E(66)}));
	{
		const auto& record = registry.Get<ChallengeRecord>(outcome.record);
		EXPECT_EQ(record.success, 1.0f);
		EXPECT_EQ(record.doneTurn, k_Turn + 1);
		EXPECT_EQ(record.position, glm::vec3(9, 9, 9));
		EXPECT_EQ(record.focus, glm::vec3(8, 8, 8));
	}

	// A picture of the second being taken: the reminder script and its arguments are cleared, the camera replaced, the
	// quest kept
	snapshots::Snapshot picture;
	picture.challenge = 7;
	picture.alignment = 1.0f;
	picture.success = 0.5f;
	picture.position = glm::vec3 {3, 3, 3};
	picture.focus = glm::vec3 {2, 2, 2};
	picture.takingPicture = true;
	outcome = records::Record(registry, picture, snapshots::Call::Picture, k_Turn, held.references);
	EXPECT_EQ(outcome.alignment, 1.0f);
	EXPECT_TRUE(outcome.takesPicture);
	EXPECT_EQ(held.removed, (std::vector {E(77), E(88)}));
	{
		const auto& record = registry.Get<ChallengeRecord>(outcome.record);
		EXPECT_EQ(record.reminderScript, "");
		EXPECT_EQ(record.title, 0u);
		EXPECT_TRUE(record.arguments.empty());
		EXPECT_EQ(record.success, 0.5f);
		EXPECT_EQ(record.position, glm::vec3(3, 3, 3));
		EXPECT_TRUE(record.quest);
		EXPECT_FALSE(record.doneTurn.has_value());
	}

	const auto ordered = records::InOrder(registry);
	ASSERT_EQ(ordered.size(), 2u);
	EXPECT_EQ(registry.Get<ChallengeRecord>(ordered[0]).challenge, 12u);
	EXPECT_EQ(registry.Get<ChallengeRecord>(ordered[1]).challenge, 7u);
}

TEST(ChallengeRecordsOriginal, ChallengesAreToldApartByTheirLowSixteenBits)
{
	openblack::ecs::Registry registry;
	records::Record(registry, Start(12, "A", 1, 0, 0, {}, {}, false, {}), snapshots::Call::Start, 1, {});
	const auto outcome =
	    records::Record(registry, Start(0x10000 + 12, "B", 2, 0, 0, {}, {}, false, {}), snapshots::Call::Start, 2, {});
	EXPECT_TRUE(outcome.alignment.has_value());
	EXPECT_EQ(records::InOrder(registry).size(), 1u);
}

TEST(ChallengeRecordsOriginal, AScriptRestartLetsGoOfEveryRecordsThingsThenEmptiesTheList)
{
	openblack::ecs::Registry registry;
	Held held;
	records::Record(registry, Start(12, "A", 1, 0, 0, {}, {}, false, {Thing(77), Number(3)}), snapshots::Call::Start, 1,
	                held.references);
	records::Record(registry, Start(7, "B", 2, 0, 0, {}, {}, false, {Thing(88), Thing(66)}), snapshots::Call::Start, 1,
	                held.references);
	records::Clear(registry, held.references);
	// In the order the challenges started, each record's things in its order
	EXPECT_EQ(held.removed, (std::vector {E(77), E(88), E(66)}));
	EXPECT_TRUE(records::InOrder(registry).empty());
	EXPECT_FALSE(records::Find(registry, 12).has_value());
	// A snapshot afterwards starts a new record, at the head
	const auto outcome = records::Record(registry, Start(7, "B", 2, 0, 0, {}, {}, false, {}), snapshots::Call::Start, 2, {});
	EXPECT_FALSE(outcome.alignment.has_value());
	EXPECT_EQ(registry.Get<ChallengeRecord>(outcome.record).order, 0u);
}

TEST(ChallengeRecordsOriginal, TheAlignmentChangeIsTheOriginals)
{
	struct Case
	{
		float player;
		float snapshot;
		float scale;
		uint32_t change;
	};
	// The player's alignment, the snapshot's, the game's script scale, and the change the original applies
	constexpr std::array<Case, 9> k_Cases {{
	    {0.0f, 0.9f, 0.1f, 0x3DB851EB},
	    {0.5f, 0.9f, 0.1f, 0x3D8A3D70},
	    {0.5f, -0.4f, 0.1f, 0xBD4CCCCE},
	    {-0.5f, -0.4f, 0.1f, 0xBCF5C290},
	    {0.3f, 1.0f, 0.07f, 0x3D73B646},
	    {-0.8f, 0.6f, 0.15f, 0x3E010625},
	    {0.99f, 1.0f, 1.0f, 0x3F0147AE},
	    {-0.95f, -1.0f, 1.0f, 0xBF066666},
	    {0.123f, -0.75f, 0.2f, 0xBE230BE1},
	}};
	for (const auto& c : k_Cases)
	{
		EXPECT_EQ(std::bit_cast<uint32_t>(records::AlignmentChange(c.player, c.snapshot, c.scale)), c.change)
		    << c.player << " " << c.snapshot << " " << c.scale;
	}
}

TEST(ChallengeRecordsOriginal, AScriptsNumbersAreKeptInRangeAsTheOriginalKeepsThem)
{
	const float nan = std::numeric_limits<float>::quiet_NaN();
	EXPECT_EQ(snapshots::ClampAlignment(nan), -1.0f);
	EXPECT_EQ(snapshots::ClampAlignment(-1.5f), -1.0f);
	EXPECT_EQ(snapshots::ClampAlignment(1.5f), 1.0f);
	EXPECT_EQ(snapshots::ClampAlignment(-0.25f), -0.25f);
	EXPECT_EQ(snapshots::ClampSuccess(nan), 0.0f);
	EXPECT_EQ(snapshots::ClampSuccess(-0.5f), 0.0f);
	EXPECT_EQ(snapshots::ClampSuccess(2.0f), 1.0f);
	EXPECT_EQ(snapshots::ClampSuccess(0.75f), 0.75f);
	// A start may hand its reminder twelve arguments, an update eleven
	EXPECT_EQ(snapshots::MaxArguments(snapshots::Call::Start), 12u);
	EXPECT_EQ(snapshots::MaxArguments(snapshots::Call::Update), 11u);
	EXPECT_EQ(snapshots::MaxArguments(snapshots::Call::Picture), 0u);
}
