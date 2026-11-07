/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The morph parser's creature block: what follows the animations in a creature's .cbn file and is not in the hand's
// .hbn file

#include <MorphFile.h>
#include <gtest/gtest.h>

#include "creature/SyntheticCreatureBlock.h"

using namespace openblack;
using namespace openblack::test::creature_block;

namespace
{
std::string FolderName()
{
	return std::string("openblack_test_morph_block_") + ::testing::UnitTest::GetInstance()->current_test_info()->name();
}

const std::vector<std::pair<std::string, std::vector<std::string>>> k_Sets {
    {"move", {"Cstand", "Wwalk", "Wrun"}},
    {"face", {"Nsmile"}},
};

Block ThreeClips(uint32_t version)
{
	Block block;
	block.version = version;
	block.clips = {Clip {.durationMs = 1000}, std::nullopt, Clip {.durationMs = 3000}, Clip {.durationMs = 4000}};
	return block;
}
} // namespace

TEST(MorphCreatureBlock, TheHandsFileHasNoCreatureBlock)
{
	const TempFolder folder(FolderName());
	WriteSpec(folder.Path(), false, 90, k_Sets);
	const auto block = Build(ThreeClips(0));
	morph::MorphFile file;
	ASSERT_EQ(file.Open(block, folder.Path()), morph::MorphResult::Success);
	EXPECT_EQ(file.GetBaseAnimationSet().size(), 3u);
	EXPECT_FALSE(file.GetCreatureActionPoints().has_value());
	EXPECT_FALSE(file.GetCreatureEyes().has_value());
	EXPECT_FALSE(file.GetTattooSites().has_value());
	EXPECT_FALSE(file.GetLeashBone().has_value());
	EXPECT_TRUE(file.GetSoundBankName().empty());
	EXPECT_TRUE(file.GetCreatureTail().empty());
	EXPECT_EQ(file.GetCreatureVersion(), 0u);
}

TEST(MorphCreatureBlock, TheSpecFileCanBeGivenByItsName)
{
	// The spec file the header names is asked for by its name and read from the bytes given, with Windows line ends
	// as the game's own spec files have them: the same animations as the spec file read from a folder
	const TempFolder folder(FolderName());
	WriteSpec(folder.Path(), false, 90, k_Sets);
	const auto block = Build(ThreeClips(0));
	morph::MorphFile fromFolder;
	ASSERT_EQ(fromFolder.Open(block, folder.Path()), morph::MorphResult::Success);

	const std::string spec = "90\r\n=move\r\nCstand\r\nWwalk\r\nWrun\r\n=face\r\nNsmile\r\nE";
	std::vector<std::string> asked;
	morph::MorphFile given;
	ASSERT_EQ(given.Open(block,
	                     [&asked, &spec](const std::string& name) -> std::optional<std::vector<uint8_t>> {
		                     asked.push_back(name);
		                     return std::vector<uint8_t>(spec.begin(), spec.end());
	                     }),
	          morph::MorphResult::Success);
	ASSERT_EQ(asked.size(), 1u);
	EXPECT_EQ(asked.front(), "hndspec90.txt");
	ASSERT_EQ(given.GetBaseAnimationSet().size(), fromFolder.GetBaseAnimationSet().size());
	for (size_t i = 0; i < 4; ++i)
	{
		const auto* a = given.GetBaseAnimation(i);
		const auto* b = fromFolder.GetBaseAnimation(i);
		ASSERT_EQ(a == nullptr, b == nullptr);
		if (a != nullptr)
		{
			EXPECT_EQ(a->name, b->name);
			EXPECT_EQ(a->setName, b->setName);
			EXPECT_EQ(a->header.duration, b->header.duration);
		}
	}
	EXPECT_EQ(given.GetBaseAnimation(2)->name, "Wrun");

	// a spec file that cannot be read
	morph::MorphFile none;
	EXPECT_EQ(none.Open(block, [](const std::string& /*name*/) { return std::optional<std::vector<uint8_t>> {}; }),
	          morph::MorphResult::ErrSpecFileCantOpen);
}

TEST(MorphCreatureBlock, Version21HasActionPointsEyesTattooSitesAndBank)
{
	const TempFolder folder(FolderName());
	WriteSpec(folder.Path(), true, 90, k_Sets);
	auto source = ThreeClips(21);
	source.eyes.scale = 0.75f;
	source.eyes.points[1] = {
	    .intersect = {.primitive = 2, .vertices = {3, 4, 5}, .vertexGroups = {0, 1, 2}, .u = 0.25f, .v = 0.5f},
	    .enabled = true,
	    .depth = 0.125f};
	source.eyes.lidAngles = {{{0.0f, 0.1f, 0.0f}, {0.0f, 0.6f, 0.0f}, {0.0f, 0.3f, 0.0f}}};
	for (size_t i = 0; i < source.sites.size(); ++i)
	{
		source.sites.at(i) = {.enabled = i % 2 == 0,
		                      .u = static_cast<uint8_t>(10 + i),
		                      .v = static_cast<uint8_t>(20 + i),
		                      .skin = static_cast<uint8_t>(i % 4),
		                      .size = 0.25f,
		                      .mirror = i == 2,
		                      .rotation = static_cast<uint32_t>(i % 4)};
	}
	morph::MorphFile file;
	ASSERT_EQ(file.Open(Build(source), folder.Path()), morph::MorphResult::Success);

	ASSERT_TRUE(file.GetCreatureActionPoints().has_value());
	const auto& points = *file.GetCreatureActionPoints();
	EXPECT_EQ(points.rightHand, 4);
	EXPECT_EQ(points.groin, 10);
	EXPECT_EQ(points.pickUpTime, 300);
	EXPECT_EQ(points.putDownTime, 800);
	EXPECT_EQ(points.kickTime, 900);
	EXPECT_EQ(points.unknownTime2, 910);
	EXPECT_EQ(file.GetLeashBone(), 11u);

	ASSERT_TRUE(file.GetCreatureEyes().has_value());
	const auto& eyes = *file.GetCreatureEyes();
	EXPECT_FLOAT_EQ(eyes.scale, 0.75f);
	EXPECT_FALSE(eyes.points[0].enabled);
	EXPECT_TRUE(eyes.points[1].enabled);
	EXPECT_EQ(eyes.points[1].intersect.primitive, 2u);
	EXPECT_EQ(eyes.points[1].intersect.vertices[2], 5u);
	EXPECT_FLOAT_EQ(eyes.points[1].depth, 0.125f);
	EXPECT_FLOAT_EQ(eyes.lidAngles[1][1], 0.6f);

	EXPECT_EQ(file.GetSoundBankName(), "ape_voice");
	ASSERT_TRUE(file.GetTattooSites().has_value());
	const auto& sites = *file.GetTattooSites();
	EXPECT_EQ(sites.size(), 8u);
	EXPECT_TRUE(sites[2].enabled);
	EXPECT_EQ(sites[2].u, 12);
	EXPECT_EQ(sites[2].v, 22);
	EXPECT_EQ(sites[2].skin, 2);
	EXPECT_TRUE(sites[2].mirror);
	EXPECT_EQ(sites[2].rotation, 2u);
	EXPECT_FALSE(sites[3].enabled);
	EXPECT_FALSE(sites[0].mirror);
}

TEST(MorphCreatureBlock, OlderVersionsStopBeforeTheEyes)
{
	const TempFolder folder(FolderName());
	WriteSpec(folder.Path(), true, 90, k_Sets);
	morph::MorphFile file;
	ASSERT_EQ(file.Open(Build(ThreeClips(13)), folder.Path()), morph::MorphResult::Success);
	ASSERT_TRUE(file.GetCreatureActionPoints().has_value());
	EXPECT_EQ(file.GetCreatureActionPoints()->eatTime, 600);
	EXPECT_EQ(file.GetLeashBone(), 11u);
	EXPECT_FALSE(file.GetCreatureEyes().has_value());
	EXPECT_FALSE(file.GetTattooSites().has_value());
	// the right eye is read with the leash bone, but the end of the block is kept only from the eyes' version on
	EXPECT_EQ(file.GetCreatureActionPoints()->rightEye, 12);
	EXPECT_TRUE(file.GetCreatureTail().empty());
	EXPECT_EQ(file.GetCreatureVersion(), 0u);
}

TEST(MorphCreatureBlock, TheEndOfTheBlockIsKeptWithItsVersion)
{
	const TempFolder folder(FolderName());
	WriteSpec(folder.Path(), true, 90, k_Sets);
	auto source = ThreeClips(20);
	source.tail = {7, 1, 0, 3, 2};
	morph::MorphFile file;
	ASSERT_EQ(file.Open(Build(source), folder.Path()), morph::MorphResult::Success);
	ASSERT_TRUE(file.GetCreatureActionPoints().has_value());
	EXPECT_EQ(file.GetCreatureActionPoints()->rightEye, 12);
	EXPECT_EQ(file.GetCreatureTail(), (std::vector<int32_t> {7, 1, 0, 3, 2}));
	EXPECT_EQ(file.GetCreatureVersion(), 20u);
}

TEST(MorphCreatureBlock, OnlyTheLastNumbersOfALongEndAreKept)
{
	const TempFolder folder(FolderName());
	WriteSpec(folder.Path(), true, 90, k_Sets);
	auto source = ThreeClips(21);
	source.tail.resize(600);
	for (size_t i = 0; i < source.tail.size(); ++i)
	{
		source.tail[i] = static_cast<int32_t>(i);
	}
	morph::MorphFile file;
	ASSERT_EQ(file.Open(Build(source), folder.Path()), morph::MorphResult::Success);
	const auto& tail = file.GetCreatureTail();
	ASSERT_EQ(tail.size(), 512u);
	EXPECT_EQ(tail.front(), 88);
	EXPECT_EQ(tail.back(), 599);
}

TEST(MorphCreatureBlock, TheIndexMapsSkipMissingAnimations)
{
	const TempFolder folder(FolderName());
	WriteSpec(folder.Path(), true, 90, k_Sets);
	morph::MorphFile file;
	ASSERT_EQ(file.Open(Build(ThreeClips(21)), folder.Path()), morph::MorphResult::Success);
	ASSERT_NE(file.GetBaseAnimation(0), nullptr);
	EXPECT_EQ(file.GetBaseAnimation(0)->header.duration, 1000u);
	EXPECT_EQ(file.GetBaseAnimation(1), nullptr);
	ASSERT_NE(file.GetBaseAnimation(2), nullptr);
	EXPECT_EQ(file.GetBaseAnimation(2)->header.duration, 3000u);
	EXPECT_EQ(file.GetBaseAnimation(2)->name, "Wrun");
	EXPECT_EQ(file.GetBaseAnimation(3)->setName, "face");
	EXPECT_EQ(file.GetBaseAnimation(4), nullptr);
	// no variant has animations of its own
	EXPECT_EQ(file.GetVariantAnimation(0, 0), nullptr);
}
